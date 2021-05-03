cmake_minimum_required(VERSION 3.12)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if("${TARGET}" STREQUAL "Pico")
  set(PICO_BOARD vgaboard)

  # Pull in PICO SDK (must be before project)
  include(pico_sdk_import.cmake)

  # We also need PICO EXTRAS
  include(pico_extras_import.cmake)

  project(memu C CXX)

  # Initialize the Pico SDK
  pico_sdk_init()

  add_executable(memu-pico)

#  add_compile_options(-g)
#  add_link_options(-g)

  target_compile_definitions(memu-pico PUBLIC
    -DPICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=322
    -DPICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=8
    -DPICO_SD_CARD_EXTRA_CLOCK_DIVIDER=16
    -DLSB_FIRST
    -D__Pico__
    -DSMALL_MEM
    -DDYNAMIC_ROMS
    -DALT_ENTRY=memu
    -DALT_KEYPRESS=test_cfg_key
    -DALT_OPTIONS=cfg_options
    _DALT_USAGE=cfg_usage
    )

  target_include_directories(memu-pico PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    )

  pico_enable_stdio_uart(memu-pico 0)
  pico_enable_stdio_usb(memu-pico 0)
  pico_add_extra_outputs(memu-pico)
  target_link_libraries(memu-pico PUBLIC
    memu_src
    Z80_emu
    fatfs
    pico_stdlib
    pico_sd_card
    pico_scanvideo_dpi
    pico_multicore
    tinyusb_host
    )

endif()

# Specify the source files
add_subdirectory(src)
