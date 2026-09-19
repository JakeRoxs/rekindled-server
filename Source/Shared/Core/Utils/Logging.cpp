/*
 * Rekindled Server
 * Copyright (C) 2021 Tim Leonard
 * Copyright (C) 2026 Jake Morgeson
 *
 * This program is free software; licensed under the MIT license.
 * You should have received a copy of the license along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 */

#include "Shared/Core/Utils/Logging.h"
#include "Shared/Platform/Platform.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <cstdarg>
#include <cstdio>
#include <sstream>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace {
std::mutex RecentMessageMutex;
std::list<LogMessage> RecentMessages;
std::mutex OutputMutex;
std::atomic<bool> QuietLoggingEnabled{false};
std::atomic<bool> SinkInstalled{false};
LogSink Sink = nullptr;

// Logging must not overwrite the Win32/Winsock error the caller is diagnosing.
struct ErrorState {
#ifdef _WIN32
  DWORD Saved = GetLastError();
  ~ErrorState() { SetLastError(Saved); }
#endif
};

std::string FormatMessage(const char* format, va_list args) {
  va_list copy;
  va_copy(copy, args);
  const int size = vsnprintf(nullptr, 0, format, copy);
  va_end(copy);
  if (size < 0)
    return "[log formatting failed]";
  std::vector<char> buffer(static_cast<size_t>(size) + 1);
  va_copy(copy, args);
  vsnprintf(buffer.data(), buffer.size(), format, copy);
  va_end(copy);
  return std::string(buffer.data(), static_cast<size_t>(size));
}
} // namespace

void SetQuietLogging(bool enabled) {
  QuietLoggingEnabled.store(enabled);
}

void SetLogSink(LogSink sink) {
  std::lock_guard<std::mutex> lock(OutputMutex);
  Sink = sink;
  SinkInstalled.store(sink != nullptr);
}

bool HasLogSink() {
  return SinkInstalled.load();
}

std::list<LogMessage> GetRecentLogs() {
  std::lock_guard<std::mutex> lock(RecentMessageMutex);
  return RecentMessages;
}

void WriteLogV(bool quietLoggable, ConsoleColor color, const char* source, const char* level, const char* format, va_list args) {
  ErrorState preserveError;
  const std::string message = FormatMessage(format, args);
  std::lock_guard<std::mutex> outputLock(OutputMutex);
  const auto now = std::chrono::system_clock::now();
  const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
#ifdef _WIN32
  gmtime_s(&utc, &time);
#else
  gmtime_r(&time, &utc);
#endif
  char timestamp[32]{};
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &utc);
  char fraction[8]{};
  snprintf(fraction, sizeof(fraction), ".%03dZ", static_cast<int>(milliseconds));
  std::ostringstream line;
  line << timestamp << fraction;
#ifdef _WIN32
  line << " [pid=" << GetCurrentProcessId() << " tid=" << GetCurrentThreadId() << "]";
#else
  line << " [tid=" << std::this_thread::get_id() << "]";
#endif
  line << " [" << level << "] [" << (source && *source ? source : "General") << "] " << message;
  if (message.empty() || message.back() != '\n')
    line << '\n';
  const std::string rendered = line.str();
  if (Sink)
    Sink(rendered.c_str());
  if (!QuietLoggingEnabled.load() || quietLoggable)
    WriteToConsole(color, rendered.c_str());

  LogMessage recent{GetSeconds(), source ? source : "", level, message};
  std::lock_guard<std::mutex> recentLock(RecentMessageMutex);
  RecentMessages.push_back(std::move(recent));
  if (RecentMessages.size() > 128)
    RecentMessages.pop_front();
}

void WriteLog(bool quietLoggable, ConsoleColor color, const char* source, const char* level, const char* format, ...) {
  va_list args;
  va_start(args, format);
  WriteLogV(quietLoggable, color, source, level, format, args);
  va_end(args);
}
