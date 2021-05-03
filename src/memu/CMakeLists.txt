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
endif ()
