#== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==
#Rekindled Server
#Copyright(C) 2021 Tim Leonard
#Copyright(C) 2026 Jake Morgeson
#== == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==

project(Injector C CXX)

set(INJECTOR_CORE_SOURCES
    Config/BuildConfig.h
    Config/RuntimeConfig.h
    Config/RuntimeConfig.cpp
    Injector/Injector.h
    Injector/Injector.cpp
    ShutdownSignal.h
    ShutdownSignal.cpp
    HookManager.h
    HookManager.cpp
    Hooks/Hook.h
    Hooks/Hook.cpp
    Hooks/DarkSouls3/DS3_ReplaceServerAddressHook.h
    Hooks/DarkSouls3/DS3_ReplaceServerAddressHook.cpp
    Hooks/DarkSouls2/DS2_ReplaceServerAddressHook.h
    Hooks/DarkSouls2/DS2_ReplaceServerAddressHook.cpp
    Hooks/DarkSouls2/DS2_LogProtobufsHook.h
    Hooks/DarkSouls2/DS2_LogProtobufsHook.cpp
    Hooks/Shared/ReplaceServerPortHook.h
    Hooks/Shared/ReplaceServerPortHook.cpp
    Hooks/Shared/ChangeSaveGameFilenameHook.h
    Hooks/Shared/ChangeSaveGameFilenameHook.cpp
)

# Core implementation is shared between the DLL and unit tests.
add_library(InjectorCore STATIC ${INJECTOR_CORE_SOURCES})
add_library(${PROJECT_NAME} SHARED Entry.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE InjectorCore)

foreach(target IN ITEMS InjectorCore ${PROJECT_NAME})
    target_include_directories(${target} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
    target_compile_definitions(${target} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WINSOCKAPI_
        _WIN32_WINNT=0x0A00
    )
    target_link_libraries(${target} PUBLIC Shared detours)
endforeach()

if(BUILD_TESTING)
    add_subdirectory(Tests)
endif()
