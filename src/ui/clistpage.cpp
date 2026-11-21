#include "defeed.h"
#include "global.h"
#include "sb.h"
#include "ui.h"

namespace UI {

using namespace ftxui;

ListPage::ListPage(const Schemas &ss) {
  pid = PCB::create(ss);
  if (pid == 0) {
    PageCtx &pcx = PCB::getRefTo(pid);
    pcx.entry_key = "display";
    pcx.entries = Schema::get_all(ss, pcx.entry_key);
    update_entries(pcx.entries);
  } else {
    assert(0 && "TBD");
  }
}

ListPage::ListPage(const Schemas &ss, const std::string &entry_key) {}

Element ListPage::OnRender() {
  Dimensions dims = ftxui::Terminal::Size();
  int pg_size = dims.dimy - (DefeedCtx::show_logs ? 11 : 2);
  return Render() | vscroll_indicator | yframe |
         size(ftxui::HEIGHT, ftxui::LESS_THAN, pg_size);
}

Component ListPage::update_entries(const PageID pid,
                                   const std::string &entry_key) {
  this->pid = pid;
  PageCtx &pcx = PCB::getRefTo(pid);
  if (pcx.ss.size() == 0) {
    // NOTE: Try for XML::Node; in which case the pcx.entries is already
    // populated (assumption)
  } else {
    pcx.entry_key = entry_key;
    pcx.entries = Schema::get_all(pcx.ss, pcx.entry_key);
  }
  Component pg = Menu(pcx.entries, &selected, Page::menu_style());
  update(pg, pcx.selected, pcx.entries.size());
  return pg;
}

Component ListPage::update_entries(const Schemas &ss,
                                   const std::string &entry_key) {
  pid = PCB::create(ss);
  PageCtx &pcx = PCB::getRefTo(pid);
  pcx.entry_key = entry_key;
  pcx.entries = Schema::get_all(ss, pcx.entry_key);
  sb.append("PCX Entries: " + std::to_string(pcx.entries.size()));
  return update_entries(pcx.entries);
}

Component ListPage::update_entries(const std::vector<std::string> &entries) {
  selected = 0;
  max_entries = entries.size();
  Component pg = Menu(entries, &selected, Page::menu_style());
  update(pg);
  return pg;
}

void ListPage::update(Component child) {
  selected = 0;
  DetachAllChildren();
  Add(std::move(child));
}

void ListPage::update(Component child, int selected, int max_entries) {
  this->selected = selected;
  this->max_entries = max_entries;
  DetachAllChildren();
  Add(std::move(child));
}

} // namespace UI
