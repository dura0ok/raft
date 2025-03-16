#ifndef LOGGER_H
#define LOGGER_H

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

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
    oss << std::put_time(&timeinfo, "%F %H:%M:%S") << ":" << std::setfill('0') << std::setw(3) << millis;

    return oss.str();
}

class Logger
{
  public:
    static void log(const std::string &message)
    {
        std::ostringstream log_stream;
        auto thread_id = std::this_thread::get_id();

        log_stream << "[" << getCurrentMsTime() << "][" << thread_id << "] " << message;
        std::cout << log_stream.str() << std::endl;
    }
};

#endif // LOGGER_H
