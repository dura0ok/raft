#ifndef UTIL_HPP
#define UTIL_HPP

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

using namespace std::chrono;

inline std::string getCurrentMsTime()
{
    using namespace std::chrono;

    const auto currentTime = system_clock::now();
    const auto transformed = duration_cast<milliseconds>(currentTime.time_since_epoch()).count();
    const auto millis = transformed % 1000;

    const std::time_t tt = system_clock::to_time_t(currentTime);
    std::tm timeinfo{};
    localtime_r(&tt, &timeinfo);

    std::ostringstream oss;
    oss << std::put_time(&timeinfo, "%F %H:%M:%S") // Год-месяц-день часы:минуты:секунды
        << ":" << std::setfill('0') << std::setw(3) << millis; // Добавляем миллисекунды

    return oss.str();
}

#endif // UTIL_HPP