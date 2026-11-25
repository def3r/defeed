#include "defeed.h"
#include "global.h"
#include "sb.h"
#include "ui.h"

namespace UI {

using namespace ftxui;

bool CmdLine::exec(std::string &cmd) {
  // clean the stinky string
  cmd = Utils::strStrip(cmd);
  bool retval = true;

  if (cmd == "log") {
    if ((DefeedCtx::show_logs = not DefeedCtx::show_logs)) {
      parent->log->TakeFocus();
    } else {
      DefeedCtx::shift_focus = true;
    }
  } else if (cmd == "list" or cmd == "ls") {
    // nop
  } else if (cmd == "q") {
    DefeedCtx::quit = true;
  } else if (cmd == "fetch") {
    parent->begin_fetch();
  } else {
    retval = false;
  }

  cmd = retval ? "" : cmd;
  return retval;
}

CmdLine::CmdLine(UI::App *_Nonnull parent) : parent(parent) {
  Utils::Assert(parent, "CmdLine cannot have nullptr for parent");
}

void CmdLine::set_search_pid(const PageID pid) {
  sb.append("Updated pid");
  this->search_pid = pid;
}

void CmdLine::exit_search(Event ev) {
  if (ev == Event::Return) {
    // If search page only contains 1 entry, open it by default
    if (search_ss.size() == 1) {
      DefeedCtx::switch_request = true;
      DefeedCtx::switch_type = DefeedCtx::SwitchPage::Forward;
      // no need to handle call history explicitly, swtich_request is going to
      // handle it
    } else {
      parent->call_history.push(search_pid);
    }
  } else if (ev == Event::Escape) {
    parent->switch_page(search_pid);
  }
  search_pid = -1;
  search_ss.clear();
}

Schemas CmdLine::search(const std::string &needle) {
  Utils::Assert(search_pid != -1,
                "Must set search pid before using Search method");

  if (needle.empty()) {
    parent->switch_page(search_pid);
    return {};
  }

  search_ss.clear();
  search_schemas(search_pid, Utils::strToLower(needle));

  std::shared_ptr<ListPage> lipg =
      std::static_pointer_cast<ListPage>(parent->page_list);
  PageID temp_pid = PCB::Temp::create(search_ss, search_pid);
  lipg->update_entries(temp_pid);
  parent->active_pid = temp_pid;

  return search_ss;
}

void CmdLine::search_schemas(const PageID search_pid,
                             const std::string l_needle) {
  PageCtx &pcx = PCB::getRefTo(search_pid);
  if (pcx.ss.size()) {
    for (Schema &s : pcx.ss) {
      if (s.find("display")) {
        std::string display = Utils::strToLower(s.m["display"]);
        if (display.find(l_needle) != std::string::npos) {
          search_ss.push_back(s);
        }
      }

      if (not s.hasPage() and s.find("file")) {
        // TODO: setting schema's pid should be PCB's responsibility
        s.pid = PCB::create(DefeedCtx::defeed + s.m.find("file")->second);
      }

      if (s.hasPage()) {
        search_schemas(s.pid, l_needle);
      }
    }
  }
  // TODO: Search in xml roots as well, when ss.size == 0
}

} // namespace UI
