# Wii-extension controllers over I2C (NES Classic Mini, SNES Classic Mini, Wii
# Classic Controller (Pro)), vendored verbatim from the pico_shared root
# (wiipad.h / wiipad.cpp) — see the parent CMakeLists.txt for the resync
# procedure. No local patches.
#
# The whole translation unit is wrapped in
#   #if WII_PIN_SDA >= 0 and WII_PIN_SCL >= 0
# so a board whose cflags header sets the pins to -1 compiles it away entirely.
# Note that an *undefined* WII_PIN_SDA would make that test pass (0 >= 0), which
# is why the parent only adds this directory for PICO_ON_DEVICE builds, where
# the force-included board header always defines both.
add_library(pico_shared_wiipad INTERFACE)
target_sources(pico_shared_wiipad INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/wiipad.cpp
)
target_include_directories(pico_shared_wiipad INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(pico_shared_wiipad INTERFACE pico_stdlib hardware_i2c)
