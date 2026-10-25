#pragma once
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace chalk {
    // Call this once in main() to enable colors on Windows Command Prompt/PowerShell
    inline void init() {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return;
        DWORD dwMode = 0;
        if (!GetConsoleMode(hOut, &dwMode)) return;
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
#endif
    }

    const std::string reset   = "\033[0m";
    const std::string bold    = "\033[1m";
    const std::string red     = "\033[31m";
    const std::string green   = "\033[32m";
    const std::string yellow  = "\033[33m";
    const std::string blue    = "\033[34m";
    const std::string magenta = "\033[35m";
    const std::string cyan    = "\033[36m";
}