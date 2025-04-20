#include "log_entry.h"

#include <stdexcept>

void Log::addEntry(const LogEntry &entry)
{
    entries.push_back(entry);
}

LogEntry Log::getEntry(int index) const
{
    return entries.at(index);
}

int Log::getLastTerm() const
{
    return entries.empty() ? 0 : entries.back().term;
}

int Log::getLastIndex() const
{
    return entries.empty() ? -1 : entries.back().index;
}

std::vector<LogEntry> Log::getEntriesAfter(int index) const
{
    if (index + 1 >= static_cast<int>(entries.size()))
        return {};
    return {entries.begin() + index + 1, entries.end()};
}

void Log::deleteEntriesFrom(int index)
{
    if (index < 0 || index >= static_cast<int>(entries.size()))
    {
        throw std::out_of_range("Index out of range");
    }
    entries.erase(entries.begin() + index, entries.end());
}