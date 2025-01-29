add_library(memu_src INTERFACE)
  
target_include_directories(memu_src INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
  )

target_sources(memu_src INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/common.c
  ${CMAKE_CURRENT_LIST_DIR}/config.c
  ${CMAKE_CURRENT_LIST_DIR}/diag.c
  ${CMAKE_CURRENT_LIST_DIR}/dis.c
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
  ${CMAKE_CURRENT_LIST_DIR}/tape.c
  ${CMAKE_CURRENT_LIST_DIR}/txtwin.c
  ${CMAKE_CURRENT_LIST_DIR}/vdeb.c
  ${CMAKE_CURRENT_LIST_DIR}/win.c
  )

if("${TARGET}" STREQUAL "Pico")

  pico_generate_pio_header(memu_src ${CMAKE_CURRENT_LIST_DIR}/sd_spi.pio)
  pico_generate_pio_header(memu_src ${CMAKE_CURRENT_LIST_DIR}/sound.pio)

  target_sources(memu_src INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/80col_pico.c
    ${CMAKE_CURRENT_LIST_DIR}/console_pico.c
    ${CMAKE_CURRENT_LIST_DIR}/ctc.c
    ${CMAKE_CURRENT_LIST_DIR}/dirt_ff.c
    ${CMAKE_CURRENT_LIST_DIR}/dirmap.c
    ${CMAKE_CURRENT_LIST_DIR}/ff_disk.c
    ${CMAKE_CURRENT_LIST_DIR}/ff_stdio.c
    ${CMAKE_CURRENT_LIST_DIR}/kbd_pico.c
    ${CMAKE_CURRENT_LIST_DIR}/main_pico.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_spi2.c
    ${CMAKE_CURRENT_LIST_DIR}/snd_pico.c
    ${CMAKE_CURRENT_LIST_DIR}/vid_pico.c
    ${CMAKE_CURRENT_LIST_DIR}/win_pico.c
    )

elseif("${TARGET}" STREQUAL "XWin")

  target_sources(memu_src INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/cfx2.c
    ${CMAKE_CURRENT_LIST_DIR}/cpm.c
    ${CMAKE_CURRENT_LIST_DIR}/ctc.c
    ${CMAKE_CURRENT_LIST_DIR}/dart.c
    ${CMAKE_CURRENT_LIST_DIR}/dirmap.c
    ${CMAKE_CURRENT_LIST_DIR}/dirt_unix.c
    ${CMAKE_CURRENT_LIST_DIR}/joy_lin.c
    ${CMAKE_CURRENT_LIST_DIR}/kbd2.c
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/mfx.c
    ${CMAKE_CURRENT_LIST_DIR}/nfx.c
    ${CMAKE_CURRENT_LIST_DIR}/sdcard.c
    ${CMAKE_CURRENT_LIST_DIR}/snd.c
    ${CMAKE_CURRENT_LIST_DIR}/spec.c
    ${CMAKE_CURRENT_LIST_DIR}/ui.c
    ${CMAKE_CURRENT_LIST_DIR}/vdp.c
    ${CMAKE_CURRENT_LIST_DIR}/vga.c
    ${CMAKE_CURRENT_LIST_DIR}/vid.c
    ${CMAKE_CURRENT_LIST_DIR}/win_x.c
    )

  if(HAVE_HW)
    target_sources(memu_src INTERFACE
      ${CMAKE_CURRENT_LIST_DIR}/gpio.c
      ${CMAKE_CURRENT_LIST_DIR}/hardware.c
      ${CMAKE_CURRENT_LIST_DIR}/TxtRead.c
      )
  endif()

elseif("${TARGET}" STREQUAL "FBuf")
  
  target_sources(memu_src INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/cfx2.c
    ${CMAKE_CURRENT_LIST_DIR}/cpm.c
    ${CMAKE_CURRENT_LIST_DIR}/ctc.c
    ${CMAKE_CURRENT_LIST_DIR}/dart.c
    ${CMAKE_CURRENT_LIST_DIR}/dirmap.c
    ${CMAKE_CURRENT_LIST_DIR}/dirt_unix.c
    ${CMAKE_CURRENT_LIST_DIR}/joy_lin.c
    ${CMAKE_CURRENT_LIST_DIR}/kbd2.c
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/mfx.c
    ${CMAKE_CURRENT_LIST_DIR}/nfx.c
    ${CMAKE_CURRENT_LIST_DIR}/sdcard.c
    ${CMAKE_CURRENT_LIST_DIR}/snd.c
    ${CMAKE_CURRENT_LIST_DIR}/spec.c
    ${CMAKE_CURRENT_LIST_DIR}/ui.c
    ${CMAKE_CURRENT_LIST_DIR}/vdp.c
    ${CMAKE_CURRENT_LIST_DIR}/vga.c
    ${CMAKE_CURRENT_LIST_DIR}/vid.c
    ${CMAKE_CURRENT_LIST_DIR}/win_fb.c
    )

  if(HAVE_HW)
    target_sources(memu_src INTERFACE
      ${CMAKE_CURRENT_LIST_DIR}/gpio.c
      ${CMAKE_CURRENT_LIST_DIR}/hardware.c
      ${CMAKE_CURRENT_LIST_DIR}/TxtRead.c
      )
  endif()

