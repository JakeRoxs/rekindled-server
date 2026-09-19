/*
 * Rekindled Server
 * Copyright (C) 2021 Tim Leonard
 * Copyright (C) 2026 Jake Morgeson
 *
 * This program is free software; licensed under the MIT license.
 * You should have received a copy of the license along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace InjectorShutdown {
// Request shutdown from a normal worker thread. Never call under loader lock
// (DllMain), because notifying registered waiters acquires C++ mutexes.
void RequestShutdown();

// Returns true if shutdown has been requested.
bool IsShutdownRequested();

// Register a mutex+condition variable to be notified when shutdown is requested.
// This is used by Injector::RunUntilQuit to wake up cleanly.
void RegisterNotifier(std::mutex& mutex, std::condition_variable& cv);

// Unregister a previously registered notifier.
void UnregisterNotifier();
} // namespace InjectorShutdown
