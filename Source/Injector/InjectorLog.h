#pragma once

#include <filesystem>

bool InitLogFile(const std::filesystem::path& directory = {}, const char* filename = "rekindled-injector.log");
void CloseLogFile();
std::filesystem::path GetLogFilePath();
void LogToFile(const char* format, ...);
extern "C" void InjectorLogToFile(const char* format, ...);
