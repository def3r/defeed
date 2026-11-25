#pragma once

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "types.h"
#include "xml.h"

// clang-format off
#define XM_Schema_Fields           \
  X(TITLE, "title",      1L << 1)  \
  X(LINK,  "link",       1L << 2)  \
  X(RSS,   "rss",        1L << 3)  \
  X(YT,    "yt",         1L << 4)  \
  X(NLTR,  "newsletter", 1L << 5)  \
  X(CTGRY, "category",   1L << 6)  \
  X(FNAME, "file",       1L << 7)  \
  X(DSPLY, "display",    1L << 8)  \
  /* Others */                     \
  X(OTHER, "",           1L << 31)
// clang-format on

// TODO: Evaluate if Schema can be merged directly with XML; they both do
// effectively the same thing
struct Schema;
using Schemas = std::vector<Schema>;
struct Schema {
  using SchemaField = size_t;

  PageID pid = -1;            // Is a page assoc with the cur schema, if not -1
  UNUSED bool linked = false; // If a link assoc with the cur schema is open
  SchemaField fields = 0;
  std::unordered_map<std::string, std::string> m;

  enum Field {
#define X(FIELD, _, FIELDVAL) FIELD = FIELDVAL,
    XM_Schema_Fields
#undef X
  };

  using fieldDecodeTy = std::map<SchemaField, std::string>;
  static inline std::map<SchemaField, std::string> fieldDecode = {
#define X(FIELD, FIELDNAME, _) {Field::FIELD, FIELDNAME},
      XM_Schema_Fields
#undef X
  };

  using fieldEncodeTy = std::map<std::string, SchemaField>;
  static inline std::map<std::string, SchemaField> fieldEncode = {
#define X(FIELD, FIELDNAME, _) {FIELDNAME, Field::FIELD},
      XM_Schema_Fields
#undef X
  };

  void put(const std::string &key, const std::string &val);
  bool find(const std::string &key) const;
  static Strings get_all(const Schemas &ss, const std::string &key);
  bool hasPage();
};

// NOTE: toplevel page always has id = 0
struct PageCtx {
  PageID ppid = -1, pid = -1;

  // A page can either hold ss, or root;
  // Order or evaluation:
  //   if ss.size() == 0 -> root
  Schemas ss{};
  XML::Node root{};

  int selected = 0;
  std::string entry_key = "";
  Strings entries{};
};

namespace PCB {

inline std::vector<PageCtx> pctx{};

inline void init() { pctx.reserve(32); }

// create a page using schemas
PageID create(const Schemas &ss);
PageID create(const Schemas &ss, const std::string &entry_key);

// create a page using schema file name
PageID create(const std::string fname);

// create a page using rss url, with display as entry_key
PageID create(const std::string url, const std::string &entry_key);

PageCtx &getRefTo(const PageID pid);
Schema &getSchemaAt(const PageID pid, const size_t index);

// For temporary pages
//
// temporary pages have 31st bit set in the PageID
namespace Temp {

UNUSED inline std::unordered_map<PageID, PageCtx> t_pctx{};
inline PageCtx tp{};

// create a temporary page using schema file name
PageID create(const Schemas &ss, const PageID ppid);

bool check(const PageID pid);

}; // namespace Temp

}; // namespace PCB
