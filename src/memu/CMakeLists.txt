if("${TARGET}" STREQUAL "Pico")
  add_library(memu_src INTERFACE)

  pico_generate_pio_header(memu_src ${CMAKE_CURRENT_LIST_DIR}/sd_spi.pio)
  pico_generate_pio_header(memu_src ${CMAKE_CURRENT_LIST_DIR}/sound.pio)
  
  target_include_directories(memu_src INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    )

  target_sources(memu_src INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/80col_pico.c
    ${CMAKE_CURRENT_LIST_DIR}/cfgwin_pico.c
    ${CMAKE_CURRENT_LIST_DIR}/common.c
    ${CMAKE_CURRENT_LIST_DIR}/config.c
    ${CMAKE_CURRENT_LIST_DIR}/console_pico.c
    ${CMAKE_CURRENT_LIST_DIR}/ctc.c
    ${CMAKE_CURRENT_LIST_DIR}/diag.c
    ${CMAKE_CURRENT_LIST_DIR}/dirt_ff.c
    ${CMAKE_CURRENT_LIST_DIR}/display_pico.c
    ${CMAKE_CURRENT_LIST_DIR}/ff_disk.c
    ${CMAKE_CURRENT_LIST_DIR}/ff_stdio.c
    ${CMAKE_CURRENT_LIST_DIR}/kbd_pico.c
    ${CMAKE_CURRENT_LIST_DIR}/main_pico.c
    ${CMAKE_CURRENT_LIST_DIR}/mem.c
    ${CMAKE_CURRENT_LIST_DIR}/memu.c
    ${CMAKE_CURRENT_LIST_DIR}/mon_pico.c
    ${CMAKE_CURRENT_LIST_DIR}/monprom.c
    ${CMAKE_CURRENT_LIST_DIR}/printer.c
    ${CMAKE_CURRENT_LIST_DIR}/rom_os.c
    ${CMAKE_CURRENT_LIST_DIR}/rom_assem.c
    ${CMAKE_CURRENT_LIST_DIR}/rom_basic.c
    ${CMAKE_CURRENT_LIST_DIR}/rom_boot_type07.c
    ${CMAKE_CURRENT_LIST_DIR}/rom_sdx_type07.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_spi2.c
    ${CMAKE_CURRENT_LIST_DIR}/sdxfdc.c
    ${CMAKE_CURRENT_LIST_DIR}/snd_pico.c
    ${CMAKE_CURRENT_LIST_DIR}/tape.c
    ${CMAKE_CURRENT_LIST_DIR}/vid_pico.c
    )

elseif("${TARGET}" STREQUAL "XWin")
  set (xwin_src
      ${CMAKE_CURRENT_LIST_DIR}/cfgwin.c
      ${CMAKE_CURRENT_LIST_DIR}/cfx2.c
      ${CMAKE_CURRENT_LIST_DIR}/common.c
      ${CMAKE_CURRENT_LIST_DIR}/config.c
      ${CMAKE_CURRENT_LIST_DIR}/cpm.c
      ${CMAKE_CURRENT_LIST_DIR}/ctc.c
      ${CMAKE_CURRENT_LIST_DIR}/dart.c
      ${CMAKE_CURRENT_LIST_DIR}/diag.c
      ${CMAKE_CURRENT_LIST_DIR}/dirmap.c
      ${CMAKE_CURRENT_LIST_DIR}/dirt_unix.c
      ${CMAKE_CURRENT_LIST_DIR}/dis.c
      ${CMAKE_CURRENT_LIST_DIR}/joy_lin.c
      ${CMAKE_CURRENT_LIST_DIR}/kbd2.c
      ${CMAKE_CURRENT_LIST_DIR}/main.c
      ${CMAKE_CURRENT_LIST_DIR}/mem.c
      ${CMAKE_CURRENT_LIST_DIR}/memu.c
      ${CMAKE_CURRENT_LIST_DIR}/mfx.c
      ${CMAKE_CURRENT_LIST_DIR}/mon.c
      ${CMAKE_CURRENT_LIST_DIR}/monprom.c
      ${CMAKE_CURRENT_LIST_DIR}/nfx.c
      ${CMAKE_CURRENT_LIST_DIR}/printer.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_os.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_assem.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_basic.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_boot_type07.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_sdx_type07.c
      ${CMAKE_CURRENT_LIST_DIR}/sdcard.c
      ${CMAKE_CURRENT_LIST_DIR}/sdxfdc.c
      ${CMAKE_CURRENT_LIST_DIR}/sid.c
      ${CMAKE_CURRENT_LIST_DIR}/snd.c
      ${CMAKE_CURRENT_LIST_DIR}/spec.c
      ${CMAKE_CURRENT_LIST_DIR}/tape.c
      ${CMAKE_CURRENT_LIST_DIR}/ui.c
      ${CMAKE_CURRENT_LIST_DIR}/vdeb.c
      ${CMAKE_CURRENT_LIST_DIR}/vga.c
      ${CMAKE_CURRENT_LIST_DIR}/vid.c
      ${CMAKE_CURRENT_LIST_DIR}/win_x.c
      )
  
  if( NOT (${CMAKE_VERSION} VERSION_LESS "3.12.0"))
    
    add_library(memu_src INTERFACE)
    
    target_include_directories(memu_src INTERFACE
      ${CMAKE_CURRENT_LIST_DIR}
      )
    
    target_sources(memu_src INTERFACE ${xwin_src})

  else()
    cmake_minimum_required(VERSION 3.5)

    project(memu C)

    add_executable(memu-x ${xwin_src})

    target_include_directories(memu-x PUBLIC
      ${CMAKE_CURRENT_LIST_DIR}
      )

    target_compile_definitions(memu-x PUBLIC ${src_defs})

    target_compile_options(memu-x PRIVATE -g)
    target_link_options(memu-x PRIVATE -g)
    
    target_link_libraries(memu-x PUBLIC
      Z80_emu
      portaudio
      X11
      m
      )
    
  endif ()

