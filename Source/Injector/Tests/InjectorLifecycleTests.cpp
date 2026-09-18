#include "Injector/ControlProtocol.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <string>

namespace {
void Check(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "%s (Win32 error=%lu)\n", message, GetLastError());
    std::exit(1);
  }
}

DWORD HandleCount() {
  DWORD count = 0;
  Check(GetProcessHandleCount(GetCurrentProcess(), &count) != FALSE, "query handle count");
  return count;
}

} // namespace

extern "C" __declspec(dllexport) __declspec(noinline) int WINAPI LifecycleTarget(int value) {
  volatile int result = value;
  result = result * 3;
  result = result + 7;
  return result;
}

namespace {

InjectorControl::Response Exchange(HANDLE pipe, InjectorControl::Command command) {
  const InjectorControl::Request request{InjectorControl::Version, command};
  DWORD count = 0;
  Check(WriteFile(pipe, &request, sizeof(request), &count, nullptr) && count == sizeof(request), "write request");
  InjectorControl::Response response;
  const auto deadline = GetTickCount64() + 5000;
  for (;;) {
    DWORD available = 0;
    Check(PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) != FALSE, "peek response");
    if (available)
      break;
    Check(GetTickCount64() < deadline, "response timeout");
    Sleep(10);
  }
  Check(ReadFile(pipe, &response, sizeof(response), &count, nullptr) && count == sizeof(response), "read response");
  const char ack = 1;
  Check(WriteFile(pipe, &ack, sizeof(ack), &count, nullptr) != FALSE, "ack response");
  return response;
}

HANDLE Connect() {
  const auto deadline = GetTickCount64() + 10000;
  for (;;) {
    HANDLE pipe = CreateFileW(InjectorControl::PipeName(GetCurrentProcessId()).c_str(),
                              GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe != INVALID_HANDLE_VALUE)
      return pipe;
    Check(GetTickCount64() < deadline, "connect timeout");
    Sleep(10);
  }
}

void TestDetourCycles(const wchar_t* fixturePath) {
  DWORD baselineHandles = 0;
  std::array<unsigned char, 32> originalBytes{};
  std::memcpy(originalBytes.data(), reinterpret_cast<void*>(&LifecycleTarget), originalBytes.size());
  for (int cycle = 0; cycle < 25; ++cycle) {
    HMODULE fixture = LoadLibraryW(fixturePath);
    Check(fixture != nullptr, "load detour fixture");
    using Install = LONG(WINAPI*)(void*, HANDLE, HANDLE, BOOL);
    using Detach = LONG(WINAPI*)();
    const auto install = reinterpret_cast<Install>(GetProcAddress(fixture, "InstallFixture"));
    const auto detach = reinterpret_cast<Detach>(GetProcAddress(fixture, "DetachFixture"));
    Check(install && detach, "fixture exports");
    HANDLE entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Check(entered && release, "callback events");
    Check(install(reinterpret_cast<void*>(&LifecycleTarget), entered, release, cycle % 2) == NO_ERROR, "attach test detour");
    int hookedResult = 0;
    std::thread caller([&] { hookedResult = LifecycleTarget(5); });
    Check(WaitForSingleObject(entered, 5000) == WAIT_OBJECT_0, "callback entered");
    Check(detach() == ERROR_BUSY, "busy callback must prevent detach");
    SetEvent(release);
    caller.join();
    Check(hookedResult == 122, "detour preserved after refusal");
    Check(detach() == NO_ERROR, "detach after callback returns");
    Check(detach() == NO_ERROR, "detach is idempotent");
    Check(LifecycleTarget(5) == 22, "original behavior restored");
    Check(std::memcmp(originalBytes.data(), reinterpret_cast<void*>(&LifecycleTarget), originalBytes.size()) == 0, "original code bytes restored");
    Check(FreeLibrary(fixture) != FALSE, "unload fixture");
    Check(GetModuleHandleW(fixturePath) == nullptr, "fixture unmapped");
    Check(LifecycleTarget(5) == 22, "original remains callable after DLL unload");
    CloseHandle(entered);
    CloseHandle(release);
    if (cycle == 0)
      baselineHandles = HandleCount();
    else
      Check(HandleCount() <= baselineHandles, "detour cycles do not leak handles");
  }
}

