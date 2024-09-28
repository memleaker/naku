#ifndef NAKU_LOGGER_H
#define NAKU_LOGGER_H

#include <iostream>

namespace naku {

class logger
{
public:
    enum LOG_LEVEL {INFO, WARN, ERROR, FATAL};

public:
    logger() = delete;
    logger(const char *file, int line, LOG_LEVEL _level) 
        : level(_level) { std::cout << "FILE: " << file << "Line: " << line; }
    ~logger() { if (level == FATAL) ::abort(); }

public:
    std::ostream& stream(void) { return std::cout; }

private:
    LOG_LEVEL level;
};

#define LOG_INFO  logger(__FILE__, __LINE__, logger::INFO).stream()
#define LOG_WARN  logger(__FILE__, __LINE__, logger::WARN).stream()
#define LOG_ERROR logger(__FILE__, __LINE__, logger::ERROR).stream()
#define LOG_FATAL logger(__FILE__, __LINE__, logger::FATAL).stream()

} // namespace naku

#endif // NAKU_LOGGER_H