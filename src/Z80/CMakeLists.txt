add_library(Z80_emu INTERFACE)

target_include_directories(Z80_emu INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
  )

target_sources(Z80_emu INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/Z80.c
  )
