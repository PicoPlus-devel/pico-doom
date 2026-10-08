#ifndef OLIMEXPICOPC_CFLAGS_H
#define OLIMEXPICOPC_CFLAGS_H 1
// Board config for pico_shared HW_CONFIG 15: Olimex RP2040-PICO-PC carrying a
// Raspberry Pi Pico 2 (RP2350A, 4 MB flash) — HSTX video on the HDMI socket,
// a USB-A host port on the Pico's native USB, an SD card on SPI0, one NES pad
// on the UEXT connector and a PWM audio jack. No I2S DAC. USB host runs on the
// RP2350's NATIVE USB controller (tinyusb) — build with -DENABLE_PIO_USB=0, see
// olimexpicopc-build.sh.
// Pin values mirror pico-infonesPlus/pico_shared/BoardConfigs.cmake HW_CONFIG 15.
#define PICO_SCANVIDEO_COLOR_PIN_BASE 8
#define PICO_SCANVIDEO_PIXEL_RCOUNT 5
#define PICO_SCANVIDEO_PIXEL_GCOUNT 5
#define PICO_SCANVIDEO_PIXEL_BCOUNT 5
// also affects palette conversion for hstx! pico_hdmi expects RGB555.
#define PICO_SCANVIDEO_PIXEL_RSHIFT 10
#define PICO_SCANVIDEO_PIXEL_GSHIFT 5
#define PICO_SCANVIDEO_PIXEL_BSHIFT 0
#define PICO_SCANVIDEO_SYNC_PIN_BASE 6
// SD pins (SPI0-capable; used by the doom_tiny_full WHD_LOAD_FROM_SD variant,
// wiring documentation otherwise). SD_TX = MOSI, SD_RX = MISO.
#define SD_TX 7
#define SD_RX 4
#define SD_SCK 6
#define SD_CS 22
#define USE_SD 1
#define USE_HSTX 1
// GPIO 0/1 — the pico2 default UART pins — are the board's PS/2 port. Keep
// the UART entirely off: NO_USE_UART disables the UART keyboard input path in
// i_input.c, and the build script passes -DDOOM_NO_STDIO_UART=1 so stdio
// never claims uart0 (matches UART_ENABLED 0 in BoardConfigs.cmake).
#define NO_USE_UART 1
// No I2S DAC on this board. Driver 0 = PICO_AUDIO_I2S_DRIVER_NONE (see
// audio_i2s.h): i_picosound.c then skips the I2S setup, pacing and fan-out
// entirely and the HDMI data-island ring alone paces the mixer. The I2S pins
// are unused (-1, as in BoardConfigs.cmake).
#define DOOM_AUDIO_I2S_DRIVER 0
#define PICO_AUDIO_I2S_DATA_PIN -1
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE -1
#define PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0
#define PICO_AUDIO_I2S_PIO 1
#define PICO_AUDIO_I2S_RESET_PIN -1
#define PICO_AUDIO_I2S_INTERRUPT_PIN -1
#define PICO_AUDIO_I2S_INTERRUPT_IS_BUTTON 0
// PWM audio jack: RC-filtered PWM, left on GPIO 28, right on GPIO 27, fed with
// the very samples that go to HDMI (pwm_audio_push() in pico_hdmi_glue.c's
// hstx_push_audio_sample), so both play at the same time. GPIO 23 high takes
// the Pico 2's SMPS out of its power-save mode, which adds audible hiss to
// the PWM output (i_main.c drives PICO_SMPS_MODE_PIN high already; the driver
// does the same on its own). Matches BoardConfigs.cmake HW_CONFIG 15.
#define PWM_AUDIO_PIN_L 28
#define PWM_AUDIO_PIN_R 27
#define PWM_AUDIO_SMPS_PIN 23
// No TLV320 codec and no Wii extension port on this board. The pins are unused
// (-1 is fine: the codec init never runs without an I2S driver, and
// doom_wiipad.cpp compiles away), but WIIPAD_I2C must stay a valid i2c
// instance: tlv320dac3100.c uses it as an i2c_inst_t* in always-compiled code.
#define WIIPAD_I2C i2c1
#define WII_PIN_SDA -1
#define WII_PIN_SCL -1
// Pin the pico_hdmi audio Data-Island ring at 0x20076000. That address is
// SHORTPTR_BASE + 0x40000 — the top of Doom's zone heap (see i_system.c's
// I_ZoneBase()) — and __HeapLimit is 0x20080000, so the 36 KB ring lives in
// the 40 KB SRAM gap between the zone and the SCRATCH banks. Neither Doom's
// zone allocator nor the pico-sdk's stack region touches this address, so
// the ring costs zero zone bytes AND zero .bss bytes. The vendored
// hstx_data_island_queue.c honors this at compile time; without it, it
// would malloc from the zone (its historical behaviour).
#define HSTX_DI_RING_ADDRESS 0x20076000u

