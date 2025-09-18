/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Scott Shawcroft for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "pico/stdlib.h"
#include <stdlib.h>

// This is from: https://github.com/raspberrypi/pico-examples-rp2350/blob/a1/hstx/dvi_out_hstx_encoder/dvi_out_hstx_encoder.c

#include "hardware/dma.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/clocks.h"

typedef struct {
    uint32_t *framebuffer;
    size_t framebuffer_len; // in words
    uint32_t *dma_commands;
    size_t dma_commands_len; // in words
    uint32_t width;
    uint32_t height;
    uint32_t output_width;
    uint16_t pitch; // Number of words between rows. (May be more than a width's worth.)
    uint8_t color_depth;
    int dma_pixel_channel;
    int dma_command_channel;
    int frameno;
} picodvi_framebuffer_obj_t;

// ----------------------------------------------------------------------------
// DVI constants

#define DMA_IRQ_HSTX (DMA_IRQ_2)

#define TMDS_CTRL_00 0x354u
#define TMDS_CTRL_01 0x0abu
#define TMDS_CTRL_10 0x154u
#define TMDS_CTRL_11 0x2abu

#define SYNC_V0_H0 (TMDS_CTRL_00 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V0_H1 (TMDS_CTRL_01 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H0 (TMDS_CTRL_10 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H1 (TMDS_CTRL_11 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))

#define MODE_720_H_SYNC_POLARITY 0
#define MODE_720_H_FRONT_PORCH   8
#define MODE_720_H_SYNC_WIDTH    32
#define MODE_720_H_BACK_PORCH    40
#define MODE_720_H_ACTIVE_PIXELS 720

#define MODE_720_V_SYNC_POLARITY 0
#define MODE_720_V_FRONT_PORCH   3
#define MODE_720_V_SYNC_WIDTH    4
#define MODE_720_V_BACK_PORCH    218
#define MODE_720_V_ACTIVE_LINES  400

#define MODE_640_H_SYNC_POLARITY 0
#define MODE_640_H_FRONT_PORCH   16
#define MODE_640_H_SYNC_WIDTH    96
#define MODE_640_H_BACK_PORCH    48
#define MODE_640_H_ACTIVE_PIXELS 640

#define MODE_640_V_SYNC_POLARITY 0
#define MODE_640_V_FRONT_PORCH   10
#define MODE_640_V_SYNC_WIDTH    2
#define MODE_640_V_BACK_PORCH    33
#define MODE_640_V_ACTIVE_LINES  480

#define MODE_720_V_TOTAL_LINES  ( \
    MODE_720_V_FRONT_PORCH + MODE_720_V_SYNC_WIDTH + \
    MODE_720_V_BACK_PORCH + MODE_720_V_ACTIVE_LINES \
    )
#define MODE_640_V_TOTAL_LINES  ( \
    MODE_640_V_FRONT_PORCH + MODE_640_V_SYNC_WIDTH + \
    MODE_640_V_BACK_PORCH + MODE_640_V_ACTIVE_LINES \
    )

#define HSTX_CMD_RAW         (0x0u << 12)
#define HSTX_CMD_RAW_REPEAT  (0x1u << 12)
#define HSTX_CMD_TMDS        (0x2u << 12)
#define HSTX_CMD_TMDS_REPEAT (0x3u << 12)
#define HSTX_CMD_NOP         (0xfu << 12)

// ----------------------------------------------------------------------------
// HSTX command lists

#define VSYNC_LEN 6
#define VACTIVE_LEN 9

static uint32_t vblank_line640_vsync_off[VSYNC_LEN] = {
    HSTX_CMD_RAW_REPEAT | MODE_640_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT | MODE_640_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_640_H_BACK_PORCH + MODE_640_H_ACTIVE_PIXELS),
    SYNC_V1_H1
};

static uint32_t vblank_line640_vsync_on[VSYNC_LEN] = {
    HSTX_CMD_RAW_REPEAT | MODE_640_H_FRONT_PORCH,
    SYNC_V0_H1,
    HSTX_CMD_RAW_REPEAT | MODE_640_H_SYNC_WIDTH,
    SYNC_V0_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_640_H_BACK_PORCH + MODE_640_H_ACTIVE_PIXELS),
    SYNC_V0_H1
};

