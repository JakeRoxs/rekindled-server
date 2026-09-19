/*
 * Rekindled Server
 * Copyright (C) 2021 Tim Leonard
 * Copyright (C) 2026 Jake Morgeson
 *
 * This program is free software; licensed under the MIT license.
 * You should have received a copy of the license along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 */

#include "Injector/HookManager.h"
#include "Shared/Core/Utils/Logging.h"
#include <Windows.h>
#include "Injector/DetourLifetime.h"

HookManager::~HookManager() {
  if (UninstallAll() != ERROR_SUCCESS) {
    // A failed teardown must not destroy state still referenced by callbacks.
    // The production worker retains the manager and retries instead.
    for (auto& hook : Hooks)
      hook.release();
  }
}

void HookManager::AddHook(std::unique_ptr<Hook> hook) {
  Hooks.push_back(std::move(hook));
}

DWORD HookManager::InstallAll(const InjectorContext& context) {
  if (!InstalledHooks.empty())
    return ERROR_ALREADY_EXISTS;
  InstalledHooks.reserve(Hooks.size());
  for (auto& hook : Hooks) {
    // Also roll back partially installed hooks when a later operation fails.
    InstalledHooks.push_back(hook.get());
    HookError error = hook->Install(context);
    if (error == HookError::Success) {
      Success("\t%s: Success", hook->GetName());
    } else {
      Error("\t%s: Failed (%s)", hook->GetName(), HookErrorToString(error));
      if (error == HookError::DetourFailed) {
        ErrorS("Hooks", "Install failed: hook=%s, stage=%s, thread=%lu", hook->GetName(), InjectorDetours::LastFailureStage(), InjectorDetours::LastFailureThread());
      }
      UninstallAll();
      return static_cast<DWORD>(error);
    }
  }

  return static_cast<DWORD>(HookError::Success);
}

DWORD HookManager::UninstallAll() {
  if (InstalledHooks.empty())
    return ERROR_SUCCESS;
  const LONG detachResult = InjectorDetours::DetachAll();
  if (detachResult != NO_ERROR) {
    ErrorS("Hooks", "Detach refused: error=%ld, stage=%s, thread=%lu", detachResult, InjectorDetours::LastFailureStage(), InjectorDetours::LastFailureThread());
    return detachResult;
  }
  while (!InstalledHooks.empty()) {
    auto* hook = InstalledHooks.back();
    Log("\t%s", hook->GetName());
    if (!hook->Uninstall()) {
      ErrorS("Hooks", "Memory/state restoration failed: hook=%s", hook->GetName());
      return ERROR_WRITE_FAULT;
    }
    InstalledHooks.pop_back();
  }
  return ERROR_SUCCESS;
}
