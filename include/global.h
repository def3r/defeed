#pragma once

#include <algorithm>
#include <string>

#include "types.h"

extern SharedBuffer sb;

namespace Utils {

// impl from cppreference
inline std::string strToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

inline std::string strStrip(std::string s) {
  s.erase(s.find_last_not_of(" \t") + 1);
  s.erase(0, s.find_first_not_of(" \t"));
  return s;
}

inline void Assert(bool cond,
                   const std::string &msg = "no failure msg provided") {}

} // namespace Utils

#define REGISTER(details, ...)                                                 \
  if ((not event_is_handled_) and (__VA_ARGS__) and (event_is_handled_ = true))
#define EVENT_IS(event) ev == Event::event

#define REGISTER_CHAR(details, c) REGISTER(details, ev == Event::Character(#c))
#define REGISTER_EV(details, event) REGISTER(details, ev == Event::event)

#define EVENT_HANDLER_PROLOGUE bool event_is_handled_ = false
#define EVENT_HANDLER_EPILOGUE                                                 \
  do {                                                                         \
    if (event_is_handled_)                                                     \
      return true;                                                             \
  } while (0)