elseif("${TARGET}" STREQUAL "RPi")
  
  target_sources(memu_src INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/cfx2.c
    ${CMAKE_CURRENT_LIST_DIR}/cpm.c
    ${CMAKE_CURRENT_LIST_DIR}/ctc.c
    ${CMAKE_CURRENT_LIST_DIR}/dart.c
    ${CMAKE_CURRENT_LIST_DIR}/dirmap.c
    ${CMAKE_CURRENT_LIST_DIR}/dirt_unix.c
    ${CMAKE_CURRENT_LIST_DIR}/joy_lin.c
    ${CMAKE_CURRENT_LIST_DIR}/kbd2.c
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/mfx.c
    ${CMAKE_CURRENT_LIST_DIR}/nfx.c
    ${CMAKE_CURRENT_LIST_DIR}/sdcard.c
    ${CMAKE_CURRENT_LIST_DIR}/snd.c
    ${CMAKE_CURRENT_LIST_DIR}/spec.c
    ${CMAKE_CURRENT_LIST_DIR}/ui.c
    ${CMAKE_CURRENT_LIST_DIR}/vdp.c
    ${CMAKE_CURRENT_LIST_DIR}/vga.c
    ${CMAKE_CURRENT_LIST_DIR}/vid.c
    ${CMAKE_CURRENT_LIST_DIR}/win_vc.c
    ${CMAKE_CURRENT_LIST_DIR}/gpio.c
    ${CMAKE_CURRENT_LIST_DIR}/hardware.c
    ${CMAKE_CURRENT_LIST_DIR}/TxtRead.c
    )

elseif("${TARGET}" STREQUAL "Win")

  target_sources(memu_src INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/cfx2.c
    ${CMAKE_CURRENT_LIST_DIR}/cpm.c
    ${CMAKE_CURRENT_LIST_DIR}/ctc.c
    ${CMAKE_CURRENT_LIST_DIR}/dirmap.c
    ${CMAKE_CURRENT_LIST_DIR}/dirt_w32.c
    ${CMAKE_CURRENT_LIST_DIR}/joy_w32.c
    ${CMAKE_CURRENT_LIST_DIR}/kbd2.c
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/mfx.c
    ${CMAKE_CURRENT_LIST_DIR}/nfx.c
    ${CMAKE_CURRENT_LIST_DIR}/sdcard.c
    ${CMAKE_CURRENT_LIST_DIR}/snd.c
    ${CMAKE_CURRENT_LIST_DIR}/spec.c
    ${CMAKE_CURRENT_LIST_DIR}/ui.c
    ${CMAKE_CURRENT_LIST_DIR}/vdp.c
    ${CMAKE_CURRENT_LIST_DIR}/vga.c
    ${CMAKE_CURRENT_LIST_DIR}/vid.c
    ${CMAKE_CURRENT_LIST_DIR}/win_w32.c
    )
  
elseif("${TARGET}" STREQUAL "Circle")

  target_sources(memu_src INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/cfx2.c
    ${CMAKE_CURRENT_LIST_DIR}/circleio.cpp
    ${CMAKE_CURRENT_LIST_DIR}/console_cir.cpp
    ${CMAKE_CURRENT_LIST_DIR}/cpm.c
    ${CMAKE_CURRENT_LIST_DIR}/ctc.c
    ${CMAKE_CURRENT_LIST_DIR}/dirt_ff.c
    ${CMAKE_CURRENT_LIST_DIR}/ff_stdio.c
    ${CMAKE_CURRENT_LIST_DIR}/gpio.c
    ${CMAKE_CURRENT_LIST_DIR}/hardware.c
    ${CMAKE_CURRENT_LIST_DIR}/kbd2.c
    ${CMAKE_CURRENT_LIST_DIR}/mem.c
    ${CMAKE_CURRENT_LIST_DIR}/snd.c
    ${CMAKE_CURRENT_LIST_DIR}/spec.c
    ${CMAKE_CURRENT_LIST_DIR}/TxtRead.c
    ${CMAKE_CURRENT_LIST_DIR}/ui.c
    ${CMAKE_CURRENT_LIST_DIR}/vdp.c
    ${CMAKE_CURRENT_LIST_DIR}/vga.c
    ${CMAKE_CURRENT_LIST_DIR}/vid.c
    ${CMAKE_CURRENT_LIST_DIR}/win_cir.c
    )

elseif("${TARGET}" STREQUAL "SDL")

  target_sources(memu_src INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/cfx2.c
    ${CMAKE_CURRENT_LIST_DIR}/cpm.c
    ${CMAKE_CURRENT_LIST_DIR}/ctc.c
    ${CMAKE_CURRENT_LIST_DIR}/dirmap.c
    ${CMAKE_CURRENT_LIST_DIR}/joy_sdl.c
    ${CMAKE_CURRENT_LIST_DIR}/kbd2.c
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/mfx.c
    ${CMAKE_CURRENT_LIST_DIR}/nfx.c
    ${CMAKE_CURRENT_LIST_DIR}/sdcard.c
    ${CMAKE_CURRENT_LIST_DIR}/snd_sdl.c
    ${CMAKE_CURRENT_LIST_DIR}/spec.c
    ${CMAKE_CURRENT_LIST_DIR}/ui.c
    ${CMAKE_CURRENT_LIST_DIR}/vdp.c
    ${CMAKE_CURRENT_LIST_DIR}/vga.c
    ${CMAKE_CURRENT_LIST_DIR}/vid.c
    ${CMAKE_CURRENT_LIST_DIR}/win_sdl.c
    )

  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    target_sources(memu_src INTERFACE
      ${CMAKE_CURRENT_LIST_DIR}/dirt_unix.c
      )
  elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    target_sources(memu_src INTERFACE
      ${CMAKE_CURRENT_LIST_DIR}/dirt_w32.c
      )
  else()
    message(FATAL_ERROR "Directory Traversal Routines not supported")
  endif()

else()

  message(FATAL_ERROR "No valid target specified in MEMU/src/memu/CMakeLists.txt")

endif()
