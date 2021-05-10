
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if("${TARGET}" STREQUAL "Pico")
  cmake_minimum_required(VERSION 3.12)
  
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
    -DLSB_FIRST
    -D__Pico__
    -DSMALL_MEM
    -DDYNAMIC_ROMS
    -DALT_EXIT=exit_pico
    -DALT_KEYPRESS=test_cfg_key
    -DALT_OPTIONS=cfg_options
    -DALT_PRINTF=printf_pico
    -DALT_USAGE=cfg_usage
    -DROM_PATCH_KBD
    -DROM_PATCH_TAPE
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

elseif("${TARGET}" STREQUAL "XWin")
  set(src_defs
    -DLSB_FIRST
    -DDYNAMIC_ROMS
    -DALT_KEYPRESS=test_cfg_key
    -DALT_OPTIONS=cfg_options
    -DALT_USAGE=cfg_usage
    -DUNIX
    -DHAVE_AUTOTYPE
    -DHAVE_CFX2
    -DHAVE_CONSOLE
    -DHAVE_DART
    -DHAVE_DISASS
    -DHAVE_JOY
    -DHAVE_OSFS
    -DHAVE_SID
    -DHAVE_SPEC
    -DHAVE_UI
    -DHAVE_VDEB
    -DHAVE_VGA
    -DZ80_DEBUG
    )
  
  if(NOT (${CMAKE_VERSION} VERSION_LESS "3.12.0"))
    
    project(memu C)
    
    add_executable(memu-x)
    
    #  add_compile_options(-g)
    #  add_link_options(-L/usr/lib/X11/lib)
    
    target_compile_definitions(memu-x PUBLIC ${src_defs})
    
    target_link_libraries(memu-x PUBLIC
      memu_src
      Z80_emu
      portaudio
      X11
      m
      )
  endif()

elseif("${TARGET}" STREQUAL "FBuf")

  set(src_defs
    -DLSB_FIRST
    -DDYNAMIC_ROMS
    -DALT_KEYPRESS=test_cfg_key
    -DALT_OPTIONS=cfg_options
    -DALT_USAGE=cfg_usage
    -DUNIX
    -DHAVE_CFX2
    -DHAVE_CONSOLE
    -DHAVE_DART
    -DHAVE_DISASS
    -DHAVE_JOY
    -DHAVE_OSFS
    -DHAVE_SID
    -DHAVE_SPEC
    -DHAVE_UI
    -DHAVE_VDEB
    -DHAVE_VGA
    -DZ80_DEBUG
    )
  
  if(NOT (${CMAKE_VERSION} VERSION_LESS "3.12.0"))
    
    project(memu C)
    
    add_executable(memu-fb)
    
    #  add_compile_options(-g)
    #  add_link_options(-L/usr/lib/X11/lib)
    
    target_compile_definitions(memu-fb PUBLIC ${src_defs})
    
    target_link_libraries(memu-fb PUBLIC
      memu_src
      Z80_emu
      portaudio
      X11
      m
      )
  endif()

elseif("${TARGET}" STREQUAL "Win")
  set(src_defs
    -D_CRT_SECURE_NO_WARNINGS
    -DLSB_FIRST
    -DDYNAMIC_ROMS
    -DALT_KEYPRESS=test_cfg_key
    -DALT_OPTIONS=cfg_options
    -DALT_USAGE=cfg_usage
    -DWIN32
    -DHAVE_AUTOTYPE
    -DHAVE_CFX2
    -DHAVE_CONSOLE
    -DHAVE_DISASS
    -DHAVE_JOY
    -DHAVE_OSFS
    -DHAVE_SID
    -DHAVE_SPEC
    -DHAVE_UI
    -DHAVE_VDEB
    -DHAVE_VGA
    -DZ80_DEBUG
    )
    
    project(memu C)
    
    add_executable(memu-win)

    add_subdirectory(portaudio EXCLUDE_FROM_ALL)
    
    #  add_compile_options(-g)
    #  add_link_options(-L/usr/lib/X11/lib)
    
    target_compile_definitions(memu-win PUBLIC ${src_defs})
    
    target_link_libraries(memu-win PUBLIC
      memu_src
      Z80_emu
      portaudio
      winmm
      dinput8
      Shcore
      )
  
endif()

# Specify the source files
add_subdirectory(src)
