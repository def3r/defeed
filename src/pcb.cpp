#include "global.h"
#include "page.h"
#include "sb.h"
#include "defeed.h"

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

PageID PCB::create(const std::string fname) {
  Schemas ss{};
  XML::Extract x{fname};
  x.extract();
  XML::Internal root = XML::cast_Internal(x.get_root());
  XML::Siblings &items = root->children["item"];
  for (XML::Node &item : items) {
    Schema s_child{};
    assert(item->type == XML::NodeType::Internal &&
           "Nested Internal nodes not supported yet!");
    XML::Internal internal = XML::cast_Internal(item);
    for (auto &[key, sibs] : internal->children) {
      assert(sibs.size() <= 1 && "Probable duplicate tags");
      XML::Leaf leaf = XML::cast_Leaf(sibs[0]);
      assert(leaf->content_idx == 0 && "Expected string content, got HTML");
      std::string val = std::get<0>(leaf->content);
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
      (XML::cast_InternalBase(root->children["channel"][0]))->children["item"];

  std::vector<std::string> entries{};
  for (int i = 0; i < items.size(); i++) {
    XML::LeafBase *title = XML::cast_LeafBase(
        XML::cast_InternalBase(items[i])->children[entry_key][0]);
    if (title == nullptr) {
      sb.append("[PAGE] Title is nullptr");
      continue;
    }
    if (title->content_idx != 0) {
      sb.append("[PAGE] Title is HTML data");
      continue;
    }
    entries.push_back(std::get<0>(title->content));
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

  assert(pid < PCB::pctx.size() && "getRefTo: pid > PCB size");
  return PCB::pctx[pid];
}
Schema &PCB::getSchemaAt(const PageID pid, const size_t index) {
  // Is pid referring to a temp page
  if (PCB::Temp::check(pid)) {
    assert(index < PCB::Temp::tp.ss.size() &&
           "getSchemaAt: index > schema set");
    return PCB::Temp::tp.ss[index];
  }

  assert(pid < PCB::pctx.size() && "getSchemaAt: pid > PCB size");
  assert(index < PCB::pctx[pid].ss.size() && "getSchemaAt: index > schema set");
  return PCB::pctx[pid].ss[index];
}

PageID PCB::Temp::create(const Schemas &ss) {
  PageID pid = (1 << 31 | PCB::pctx.size());
  tp = (PageCtx){.pid = pid, .ss = ss};
  return pid;
}

bool PCB::Temp::check(const PageID pid) { return (pid >> 31) & 1; }
