add_library(common_pico INTERFACE)
target_sources(common_pico INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/doom_boot.c
        ${CMAKE_CURRENT_LIST_DIR}/i_glob.c
        ${CMAKE_CURRENT_LIST_DIR}/i_input.c
        ${CMAKE_CURRENT_LIST_DIR}/i_system.c
        ${CMAKE_CURRENT_LIST_DIR}/piconet.c
        ${CMAKE_CURRENT_LIST_DIR}/i_timer.c
        ${CMAKE_CURRENT_LIST_DIR}/i_video.c
        ${CMAKE_CURRENT_LIST_DIR}/pico_hdmi_glue.c
        ${CMAKE_CURRENT_LIST_DIR}/stubs.c

        ${CMAKE_CURRENT_LIST_DIR}/i_picosound.c
)
if (PICO_ON_DEVICE)
    target_sources(common_pico INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/blit.S
            ${CMAKE_CURRENT_LIST_DIR}/doom_errscreen.c
            ${CMAKE_CURRENT_LIST_DIR}/doom_leds.c
            ${CMAKE_CURRENT_LIST_DIR}/doom_sdcard.c
            ${CMAKE_CURRENT_LIST_DIR}/doom_settings.c
            ${CMAKE_CURRENT_LIST_DIR}/doom_wiipad.cpp
            ${CMAKE_CURRENT_LIST_DIR}/i_usbhid.cpp
            )
    # WS2812 program for the Fruit Jam VU meter, a verbatim copy of
    # pico_shared/ws2812.pio. Generated here rather than next to video_doom.pio
    # below because doom_leds.c is its only consumer and is device-only; the
    # boards without a strip set DOOM_VU_WS2812_PIN -1 and never include the
    # generated header.
    pico_generate_pio_header(common_pico ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)
    pico_wrap_function(common_pico malloc)
    pico_wrap_function(common_pico calloc)
    pico_wrap_function(common_pico free)
    target_compile_definitions(common_pico INTERFACE
            PICO_HEAP_SIZE=0
            USE_ZONE_FOR_MALLOC=1
    )
    # hardware_i2c: TLV320 codec and Wii pad I2C. pico_shared_nespad: legacy
    # NES/SNES pads on PIO — compiled everywhere, only polled when the board's
    # cflags header defines NES_PIN_CLK != -1 (see i_usbhid.cpp).
    # pico_shared_wiipad: Wii-extension pads (NES/SNES Classic Mini) on that
    # same I2C bus, under the matching WII_PIN_SDA/SCL != -1 convention; it
    # needs i2c_bus_recovery for the shared-bus dance in doom_wiipad.cpp.
    # pico_fatfs: save games and settings live on the SD card on every board,
    # so the driver is no longer specific to the WHD_LOAD_FROM_SD build.
    # hardware_pio: the WS2812 VU meter in doom_leds.c, which must not depend
    # on the USE_HSTX branch below for its link.
    target_link_libraries(common_pico INTERFACE  hardware_i2c pico_shared_nespad
            pico_shared_wiipad i2c_bus_recovery pico_fatfs hardware_pio)
endif()
target_include_directories(common_pico INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/..
        ${CMAKE_CURRENT_LIST_DIR}/../../textscreen
        )

target_compile_definitions(common_pico INTERFACE
        PICO_USE_SW_SPIN_LOCKS=0
        PICO_USE_STACK_GUARDS=0 # todo we can actually use these sensibly, but right now we do overflow!
)

# hardware_watchdog: doom_boot.c resets the chip on quit (it only arrived
# transitively before).
target_link_libraries(common_pico INTERFACE pico_stdlib pico_multicore hardware_watchdog)

if (USE_HSTX)
    target_compile_definitions(common_pico INTERFACE USE_HSTX=1)
    target_link_libraries(common_pico INTERFACE
            pico_hdmi
            pico_audio_i2s_shared
            tlv320dac3100
            hardware_dma
            hardware_pio
            hardware_i2c
    )
else()
    pico_generate_pio_header(common_pico ${CMAKE_CURRENT_LIST_DIR}/video_doom.pio)
    target_link_libraries(common_pico INTERFACE pico_scanvideo_dpi)
    target_compile_definitions(common_pico INTERFACE USE_HSTX=0)
endif()

add_library(pico_cd INTERFACE)
if (TARGET tinyusb_host)
    # ENABLE_PIO_USB picks the USB host transport at link time and must agree
    # with the HAS_USBPIO macro in the board's cflags header (a PIO header
    # without this link fails loudly on the missing pio_usb includes; the
    # native boards pass -DENABLE_PIO_USB=0). Default ON so the original
    # fruitjam build scripts keep working unchanged.
    if (NOT DEFINED ENABLE_PIO_USB)
        set(ENABLE_PIO_USB 1)
    endif()
    target_link_libraries(pico_cd INTERFACE tinyusb_host pico_shared_usb_hid)
    # The MantaPad NES and SNES shells are the same USB device (081f:e401) and
    # the SNES X button shares a report bit with the NES B button, so the driver
    # cannot tell them apart and picks a mode instead. Upstream defaults to NES
    # and only switches to SNES once Y is pressed, which left X dead on an SNES
    # pad until then. Doom wants all four face buttons, and mode 2 is the better
    # default for a NES pad here too: its B lands on io X (fire) instead of
    # io B (run), and with mode 1 the pad had no fire button at all.
    target_compile_definitions(pico_cd INTERFACE MANTAPAD_DEFAULT_MODE=2)
    if (ENABLE_PIO_USB)
        target_link_libraries(pico_cd INTERFACE tinyusb_pico_pio_usb)
    endif()
endif()