elseif("${TARGET}" STREQUAL "FBuf")
  
  set (fbuf_src
      ${CMAKE_CURRENT_LIST_DIR}/cfgwin.c
      ${CMAKE_CURRENT_LIST_DIR}/cfx2.c
      ${CMAKE_CURRENT_LIST_DIR}/common.c
      ${CMAKE_CURRENT_LIST_DIR}/config.c
      ${CMAKE_CURRENT_LIST_DIR}/cpm.c
      ${CMAKE_CURRENT_LIST_DIR}/ctc.c
      ${CMAKE_CURRENT_LIST_DIR}/dart.c
      ${CMAKE_CURRENT_LIST_DIR}/diag.c
      ${CMAKE_CURRENT_LIST_DIR}/dirmap.c
      ${CMAKE_CURRENT_LIST_DIR}/dirt_unix.c
      ${CMAKE_CURRENT_LIST_DIR}/dis.c
      ${CMAKE_CURRENT_LIST_DIR}/joy_lin.c
      ${CMAKE_CURRENT_LIST_DIR}/kbd2.c
      ${CMAKE_CURRENT_LIST_DIR}/main.c
      ${CMAKE_CURRENT_LIST_DIR}/mem.c
      ${CMAKE_CURRENT_LIST_DIR}/memu.c
      ${CMAKE_CURRENT_LIST_DIR}/mon.c
      ${CMAKE_CURRENT_LIST_DIR}/monprom.c
      ${CMAKE_CURRENT_LIST_DIR}/nfx.c
      ${CMAKE_CURRENT_LIST_DIR}/printer.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_os.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_assem.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_basic.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_boot_type07.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_sdx_type07.c
      ${CMAKE_CURRENT_LIST_DIR}/sdxfdc.c
      ${CMAKE_CURRENT_LIST_DIR}/sid.c
      ${CMAKE_CURRENT_LIST_DIR}/snd.c
      ${CMAKE_CURRENT_LIST_DIR}/spec.c
      ${CMAKE_CURRENT_LIST_DIR}/tape.c
      ${CMAKE_CURRENT_LIST_DIR}/ui.c
      ${CMAKE_CURRENT_LIST_DIR}/vdeb.c
      ${CMAKE_CURRENT_LIST_DIR}/vga.c
      ${CMAKE_CURRENT_LIST_DIR}/vid.c
      ${CMAKE_CURRENT_LIST_DIR}/win_fb.c
      )
    
  if( NOT (${CMAKE_VERSION} VERSION_LESS "3.12.0"))
    
    add_library(memu_src INTERFACE)
    
    target_include_directories(memu_src INTERFACE
      ${CMAKE_CURRENT_LIST_DIR}
      )
    
    target_sources(memu_src INTERFACE ${fbuf_src})

  else()
    cmake_minimum_required(VERSION 3.5)

    project(memu C)

    add_executable(memu-fb ${fbuf_src})

    target_include_directories(memu-fb PUBLIC
      ${CMAKE_CURRENT_LIST_DIR}
      )

    #  add_compile_options(-g)
    #  add_link_options(-g)

    target_compile_definitions(memu-fb PUBLIC ${src_defs})
    
    target_link_libraries(memu-fb PUBLIC
      Z80_emu
      portaudio
      m
      )
    
  endif ()