// Override the emu8950 OPL native rate (49716 Hz) with a standard HDMI-audio
// rate. pico_hdmi's ACR N/CTS table only covers 32k/44.1k/48k/88.2k/96k/etc.,
// and 49716 falls through to the 48 kHz default — sink plays at 48000 while
// we push at 49716, causing constant FIFO overruns that sound like static.
//
// 48000 Hz is the exact-lock rate for a 25.2 MHz pixel clock (N=6144,
// CTS=25200) AND matches the sample-frequency code baked into
// hstx_packet.c's channel_status_bit[] table. Strict HDMI monitors require
// the IEC 60958 channel status sample-rate field to agree with the actual
// stream rate, so both must be 48 kHz. It is also an exact PWM rate:
// 378 MHz / 48 kHz = 7875 clk_sys cycles per sample.
#undef PICO_SOUND_SAMPLE_FREQ
#define PICO_SOUND_SAMPLE_FREQ 48000
// HSTX lanes for the Olimex RP2040-PICO-PC (BoardConfigs HW_CONFIG 15;
// inverted: D- = D+ - 1). Note D1 and D2 are swapped compared with the Fruit
// Jam and the Murmulator M2. pico_hdmi (video_output.c) reads these.
#define GPIOHSTXCK 13
#define GPIOHSTXD0 15
#define GPIOHSTXD1 19
#define GPIOHSTXD2 17
#define GPIOHSTXINVERTED 1

// Native USB host: no HAS_USBPIO / PIN_USB_HOST_* here. tusb_config.h sees
// HAS_USBPIO undefined and selects the native RP2350 controller (rhport 0),
// which the board routes to its USB-A socket.

// pico_shared BoardConfigs.cmake identity of this board. Consumed by the
// vendored nespad.cpp to pick its PIO program variant; 15 takes the original
// program (separate CLK/LAT per port).
#define HW_CONFIG 15
// One legacy NES/SNES controller port (SNES auto-detected) on the UEXT
// connector, polled via PIO by the vendored pico_shared nespad driver — see
// i_usbhid.cpp. No second port; -1 disables it.
#define NES_PIN_CLK 5
#define NES_PIN_DATA 20
#define NES_PIN_LAT 9
#define NES_PIO pio1
#define NES_PIN_CLK_1 -1
#define NES_PIN_DATA_1 -1
#define NES_PIN_LAT_1 -1
#define NES_PIO_1 pio1

// --- Status LEDs (src/pico/doom_leds.c) -------------------------------------
// -1 means the board does not have it, as with the NES pins above.
// Plain onboard LED, blinked every 60 frames. = PICO_DEFAULT_LED_PIN of the
// Pico 2 the board carries.
#define DOOM_LED_PIN 25
// No NeoPixels on this board, so no VU meter.
#define DOOM_VU_WS2812_PIN -1

// Move the WAD base address up so it lives past the bootloader app slot.
// Standalone build: 0x10080000 (image at 0x10000000, WHX right after the
// 512 KB slot — fits a genuine 4 MB Pico 2).
// pico-bootLoader build (BUILD_FOR_BOOTLOADER=1): 0x10200000. This is a Pico 2
// board, so the whole bootloader map is capped to the 4 MB chip:
//   0x10000000 bootloader (512 KB)
//   0x10080000 emulator app slot (1.5 MB, DOOM_APP_SIZE=0x180000 in the build)
//   0x10200000 doom WHX -> ends 0x103B7898
//   0x103BF000 pico-launcher (the last 260 KB of flash; BoardConfigs.cmake's
//              FLASH_RESERVED_TOP) -> nothing of Doom's may go here
// The WHX clears pico-launcher by ~30 KB, so it must not grow by more than
// that. The WHX sits above the app slot so it survives re-flashing doom (or a
// smaller emulator). The build script passes matching -DDOOM_APP_SIZE /
// -DDOOM_FLASH_TOTAL / picotool -o; keep all four in sync.
// See cmake/BootPartition.cmake for the full flash-map rationale.
#undef TINY_WAD_ADDR
#if BUILD_FOR_BOOTLOADER
#define TINY_WAD_ADDR 0x10200000
#else
#define TINY_WAD_ADDR 0x10080000
#endif

// --- SD card and PSRAM ------------------------------------------------------
// SD SPI instance for the vendored pico_fatfs driver (the SD_* pins above are
// valid hardware-SPI0 pins; CS is a plain GPIO) and the PSRAM chip select:
// QMI CS1 = GPIO 8. A stock Pico 2 has no PSRAM, so doom_tiny_full needs one
// fitted on GPIO 8; doom_tiny does not use it. Values mirror
// pico-infonesPlus/pico_shared/BoardConfigs.cmake HW_CONFIG 15.
// The card is used by every build, for save games and settings
// (src/pico/doom_sdcard.c); doom_tiny_full additionally streams the WHD off
// it at boot.
#define SDCARD_SPI spi0
#define SDCARD_PIO pio1
#define PSRAM_CS_PIN 8
#if WHD_LOAD_FROM_SD
// The full WHD is copied from /roms/doom/doom.whd into PSRAM at boot
// (src/pico/whd_sdload.c) and read zero-copy through the cached XIP CS1
// window — same address for standalone and bootloader builds.
#undef TINY_WAD_ADDR
#define TINY_WAD_ADDR 0x11000000
#endif

#endif
