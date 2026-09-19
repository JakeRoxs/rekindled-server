#include "Injector/ControlServer.h"
#include "Shared/Core/Utils/Logging.h"

InjectorControlServer::InjectorControlServer() {
  Pipe = CreateNamedPipeW(InjectorControl::PipeName(GetCurrentProcessId()).c_str(),
                          PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                          PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS,
                          1, 1024, 1024, 0, nullptr);
  if (IsValid()) {
    LogS("Control", "Listening on %ls", InjectorControl::PipeName(GetCurrentProcessId()).c_str());
  } else {
    ErrorS("Control", "CreateNamedPipe failed (error=%lu)", GetLastError());
  }
}

InjectorControlServer::~InjectorControlServer() {
  if (IsValid()) {
    DisconnectNamedPipe(Pipe);
    CloseHandle(Pipe);
    LogS("Control", "Pipe closed");
  }
}

void InjectorControlServer::Run(HMODULE module, bool initialized, const std::function<bool()>& detach) {
  using namespace InjectorControl;
  for (;;) {
    const BOOL connected = ConnectNamedPipe(Pipe, nullptr);
    const DWORD connectError = connected ? ERROR_SUCCESS : GetLastError();
    if (!connected && connectError != ERROR_PIPE_CONNECTED) {
      if (connectError == ERROR_NO_DATA)
        DisconnectNamedPipe(Pipe);
      Sleep(20);
      continue;
    }

    Request request{};
    DWORD count = 0;
    const auto deadline = GetTickCount64() + 3000;
    bool received = false;
    do {
      if (ReadFile(Pipe, &request, sizeof(request), &count, nullptr)) {
        received = count == sizeof(request);
        break;
      }
      if (GetLastError() != ERROR_NO_DATA)
        break;
      Sleep(10);
    } while (GetTickCount64() < deadline);

    bool detached = false;
    Response response{Version, initialized ? State::Running : State::Failed, static_cast<std::uint32_t>(initialized ? ERROR_SUCCESS : ERROR_DLL_INIT_FAILED),
                      GetCurrentThreadId(), reinterpret_cast<std::uint64_t>(module)};
    if (!received || request.Protocol != Version) {
      response.Error = ERROR_INVALID_DATA;
      WarningS("Control", "Rejected malformed request: received=%d, bytes=%lu, version=%u", received, count, request.Protocol);
    } else if (request.Action == Command::Detach) {
      LogS("Control", "Detach requested; checking hooks and pending callbacks");
      detached = detach();
      response.Error = detached ? ERROR_SUCCESS : ERROR_BUSY;
      response.Status = detached ? State::Detached : State::Failed;
      if (!detached)
        initialized = false;
      LogS("Control", "Detach result: detached=%d, error=%u", detached, response.Error);
    } else if (request.Action != Command::Status) {
      response.Error = ERROR_INVALID_FUNCTION;
      WarningS("Control", "Unknown command=%u", static_cast<unsigned>(request.Action));
    } else {
      LogS("Control", "Status requested: state=%u, error=%u", static_cast<unsigned>(response.Status), response.Error);
    }

    if (WriteFile(Pipe, &response, sizeof(response), &count, nullptr) && count == sizeof(response)) {
      // Wait for acknowledgment, not FlushFileBuffers (which can block forever).
      // A client that disappears cannot keep this worker or the DLL alive.
      const auto ackDeadline = GetTickCount64() + 1000;
      char ack = 0;
      while (GetTickCount64() < ackDeadline) {
        if (ReadFile(Pipe, &ack, sizeof(ack), &count, nullptr) || GetLastError() != ERROR_NO_DATA)
          break;
        Sleep(10);
      }
    } else {
      WarningS("Control", "Response delivery failed (error=%lu); detached=%d", GetLastError(), detached);
    }
    DisconnectNamedPipe(Pipe);
    if (detached)
      return;
  }
}
