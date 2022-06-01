
#include <iostream>

inline std::ostream& red(std::ostream& s)
{
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hStdout, FOREGROUND_RED | FOREGROUND_INTENSITY);
    return s;
}

inline std::ostream& green(std::ostream& s)
{
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hStdout, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    return s;
}

inline std::ostream& blue(std::ostream& s)
{
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hStdout, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    return s;
}

inline std::ostream& yellow(std::ostream& s)
{
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hStdout, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
    return s;
}

inline std::ostream& white(std::ostream& s)
{
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hStdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    return s;
}

void error(const char* tag, const char* msg...)
{
    char buf[256];
    va_list args;
    va_start(args, msg);
    vsnprintf_s(buf, _TRUNCATE, msg, args);
    va_end(args);

    std::cout << white << "[" << red << tag << white << "] " << buf << std::endl;
}

void warning(const char* tag, const char* msg...)
{
    char buf[256];
    va_list args;
    va_start(args, msg);
    vsnprintf_s(buf, _TRUNCATE, msg, args);
    va_end(args);

    std::cout << white << "[" << yellow << tag << white << "] " << buf << std::endl;
}

void info(const char* tag, const char* msg...)
{
    char buf[256];
    va_list args;
    va_start(args, msg);
    vsnprintf_s(buf, _TRUNCATE, msg, args);
    va_end(args);

    std::cout << white << "[" << tag << "] " << buf << std::endl;
}

void debug(const char* tag, const char* msg...)
{
#ifdef _DEBUG
    char buf[256];
    va_list args;
    va_start(args, msg);
    vsnprintf_s(buf, _TRUNCATE, msg, args);
    va_end(args);

    std::cout << white << "[" << blue << tag << white << "] " << buf << std::endl;
#endif
}
