#ifndef FRUITJAM_CFLAGS_H
#define FRUITJAM_CFLAGS_H 1
#define PICO_SCANVIDEO_COLOR_PIN_BASE 8
#define PICO_SCANVIDEO_PIXEL_RCOUNT 5
#define PICO_SCANVIDEO_PIXEL_GCOUNT 6
#define PICO_SCANVIDEO_PIXEL_BCOUNT 5
// also affects palette conversion for hstx!
#define PICO_SCANVIDEO_PIXEL_RSHIFT 11
#define PICO_SCANVIDEO_PIXEL_GSHIFT 5
#define PICO_SCANVIDEO_PIXEL_BSHIFT 0
#define PICO_SCANVIDEO_SYNC_PIN_BASE 6
#define SD_TX 35
#define SD_RX 36
#define SD_SCK 34
#define SD_CS 39
#define USE_SD 1
#define USE_HSTX 1
#define PICO_DEFAULT_UART 0
#define PICO_DEFAULT_UART_TX_PIN 44
#define PICO_DEFAULT_UART_RX_PIN 45
#define PICO_AUDIO_I2S_DATA_PIN 24
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE 26
#define HSTX_CKP 13
#define HSTX_D0P 15
#define HSTX_D1P 17
#define HSTX_D2P 19

// Move the WAD base address way up since we have plenty of flash
// Original builds were as low as 0x10040000, this gives an extra 256kB for code since we're chunky
#undef TINY_WAD_ADDR
#define TINY_WAD_ADDR 0x10080000

#endif
