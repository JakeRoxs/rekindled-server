#include "Injector/DetourLifetime.h"
#include "ThirdParty/detours/src/detours.h"

#include <TlHelp32.h>
#include <atomic>
#include <cstdint>
#include <vector>

namespace InjectorDetours {
namespace {
struct Binding {
  void** Original;
  void* Callback;
};
std::vector<Binding> Bindings;
std::atomic<unsigned> ActiveCallbacks{0};
const char* FailureStage = "none";
DWORD FailureThread = 0;

// A callback's C++ epilogue can call runtime code after its scope counter was
// decremented. Walk return addresses as well as the current PC before unmapping
// the DLL. Invalid/unwindless non-leaf stacks fail closed. No C++ objects or heap
// allocation in this SEH boundary while the inspected thread is suspended.
LONG CheckStack(CONTEXT context, void* moduleBase) {
#if defined(_M_X64)
  __try {
    for (unsigned frame = 0; frame < 512; ++frame) {
      if (!context.Rip)
        return NO_ERROR;
      MEMORY_BASIC_INFORMATION region{};
      if (!VirtualQuery(reinterpret_cast<void*>(context.Rip), &region, sizeof(region)) ||
          region.State != MEM_COMMIT || !(region.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
        return ERROR_BUSY;
      if (region.AllocationBase == moduleBase)
        return ERROR_BUSY;

      const DWORD64 oldStack = context.Rsp;
      DWORD64 imageBase = 0;
      const auto function = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
      if (function) {
        void* handlerData = nullptr;
        DWORD64 establisherFrame = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context,
                         &handlerData, &establisherFrame, nullptr);
      } else {
        // Win64 leaf functions leave their return address at RSP.
        SIZE_T bytes = 0;
        DWORD64 returnAddress = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(context.Rsp),
                               &returnAddress, sizeof(returnAddress), &bytes) ||
            bytes != sizeof(returnAddress))
          return ERROR_BUSY;
        context.Rip = returnAddress;
        context.Rsp += sizeof(returnAddress);
      }
      if (context.Rsp <= oldStack)
        return ERROR_BUSY;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return ERROR_BUSY;
  }
  return ERROR_BUSY;
#else
  // Reliable frame inspection here depends on the Win64 unwind ABI.
  return ERROR_NOT_SUPPORTED;
#endif
}

class Threads {
public:
  ~Threads() {
    for (HANDLE handle : Handles)
      CloseHandle(handle);
  }

  LONG Collect() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
      return GetLastError();
    THREADENTRY32 entry{sizeof(entry)};
    BOOL found = Thread32First(snapshot, &entry);
    LONG result = ERROR_SUCCESS;
    while (found) {
      if (entry.th32OwnerProcessID == GetCurrentProcessId() && entry.th32ThreadID != GetCurrentThreadId()) {
        HANDLE handle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION | SYNCHRONIZE,
                                   FALSE, entry.th32ThreadID);
        if (!handle) {
          result = GetLastError();
          break;
        }
        Handles.push_back(handle);
      }
      found = Thread32Next(snapshot, &entry);
    }
    if (result == ERROR_SUCCESS && GetLastError() != ERROR_NO_MORE_FILES)
      result = GetLastError();
    CloseHandle(snapshot);
    return result;
  }

