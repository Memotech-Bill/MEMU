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

  #  add_compile_options(-Werror -Wall -Wextra -Wnull-dereference)
  #  add_link_options(-g)
  target_compile_options(memu-pico PUBLIC -O1 -g)
  target_link_options(memu-pico PUBLIC -g)

  target_compile_definitions(memu-pico PUBLIC
    -DPICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=322
    -DPICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=8
    -DLSB_FIRST
    -D__Pico__
    -DPICO_PANIC_FUNCTION=fatal
    -DSMALL_MEM
    -DHAVE_SID
    -DDYNAMIC_ROMS
    -DALT_EXIT=exit_pico
    -DHAVE_CONFIG
    -DALT_OPTIONS=cfg_options
    -DALT_USAGE=cfg_usage
    -DALT_PRINTF=printf_pico
    -DROM_PATCH_KBD
    -DROM_PATCH_TAPE
    -DMAP_PATH
    -DHAVE_VDEB
    -DHAVE_DISASS
    -DZ80_DEBUG
    )

  if (SERIAL_DIAG)
    pico_enable_stdio_uart(memu-pico 1)
    target_compile_definitions(memu-pico PUBLIC -DSERIAL_DIAG=1)
  else ()
    pico_enable_stdio_uart(memu-pico 0)
  endif ()
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
    -DHAVE_CONFIG
    -DALT_OPTIONS=cfg_options
    -DALT_USAGE=cfg_usage
    -DUNIX
    -DHAVE_AUTOTYPE
    -DHAVE_CFX2
    -DHAVE_CONSOLE
    -DHAVE_DART
    -DHAVE_DISASS
    -DHAVE_JOY
    -DHAVE_MFX
    -DMAP_PATH
    -DHAVE_OSFS
    -DHAVE_SID
    -DHAVE_SPEC
    -DHAVE_UI
    -DHAVE_VDEB
    -DHAVE_VGA
    -DHAVE_NFX
    -DZ80_DEBUG
    )
  
  if(NOT (${CMAKE_VERSION} VERSION_LESS "3.12.0"))
    
    project(memu C)
    
    add_executable(memu-x)

    set_target_properties(memu-x PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/run_time
      )
    
    target_compile_options(memu-x PUBLIC -g)
    target_link_options(memu-x PUBLIC -g)
    # target_compile_options(memu-x PUBLIC -fsanitize=address -fsanitize=leak)
    # target_link_options(memu-x PUBLIC -fsanitize=address -fsanitize=leak -static-libasan)
    
    target_compile_definitions(memu-x PUBLIC ${src_defs})
    
    target_link_libraries(memu-x PUBLIC
      memu_src
      Z80_emu
      portaudio
      mcheck
      X11
      m
      )
  endif()

elseif("${TARGET}" STREQUAL "FBuf")

  set(src_defs
    -DLSB_FIRST
    -DDYNAMIC_ROMS
    -DHAVE_CONFIG
    -DALT_OPTIONS=cfg_options
    -DALT_USAGE=cfg_usage
    -DUNIX
    -DHAVE_CFX2
    -DHAVE_CONSOLE
    -DHAVE_DART
    -DHAVE_DISASS
    -DHAVE_JOY
    -DMAP_PATH
    -DHAVE_OSFS
    -DHAVE_SID
    -DHAVE_SPEC
    -DHAVE_UI
    -DHAVE_VDEB
    -DHAVE_VGA
    -DHAVE_NFX
    -DZ80_DEBUG
    )
  
  if(NOT (${CMAKE_VERSION} VERSION_LESS "3.12.0"))
    
    project(memu C)
    
    add_executable(memu-fb)

    set_target_properties(memu-fb PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/run_time
      )
    
    #  add_compile_options(-g)
    #  add_link_options(-g)
    
    target_compile_definitions(memu-fb PUBLIC ${src_defs})
    
    target_link_libraries(memu-fb PUBLIC
      memu_src
      Z80_emu
      portaudio
      m
      )
  endif()

