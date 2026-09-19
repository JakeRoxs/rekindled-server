#pragma once

#include <Windows.h>

// All callbacks must hold this guard, including while calling the original.
// Lifecycle operations are serialized on the injector's worker thread.
namespace InjectorDetours {
class CallbackScope {
public:
  CallbackScope() noexcept;
  ~CallbackScope();
  CallbackScope(const CallbackScope&) = delete;
  CallbackScope& operator=(const CallbackScope&) = delete;
};

LONG Attach(void** original, void* callback);
// Failure leaves every detour and its callback state alive for a later retry.
LONG DetachAll();
// Read on the lifecycle worker after failure; no logging occurs while peers are suspended.
const char* LastFailureStage();
DWORD LastFailureThread();
} // namespace InjectorDetours
