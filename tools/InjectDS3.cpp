// Development injector: attach, inspect, detach, or load a new DLL without
// restarting DS3. Old DLLs without the cooperative protocol require one restart.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include "../Source/Injector/ControlProtocol.h"
#include "../Source/Injector/InjectorLog.h"
#include "Shared/Core/Utils/Logging.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include <cwchar>
#include <cerrno>
#include <algorithm>

namespace {
bool ApiFailure(const char* operation) {
  const DWORD error = GetLastError();
  char description[512]{};
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                 error, 0, description, sizeof(description), nullptr);
  ErrorS("InjectDS3", "%s failed (error=%lu): %s", operation, error, description);
  SetLastError(error);
  return false;
}
class Handle {
public:
  explicit Handle(HANDLE value = nullptr) : Value(value) {}
  ~Handle() {
    if (Valid())
      CloseHandle(Value);
  }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  bool Valid() const { return Value && Value != INVALID_HANDLE_VALUE; }
  HANDLE Get() const { return Value; }

private:
  HANDLE Value;
};
struct Module {
  std::uintptr_t Base;
  std::filesystem::path Path;
};

bool Modules(DWORD pid, std::vector<Module>& modules) {
  modules.clear();
  HANDLE raw = INVALID_HANDLE_VALUE;
  for (int attempt = 0; attempt < 10; ++attempt) {
    raw = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (raw != INVALID_HANDLE_VALUE || GetLastError() != ERROR_BAD_LENGTH)
      break;
  }
  Handle snapshot(raw);
  if (!snapshot.Valid())
    return ApiFailure("CreateToolhelp32Snapshot(modules)");
  MODULEENTRY32W entry{sizeof(entry)};
  if (!Module32FirstW(snapshot.Get(), &entry))
    return ApiFailure("Module32FirstW");
  do {
    modules.push_back({reinterpret_cast<std::uintptr_t>(entry.modBaseAddr), entry.szExePath});
  } while (Module32NextW(snapshot.Get(), &entry));
  return GetLastError() == ERROR_NO_MORE_FILES;
}

bool Command(DWORD pid, InjectorControl::Command command, InjectorControl::Response& response) {
  Handle pipe(CreateFileW(InjectorControl::PipeName(pid).c_str(), GENERIC_READ | GENERIC_WRITE,
                          0, nullptr, OPEN_EXISTING, 0, nullptr));
  if (!pipe.Valid())
    return false;
  ULONG serverPid = 0;
  if (!GetNamedPipeServerProcessId(pipe.Get(), &serverPid) || serverPid != pid)
    return false;
  DWORD mode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(pipe.Get(), &mode, nullptr, nullptr))
    return false;
  const InjectorControl::Request request{InjectorControl::Version, command};
  DWORD count = 0;
  if (!WriteFile(pipe.Get(), &request, sizeof(request), &count, nullptr) || count != sizeof(request))
    return false;
  const auto deadline = GetTickCount64() + 10000;
  do {
    DWORD available = 0;
    if (!PeekNamedPipe(pipe.Get(), nullptr, 0, nullptr, &available, nullptr))
      return false;
    if (available) {
      if (!ReadFile(pipe.Get(), &response, sizeof(response), &count, nullptr) || count != sizeof(response))
        return false;
      const char ack = 1;
      WriteFile(pipe.Get(), &ack, sizeof(ack), &count, nullptr);
      return response.Protocol == InjectorControl::Version;
    }
    Sleep(10);
  } while (GetTickCount64() < deadline);
  SetLastError(ERROR_TIMEOUT);
  return false;
}

bool Status(DWORD pid, InjectorControl::Response& response) {
  // Allow the previous client to finish its acknowledgment/disconnect.
  for (int attempt = 0; attempt < 20; ++attempt) {
    if (Command(pid, InjectorControl::Command::Status, response))
      return true;
    if (GetLastError() != ERROR_PIPE_BUSY)
      return false;
    Sleep(20);
  }
  return false;
}

bool IsShadow(const std::filesystem::path& path) {
  return path.filename().wstring().find(L"Injector-hot-") == 0 && path.extension() == L".dll";
}

void RemoveShadow(const std::filesystem::path& path) {
  if (IsShadow(path) && !DeleteFileW(path.c_str()))
    LogS("InjectDS3", "Could not remove unloaded shadow DLL (error=%lu): %ls\n", GetLastError(), path.c_str());
}

