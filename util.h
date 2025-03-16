#ifndef UTIL_H
#define UTIL_H

#include <random>

namespace util
{
inline int get_random_interval(int min, int max)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
}
} // namespace util

#endif // UTIL_H