static uint32_t vactive_line640[VACTIVE_LEN] = {
    HSTX_CMD_RAW_REPEAT | MODE_640_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_640_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_640_H_BACK_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_TMDS | MODE_640_H_ACTIVE_PIXELS
};

static uint32_t vblank_line720_vsync_off[VSYNC_LEN] = {
    HSTX_CMD_RAW_REPEAT | MODE_720_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT | MODE_720_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_720_H_BACK_PORCH + MODE_720_H_ACTIVE_PIXELS),
    SYNC_V1_H1
};

static uint32_t vblank_line720_vsync_on[VSYNC_LEN] = {
    HSTX_CMD_RAW_REPEAT | MODE_720_H_FRONT_PORCH,
    SYNC_V0_H1,
    HSTX_CMD_RAW_REPEAT | MODE_720_H_SYNC_WIDTH,
    SYNC_V0_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_720_H_BACK_PORCH + MODE_720_H_ACTIVE_PIXELS),
    SYNC_V0_H1
};

static uint32_t vactive_line720[VACTIVE_LEN] = {
    HSTX_CMD_RAW_REPEAT | MODE_720_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_720_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_720_H_BACK_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_TMDS | MODE_720_H_ACTIVE_PIXELS
};

picodvi_framebuffer_obj_t *active_picodvi = NULL;
picodvi_framebuffer_obj_t picodvi;

static void __not_in_flash_func(dma_irq_handler)(void) {
__asm__("nop");
    if (active_picodvi == NULL) {
        return;
    }

    uint ch_num = active_picodvi->dma_pixel_channel;
    dma_hw->intr = 1u << ch_num;

    active_picodvi->frameno++;
    // Set the read_addr back to the start and trigger the first transfer (which
    // will trigger the pixel channel).
    dma_channel_hw_t *ch = &dma_hw->ch[active_picodvi->dma_command_channel];
    ch->al3_read_addr_trig = (uintptr_t)active_picodvi->dma_commands;
}

bool common_hal_picodvi_framebuffer_construct(picodvi_framebuffer_obj_t *self,
    uint32_t width, uint32_t height,
    int clk_dp, int red_dp, int green_dp, int blue_dp,
    uint32_t color_depth) {
    if (active_picodvi != NULL) {
        printf("in use\n");
        return false;
    }

    uint f_pll_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY);
uint32_t freq = 125875000; // what is this nonsense??
    clock_configure(clk_hstx, 0, CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLK_SYS, clock_get_hz(clk_sys), freq); 
