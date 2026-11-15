#ifndef PICOCOMPUTER3_CFLAGS_H
#define PICOCOMPUTER3_CFLAGS_H 1
// Board config for pico_shared HW_CONFIG 17: the Pico Computer 3 (PC3,
// https://github.com/UKTailwind/Pico-Computer-3), an RP2350B with 16 MB flash
// and 8 MB PSRAM (QMI CS1 = GPIO 47) — HSTX video on its HDMI connector, a
// PCM5102 I2S DAC on the audio jack, an SD card on SPI1, a four-port USB hub
// on the RP2350's native USB controller, and an I/O header with two NES/SNES
// controller ports (shared CLK/LAT) and a Wii extension port.
// The build uses PICO_BOARD=weact_studio_rp2350b_core, as pico_shared does for
// this config; the pico-sdk ships it, and the SD pins need its RP2350B pin
// tables.
// Pin values mirror pico-infonesPlus/pico_shared/BoardConfigs.cmake HW_CONFIG 17.
#define PICO_SCANVIDEO_COLOR_PIN_BASE 8
#define PICO_SCANVIDEO_PIXEL_RCOUNT 5
#define PICO_SCANVIDEO_PIXEL_GCOUNT 5
#define PICO_SCANVIDEO_PIXEL_BCOUNT 5
// also affects palette conversion for hstx! pico_hdmi expects RGB555.
#define PICO_SCANVIDEO_PIXEL_RSHIFT 10
#define PICO_SCANVIDEO_PIXEL_GSHIFT 5
#define PICO_SCANVIDEO_PIXEL_BSHIFT 0
#define PICO_SCANVIDEO_SYNC_PIN_BASE 6
// (The two pin bases are only read by pico_scanvideo_dpi, which HSTX builds do
// not link, so they do not claim GPIO 6/8 here.)
// SD pins on hardware SPI1 (GPIO 30 = SCK, 31 = TX, 28 = RX; CS 33 is a plain
// GPIO). GPIO 30/31 are only in the vendored pico_fatfs SPI1 table on RP2350B,
// hence the board header above. SD_TX = MOSI, SD_RX = MISO.
#define SD_TX 31
#define SD_RX 28
#define SD_SCK 30
#define SD_CS 33
#define USE_SD 1
#define USE_HSTX 1
// UART1 on GPIO 8/9, the board's serial console, so stdio and the UART keyboard
// input path stay on (UART_ENABLED 1 in BoardConfigs). The board header's
// default, UART0 on GPIO 12/13, would take two HSTX pins.
#define PICO_DEFAULT_UART 1
#define PICO_DEFAULT_UART_TX_PIN 8
#define PICO_DEFAULT_UART_RX_PIN 9
// I2S audio: PCM5102 DAC. Driver 2 = PICO_AUDIO_I2S_DRIVER_PCM5000A (see
// audio_i2s.h). Audio always ALSO goes out over HDMI; without a
// headphone-detect pin the build drives both sinks simultaneously (see
// i_picosound.c). BCK on GPIO 10, LRCK on GPIO 11, DIN on GPIO 22.
#define DOOM_AUDIO_I2S_DRIVER 2
#define PICO_AUDIO_I2S_DATA_PIN 22
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE 10
#define PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED 0
#define PICO_AUDIO_I2S_PIO 1
#define PICO_AUDIO_I2S_RESET_PIN -1
#define PICO_AUDIO_I2S_INTERRUPT_PIN -1
#define PICO_AUDIO_I2S_INTERRUPT_IS_BUTTON 0
// PCM510x has no volume register and benefits from the DC blocker in
// audio_i2s.c (see the I2S_AUDIO_COMPENSATE_DC_OFFSET note in audio_i2s.h).
#define I2S_AUDIO_COMPENSATE_DC_OFFSET 1
// Wii extension port (NES/SNES Classic Mini, Wii Classic Controller (Pro)) on
// the I/O header, read over I2C by src/pico/doom_wiipad.cpp: i2c0 on GPIO
// 20/21, the board's system I2C, which it shares with the RTC. Matches
// BoardConfigs.cmake HW_CONFIG 17. No TLV320 codec on this board; WIIPAD_I2C
// must be a valid i2c instance anyway because tlv320dac3100.c uses it as an
// i2c_inst_t* in always-compiled code.
#define WIIPAD_I2C i2c0
#define WII_PIN_SDA 20
#define WII_PIN_SCL 21
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
// stream rate, so both must be 48 kHz.
#undef PICO_SOUND_SAMPLE_FREQ
#define PICO_SOUND_SAMPLE_FREQ 48000
// HSTX lanes on the HDMI connector (BoardConfigs HW_CONFIG 17; inverted:
// D- = D+ - 1), the same as the Fruit Jam and the Murmulator M2. pico_hdmi
// (video_output.c) reads these.
#define GPIOHSTXCK 13
#define GPIOHSTXD0 15
#define GPIOHSTXD1 17
#define GPIOHSTXD2 19
#define GPIOHSTXINVERTED 1

