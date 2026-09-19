#include "Injector/InjectorLog.h"
#include "Shared/Core/Utils/Logging.h"
#include <Windows.h>
#include <fstream>
#include <iterator>
#include <thread>
#include <vector>
#include <cstdlib>

namespace {
void CheckLog(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "Logging test failed: %s\n", message);
    std::exit(1);
  }
}

std::string Read(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
} // namespace

void RunLoggingTests() {
  const auto directory = std::filesystem::temp_directory_path() / ("rekindled_logging_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(GetTickCount64()));
  std::filesystem::create_directories(directory);
  SetQuietLogging(true);
  CheckLog(InitLogFile(directory), "open primary file");
  const auto path = GetLogFilePath();
  const std::string longMessage(12000, 'x');
  SetLastError(12345);
  LogS("LoggingTest", "long=%s; number=%d; tail=complete", longMessage.c_str(), 42);
  CheckLog(GetLastError() == 12345, "preserve caller Win32 error");
  WarningS("LoggingTest", "warning captured");
  ErrorS("LoggingTest", "error captured");
  std::vector<std::thread> workers;
  for (int worker = 0; worker < 4; ++worker) {
    workers.emplace_back([worker] {
      for (int message = 0; message < 50; ++message) {
        LogS("Concurrent", "worker=%d record=%d end", worker, message);
      }
    });
  }
  for (auto& worker : workers)
    worker.join();
  CloseLogFile();
  CheckLog(!HasLogSink(), "close unregisters sink");
  const auto content = Read(path);
  CheckLog(content.find(longMessage + "; number=42; tail=complete") != std::string::npos, "long variadic record is complete");
  CheckLog(content.find("[Warning] [LoggingTest] warning captured") != std::string::npos, "warning level and source");
  CheckLog(content.find("[Error] [LoggingTest] error captured") != std::string::npos, "error level and source");
  CheckLog(content.find("Z [pid=") != std::string::npos && content.find(" tid=") != std::string::npos, "UTC/process/thread metadata");
  for (int worker = 0; worker < 4; ++worker) {
    for (int message = 0; message < 50; ++message) {
      const auto record = "[Concurrent] worker=" + std::to_string(worker) + " record=" + std::to_string(message) + " end\n";
      const auto found = content.find(record);
      CheckLog(found != std::string::npos && content.find(record, found + record.size()) == std::string::npos, "concurrent record occurs exactly once");
    }
  }
  CheckLog(InitLogFile(directory), "reopen primary file");
  LogS("LoggingTest", "second session");
  CloseLogFile();
  CheckLog(Read(path).find("tail=complete") != std::string::npos, "reopen appends rather than truncates");

  CheckLog(InitLogFile(directory, "rotation.log"), "open rotation log");
  const std::string oversized(8 * 1024 * 1024, 'r');
  for (int index = 0; index < 4; ++index) {
    LogS("Rotation", "record=%d %s", index, oversized.c_str());
  }
  LogS("Rotation", "final record");
  CloseLogFile();
  CheckLog(Read(directory / "rotation.log").find("final record") != std::string::npos, "current file after rotation");
  CheckLog(std::filesystem::exists(directory / "rotation.log.3") && !std::filesystem::exists(directory / "rotation.log.4"), "three retained backups");
  CheckLog(std::filesystem::file_size(directory / "rotation.log.1") > oversized.size(), "oversized record preserved intact");

  const auto blocked = directory / "not-a-directory";
  std::ofstream(blocked).put('x');
  CheckLog(InitLogFile(blocked, "fallback-test.log"), "fallback when primary directory cannot be used");
  const auto fallback = GetLogFilePath();
  LogS("LoggingTest", "fallback record");
  CloseLogFile();
  const auto fallbackContent = Read(fallback);
  CheckLog(fallbackContent.find("Primary log directory unavailable") != std::string::npos && fallbackContent.find("fallback record") != std::string::npos, "fallback warning and record persisted");
  std::filesystem::remove(fallback);
  std::filesystem::remove_all(directory);
  SetQuietLogging(false);
}
