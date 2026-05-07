#pragma once

#include <string_view>

namespace Exo {

enum class LogLevel {
    Info,
    Warning,
    Error
};

class Logger {
public:
    static void write(LogLevel level, std::string_view message);
    static void info(std::string_view message);
    static void warn(std::string_view message);
    static void error(std::string_view message);
};

} // namespace Exo
