#include "Injector/ControlServer.h"
#include "Injector/DetourLifetime.h"

namespace {
using Target = int(WINAPI*)(int);
Target Original = nullptr;

int WINAPI Callback(int value) {
  InjectorDetours::CallbackScope scope;
  return Original(value) + FIXTURE_VERSION;
}

void Run(HMODULE module) {
  InjectorControlServer server;
  if (!server.IsValid())
    return;
  Original = reinterpret_cast<Target>(GetProcAddress(GetModuleHandleW(nullptr), "LifecycleTarget"));
  const bool installed = Original && InjectorDetours::Attach(reinterpret_cast<void**>(&Original), reinterpret_cast<void*>(Callback)) == NO_ERROR;
  server.Run(module, installed, [] { return InjectorDetours::DetachAll() == NO_ERROR; });
}

DWORD WINAPI Worker(void* parameter) {
  const auto module = static_cast<HMODULE>(parameter);
  Run(module);
  FreeLibraryAndExitThread(module, 0);
}
} // namespace

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    HANDLE worker = CreateThread(nullptr, 0, Worker, module, 0, nullptr);
    if (!worker)
      return FALSE;
    CloseHandle(worker);
  }
  return TRUE;
}