  LONG Enlist(bool requireQuiescence) {
    for (HANDLE handle : Handles) {
      FailureStage = "enlist thread";
      FailureThread = GetThreadId(handle);
      const LONG result = DetourUpdateThread(handle);
      if (result != NO_ERROR)
        return result;
    }
    // Threads created between the first snapshot and suspension were not
    // enlisted. Abort rather than patch while any such thread can run.
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
      return GetLastError();
    THREADENTRY32 entry{sizeof(entry)};
    BOOL found = Thread32First(snapshot, &entry);
    LONG snapshotResult = NO_ERROR;
    while (found) {
      if (entry.th32OwnerProcessID == GetCurrentProcessId() && entry.th32ThreadID != GetCurrentThreadId()) {
        bool enlisted = false;
        for (HANDLE handle : Handles)
          enlisted = enlisted || GetThreadId(handle) == entry.th32ThreadID;
        if (!enlisted) {
          FailureStage = "thread created during transaction";
          FailureThread = entry.th32ThreadID;
          snapshotResult = ERROR_BUSY;
          break;
        }
      }
      found = Thread32Next(snapshot, &entry);
    }
    if (snapshotResult == NO_ERROR && GetLastError() != ERROR_NO_MORE_FILES)
      snapshotResult = GetLastError();
    CloseHandle(snapshot);
    if (snapshotResult != NO_ERROR)
      return snapshotResult;
    if (!requireQuiescence)
      return NO_ERROR;

    // The counter also covers callbacks currently inside external game/OS code.
    if (ActiveCallbacks.load(std::memory_order_seq_cst) != 0) {
      FailureStage = "active callbacks";
      FailureThread = 0;
      return ERROR_BUSY;
    }

    // Cover the prologue before increment and all return paths after decrement.
    MEMORY_BASIC_INFORMATION ownRegion{};
    if (!VirtualQuery(reinterpret_cast<void*>(&DetachAll), &ownRegion, sizeof(ownRegion)))
      return GetLastError();
    for (HANDLE handle : Handles) {
      FailureStage = "callback return stack or unreadable stack";
      FailureThread = GetThreadId(handle);
      CONTEXT context{};
      context.ContextFlags = CONTEXT_FULL;
      if (!GetThreadContext(handle, &context))
        return GetLastError();
      const LONG result = CheckStack(context, ownRegion.AllocationBase);
      if (result != NO_ERROR)
        return result;
    }
    return NO_ERROR;
  }

private:
  std::vector<HANDLE> Handles;
};

LONG Apply(bool detach, Binding addition = {}) {
  FailureStage = "collect process threads";
  FailureThread = 0;
  Threads threads;
  LONG result = threads.Collect();
  if (result != NO_ERROR)
    return result;
  FailureStage = "begin transaction";
  result = DetourTransactionBegin();
  if (result != NO_ERROR)
    return result;

  // Queue changes before suspending threads: Detours allocates trampolines here.
  if (detach) {
    FailureStage = "queue detour removal";
    for (auto it = Bindings.rbegin(); it != Bindings.rend(); ++it) {
      result = DetourDetach(it->Original, it->Callback);
      if (result != NO_ERROR)
        break;
    }
  } else {
    FailureStage = "queue detour attachment";
    result = DetourAttach(addition.Original, addition.Callback);
  }
  if (result == NO_ERROR)
    result = threads.Enlist(detach);
  if (result != NO_ERROR) {
    DetourTransactionAbort(); // Also resumes every enlisted thread.
    return result;
  }
  FailureStage = "commit transaction";
  FailureThread = 0;
  return DetourTransactionCommit();
}
} // namespace

CallbackScope::CallbackScope() noexcept {
  ActiveCallbacks.fetch_add(1, std::memory_order_seq_cst);
}

CallbackScope::~CallbackScope() {
  ActiveCallbacks.fetch_sub(1, std::memory_order_seq_cst);
}

LONG Attach(void** original, void* callback) {
  // Allocate bookkeeping before making a callback reachable.
  Bindings.reserve(Bindings.size() + 1);
  const Binding binding{original, callback};
  const LONG result = Apply(false, binding);
  if (result == NO_ERROR)
    Bindings.push_back(binding);
  return result;
}

LONG DetachAll() {
  if (Bindings.empty())
    return NO_ERROR;
  const LONG result = Apply(true);
  if (result == NO_ERROR)
    Bindings.clear();
  return result;
}

const char* LastFailureStage() {
  return FailureStage;
}
DWORD LastFailureThread() {
  return FailureThread;
}
} // namespace InjectorDetours