elseif("${TARGET}" STREQUAL "RPi")
    
    project(memu C)
    
    add_executable(memu-pi)

    set_target_properties(memu-pi PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/run_time
      )
    
    #  add_compile_options(-g)
    #  add_link_options(-g)

    target_include_directories(memu-pi PUBLIC
      /opt/vc/include
      /opt/vc/include/interface/vcos/pthreads
      /opt/vc/include/interface/vmcs_host/linux
      )
    
    target_compile_definitions(memu-pi PUBLIC
    -DLSB_FIRST
    -DDYNAMIC_ROMS
    -DALT_HANDLE_EVENTS=hw_handle_events
    -DALT_KBD_SENSE1=hw_kbd_sense_1
    -DALT_KBD_SENSE2=hw_kbd_sense_2
    -DHAVE_CONFIG
    -DALT_OPTIONS=hw_options
    -DALT_USAGE=hw_usage
    -DALT_Z80_OUT=hw_Z80_out
    -DALT_Z80_IN=hw_Z80_in
    -DUNIX
    -DHAVE_CFX2
    -DHAVE_CONSOLE
    -DHAVE_DART
    -DHAVE_DISASS
    -DHAVE_JOY
    -DMAP_PATH
    -DHAVE_OSFS
    -DHAVE_SID
    -DHAVE_SPEC
    -DHAVE_UI
    -DHAVE_VDEB
    -DHAVE_VGA
    -DZ80_DEBUG
    )

  target_link_options(memu-pi PUBLIC -L/opt/vc/lib)
  
  target_link_libraries(memu-pi PUBLIC
    memu_src
    Z80_emu
    portaudio
    bcm_host
    vcos
    vchostif
    vchiq_arm
    m
    )

elseif("${TARGET}" STREQUAL "Win")
  set(src_defs
    -D_CRT_SECURE_NO_WARNINGS
    -DLSB_FIRST
    -DDYNAMIC_ROMS
    -DHAVE_CONFIG
    -DALT_OPTIONS=cfg_options
    -DALT_USAGE=cfg_usage
    -DWIN32
    -DHAVE_AUTOTYPE
    -DHAVE_CFX2
    -DHAVE_CONSOLE
    -DHAVE_DISASS
    -DHAVE_JOY
    -DHAVE_MFX
    -DMAP_PATH
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

    set_target_properties(memu-win PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_LIST_DIR}/run_time
      )

    add_subdirectory(../portaudio portaudio EXCLUDE_FROM_ALL)

    set_target_properties(portaudio PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_LIST_DIR}/run_time
      )
    
    #  add_compile_options(-g)
    #  add_link_options(-g)
    
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
    -DHAVE_CONFIG
    -DALT_OPTIONS=hw_options
    -DALT_USAGE=hw_usage
    -DALT_Z80_OUT=hw_Z80_out
    -DALT_Z80_IN=hw_Z80_in
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

elseif("${TARGET}" STREQUAL "SDL")

  set (SDL2_DIR "/usr/lib/x86_64-linux-gnu/cmake/SDL2/")
  find_package(SDL2 REQUIRED)
  
  set(src_defs
    -DLSB_FIRST
    -DDYNAMIC_ROMS
    -DHAVE_CONFIG
    -DALT_OPTIONS=cfg_options
    -DALT_USAGE=cfg_usage
    -DUNIX
    -DHAVE_AUTOTYPE
    -DHAVE_CFX2
    -DHAVE_CONSOLE
    -DHAVE_DART
    -DHAVE_DISASS
    -DHAVE_JOY
    -DHAVE_MFX
    -DMAP_PATH
    -DHAVE_OSFS
    -DHAVE_SID
    -DHAVE_SPEC
    -DHAVE_UI
    -DHAVE_VDEB
    -DHAVE_VGA
    -DHAVE_NFX
    -DZ80_DEBUG
    )
  
  if(NOT (${CMAKE_VERSION} VERSION_LESS "3.12.0"))
    
    project(memu C)
    
    add_executable(memu-sdl)

    set_target_properties(memu-sdl PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/run_time
      )
    
    target_compile_options(memu-sdl PUBLIC -g)
    target_link_options(memu-sdl PUBLIC -g)
    # target_compile_options(memu-sdl PUBLIC -fsanitize=address -fsanitize=leak)
    # target_link_options(memu-sdl PUBLIC -fsanitize=address -fsanitize=leak -static-libasan)
    
    target_compile_definitions(memu-sdl PUBLIC ${src_defs})
    
    target_link_libraries(memu-sdl PUBLIC
      memu_src
      Z80_emu
      portaudio
      ${SDL2_LIBRARIES}
      m
      )
  endif()

else()
  message(FATAL_ERROR "No valid target specified in MEMU/CMakeLists.txt")

endif()

# Specify the source files
add_subdirectory(src)
