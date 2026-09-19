#pragma once

#include "Injector/ControlProtocol.h"
#include <functional>

// One client at a time. Nonblocking pipe I/O keeps lifecycle work on one thread.
class InjectorControlServer {
public:
  InjectorControlServer();
  ~InjectorControlServer();
  InjectorControlServer(const InjectorControlServer&) = delete;
  InjectorControlServer& operator=(const InjectorControlServer&) = delete;

  bool IsValid() const { return Pipe != INVALID_HANDLE_VALUE; }
  void Run(HMODULE module, bool initialized, const std::function<bool()>& detach);

private:
  HANDLE Pipe = INVALID_HANDLE_VALUE;
};