elseif("${TARGET}" STREQUAL "RPi")
  
  add_library(memu_src INTERFACE)
  
  target_include_directories(memu_src INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    )
  
  target_sources(memu_src INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/cfgwin.c
    ${CMAKE_CURRENT_LIST_DIR}/cfx2.c
    ${CMAKE_CURRENT_LIST_DIR}/common.c
    ${CMAKE_CURRENT_LIST_DIR}/config.c
    ${CMAKE_CURRENT_LIST_DIR}/cpm.c
    ${CMAKE_CURRENT_LIST_DIR}/ctc.c
    ${CMAKE_CURRENT_LIST_DIR}/dart.c
    ${CMAKE_CURRENT_LIST_DIR}/diag.c
    ${CMAKE_CURRENT_LIST_DIR}/dirmap.c
    ${CMAKE_CURRENT_LIST_DIR}/dirt_unix.c
    ${CMAKE_CURRENT_LIST_DIR}/dis.c
    ${CMAKE_CURRENT_LIST_DIR}/gpio.c
    ${CMAKE_CURRENT_LIST_DIR}/hardware.c
    ${CMAKE_CURRENT_LIST_DIR}/joy_lin.c
    ${CMAKE_CURRENT_LIST_DIR}/kbd2.c
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/mem.c
    ${CMAKE_CURRENT_LIST_DIR}/memu.c
    ${CMAKE_CURRENT_LIST_DIR}/mon.c
    ${CMAKE_CURRENT_LIST_DIR}/monprom.c
    ${CMAKE_CURRENT_LIST_DIR}/printer.c
    ${CMAKE_CURRENT_LIST_DIR}/rom_os.c
    ${CMAKE_CURRENT_LIST_DIR}/rom_assem.c
    ${CMAKE_CURRENT_LIST_DIR}/rom_basic.c
    ${CMAKE_CURRENT_LIST_DIR}/rom_boot_type07.c
    ${CMAKE_CURRENT_LIST_DIR}/rom_sdx_type07.c
    ${CMAKE_CURRENT_LIST_DIR}/sdxfdc.c
    ${CMAKE_CURRENT_LIST_DIR}/sid.c
    ${CMAKE_CURRENT_LIST_DIR}/snd.c
    ${CMAKE_CURRENT_LIST_DIR}/spec.c
    ${CMAKE_CURRENT_LIST_DIR}/tape.c
    ${CMAKE_CURRENT_LIST_DIR}/TxtRead.c
    ${CMAKE_CURRENT_LIST_DIR}/ui.c
    ${CMAKE_CURRENT_LIST_DIR}/vdeb.c
    ${CMAKE_CURRENT_LIST_DIR}/vga.c
    ${CMAKE_CURRENT_LIST_DIR}/vid.c
    ${CMAKE_CURRENT_LIST_DIR}/win_vc.c
    )

