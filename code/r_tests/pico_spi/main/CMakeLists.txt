add_executable(test_spi test_spi.c)

# enable usb output, disable uart output
pico_enable_stdio_usb(test_spi 1)
pico_enable_stdio_uart(test_spi 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(test_spi)

target_compile_options(test_spi PRIVATE -Wall)

# pull in common dependencies
target_link_libraries(test_spi pico_stdlib hardware_spi)