bool Detach(DWORD pid) {
  InjectorControl::Response status;
  if (!Status(pid, status)) {
    LogS("InjectDS3", "No compatible control endpoint. A legacy injector requires a game restart.\n");
    return false;
  }
  std::vector<Module> modules;
  if (!Modules(pid, modules))
    return false;
  std::filesystem::path oldPath;
  for (const auto& module : modules)
    if (module.Base == status.Module)
      oldPath = module.Path;
  if (oldPath.empty())
    return false;
  Handle worker(OpenThread(SYNCHRONIZE | THREAD_QUERY_LIMITED_INFORMATION, FALSE, status.WorkerThreadId));
  if (!worker.Valid() || GetProcessIdOfThread(worker.Get()) != pid)
    return false;

  InjectorControl::Response response;
  bool replied = false;
  for (int attempt = 0; attempt < 20; ++attempt) {
    if (Command(pid, InjectorControl::Command::Detach, response)) {
      replied = true;
      break;
    }
    if (GetLastError() != ERROR_PIPE_BUSY)
      break;
    Sleep(20);
  }
  if (!replied || response.Module != status.Module || response.Status != InjectorControl::State::Detached || response.Error != ERROR_SUCCESS) {
    LogS("InjectDS3", "Detach was not confirmed; no replacement will be injected. Check --status before retrying.\n");
    return false;
  }
  if (WaitForSingleObject(worker.Get(), 30000) != WAIT_OBJECT_0 || !Modules(pid, modules)) {
    LogS("InjectDS3", "Timed out or could not verify unload; replacement will not be injected.\n");
    return false;
  }
  for (const auto& module : modules) {
    if (module.Base == status.Module) {
      LogS("InjectDS3", "DLL still mapped (possibly another LoadLibrary reference); refusing replacement.\n");
      return false;
    }
  }
  RemoveShadow(oldPath);
  LogS("InjectDS3", "Injector detached and DLL unloaded.\n");
  return true;
}

bool HasInjector(const std::vector<Module>& modules, const std::filesystem::path& requested) {
  for (const auto& module : modules) {
    if (_wcsicmp(module.Path.filename().c_str(), L"Injector.dll") == 0 || IsShadow(module.Path) ||
        _wcsicmp(module.Path.c_str(), requested.c_str()) == 0)
      return true;
  }
  return false;
}

bool Inject(HANDLE process, DWORD pid, const std::filesystem::path& shadow) {
  // Resolve the owner of LoadLibraryW (it may be forwarded to KernelBase), then
  // apply its RVA to the target module; do not assume equal ASLR bases.
  const auto loadLibrary = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
  HMODULE owner = nullptr;
  if (!loadLibrary || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                          reinterpret_cast<LPCWSTR>(loadLibrary), &owner))
    return false;
  wchar_t ownerPath[32768]{};
  if (!GetModuleFileNameW(owner, ownerPath, 32768))
    return false;
  const auto ownerName = std::filesystem::path(ownerPath).filename();
  std::vector<Module> modules;
  if (!Modules(pid, modules))
    return false;
  std::uintptr_t remoteLoad = 0;
  for (const auto& module : modules) {
    if (_wcsicmp(module.Path.filename().c_str(), ownerName.c_str()) == 0)
      remoteLoad = module.Base + reinterpret_cast<std::uintptr_t>(loadLibrary) - reinterpret_cast<std::uintptr_t>(owner);
  }
  if (!remoteLoad) {
    ErrorS("InjectDS3", "Could not resolve LoadLibraryW owner in target process");
    return false;
  }

  const auto bytes = (shadow.native().size() + 1) * sizeof(wchar_t);
  void* parameter = VirtualAllocEx(process, nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (!parameter)
    return ApiFailure("VirtualAllocEx");
  SIZE_T written = 0;
  if (!WriteProcessMemory(process, parameter, shadow.c_str(), bytes, &written) || written != bytes) {
    ApiFailure("WriteProcessMemory(DLL path)");
    VirtualFreeEx(process, parameter, 0, MEM_RELEASE);
    return false;
  }
  Handle thread(CreateRemoteThread(process, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteLoad), parameter, 0, nullptr));
  if (!thread.Valid()) {
    ApiFailure("CreateRemoteThread(LoadLibraryW)");
    VirtualFreeEx(process, parameter, 0, MEM_RELEASE);
    return false;
  }
  if (WaitForSingleObject(thread.Get(), 30000) != WAIT_OBJECT_0) {
    // Remote code may still be reading the argument. Never free it on timeout.
    LogS("InjectDS3", "LoadLibrary timed out; remote argument retained until process exit.\n");
    return false;
  }
  VirtualFreeEx(process, parameter, 0, MEM_RELEASE);

  // Thread exit codes truncate 64-bit HMODULEs; inspect the module list instead.
  if (!Modules(pid, modules))
    return false;
  std::uintptr_t base = 0;
  for (const auto& module : modules)
    if (_wcsicmp(module.Path.c_str(), shadow.c_str()) == 0)
      base = module.Base;
  if (!base) {
    ErrorS("InjectDS3", "Remote loader finished but requested DLL is not mapped: %ls", shadow.c_str());
    return false;
  }
  const auto deadline = GetTickCount64() + 30000;
  do {
    InjectorControl::Response response;
    if (Status(pid, response) && response.Module == base) {
      if (response.Status == InjectorControl::State::Running && response.Error == ERROR_SUCCESS) {
        LogS("InjectDS3", "Replacement initialized and hooks installed.\n");
        return true;
      }
      LogS("InjectDS3", "DLL loaded but initialization failed. Inspect rekindled-injector.log; use --detach before retrying.\n");
      return false;
    }
    Sleep(50);
  } while (GetTickCount64() < deadline);
  LogS("InjectDS3", "DLL readiness was not confirmed. Use --status before retrying.\n");
  return false;
}

