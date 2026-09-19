#include "Injector/DetourLifetime.h"

namespace {
using Target = int(WINAPI*)(int);
Target Original = nullptr;
HANDLE Entered = nullptr;
HANDLE Release = nullptr;
bool LateReturn = false;

int WINAPI Callback(int value) {
  int result = 0;
  {
    InjectorDetours::CallbackScope scope;
    if (Entered && !LateReturn) {
      SetEvent(Entered);
      WaitForSingleObject(Release, INFINITE);
    }
    result = Original(value) + 100;
  }
  // Simulate runtime epilogue code after the active-callback counter drops.
  // Stack inspection must still refuse to unload this return address.
  if (Entered && LateReturn) {
    SetEvent(Entered);
    WaitForSingleObject(Release, INFINITE);
  }
  return result;
}
} // namespace

extern "C" __declspec(dllexport) LONG WINAPI InstallFixture(void* target, HANDLE entered, HANDLE release, BOOL lateReturn) {
  Original = reinterpret_cast<Target>(target);
  Entered = entered;
  Release = release;
  LateReturn = lateReturn != FALSE;
  return InjectorDetours::Attach(reinterpret_cast<void**>(&Original), reinterpret_cast<void*>(Callback));
}

extern "C" __declspec(dllexport) LONG WINAPI DetachFixture() {
  return InjectorDetours::DetachAll();
}
