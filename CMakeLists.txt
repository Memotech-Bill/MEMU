set(CMAKE_C_STANDARD 11)

if("${TARGET}" STREQUAL "Pico")
  cmake_minimum_required(VERSION 3.12)
  set(CMAKE_CXX_STANDARD 17)
  
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

elseif("${TARGET}" STREQUAL "Circle")
  cmake_minimum_required(VERSION 3.16)
  set(CMAKE_CXX_STANDARD 14)

  project(memu C CXX ASM)

  set(CIRCLE_DIR ${CMAKE_CURRENT_LIST_DIR}/../circle)

  set(CMAKE_LINKER ld)
  set(CMAKE_CXX_LINK_EXECUTABLE
    "<CMAKE_LINKER> -o <TARGET> <LINK_FLAGS> <OBJECTS> --start-group <LINK_LIBRARIES> --end-group")

  if (${RPI} EQUAL 1)
    set(ARCH -DAARCH=32 -mcpu=arm1176jzf-s -marm -mfpu=vfp -mfloat-abi=hard)
    add_compile_definitions(RASPPI=1)
    set(KERNEL kernel.img)
  elseif (${RPI} EQUAL 2)
    set(ARCH -DAARCH=32 -mcpu=cortex-a7 -marm -mfpu=neon-vfpv4 -mfloat-abi=hard)
    add_compile_definitions(RASPPI=2)
    set(KERNEL kernel7.img)
  elseif (${RPI} EQUAL 3)
    set(ARCH -DAARCH=32 -mcpu=cortex-a53 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard)
    add_compile_definitions(RASPPI=3)
    set(KERNEL kernel8-32.img)
  elseif (${RPI} EQUAL 4)
    set(ARCH -DAARCH=32 -mcpu=cortex-a72 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard)
    add_compile_definitions(RASPPI=4)
    set(KERNEL kernel7l.img)
  endif()

  add_compile_definitions(__circle__)
  add_compile_options(${ARCH})
  # add_compile_options(-nostdinc)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>)
  add_compile_options(-O2)
  add_compile_options(-Wall -fsigned-char -ffreestanding)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-std=c++14>)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-aligned-new>)
  add_link_options(--section-start=.init=0x8000)
  add_link_options(-T ${CIRCLE_DIR}/circle.ld)
  add_compile_definitions(
    STDLIB_SUPPORT=1
	__VCCOREVER__=0x04000000
    )
  add_compile_options(
    -U__unix__
    -U__linux__
    )

  execute_process(COMMAND ${CMAKE_C_COMPILER} ${ARCH} -print-file-name=libgcc.a
                OUTPUT_VARIABLE LIBGCC_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(COMMAND ${CMAKE_C_COMPILER} ${ARCH} -print-file-name=libm.a
                OUTPUT_VARIABLE LIBM_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE)
              
  add_executable(memu-circle)

  #  add_compile_options(-g)
  #  add_link_options(-g)

  target_compile_definitions(memu-circle PUBLIC
    -DAARCH=32
    -D__circle__
    -DUNIX
    -DLINUX
    -DLSB_FIRST
    -DDYNAMIC_ROMS
    -DHAVE_AUTOTYPE
    -DHAVE_CFX2
    -DHAVE_DISASS
    -DHAVE_OSFS
    -DHAVE_SID
    -DHAVE_SPEC
    -DHAVE_UI
    -DHAVE_VDEB
    -DHAVE_VGA
	-DALT_EXIT=hw_exit
    -DALT_INIT=hw_init
    -DALT_OPTIONS=hw_options
    -DALT_USAGE=hw_usage
	-DALT_Z80_OUT=hw_Z80_out
    -DALT_Z80_IN=hw_Z80_in
	-DALT_KEYPRESS=test_cfg_key
    -DALT_KBD_SENSE1=hw_kbd_sense_1
    -DALT_KBD_SENSE2=hw_kbd_sense_2
	-DALT_KEYIN=hw_key_scan
    -DALT_HANDLE_EVENTS=hw_handle_events
    -DALT_PRINTF=printf_circle
    -DUSE_VCHIQ_SOUND=1
    -DZ80_DEBUG
    )

  include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/src/circle/include
    ${CIRCLE_DIR}/include
    ${CIRCLE_DIR}/addon
    )

  target_link_libraries(memu-circle PUBLIC
    memu_src
    Z80_emu
    fatfs
    memu_cir
    circle_fs
    circle_sdcard
    circle_sound
    circle_vchiq
    circle_usb
    circle_input
    circle_sched
    circle_linux
    circle_lib
    ${LIBM_PATH}
    ${LIBGCC_PATH}
    )

  add_custom_command(TARGET memu-circle POST_BUILD
    COMMAND objcopy memu-circle -O binary ${KERNEL}
    )

endif()

# Specify the source files
add_subdirectory(src)
