#pragma once
#include <string>

namespace term {
    const char* const reset       = "\033[0m";

    // color
    const char* const black       = "\033[30m";
    const char* const red         = "\033[31m";
    const char* const green       = "\033[32m";
    const char* const yellow      = "\033[33m";
    const char* const blue        = "\033[34m";
    const char* const magenta     = "\033[35m";
    const char* const cyan        = "\033[36m";
    const char* const white       = "\033[37m";
    const char* const black_bg    = "\033[40m";
    const char* const red_bg      = "\033[41m";
    const char* const green_bg    = "\033[42m";
    const char* const yellow_bg   = "\033[43m";
    const char* const blue_bg     = "\033[44m";
    const char* const magenta_bg  = "\033[45m";
    const char* const cyan_bg     = "\033[46m";
    const char* const white_bg    = "\033[47m";

    // bright color
    const char* const bblack      = "\033[90m";
    const char* const bred        = "\033[91m";
    const char* const bgreen      = "\033[92m";
    const char* const byellow     = "\033[93m";
    const char* const bblue       = "\033[94m";
    const char* const bmagenta    = "\033[95m";
    const char* const bcyan       = "\033[96m";
    const char* const bwhite      = "\033[97m";
    const char* const bblack_bg   = "\033[100m";
    const char* const bred_bg     = "\033[101m";
    const char* const bgreen_bg   = "\033[102m";
    const char* const byellow_bg  = "\033[103m";
    const char* const bblue_bg    = "\033[104m";
    const char* const bmagenta_bg = "\033[105m";
    const char* const bcyan_bg    = "\033[106m";
    const char* const bwhite_bg   = "\033[107m";
}