printf("Note: HSTX frequency=%d vs ideal 126MHz\n", (int)clock_get_hz (clk_hstx));
printf("Note: system clock =%d\n", (int)clock_get_hz (clk_sys));
printf("Note: system pll =%d\n", f_pll_sys);
    self->dma_command_channel = -1;
    self->dma_pixel_channel = -1;

    self->output_width = 640;
    size_t output_scaling = self->output_width / width;

    self->width = width;
    self->height = 200;
    self->color_depth = color_depth;
    // Pitch is number of 32-bit words per line. We round up pitch_bytes to the nearest word
    // so that each scanline begins on a natural 32-bit word boundary.
    size_t pitch_bytes = (self->width * color_depth) / 8;
    self->pitch = (pitch_bytes + sizeof(uint32_t) - 1) / sizeof(uint32_t);
    size_t framebuffer_size = self->pitch * self->height;

    // We check that allocations aren't in PSRAM because we haven't added XIP
    // streaming support.
    self->framebuffer = (uint32_t *)malloc(framebuffer_size * sizeof(uint32_t));
    if (self->framebuffer == NULL || ((size_t)self->framebuffer & 0xf0000000) == 0x10000000) {
        return false;
    }
    memset(self->framebuffer, 0, framebuffer_size * sizeof(uint32_t));

    // We compute all DMA transfers needed for a single frame. This ensure we don't have any super
    // quick interrupts that we need to respond to. Each transfer takes two words, trans_count and
    // read_addr. Active pixel lines need two transfers due to different read addresses. When pixel
    // doubling, then we must also set transfer size.
    size_t dma_command_size = 2;
    if (output_scaling > 1) {
        dma_command_size = 4;
    }

    if (self->output_width == 640) {
        self->dma_commands_len = (MODE_640_V_FRONT_PORCH + MODE_640_V_SYNC_WIDTH + MODE_640_V_BACK_PORCH + 2 * MODE_640_V_ACTIVE_LINES + 1) * dma_command_size;
    } else {
        self->dma_commands_len = (MODE_720_V_FRONT_PORCH + MODE_720_V_SYNC_WIDTH + MODE_720_V_BACK_PORCH + 2 * MODE_720_V_ACTIVE_LINES + 1) * dma_command_size;
    }
    self->dma_commands = (uint32_t *)malloc(self->dma_commands_len * sizeof(uint32_t));
    if (self->dma_commands == NULL || ((size_t)self->dma_commands & 0xf0000000) == 0x10000000) {
        free(self->framebuffer);
        return false;
    }

    // The command channel and the pixel channel form a pipeline that feeds combined HSTX
    // commands and pixel data to the HSTX FIFO. The command channel reads a pre-computed
    // list of control/status words from the dma_commands buffer and writes them to the
    // pixel channel's control/status registers. Under control of the command channel, the
    // pixel channel smears/swizzles pixel data from the framebuffer and combines
    // it with HSTX commands, forwarding the combined stream to the HSTX FIFO.

    self->dma_pixel_channel = dma_claim_unused_channel(false);
    self->dma_command_channel = dma_claim_unused_channel(false);
    if (self->dma_pixel_channel < 0 || self->dma_command_channel < 0) {
        free(self->framebuffer);
        free(self->dma_commands);
        return false;
    }

    size_t command_word = 0;
    size_t frontporch_start;
    if (self->output_width == 640) {
        frontporch_start = MODE_640_V_TOTAL_LINES - MODE_640_V_FRONT_PORCH;
    } else {
        frontporch_start = MODE_720_V_TOTAL_LINES - MODE_720_V_FRONT_PORCH;
    }
    size_t frontporch_end = frontporch_start;
    if (self->output_width == 640) {
        frontporch_end += MODE_640_V_FRONT_PORCH;
    } else {
        frontporch_end += MODE_720_V_FRONT_PORCH;
    }
    size_t vsync_start = 0;
    size_t vsync_end = vsync_start;
    if (self->output_width == 640) {
        vsync_end += MODE_640_V_SYNC_WIDTH;
    } else {
        vsync_end += MODE_720_V_SYNC_WIDTH;
    }
    size_t backporch_start = vsync_end;
    size_t backporch_end = backporch_start;
    if (self->output_width == 640) {
        backporch_end += MODE_640_V_BACK_PORCH;
    } else {
        backporch_end += MODE_720_V_BACK_PORCH;
    }
    size_t active_start = backporch_end;

    uint32_t dma_ctrl = self->dma_command_channel << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB |
        DREQ_HSTX << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB |
        DMA_CH0_CTRL_TRIG_IRQ_QUIET_BITS |
        DMA_CH0_CTRL_TRIG_INCR_READ_BITS |
        DMA_CH0_CTRL_TRIG_EN_BITS;
    uint32_t dma_pixel_ctrl;
    if (output_scaling > 1) {
        // We do color_depth size transfers when pixel doubling. The memory bus will
        // duplicate the bytes read to produce 32 bits for the HSTX.
        if (color_depth == 32) {
            dma_pixel_ctrl = dma_ctrl | DMA_SIZE_32 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB;
        } else if (color_depth == 16) {
            dma_pixel_ctrl = dma_ctrl | DMA_SIZE_16 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB;
        } else {
            dma_pixel_ctrl = dma_ctrl | DMA_SIZE_8 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB;
        }
    } else {
        dma_pixel_ctrl = dma_ctrl | DMA_SIZE_32 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB;
    }
    if (self->color_depth == 16) {
        dma_pixel_ctrl |= DMA_CH0_CTRL_TRIG_BSWAP_BITS;
    }
    dma_ctrl |= DMA_SIZE_32 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB;

    uint32_t dma_write_addr = (uint32_t)&hstx_fifo_hw->fifo;
    // Write ctrl and write_addr once when not pixel doubling because they don't
    // change. (write_addr doesn't change when pixel doubling either but we need
    // to rewrite it because it is after the ctrl register.)
    if (output_scaling == 1) {
        dma_channel_hw_addr(self->dma_pixel_channel)->al1_ctrl = dma_ctrl;
        dma_channel_hw_addr(self->dma_pixel_channel)->al1_write_addr = dma_write_addr;
    }

    uint32_t *vblank_line_vsync_on = self->output_width == 640 ?  vblank_line640_vsync_on : vblank_line720_vsync_on;
    uint32_t *vblank_line_vsync_off = self->output_width == 640 ?  vblank_line640_vsync_off : vblank_line720_vsync_off;
    uint32_t *vactive_line = self->output_width == 640 ?  vactive_line640 : vactive_line720;

    size_t mode_v_total_lines;
    if (self->output_width == 640) {
        mode_v_total_lines = MODE_640_V_TOTAL_LINES;
    } else {
        mode_v_total_lines = MODE_720_V_TOTAL_LINES;
    }

    for (size_t v_scanline = 0; v_scanline < mode_v_total_lines; v_scanline++) {
        if (output_scaling > 1) {
            self->dma_commands[command_word++] = dma_ctrl;
            self->dma_commands[command_word++] = dma_write_addr;
        }
        if (vsync_start <= v_scanline && v_scanline < vsync_end) {
            self->dma_commands[command_word++] = VSYNC_LEN;
            self->dma_commands[command_word++] = (uintptr_t)vblank_line_vsync_on;
        } else if (backporch_start <= v_scanline && v_scanline < backporch_end) {
            self->dma_commands[command_word++] = VSYNC_LEN;
            self->dma_commands[command_word++] = (uintptr_t)vblank_line_vsync_off;
        } else if (frontporch_start <= v_scanline && v_scanline < frontporch_end) {
            self->dma_commands[command_word++] = VSYNC_LEN;
            self->dma_commands[command_word++] = (uintptr_t)vblank_line_vsync_off;
        } else {
            self->dma_commands[command_word++] = VACTIVE_LEN;
            self->dma_commands[command_word++] = (uintptr_t)vactive_line;
            size_t row = v_scanline - active_start;
            size_t transfer_count = self->pitch;
            if (output_scaling > 1) {
                self->dma_commands[command_word++] = dma_pixel_ctrl;
                self->dma_commands[command_word++] = dma_write_addr;
                row = row * 200 / 480;
                // When pixel scaling, we do one transfer per pixel and it gets
                // mirrored into the rest of the word.
                transfer_count = self->width;
            }
            self->dma_commands[command_word++] = transfer_count;
            uint32_t *row_start = &self->framebuffer[row * self->pitch];
            self->dma_commands[command_word++] = (uintptr_t)row_start;
        }
    }
    // Last command is NULL which will trigger an IRQ.
    if (output_scaling > 1) {
        self->dma_commands[command_word++] = DMA_CH0_CTRL_TRIG_IRQ_QUIET_BITS |
            DMA_CH0_CTRL_TRIG_EN_BITS;
        self->dma_commands[command_word++] = 0;
    }
    self->dma_commands[command_word++] = 0;
    self->dma_commands[command_word++] = 0;

    if (color_depth == 32) {
        // Configure HSTX's TMDS encoder for RGB888
        hstx_ctrl_hw->expand_tmds =
            7 << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
                16 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB |
                7 << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
                8 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB |
                7 << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
                0 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;
    } else if (color_depth == 16) {
        // Configure HSTX's TMDS encoder for RGB565
        hstx_ctrl_hw->expand_tmds =
            4 << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
                0 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB |
                5 << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
                27 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB |
                4 << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
                21 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;
    } else if (color_depth == 8) {
        // Configure HSTX's TMDS encoder for RGB332
        hstx_ctrl_hw->expand_tmds =
            2 << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
                0 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB |
                2 << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
                29 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB |
                1 << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
                26 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;
    } else if (color_depth == 4) {
        // Configure HSTX's TMDS encoder for RGBD
        hstx_ctrl_hw->expand_tmds =
            0 << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
                28 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB |
                0 << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
                27 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB |
                0 << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
                26 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;
    } else {
        // Grayscale
        uint8_t rot = 24 + color_depth;
        hstx_ctrl_hw->expand_tmds =
            (color_depth - 1) << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
                rot << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB |
                    (color_depth - 1) << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
                rot << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB |
                    (color_depth - 1) << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
                rot << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;
    }
    size_t pixels_per_word;
    if (output_scaling == 1) {
        pixels_per_word = 32 / color_depth;
    } else {
        pixels_per_word = 1;
    }

    size_t shifts_before_empty = (pixels_per_word % 32);
    if (output_scaling > 1) {
        shifts_before_empty *= output_scaling;
    }

    size_t shift_amount = color_depth % 32;

    // Pixels come in 32 bits at a time. color_depth dictates the number
    // of pixels per word. Control symbols (RAW) are an entire 32-bit word.
    hstx_ctrl_hw->expand_shift =
        shifts_before_empty << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
            shift_amount << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
            1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
            0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

    // Serial output config: clock period of 5 cycles, pop from command
    // expander every 5 cycles, shift the output shiftreg by 2 every cycle.
    hstx_ctrl_hw->csr = 0;
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        5u << HSTX_CTRL_CSR_CLKDIV_LSB |
            5u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
            2u << HSTX_CTRL_CSR_SHIFT_LSB |
            HSTX_CTRL_CSR_EN_BITS;

    // Note we are leaving the HSTX clock at the SDK default of 125 MHz; since
    // we shift out two bits per HSTX clock cycle, this gives us an output of
    // 250 Mbps, which is very close to the bit clock for 480p 60Hz (252 MHz).
    // If we want the exact rate then we'll have to reconfigure PLLs.

    // Assign clock pair to two neighbouring pins:
