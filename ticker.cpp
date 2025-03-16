#include "ticker.h"
#include "util.h"

Ticker::Ticker(int start_election_timeout, std::function<void()> on_tick_callback)
    : running_(true), on_tick_callback_(std::move(on_tick_callback)), thread_(&Ticker::run, this),
      start_election_timeout_(start_election_timeout)
{
    deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(get_random_interval());
}

void Ticker::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(get_random_interval());
    cv_.notify_one();
}

Ticker::~Ticker()
{
    stop();
}

void Ticker::run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (running_.load())
    {
        auto now = std::chrono::steady_clock::now();

        if (now < deadline_)
        {
            cv_.wait_until(lock, deadline_);
            continue;
        }

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
    return util::get_random_interval(start_election_timeout_, start_election_timeout_ + 100);
}