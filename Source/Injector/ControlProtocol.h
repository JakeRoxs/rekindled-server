#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>

namespace InjectorControl {
constexpr std::uint32_t Version = 1;
enum class Command : std::uint32_t { Status = 1,
                                     Detach = 2 };
enum class State : std::uint32_t { Running = 1,
                                   Failed = 2,
                                   Detached = 3 };

struct Request {
  std::uint32_t Protocol = Version;
  Command Action = Command::Status;
};
struct Response {
  std::uint32_t Protocol = Version;
  State Status = State::Failed;
  std::uint32_t Error = ERROR_SUCCESS;
  std::uint32_t WorkerThreadId = 0;
  std::uint64_t Module = 0;
};
static_assert(sizeof(Request) == 8 && sizeof(Response) == 24);

inline std::wstring PipeName(DWORD pid) {
  return L"\\\\.\\pipe\\Rekindled.Injector.v1." + std::to_wstring(pid);
}
} // namespace InjectorControl