DWORD FindGame() {
  Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (!snapshot.Valid())
    return MAXDWORD;
  PROCESSENTRY32W entry{sizeof(entry)};
  DWORD pid = 0;
  if (Process32FirstW(snapshot.Get(), &entry)) {
    do {
      if (_wcsicmp(entry.szExeFile, L"DarkSoulsIII.exe") == 0) {
        if (pid) {
          LogS("InjectDS3", "Multiple game processes found; specify --pid.\n");
          return MAXDWORD;
        }
        pid = entry.th32ProcessID;
      }
    } while (Process32NextW(snapshot.Get(), &entry));
  }
  if (GetLastError() != ERROR_NO_MORE_FILES)
    return MAXDWORD;
  return pid;
}

void Usage() {
  LogS("InjectDS3", "InjectDS3 [game.exe] [Injector.dll]\n"
                    "InjectDS3 --attach|--reload [Injector.dll] [--pid PID]\n"
                    "InjectDS3 --status|--detach [--pid PID]\n"
                    "Run detach/reload at the title screen, disconnected from multiplayer.\n");
}
} // namespace

int RunToolCommand(int argc, wchar_t* argv[]) {
  try {
    std::wstring action = L"launch";
    std::filesystem::path game = LR"(I:\SteamLibrary\steamapps\common\DARK SOULS III\Game\DarkSoulsIII.exe)";
    std::filesystem::path source = L"build/bin/Debug/Injector.dll";
    DWORD pid = 0;
    int next = 1;
    if (argc > 1 && std::wstring(argv[1]).find(L"--") == 0) {
      action = argv[1];
      next = 2;
      if (action != L"--attach" && action != L"--reload" && action != L"--status" && action != L"--detach") {
        Usage();
        return action == L"--help" ? 0 : 1;
      }
    }
    int positional = 0;
    for (; next < argc; ++next) {
      if (std::wstring(argv[next]) == L"--pid" && next + 1 < argc) {
        wchar_t* end = nullptr;
        const std::wstring input = argv[++next];
        errno = 0;
        const auto value = wcstoul(input.c_str(), &end, 10);
        if (!value || !end || *end || pid || errno == ERANGE ||
            !std::all_of(input.begin(), input.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; })) {
          Usage();
          return 1;
        }
        pid = value;
      } else if (std::wstring(argv[next]).find(L"--") == 0) {
        Usage();
        return 1;
      } else if (action == L"launch" && positional++ == 0) {
        game = argv[next];
      } else if ((action == L"launch" && positional == 2) ||
                 ((action == L"--attach" || action == L"--reload") && positional++ == 0)) {
        source = argv[next];
      } else {
        Usage();
        return 1;
      }
    }
    if (action != L"--status" && action != L"--detach") {
      source = std::filesystem::canonical(source);
      if (!std::filesystem::is_regular_file(source))
        return 1;
    }
    if (!pid)
      pid = FindGame();
    if (pid == MAXDWORD) {
      LogS("InjectDS3", "Cannot select a unique game process.\n");
      return 1;
    }
    if (!pid && action == L"launch") {
      STARTUPINFOW startup{sizeof(startup)};
      PROCESS_INFORMATION info{};
      if (!CreateProcessW(game.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, game.parent_path().c_str(), &startup, &info)) {
        LogS("InjectDS3", "Game launch failed (error=%lu).\n", GetLastError());
        return 1;
      }
      Handle process(info.hProcess), thread(info.hThread);
      pid = info.dwProcessId;
      LogS("InjectDS3", "Waiting for game initialization (PID %lu)...\n", pid);
      if (WaitForSingleObject(process.Get(), 30000) != WAIT_TIMEOUT)
        return 1;
    }
    if (!pid) {
      LogS("InjectDS3", "Game not found.\n");
      return 1;
    }
    LogS("InjectDS3", "Operation=%ls; target PID=%lu; source=%ls", action.c_str(), pid, source.c_str());
    Handle mutex(CreateMutexW(nullptr, FALSE, (L"Local\\Rekindled.Injector.Tool." + std::to_wstring(pid)).c_str()));
    if (!mutex.Valid())
      return 1;
    const auto lockResult = WaitForSingleObject(mutex.Get(), 0);
    if (lockResult != WAIT_OBJECT_0 && lockResult != WAIT_ABANDONED) {
      LogS("InjectDS3", "Another injector operation is in progress.\n");
      return 1;
    }
    struct Unlock {
      HANDLE Mutex;
      ~Unlock() { ReleaseMutex(Mutex); }
    } unlock{mutex.Get()};
    Handle process(OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                   PROCESS_VM_WRITE | PROCESS_VM_READ | SYNCHRONIZE,
                               FALSE, pid));
    if (!process.Valid()) {
      LogS("InjectDS3", "OpenProcess failed (error=%lu).\n", GetLastError());
      return 1;
    }
    BOOL selfWow = FALSE, targetWow = FALSE;
    if (!IsWow64Process(GetCurrentProcess(), &selfWow) || !IsWow64Process(process.Get(), &targetWow) || selfWow != targetWow) {
      LogS("InjectDS3", "Tool and game architecture must match.\n");
      return 1;
    }
    if (action == L"--status") {
      InjectorControl::Response response;
      if (!Status(pid, response)) {
        LogS("InjectDS3", "No compatible injector endpoint.\n");
        return 1;
      }
      LogS("InjectDS3", "PID %lu: %s (module=0x%llx, worker=%lu, error=%lu)\n", pid,
           response.Status == InjectorControl::State::Running ? "running" : "failed",
           static_cast<unsigned long long>(response.Module), static_cast<unsigned long>(response.WorkerThreadId),
           static_cast<unsigned long>(response.Error));
      return response.Status == InjectorControl::State::Running ? 0 : 1;
    }
    if (action == L"--detach")
      return Detach(pid) ? 0 : 1;
    std::vector<Module> modules;
    if (!Modules(pid, modules)) {
      LogS("InjectDS3", "Cannot enumerate target modules.\n");
      return 1;
    }
    InjectorControl::Response status;
    if (action != L"--reload" && (Status(pid, status) || HasInjector(modules, source))) {
      LogS("InjectDS3", "Injector already loaded. Use --reload; legacy injectors require one game restart.\n");
      return 1;
    }
    const auto shadow = source.parent_path() / (L"Injector-hot-" + std::to_wstring(pid) + L"-" + std::to_wstring(GetTickCount64()) + L".dll");
    if (!CopyFileW(source.c_str(), shadow.c_str(), TRUE)) {
      LogS("InjectDS3", "Cannot stage replacement (error=%lu).\n", GetLastError());
      return 1;
    }
    if (action == L"--reload" && !Detach(pid)) {
      RemoveShadow(shadow);
      return 1;
    }
    LogS("InjectDS3", "Loading staged DLL: %ls", shadow.c_str());
    const bool success = Inject(process.Get(), pid, shadow);
    if (!success)
      LogS("InjectDS3", "Injection not confirmed; no forced unload performed.\n");
    return success ? 0 : 1;
  } catch (const std::exception& error) {
    LogS("InjectDS3", "Injector tool failed: %s\n", error.what());
    return 1;
  }
}

int wmain(int argc, wchar_t* argv[]) {
  InitLogFile({}, "rekindled-injector-tool.log");
  const int result = RunToolCommand(argc, argv);
  LogS("InjectDS3", "Tool session ended; exit=%d", result);
  CloseLogFile();
  return result;
}
