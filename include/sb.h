#pragma once

#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

class SharedBuffer {
public:
  inline SharedBuffer() {
    buf.reserve(4096 * 8);
    entries.reserve(4096 * 8);
    lfile.open("log.txt", std::ios::out);
  };

  inline std::string get_string() {
    std::unique_lock lock{mtx};
    return std::string(buf.begin(), buf.end());
  };

  inline const std::vector<std::string_view> &get_entries() {
    std::unique_lock lock{mtx};
    return this->entries;
  };

  inline const std::size_t get_total_entries() {
    std::unique_lock lock{mtx};
    return this->entries.size();
  };

  inline void append(const std::string &s) {
    std::unique_lock lock{mtx};
    char *data = buf.data() + buf.size();
    buf.insert(buf.end(), s.begin(), s.end());
    entries.emplace_back(std::string_view(data, s.size()));
    lfile << s << std::endl;
    // TODO: buf sizecheck and invalidation on realloc
  };

  template <typename T> inline void append(T s) {
    std::stringstream ss;
    ss << s;
    append(ss.str());
  }

private:
  std::mutex mtx{};
  std::vector<char> buf{};
  std::vector<std::string_view> entries{};
  std::fstream lfile{};

  inline void lock() { mtx.lock(); };
  inline void unlock() { mtx.unlock(); };
};
