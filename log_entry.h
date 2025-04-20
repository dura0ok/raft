#ifndef LOG_H
#define LOG_H
#include <string>
#include <vector>

enum class CommandType
{
    PUT,
    DELETE,
};

struct LogEntry
{
    int term;
    CommandType command;
    int index;
    std::string key;
    std::string value;

    LogEntry(int term, CommandType command, int index, std::string key, std::string value)
        : term(term), command(command), index(index), key(std::move(key)), value(std::move(value))
    {
    }
};

class Log
{
  public:
    void addEntry(const LogEntry &entry);
    LogEntry getEntry(int index) const;
    int getLastTerm() const;
    int getLastIndex() const;
    std::vector<LogEntry> getEntriesAfter(int index) const;
    void deleteEntriesFrom(int index);

  private:
    std::vector<LogEntry> entries;
};

#endif // LOG_H
