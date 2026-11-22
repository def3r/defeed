#include "page.h"

void Schema::put(const std::string &key, const std::string &val) {
  this->m[key] = val;

  Schema::fieldEncodeTy::iterator it;
  if ((it = Schema::fieldEncode.find(key)) == Schema::fieldEncode.end()) {
    this->fields |= Schema::Field::OTHER;
  } else {
    this->fields |= it->second;
  }
}

bool Schema::find(const std::string &key) const {
  if (this->m.find(key) == this->m.end())
    return false;
  return true;
}

Strings Schema::get_all(const Schemas &ss,
                                         const std::string &key) {
  Strings entries{};
  for (const Schema &s : ss) {
    std::unordered_map<std::string, std::string>::const_iterator it =
        s.m.find(key);

    if (it == s.m.end())
      continue;

    entries.push_back(it->second);
  }
  return entries;
}

bool Schema::hasPage() { return pid != -1; }
