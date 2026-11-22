#pragma once

#include <string>
#include <vector>

#include <sys/types.h>

#define UNUSED

// Pages
using PageID = ssize_t; // Corresponds to index in PCB

// sb
class SharedBuffer;

// Quality of Code Improvements
using Strings = std::vector<std::string>;
