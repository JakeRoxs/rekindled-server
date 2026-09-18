# ================================================================================================
#  Rekindled Server Client Emulator
#  Copyright (C) 2026 Jake Morgeson
# ================================================================================================

project(ClientEmu C CXX)

SET(SOURCES
    Entry.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

target_link_libraries(${PROJECT_NAME} PRIVATE Server_lib Shared DarkSouls3 steam libprotobuf-lite)
