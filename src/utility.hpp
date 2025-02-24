#pragma once

#include "args.hpp"
#include "cppmaker.hpp"

bool install(std::filesystem::path repository, ArgReader& args, cppmaker_options& options);
bool export_project();