elseif("${TARGET}" STREQUAL "Win")
  set (win_src
      ${CMAKE_CURRENT_LIST_DIR}/cfgwin.c
      ${CMAKE_CURRENT_LIST_DIR}/cfx2.c
      ${CMAKE_CURRENT_LIST_DIR}/common.c
      ${CMAKE_CURRENT_LIST_DIR}/config.c
      ${CMAKE_CURRENT_LIST_DIR}/cpm.c
      ${CMAKE_CURRENT_LIST_DIR}/ctc.c
      ${CMAKE_CURRENT_LIST_DIR}/diag.c
      ${CMAKE_CURRENT_LIST_DIR}/dirmap.c
      ${CMAKE_CURRENT_LIST_DIR}/dirt_w32.c
      ${CMAKE_CURRENT_LIST_DIR}/dis.c
      ${CMAKE_CURRENT_LIST_DIR}/joy_w32.c
      ${CMAKE_CURRENT_LIST_DIR}/kbd2.c
      ${CMAKE_CURRENT_LIST_DIR}/main.c
      ${CMAKE_CURRENT_LIST_DIR}/mem.c
      ${CMAKE_CURRENT_LIST_DIR}/memu.c
      ${CMAKE_CURRENT_LIST_DIR}/mon.c
      ${CMAKE_CURRENT_LIST_DIR}/monprom.c
      ${CMAKE_CURRENT_LIST_DIR}/printer.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_os.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_assem.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_basic.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_boot_type07.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_sdx_type07.c
      ${CMAKE_CURRENT_LIST_DIR}/sdxfdc.c
      ${CMAKE_CURRENT_LIST_DIR}/sid.c
      ${CMAKE_CURRENT_LIST_DIR}/snd.c
      ${CMAKE_CURRENT_LIST_DIR}/spec.c
      ${CMAKE_CURRENT_LIST_DIR}/tape.c
      ${CMAKE_CURRENT_LIST_DIR}/ui.c
      ${CMAKE_CURRENT_LIST_DIR}/vdeb.c
      ${CMAKE_CURRENT_LIST_DIR}/vga.c
      ${CMAKE_CURRENT_LIST_DIR}/vid.c
      ${CMAKE_CURRENT_LIST_DIR}/win_w32.c
      )
    
    add_library(memu_src INTERFACE)
    
    target_include_directories(memu_src INTERFACE
      ${CMAKE_CURRENT_LIST_DIR}
      )
    
    target_sources(memu_src INTERFACE ${win_src})
    
  elseif("${TARGET}" STREQUAL "Circle")
    
    add_library(memu_src INTERFACE)
    
    target_include_directories(memu_src INTERFACE
      ${CMAKE_CURRENT_LIST_DIR}
      )

    target_sources(memu_src INTERFACE
      ${CMAKE_CURRENT_LIST_DIR}/cfgwin.c
      ${CMAKE_CURRENT_LIST_DIR}/cfx2.c
      ${CMAKE_CURRENT_LIST_DIR}/circleio.cpp
      ${CMAKE_CURRENT_LIST_DIR}/common.c
      ${CMAKE_CURRENT_LIST_DIR}/config.c
      ${CMAKE_CURRENT_LIST_DIR}/console_cir.cpp
      ${CMAKE_CURRENT_LIST_DIR}/cpm.c
      ${CMAKE_CURRENT_LIST_DIR}/ctc.c
      ${CMAKE_CURRENT_LIST_DIR}/diag.c
      ${CMAKE_CURRENT_LIST_DIR}/dirt_ff.c
      ${CMAKE_CURRENT_LIST_DIR}/dis.c
      ${CMAKE_CURRENT_LIST_DIR}/ff_stdio.c
      ${CMAKE_CURRENT_LIST_DIR}/gpio.c
      ${CMAKE_CURRENT_LIST_DIR}/hardware.c
      ${CMAKE_CURRENT_LIST_DIR}/kbd2.c
      ${CMAKE_CURRENT_LIST_DIR}/mem.c
      ${CMAKE_CURRENT_LIST_DIR}/memu.c
      ${CMAKE_CURRENT_LIST_DIR}/misc.cpp
      ${CMAKE_CURRENT_LIST_DIR}/mon.c
      ${CMAKE_CURRENT_LIST_DIR}/monprom.c
      ${CMAKE_CURRENT_LIST_DIR}/printer.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_os.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_assem.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_basic.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_boot_type07.c
      ${CMAKE_CURRENT_LIST_DIR}/rom_sdx_type07.c
      ${CMAKE_CURRENT_LIST_DIR}/sdxfdc.c
      ${CMAKE_CURRENT_LIST_DIR}/sid.c
      ${CMAKE_CURRENT_LIST_DIR}/snd.c
      ${CMAKE_CURRENT_LIST_DIR}/spec.c
      ${CMAKE_CURRENT_LIST_DIR}/tape.c
      ${CMAKE_CURRENT_LIST_DIR}/TxtRead.c
      ${CMAKE_CURRENT_LIST_DIR}/ui.c
      ${CMAKE_CURRENT_LIST_DIR}/vdeb.c
      ${CMAKE_CURRENT_LIST_DIR}/vid.c
      ${CMAKE_CURRENT_LIST_DIR}/vga.c
      ${CMAKE_CURRENT_LIST_DIR}/win_cir.c
      )

else()
  message(FATAL_ERROR "No valid target specified in MEMU/src/memu/CMakeLists.txt")

endif()