void TestFailedInitializationCycles(const wchar_t* injectorPath) {
  DWORD baselineHandles = 0;
  for (int cycle = 0; cycle < 5; ++cycle) {
    HMODULE injector = LoadLibraryW(injectorPath);
    Check(injector != nullptr, "load real injector");
    HANDLE pipe = Connect();
    const auto status = Exchange(pipe, InjectorControl::Command::Status);
    CloseHandle(pipe);
    Check(status.Status == InjectorControl::State::Failed, "non-game host reports failed initialization");
    Check(status.Module == reinterpret_cast<std::uint64_t>(injector), "status identifies correct module");
    HANDLE worker = OpenThread(SYNCHRONIZE, FALSE, status.WorkerThreadId);
    Check(worker != nullptr, "open worker for unload confirmation");
    pipe = Connect();
    const auto detach = Exchange(pipe, InjectorControl::Command::Detach);
    CloseHandle(pipe);
    Check(detach.Status == InjectorControl::State::Detached && detach.Error == ERROR_SUCCESS, "failed injector detaches cleanly");
    Check(WaitForSingleObject(worker, 5000) == WAIT_OBJECT_0, "worker exits after unload");
    CloseHandle(worker);
    Check(GetModuleHandleW(injectorPath) == nullptr, "real injector unmapped");
    if (cycle == 0)
      baselineHandles = HandleCount();
    else
      Check(HandleCount() <= baselineHandles, "real DLL cycles do not leak handles");
  }
}

DWORD RunTool(const wchar_t* tool, const std::wstring& arguments) {
  std::wstring command = L"\"" + std::wstring(tool) + L"\" " + arguments + L" --pid " + std::to_wstring(GetCurrentProcessId());
  STARTUPINFOW startup{sizeof(startup)};
  PROCESS_INFORMATION process{};
  Check(CreateProcessW(tool, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != FALSE, "start injector tool");
  Check(WaitForSingleObject(process.hProcess, 20000) == WAIT_OBJECT_0, "tool finishes within deadline");
  DWORD code = 0;
  Check(GetExitCodeProcess(process.hProcess, &code) != FALSE, "tool exit code");
  CloseHandle(process.hProcess);
  CloseHandle(process.hThread);
  return code;
}

void TestToolReload(const wchar_t* tool, const wchar_t* firstDll, const wchar_t* secondDll) {
  DWORD baselineHandles = 0;
  const std::wstring first = L"\"" + std::wstring(firstDll) + L"\"";
  const std::wstring second = L"\"" + std::wstring(secondDll) + L"\"";
  for (int cycle = 0; cycle < 3; ++cycle) {
    Check(RunTool(tool, L"--attach " + first) == 0, "tool attaches first DLL");
    Check(LifecycleTarget(5) == 122, "first DLL behavior");
    Check(RunTool(tool, L"--status") == 0, "tool confirms readiness");
    Check(RunTool(tool, L"--attach " + first) != 0, "tool rejects duplicate attach");
    Check(RunTool(tool, L"--reload " + second) == 0, "tool reloads second DLL");
    Check(LifecycleTarget(5) == 222, "new DLL code is executing");
    Check(RunTool(tool, L"--detach") == 0, "tool detaches second DLL");
    Check(LifecycleTarget(5) == 22, "game function restored after tool detach");
    Check(RunTool(tool, L"--status") != 0, "detached endpoint is gone");
    if (cycle == 0)
      baselineHandles = HandleCount();
    else
      Check(HandleCount() <= baselineHandles, "tool reload cycles do not leak target handles");
  }
}
} // namespace

int wmain(int argc, wchar_t* argv[]) {
  Check(argc == 6, "expected detour fixture, injector, tool, and two control fixture DLL paths");
  TestDetourCycles(argv[1]);
  TestFailedInitializationCycles(argv[2]);
  TestToolReload(argv[3], argv[4], argv[5]);
  std::puts("Passed 25 detour cycles, 5 failed-init cycles, and 3 end-to-end tool reloads with different DLL code.");
  return 0;
}