// USB host: NO HAS_USBPIO on purpose. The RP2350's native controller is the
// host port, with the board's four-port hub on it (CFG_TUH_HUB 1, three
// devices). Keep in sync with -DENABLE_PIO_USB=0 in the picocomputer3 build
// scripts (a mismatch fails the build).

// pico_shared BoardConfigs.cmake identity of this board. REQUIRED: the
// vendored nespad.cpp selects its shared-CLK/LAT PIO program variant with
// "#if HW_CONFIG == 12 || HW_CONFIG == 13 || HW_CONFIG == 17".
#define HW_CONFIG 17
// Two legacy NES/SNES controller ports on the I/O header (SNES auto-detected),
// polled via PIO by the vendored pico_shared nespad driver — see
// i_usbhid.cpp. CLK (GPIO 4) and LAT (GPIO 5) are shared between the ports;
// only DATA differs. The driver runs two lockstep state machines released by
// the same PIO IRQ.
#define NES_PIN_CLK 4
#define NES_PIN_DATA 6
#define NES_PIN_LAT 5
#define NES_PIO pio1
#define NES_PIN_CLK_1 4
#define NES_PIN_DATA_1 7
#define NES_PIN_LAT_1 5
#define NES_PIO_1 pio1

// --- Status LEDs (src/pico/doom_leds.c) -------------------------------------
// -1 means the board does not have it, as with the NES pins above.
// The HEARTBEAT LED (jumper H5 on 1-2), blinked every 60 frames
// (LED_GPIO_PIN 0 = PICO_DEFAULT_LED_PIN 25 in BoardConfigs).
#define DOOM_LED_PIN 25
// No NeoPixels on this board, so no VU meter.
#define DOOM_VU_WS2812_PIN -1

// Move the WAD base address up so it lives past the bootloader app slot.
// Standalone build: 0x10080000 (image at 0x10000000, WHX right after the
// 512 KB slot).
// pico-bootLoader build (BUILD_FOR_BOOTLOADER=1): 0x10200000. The PC3 has
// 16 MB of flash, so this is the Fruit Jam map:
//   0x10000000 bootloader (512 KB)
//   0x10080000 emulator app slot (1.5 MB, DOOM_APP_SIZE=0x180000 in the build)
//   0x10200000 doom WHX -> ends 0x103B7898
// The WHX sits above the app slot so it survives re-flashing doom (or a smaller
// emulator). The build script passes matching -DDOOM_APP_SIZE / picotool -o;
// keep them in sync. See cmake/BootPartition.cmake for the full flash-map
// rationale.
#undef TINY_WAD_ADDR
#if BUILD_FOR_BOOTLOADER
#define TINY_WAD_ADDR 0x10200000
#else
#define TINY_WAD_ADDR 0x10080000
#endif

// --- SD card and PSRAM ------------------------------------------------------
// SD SPI instance for the vendored pico_fatfs driver (the SD_* pins above are
// valid hardware-SPI1 pins on RP2350B) and the PSRAM chip select: QMI CS1 =
// GPIO 47, 8 MB on the board. doom_tiny_full needs it; doom_tiny does not use
// it. Values mirror pico-infonesPlus/pico_shared/BoardConfigs.cmake
// HW_CONFIG 17.
// The card is used by every build, for save games and settings
// (src/pico/doom_sdcard.c); doom_tiny_full additionally streams the WHD off
// it at boot.
#define SDCARD_SPI spi1
#define SDCARD_PIO pio1
#define PSRAM_CS_PIN 47
#if WHD_LOAD_FROM_SD
// The full WHD is copied from /roms/doom/doom.whd into PSRAM at boot
// (src/pico/whd_sdload.c) and read zero-copy through the cached XIP CS1
// window — same address for standalone and bootloader builds.
#undef TINY_WAD_ADDR
#define TINY_WAD_ADDR 0x11000000
#endif

#endif
