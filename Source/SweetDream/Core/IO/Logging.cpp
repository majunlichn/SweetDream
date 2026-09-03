#include <SweetDream/Core/IO/Logging.h>

namespace sd
{

spdlog::logger* GetLogger()
{
    static std::shared_ptr<spdlog::logger> logger = rad::LogManager::Instance().CreateLogger("SweetDream");
    return logger.get();
}

} // namespace sd
