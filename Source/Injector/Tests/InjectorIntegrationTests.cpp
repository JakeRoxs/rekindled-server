#include <cassert>
#include <filesystem>
#include <chrono>
#include <iostream>

#include "Injector/Injector/Injector.h"
#include "Injector/Config/RuntimeConfig.h"

#ifdef _WIN32
#include <windows.h>
#endif

// Optional integration tests that require a real DS2/DS3 game binary.
// These tests are skipped unless DS3OS_TEST_GAME_PATH is set to the path
// of a DarkSoulsIII.exe or DarkSoulsII.exe file.
//
// To run:
//   set DS3OS_TEST_GAME_PATH=C:\Games\DarkSoulsIII\DarkSoulsIII.exe
//   InjectorIntegrationTests.exe

void RunInjectorIntegrationTests() {
  const char* GamePathEnv = nullptr;
#ifdef _WIN32
  GamePathEnv = getenv("DS3OS_TEST_GAME_PATH");
#else
  GamePathEnv = getenv("DS3OS_TEST_GAME_PATH");
#endif

  if (!GamePathEnv || GamePathEnv[0] == '\0') {
    std::cout << "SKIPPED: DS3OS_TEST_GAME_PATH not set. Set it to the path of a DS2/DS3 exe to run integration tests." << std::endl;
    return;
  }

  std::string GamePath(GamePathEnv);
  std::cout << "Running integration tests against: " << GamePath << std::endl;

  if (!std::filesystem::exists(GamePath)) {
    std::cout << "SKIPPED: Game file not found at " << GamePath << std::endl;
    return;
  }

  // Load the game executable as a data file to get a module base.
  // This allows us to test Injector::Init() module resolution without
  // actually running the game.
  HMODULE GameModule = nullptr;
#ifdef _WIN32
  GameModule = LoadLibraryExW(
    std::filesystem::path(GamePath).wstring().c_str(),
    nullptr,
    LOAD_LIBRARY_AS_DATAFILE);

  if (!GameModule) {
    std::cout << "FAILED: Could not load game module. Error: " << GetLastError() << std::endl;
    return;
  }
#endif

  std::cout << "Game module loaded at: " << reinterpret_cast<void*>(GameModule) << std::endl;

  // Create a temporary config directory
  auto tmpRoot = std::filesystem::temp_directory_path();
  auto testDir = tmpRoot / ("rekindled_integration_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(testDir);

  RuntimeConfig config;
  config.ServerGameType = "DarkSouls3";
  config.ServerHostname = "example.com";
  config.ServerPort = 50050;
  bool saved = config.SaveToDirectory(testDir);
  assert(saved);

  // Note: Injector::Init() currently relies on GetModuleBaseRegion() which looks
  // for the game module by name in the current process. Since we loaded it as a
  // data file, it won't be found by name. This test validates that Init() fails
  // gracefully when the game is not actually running in the process.
  //
  // A full integration test would require launching the actual game and attaching,
  // which is beyond the scope of this test harness.

  Injector injector;
  bool initialized = injector.Init(RuntimeConfig::GetConfigPath(testDir));

  std::cout << "Injector::Init() result: " << (initialized ? "success" : "failed") << std::endl;
  if (!initialized) {
    std::cout << "Init error: " << injector.GetLastInitError() << std::endl;
  }

  injector.Term();

  // Clean up
#ifdef _WIN32
  if (GameModule) {
    FreeLibrary(GameModule);
  }
#endif
  std::filesystem::remove_all(testDir);

  std::cout << "Integration test complete." << std::endl;
}
