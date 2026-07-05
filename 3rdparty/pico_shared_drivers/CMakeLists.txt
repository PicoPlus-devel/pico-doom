# Vendored copies of Frank's pico_shared driver library.
# Upstream: https://github.com/... /pico-infonesPlus/pico_shared/drivers
#
# Each subdirectory declares an INTERFACE library:
#   pico_hdmi                (HSTX video + HDMI data-island audio)
#   pico_audio_i2s_shared    (I2S PIO+DMA, renamed to avoid clash with pico-extras)
#   tlv320dac3100            (Fruit Jam DAC + headphone-detect)
#
# Resync by re-copying from pico_shared/drivers — only pico_audio_i2s/CMakeLists.txt
# is patched (target renamed).

add_subdirectory(drivers/tlv320dac3100)
add_subdirectory(drivers/pico_audio_i2s)
add_subdirectory(drivers/pico_hdmi)
