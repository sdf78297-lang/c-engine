#include <ExoEngine/Core/Logger.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Exo {

namespace {

std::string_view labelFor(LogLevel level) {
    switch (level) {
    case LogLevel::Info:
        return "info";
    case LogLevel::Warning:
        return "warn";
    case LogLevel::Error:
        return "error";
    }
    return "log";
}

} // namespace

void Logger::write(LogLevel level, std::string_view message) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime {};
#ifdef _WIN32
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream line;
    line << '[' << std::put_time(&localTime, "%H:%M:%S") << "] "
         << std::setw(5) << labelFor(level) << ": " << message;

    if (level == LogLevel::Error) {
        std::cerr << line.str() << '\n';
    } else {
        std::cout << line.str() << '\n';
    }
}

void Logger::info(std::string_view message) {
    write(LogLevel::Info, message);
}

void Logger::warn(std::string_view message) {
    write(LogLevel::Warning, message);
}

void Logger::error(std::string_view message) {
    write(LogLevel::Error, message);
}

} // namespace Exo
