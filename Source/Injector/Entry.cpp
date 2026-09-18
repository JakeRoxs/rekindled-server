/*
 * Rekindled Server
 * Copyright (C) 2021 Tim Leonard
 * Copyright (C) 2026 Jake Morgeson
 *
 * This program is free software; licensed under the MIT license.
 * You should have received a copy of the license along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 */

#include <WinSock2.h>
#include <windows.h>
#include <exception>

#include "Shared/Core/Utils/Logging.h"

#include "Injector/Injector.h"
#include "Injector/ControlServer.h"
#include "Injector/InjectorLog.h"

DWORD WINAPI InjectorThread(void* module);
void RunInjector(HMODULE module);

// No shutdown, locks, or joins under the loader lock. The worker owns the
// LoadLibrary reference and releases it only after cooperative teardown.
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    HANDLE thread = CreateThread(nullptr, 0, InjectorThread, module, 0, nullptr);
    if (!thread)
      return FALSE;
    CloseHandle(thread);
  }
  return TRUE;
}

DWORD WINAPI InjectorThread(void* parameter) {
  const auto module = static_cast<HMODULE>(parameter);
  try {
    InitLogFile();
    RunInjector(module);
  } catch (...) {
    // Unexpected failure must never turn into an unsafe forced unload.
    ErrorS("Injector", "Worker failed with an unexpected exception; DLL retained");
    CloseLogFile();
    return ERROR_GEN_FAILURE;
  }
  LogS("Injector", "Session ended; all resources released, unloading DLL");
  CloseLogFile();
  // Every automatic object must be destroyed before this non-returning call.
  FreeLibraryAndExitThread(module, ERROR_SUCCESS);
}

void RunInjector(HMODULE module) {
  LogS("Injector", "Worker started; module=%p", module);

  // Do not replace the host's console streams or exception handler. Those
  // process-wide callbacks/handles would outlive an unloaded injector.
  InjectorControlServer control;
  if (!control.IsValid()) {
    ErrorS("Injector", "Control endpoint unavailable (error=%lu)", GetLastError());
    return;
  }

  Log(R"--(    ____             __      _____             __    )--");
  Log(R"--(   / __ \____ ______/ /__   / ___/____  __  __/ /____)--");
  Log(R"--(  / / / / __ `/ ___/ //_/   \__ \/ __ \/ / / / / ___/)--");
  Log(R"--( / /_/ / /_/ / /  / ,<     ___/ / /_/ / /_/ / (__  ) )--");
  Log(R"--(/_____/\__,_/_/  /_/|_|   /____/\____/\__,_/_/____/  )--");
  Log(R"--(  ____  _____ ___     _   _ ___       _____ ___     )--");
  Log(R"--( / __ \/ ____/ __|   / \ | |_ _|__   / ____/ _ \    )--");
  Log(R"--( \__ \/ /__ \__ \   / _ \| || '_ \ | | (___| (_) |   )--");
  Log(R"--( |__) \___||___/   /_/ \_\___| .__/| | \____\___/    )--");
  Log(R"--(                              |_|    |_|              )--");
  Log(R"--(                                                    )--");
  Log("");
  Log("https://github.com/jakeroxs/rekindled-server");
  Log("");

  // Injector needs Winsock only, not the server's process-wide platform setup.
  struct NetworkScope {
    bool Initialized = false;
    NetworkScope() {
      WSADATA data{};
      const int result = WSAStartup(MAKEWORD(2, 2), &data);
      Initialized = result == 0;
      LogS("Network", "WSAStartup result=%d", result);
    }
    ~NetworkScope() {
      if (Initialized) {
        const int result = WSACleanup();
        LogS("Network", "WSACleanup result=%d, error=%d", result, result == 0 ? 0 : WSAGetLastError());
      }
    }
  } network;

  Injector injector;
  bool initialized = false;
  try {
    initialized = network.Initialized && injector.Init();
  } catch (const std::exception& error) {
    ErrorS("Injector", "Initialization exception: %s", error.what());
  }
  if (!initialized) {
    ErrorS("Injector", "Initialization failed: %s", injector.GetLastInitError().c_str());
  } else {
    SuccessS("Injector", "Running; detach/reload control endpoint ready");
  }

  // Even failed initialization stays controllable for cleanup and retry.
  // Run returns only when all detours and reversible patches are removed.
  control.Run(module, initialized, [&injector] { return injector.Term(); });
  LogS("Injector", "Hooks detached; cleaning up worker resources");
}
