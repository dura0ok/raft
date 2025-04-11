#include "ticker.h"

#include "logger.h"
#include "util.h"

Ticker::Ticker(int start_election_timeout, std::function<void()> on_tick_callback)
    : running_(true), on_tick_callback_(std::move(on_tick_callback)), thread_(&Ticker::run, this),
      start_election_timeout_(start_election_timeout)
{
    deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(get_random_interval());
}

void Ticker::reset()
{
    Logger::log("Ticker reset mutex lock guard");
    std::lock_guard<std::mutex> lock(mutex_);
    Logger::log("Ticker reset mutex lock guard after");
    deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(get_random_interval());
    cv_.notify_one();
    Logger::log("Ticker reset ended");
}

Ticker::~Ticker()
{
    stop();
}

void Ticker::run()
{

    while (running_.load())
    {
        auto now = std::chrono::steady_clock::now();

        std::unique_lock<std::mutex> lock(mutex_);
        if (now < deadline_)
        {
            cv_.wait_until(lock, deadline_);
            continue;
        }
        lock.unlock();


        if (on_tick_callback_)
        {
            on_tick_callback_();
        }

        deadline_ = now + std::chrono::milliseconds(get_random_interval());
    }
}

void Ticker::stop()
{
    running_.store(false);
    cv_.notify_one();
    if (thread_.joinable())
    {
        thread_.join();
    }
}

int Ticker::get_random_interval()
{
    // TODO отбить
    return util::get_random_interval(start_election_timeout_, start_election_timeout_ + 200);
}