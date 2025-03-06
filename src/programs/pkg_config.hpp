#pragma once
#include <string>
#include <vector>

namespace pkg_config
{
    struct library_config
    {
        std::vector<std::string> cflags;
        std::vector<std::string> lib_flags;
    };

    library_config get_config(std::string name);
}