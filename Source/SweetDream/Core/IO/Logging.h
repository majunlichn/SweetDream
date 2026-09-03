#pragma once

#include <rad/IO/Logging.h>

namespace sd
{

[[nodiscard]] spdlog::logger* GetLogger();

} // namespace sd

#define SD_LOG(LogLevel, ...)                                                                      \
    SPDLOG_LOGGER_CALL(::sd::GetLogger(), ::spdlog::level::LogLevel, __VA_ARGS__)
