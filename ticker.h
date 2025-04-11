#ifndef TICKER_HPP
#define TICKER_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <random>
#include <thread>

class Ticker
{
  public:
    explicit Ticker(int start_election_timeout, std::function<void()> on_tick_callback);
    void reset();
    ~Ticker();

  private:
    void run();
    void stop();
    int get_random_interval();

    std::atomic<bool> running_;
    std::function<void()> on_tick_callback_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::mutex cv_mutex_;
    std::chrono::steady_clock::time_point deadline_;
    int start_election_timeout_;
};

#endif // TICKER_HPP