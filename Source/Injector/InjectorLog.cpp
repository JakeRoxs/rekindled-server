#include "Injector/InjectorLog.h"
#include "Shared/Core/Utils/Logging.h"
#include <Windows.h>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <mutex>

namespace {
std::mutex LogMutex;
std::ofstream LogFile;
std::filesystem::path LogPath;
constexpr std::uintmax_t MaxLogBytes = 8 * 1024 * 1024;
constexpr int Backups = 3;
std::uintmax_t WrittenBytes = 0;
bool ReportedFailure = false;

void Emergency(const char* message) {
  OutputDebugStringA(message);
  std::fputs(message, stderr);
}

bool Open(const std::filesystem::path& path) {
  LogFile.clear();
  LogFile.open(path, std::ios::out | std::ios::app | std::ios::binary);
  if (!LogFile.is_open())
    return false;
  LogPath = path;
  std::error_code error;
  WrittenBytes = std::filesystem::file_size(path, error);
  if (error)
    WrittenBytes = 0;
  ReportedFailure = false;
  return true;
}

void Rotate() {
  LogFile.close();
  std::error_code error;
  const auto backup = [](int index) { auto path = LogPath; path += "." + std::to_string(index); return path; };
  std::filesystem::remove(backup(Backups), error);
  bool failed = static_cast<bool>(error);
  for (int index = Backups - 1; index >= 1 && !failed; --index) {
    if (std::filesystem::exists(backup(index), error))
      std::filesystem::rename(backup(index), backup(index + 1), error);
    failed = static_cast<bool>(error);
  }
  if (!failed)
    std::filesystem::rename(LogPath, backup(1), error);
  if (failed || error)
    Emergency("Rekindled: log rotation failed; retaining current log.\n");
  const auto path = LogPath;
  Open(path);
}

void Write(const char* formatted) {
  std::lock_guard<std::mutex> lock(LogMutex);
  const auto length = std::strlen(formatted);
  if (LogFile.is_open() && WrittenBytes != 0 && WrittenBytes + length > MaxLogBytes)
    Rotate();
  if (LogFile.is_open()) {
    LogFile.write(formatted, static_cast<std::streamsize>(length));
    LogFile.flush();
    WrittenBytes += length;
  }
  if (!LogFile.is_open() || !LogFile.good()) {
    if (!ReportedFailure) {
      Emergency("Rekindled: file logging failed; records continue in debugger/stderr.\n");
      ReportedFailure = true;
    }
    Emergency(formatted);
  }
}
} // namespace

bool InitLogFile(const std::filesystem::path& directory, const char* filename) {
  CloseLogFile();
  HMODULE module = nullptr;
  wchar_t modulePath[32768]{};
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(&InitLogFile), &module))
    GetModuleFileNameW(module, modulePath, 32768);
  bool opened = false;
  bool fallback = false;
  {
    std::lock_guard<std::mutex> lock(LogMutex);
    LogPath.clear();
    const auto preferred = directory.empty() ? std::filesystem::path(modulePath).parent_path() : directory;
    if (!preferred.empty())
      opened = Open(preferred / filename);
    if (!opened) {
      std::error_code error;
      auto temp = std::filesystem::temp_directory_path(error);
      if (!error) {
        temp /= "RekindledInjector";
        temp /= std::to_string(GetCurrentProcessId());
        std::filesystem::create_directories(temp, error);
        if (!error)
          opened = Open(temp / filename);
      }
      fallback = opened;
    }
  }
  // Install even on failure so missing disk output is reported, not silently lost.
  SetLogSink(Write);
  const auto path = GetLogFilePath();
  LogS("Logging", "Session started; module=%ls; build=%s %s; log=%ls", modulePath, __DATE__, __TIME__, path.c_str());
  if (fallback) {
    WarningS("Logging", "Primary log directory unavailable; using temporary fallback %ls", path.c_str());
  } else if (!opened) {
    ErrorS("Logging", "Unable to open primary or fallback log file; debugger/stderr only");
  }
  return opened;
}

void CloseLogFile() {
  // Lock order is OutputMutex -> LogMutex. Unregister before closing, so no sink
  // callback can race DLL teardown or return into unloaded logger code.
  SetLogSink(nullptr);
  std::lock_guard<std::mutex> lock(LogMutex);
  if (LogFile.is_open())
    LogFile.close();
}

std::filesystem::path GetLogFilePath() {
  std::lock_guard<std::mutex> lock(LogMutex);
  return LogPath;
}

void LogToFile(const char* format, ...) {
  va_list args;
  va_start(args, format);
  WriteLogV(false, ConsoleColor::Grey, "Injector", "Log", format, args);
  va_end(args);
}

extern "C" void InjectorLogToFile(const char* format, ...) {
  va_list args;
  va_start(args, format);
  WriteLogV(false, ConsoleColor::Grey, "Injector", "Log", format, args);
  va_end(args);
}
