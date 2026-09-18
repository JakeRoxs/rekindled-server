#Tests for Injector components

#Ensure CTest is enabled so tests are discoverable.
enable_testing()

# Optional: enable game integration tests (requires DS2/DS3 installed)
option(DS3OS_ENABLE_GAME_INTEGRATION_TESTS "Enable integration tests against real game binaries" OFF)

add_executable(InjectorTests
    InjectorTestsMain.cpp
    InjectorInitTests.cpp
    HookManagerTests.cpp
    LoggingTests.cpp
    InjectorIntegrationTests.cpp)

target_link_libraries(InjectorTests PUBLIC InjectorCore Shared detours)

if(DS3OS_ENABLE_GAME_INTEGRATION_TESTS)
    target_compile_definitions(InjectorTests PRIVATE DS3OS_ENABLE_GAME_INTEGRATION_TESTS)
endif()

#Register with CTest so it runs with "ctest" or the CMakeTools test runner
add_test(NAME InjectorTests COMMAND InjectorTests)

target_include_directories(InjectorTests PUBLIC
    "${CMAKE_SOURCE_DIR}/Source/Injector")

# Exercise native detour removal and actual DLL unmapping without a game install.
add_library(InjectorDetourFixture SHARED DetourFixture.cpp ../DetourLifetime.cpp)
target_include_directories(InjectorDetourFixture PRIVATE "${CMAKE_SOURCE_DIR}/Source")
target_compile_definitions(InjectorDetourFixture PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(InjectorDetourFixture PRIVATE detours)

add_executable(InjectorLifecycleTests InjectorLifecycleTests.cpp)
target_include_directories(InjectorLifecycleTests PRIVATE "${CMAKE_SOURCE_DIR}/Source")
target_compile_definitions(InjectorLifecycleTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
foreach(version IN ITEMS 100 200)
    add_library(InjectorControlFixture${version} SHARED ControlFixture.cpp ../ControlServer.cpp ../DetourLifetime.cpp)
    target_include_directories(InjectorControlFixture${version} PRIVATE "${CMAKE_SOURCE_DIR}/Source")
    target_compile_definitions(InjectorControlFixture${version} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX FIXTURE_VERSION=${version})
    target_link_libraries(InjectorControlFixture${version} PRIVATE detours Shared)
endforeach()
add_dependencies(InjectorLifecycleTests InjectorDetourFixture Injector InjectDS3 InjectorControlFixture100 InjectorControlFixture200)
add_test(NAME InjectorLifecycleTests COMMAND InjectorLifecycleTests
    "$<TARGET_FILE:InjectorDetourFixture>" "$<TARGET_FILE:Injector>" "$<TARGET_FILE:InjectDS3>"
    "$<TARGET_FILE:InjectorControlFixture100>" "$<TARGET_FILE:InjectorControlFixture200>")
set_tests_properties(InjectorLifecycleTests PROPERTIES TIMEOUT 90)
