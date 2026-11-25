#include "defeed.h"
#include "global.h"
#include "page.h"
#include "sb.h"

PageID PCB::create(const Schemas &ss) {
  PageID pid = PCB::pctx.size();
  PCB::pctx.push_back((PageCtx){.pid = pid, .ss = ss});
  return pid;
}
PageID PCB::create(const Schemas &ss, const std::string &entry_key) {
  PageID pid = PCB::pctx.size();
  PCB::pctx.push_back((PageCtx){.pid = pid, .ss = ss, .entry_key = entry_key});
  return pid;
}

PageID PCB::create(const std::string &fname) {
  Schemas ss{};
  XML::Extract x{fname};
  x.extract();
  XML::Internal root = XML::cast_Internal(x.get_root());
  XML::Siblings &items = root->children["item"];
  for (XML::Node &item : items) {
    Schema s_child{};
    XML::Internal internal = XML::cast_Internal(item);
    for (auto &[key, sibs] : internal->children) {
      Utils::Assert(sibs.size() <= 1, "Probable duplicate tags");
      XML::Leaf leaf = XML::cast_Leaf(sibs[0]);
      std::string val = leaf->get_content_string();
      s_child.put(key, val);
    }
    ss.push_back(std::move(s_child));
  }
  sb.append("Schema fetched for " + fname);
  return create(ss);
}

// NOTE: Special case, we dont create Schema objects for xml that is not
// used to define Defeed's behaviour. We also do not know what type of
// layout will the rss xml have. The best we can do is guess!
PageID PCB::create(const std::string url, const std::string &entry_key) {
  XML::Node rss = DefeedCtx::loadRSS(url);
  XML::Internal root = XML::cast_Internal(rss);
  XML::Siblings &items =
      (XML::cast_Internal(root->children["channel"][0]))->children["item"];

  Strings entries{};
  for (XML::Node &item : items) {
    XML::Leaf title =
        XML::cast_Leaf(XML::cast_Internal(item)->children[entry_key][0]);
    if (title == nullptr) {
      sb.append("[PAGE] Title is nullptr");
      continue;
    }
    if (title->is_content_string()) {
      sb.append("[PAGE] Title is HTML data");
      continue;
    }
    entries.push_back(title->get_content_string());
  }

  PageID pid = PCB::pctx.size();
  PCB::pctx.push_back((PageCtx){
      .pid = pid,
      .ss = {},
      .root = root,
      .entry_key = entry_key,
      .entries = entries,
  });
  return pid;
}

PageCtx &PCB::getRefTo(const PageID pid) {
  // Is pid referring to a temp page
  if (PCB::Temp::check(pid)) {
    return PCB::Temp::tp;
  }

  Utils::Assert(pid < PCB::pctx.size(), "getRefTo: pid > PCB size");
  return PCB::pctx[pid];
}
Schema &PCB::getSchemaAt(const PageID pid, const size_t index) {
  // Is pid referring to a temp page
  if (PCB::Temp::check(pid)) {
    Utils::Assert(index < PCB::Temp::tp.ss.size(),
                  "getSchemaAt: index > schema set");
    return PCB::Temp::tp.ss[index];
  }

  Utils::Assert(pid < PCB::pctx.size(), "getSchemaAt: pid > PCB size");
  Utils::Assert(index < PCB::pctx[pid].ss.size(),
                "getSchemaAt: index > schema set");
  return PCB::pctx[pid].ss[index];
}

PageID PCB::Temp::create(const Schemas &ss, const PageID ppid) {
  PageID pid = (1 << 31 | PCB::pctx.size());
  tp = (PageCtx){.ppid = ppid, .pid = pid, .ss = ss};
  return pid;
}

bool PCB::Temp::check(const PageID pid) { return (pid >> 31) & 1; }
