#pragma once

#include <unistd.h>
#include <termios.h>

#include <string>
#include <vector>

class TerminalController {
public:
    TerminalController();
    ~TerminalController();

    void setNonCanonical();

    void restoreTerminal();

    void flushCache();

    void readSTDIN();

    void clearTerminalLine();
    void showCache();

    bool hasLine();
    const std::vector<std::string> getInput();

private:
    termios old_flags;
    std::vector<std::string> inputs_;
    bool set_{false};
    bool restored_{false};
};