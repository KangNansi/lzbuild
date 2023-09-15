#pragma once
#include "config.hpp"
#include "args.hpp"
#include "file.hpp"

int build(std::vector<file> files, config& cfg, bool dependencies_only, bool full_rebuild, bool force_linking);