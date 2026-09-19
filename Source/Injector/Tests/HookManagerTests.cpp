#include <cassert>
#include <optional>

#include "Injector/HookManager.h"
#include "Injector/Hooks/DarkSouls2/DS2_ReplaceServerAddressHook.h"

// A simple stub hook implementation that records whether Install/Uninstall were called.
struct StubHook : public Hook {
  StubHook(HookError installErrorCode)
      : InstallErrorCode(installErrorCode) {
  }

  HookError Install(const InjectorContext& /*context*/) override {
    Installed = true;
    return InstallErrorCode;
  }

  bool Uninstall() override {
    if (FailUninstall)
      return false;
    Uninstalled = true;
    return true;
  }

  const char* GetName() override {
    return "StubHook";
  }

  HookError InstallErrorCode = HookError::Success;
  bool Installed = false;
  bool Uninstalled = false;
  bool FailUninstall = false;
};

// A hook implementation that exercises the search callbacks in InjectorContext and records the result.
struct SearchCallbackHook : public Hook {
  SearchCallbackHook(HookError installErrorCode)
      : InstallErrorCode(installErrorCode) {
  }

  HookError Install(const InjectorContext& context) override {
    Installed = true;
    FoundAOB = context.SearchAOB({0xAA, std::nullopt, 0xBB});
    FoundString = context.SearchString("test");
    FoundWideString = context.SearchWString(L"test");
    return InstallErrorCode;
  }

  bool Uninstall() override {
    Uninstalled = true;
    return true;
  }

  const char* GetName() override {
    return "SearchCallbackHook";
  }

  HookError InstallErrorCode = HookError::Success;
  bool Installed = false;
  bool Uninstalled = false;

  std::vector<intptr_t> FoundAOB;
  std::vector<intptr_t> FoundString;
  std::vector<intptr_t> FoundWideString;
};

void RunHookManagerTests() {
  // A failed restore preserves its state for retry instead of being forgotten.
  {
    RuntimeConfig config;
    InjectorContext context{config, GameType::Unknown, 0, {}, {}, {}};
    HookManager manager;
    auto hook = std::make_unique<StubHook>(HookError::Success);
    auto* state = hook.get();
    state->FailUninstall = true;
    manager.AddHook(std::move(hook));
    assert(manager.InstallAll(context) == ERROR_SUCCESS);
    assert(manager.UninstallAll() == ERROR_WRITE_FAULT);
    assert(!state->Uninstalled);
    state->FailUninstall = false;
    assert(manager.UninstallAll() == ERROR_SUCCESS);
    assert(state->Uninstalled);
    assert(manager.UninstallAll() == ERROR_SUCCESS);
  }

  // Both normal DS2 restoration and rollback after only the key was patched.
  for (bool hostnameFound : {true, false}) {
    RuntimeConfig config;
    config.ServerHostname = "localhost";
    config.ServerPublicKey = "replacement-key";
    std::vector<unsigned char> key(512, 0xAB);
    std::vector<unsigned char> hostname(128, 0xCD);
    const auto originalKey = key;
    const auto originalHostname = hostname;
    InjectorContext context{
        config, GameType::DarkSouls2, 0, {}, [&](const std::string&) { return std::vector<intptr_t>{reinterpret_cast<intptr_t>(key.data())}; }, [&](const std::wstring&) { return hostnameFound ? std::vector<intptr_t>{reinterpret_cast<intptr_t>(hostname.data())} : std::vector<intptr_t>{}; }};
    HookManager manager;
    manager.AddHook(std::make_unique<DS2_ReplaceServerAddressHook>());
    const DWORD result = manager.InstallAll(context);
    assert((result == ERROR_SUCCESS) == hostnameFound);
    if (hostnameFound) {
      assert(key != originalKey && hostname != originalHostname);
      assert(manager.UninstallAll() == ERROR_SUCCESS);
    }
    assert(key == originalKey && hostname == originalHostname);
  }

  // Case 1: all hooks install; uninstall should not be called until requested.
  {
    RuntimeConfig config;
    InjectorContext context{
        config,
        GameType::Unknown,
        0,
        [](const std::vector<InjectorContext::AOBByte>&) { return std::vector<intptr_t>{}; },
        [](const std::string&) { return std::vector<intptr_t>{}; },
        [](const std::wstring&) { return std::vector<intptr_t>{}; },
    };

    HookManager manager;
    auto h1 = std::make_unique<StubHook>(HookError::Success);
    auto h2 = std::make_unique<StubHook>(HookError::Success);
    StubHook* ptr1 = h1.get();
    StubHook* ptr2 = h2.get();

    manager.AddHook(std::move(h1));
    manager.AddHook(std::move(h2));

    DWORD result = manager.InstallAll(context);
    assert(result == static_cast<DWORD>(HookError::Success));
    assert(ptr1->Installed);
    assert(ptr2->Installed);
    assert(!ptr1->Uninstalled);
    assert(!ptr2->Uninstalled);

    manager.UninstallAll();
    assert(ptr1->Uninstalled);
    assert(ptr2->Uninstalled);
  }

  // Case 2: second hook fails; first should be uninstalled.
  {
    RuntimeConfig config;
    InjectorContext context{
        config,
        GameType::Unknown,
        0,
        [](const std::vector<InjectorContext::AOBByte>&) { return std::vector<intptr_t>{}; },
        [](const std::string&) { return std::vector<intptr_t>{}; },
        [](const std::wstring&) { return std::vector<intptr_t>{}; },
    };

    HookManager manager;
    auto h1 = std::make_unique<StubHook>(HookError::Success);
    auto h2 = std::make_unique<StubHook>(HookError::GeneralFailure);
    StubHook* ptr1 = h1.get();
    StubHook* ptr2 = h2.get();

    manager.AddHook(std::move(h1));
    manager.AddHook(std::move(h2));

    DWORD result = manager.InstallAll(context);
    assert(result != static_cast<DWORD>(HookError::Success));
    assert(result == static_cast<DWORD>(HookError::GeneralFailure));
    assert(ptr1->Installed);
    assert(ptr2->Installed);
    assert(ptr1->Uninstalled);
    // A failed install may have partially patched the process and needs rollback.
    assert(ptr2->Uninstalled);
  }

  // Case 3: verify that hooks can use the InjectorContext search callbacks.
  {
    RuntimeConfig config;
    InjectorContext context{
        config,
        GameType::Unknown,
        0,
        [](const std::vector<InjectorContext::AOBByte>&) { return std::vector<intptr_t>{0x1234}; },
        [](const std::string&) { return std::vector<intptr_t>{0x5678}; },
        [](const std::wstring&) { return std::vector<intptr_t>{0x9ABC}; },
    };

    HookManager manager;
    auto h = std::make_unique<SearchCallbackHook>(HookError::Success);
    SearchCallbackHook* ptr = h.get();

    manager.AddHook(std::move(h));

    DWORD result = manager.InstallAll(context);
    assert(result == static_cast<DWORD>(HookError::Success));
    assert(ptr->Installed);

    assert(ptr->FoundAOB.size() == 1 && ptr->FoundAOB[0] == 0x1234);
    assert(ptr->FoundString.size() == 1 && ptr->FoundString[0] == 0x5678);
    assert(ptr->FoundWideString.size() == 1 && ptr->FoundWideString[0] == 0x9ABC);

    manager.UninstallAll();
    assert(ptr->Uninstalled);
  }
}