#define HSTX_FIRST_PIN 12 
    {   
    int bit = clk_dp - HSTX_FIRST_PIN;
    hstx_ctrl_hw->bit[bit    ] = HSTX_CTRL_BIT0_CLK_BITS;
    hstx_ctrl_hw->bit[bit ^ 1] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;
    }       
            

    const int pinout[] = { red_dp, green_dp, blue_dp };

    for (uint lane = 0; lane < 3; ++lane) {
        // For each TMDS lane, assign it to the correct GPIO pair based on the
        // desired pinout:
        int bit = pinout[lane] - HSTX_FIRST_PIN;
        // Output even bits during first half of each HSTX cycle, and odd bits
        // during second half. The shifter advances by two bits each cycle.
        uint32_t lane_data_sel_bits =
            (lane * 10    ) << HSTX_CTRL_BIT0_SEL_P_LSB |
            (lane * 10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB;
        // The two halves of each pair get identical data, but one pin is inverted.
        hstx_ctrl_hw->bit[bit    ] = lane_data_sel_bits;
        hstx_ctrl_hw->bit[bit ^ 1] = lane_data_sel_bits | HSTX_CTRL_BIT0_INV_BITS;
}
    
    for (int i = 12; i <= 19; ++i) {
        gpio_set_function(i, GPIO_FUNC_HSTX);
        gpio_set_drive_strength(i, GPIO_DRIVE_STRENGTH_4MA);
    }

    dma_channel_config c;
    c = dma_channel_get_default_config(self->dma_command_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, true);
    // This wraps the transfer back to the start of the write address.
    size_t wrap = 3; // 8 bytes because we write two DMA registers.
    volatile uint32_t *write_addr = &dma_hw->ch[self->dma_pixel_channel].al3_transfer_count;
    if (output_scaling > 1) {
        wrap = 4; // 16 bytes because we write all four DMA registers.
        write_addr = &dma_hw->ch[self->dma_pixel_channel].al3_ctrl;
    }
    channel_config_set_ring(&c, true, wrap);
    // No chain because we use an interrupt to reload this channel instead of a
    // third channel.
    dma_channel_configure(
        self->dma_command_channel,
        &c,
        write_addr,
        self->dma_commands,
        (1 << wrap) / sizeof(uint32_t),
        false
        );

    // ack any previously pending IRQ
    dma_hw->irq_ctrl[DMA_IRQ_HSTX - DMA_IRQ_0].ints = (1u << self->dma_pixel_channel);
    // enable the interrupt
    dma_hw->irq_ctrl[DMA_IRQ_HSTX - DMA_IRQ_0].inte = (1u << self->dma_pixel_channel);
    irq_set_exclusive_handler(DMA_IRQ_HSTX, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_HSTX, true);
    irq_set_priority(DMA_IRQ_HSTX, PICO_HIGHEST_IRQ_PRIORITY);

    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

    // For the output.
    self->framebuffer_len = framebuffer_size;

    active_picodvi = self;

    dma_irq_handler();
    return true;
}
