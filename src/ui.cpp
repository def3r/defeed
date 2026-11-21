#include <cassert>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/terminal.hpp>

#include "global.h"
#include "defeed.h"
#include "sb.h"
#include "ui.h"
#include "page.h"

#include <unistd.h>

using namespace std::chrono_literals;

namespace UI {

using namespace ftxui;

namespace Navigation {

bool vim(const Event &ev, int &selected, const int max_entries) {
  static int g_count = 0;
  static int scroll_factor = -1;
  if (scroll_factor == -1) {
    Dimensions dims = ftxui::Terminal::Size();
    int vis = dims.dimy - (DefeedCtx::show_logs ? 12 : 2);
    scroll_factor = vis / 2; // half of visible page
  }

  if (ev == Event::Character("g")) {
    if ((++g_count % 2) == 0) {
      selected = 0;
    }
    return true;
  }
  g_count = 0;

  if (ev == Event::Character("G")) {
    selected = max_entries;
    return true;
  }

  if (ev == Event::CtrlD) {
    selected = std::min(selected + scroll_factor, max_entries);
    return true;
  } else if (ev == Event::CtrlU) {
    selected = std::max(selected - scroll_factor, 0);
    return true;
  }

  return false;
}

} // namespace Navigation

Element LoadingComponent::OnRender() {
  static int img;
  constexpr int idx = 15;
  img = (img + 1) % 200;
  return spinner(idx, img);
}

LogMenuComponent::LogMenuComponent() {
  log_menu_idx = 0;
  entries = sb.get_entries();
  Component log_menu =
      Menu(&entries, &log_menu_idx, LogMenuComponent::menu_style());
  Add(log_menu);
}
bool LogMenuComponent::Focusable() const { return DefeedCtx::show_logs; }
Element LogMenuComponent::OnRender() {
  entries = sb.get_entries();
  static std::size_t last_entry_idx = log_menu_idx;

  std::size_t cur_entry_count = entries.size();
  if (last_entry_idx != cur_entry_count - 1) {
    if (log_menu_idx == last_entry_idx) {
      log_menu_idx = cur_entry_count - 1;
    }
  }
  last_entry_idx = cur_entry_count - 1;

  return Render() | vscroll_indicator | yframe;
}
bool LogMenuComponent::OnEvent(Event ev) {
  if (Navigation::vim(ev, log_menu_idx, entries.size())) {
    return true;
  }

  if (ev == Event::Character("q")) {
    DefeedCtx::show_logs = false;
    DefeedCtx::shift_focus = true;
    return true;
  }

  if (ev == Event::CtrlK) {
    DefeedCtx::shift_focus = true;
    return true;
  }

  return ComponentBase::OnEvent(ev);
}
MenuOption LogMenuComponent::menu_style() {
  MenuOption option = MenuOption::Vertical();
  option.entries_option.transform = [](EntryState state) {
    Element e = text(" " + state.label + " ");
    if (state.active)
      e = e | bold | color(Color::LightCoral);
    return e;
  };
  return option;
}

StatusLineComponent::StatusLineComponent() {
  Component loading = std::make_unique<LoadingComponent>();
  loading |= Maybe([]() { return DefeedCtx::task != DefeedCtx::Task::Ready; });
  Add(loading);
}
Element StatusLineComponent::OnRender() {
  static std::string status;
  static Color c = Color::Blue;
  // clang-format off
  if (DefeedCtx::mode == DefeedCtx::Mode::Normal) {
    switch (DefeedCtx::task) {
      case DefeedCtx::Task::Ready: status = "Normal"; break;
      case DefeedCtx::Task::Fetch: status = "Fetching"; break;
      case DefeedCtx::Task::Load:  status = "Loading"; break;
    }
  } else if (DefeedCtx::mode == DefeedCtx::Mode::Cmd) {
    status = "Cmd";
  } else if (DefeedCtx::mode == DefeedCtx::Mode::Search) {
    status = "Search";
    c = Color::Green;
  }

  if (DefeedCtx::mode != DefeedCtx::Mode::Search) {
    c = Color::Blue;
  }

  return hbox({
    text(" "),
    text(status),
    text(" "),
    Render(),
    filler() | yflex,
  }) | bgcolor(c)
     | color(Color::White);
  // clang-format on
}

CmdLineComponent::CmdLineComponent(CmdLine *_Nonnull clunit) : clunit(clunit) {
  InputOption cmd_line_style = InputOption::Default();
  cmd_line_style.transform = [](InputState state) {
    state.element |= color(Color::White);
    return state.element;
  };
  Component input = Input(&cmd, cmd_line_style);
  Add(input);
}
bool CmdLineComponent::Focusable() const { return true; }
Element CmdLineComponent::OnRender() { return Render(); }
bool CmdLineComponent::OnEvent(Event ev) {
  if (DefeedCtx::mode != DefeedCtx::Mode::Cmd &&
      DefeedCtx::mode != DefeedCtx::Mode::Search)
    return false;

  if (ev == Event::Escape) {
    if (DefeedCtx::mode == DefeedCtx::Mode::Search) {
      this->clunit->exit_search(ev);
    }
    DefeedCtx::mode = DefeedCtx::Mode::Normal;
    DefeedCtx::shift_focus = true;
    return true;
  }

  if (ev == Event::Return) {
    switch (DefeedCtx::mode) {
    case DefeedCtx::Mode::Cmd:
      this->clunit->exec(cmd);
      break;
    case DefeedCtx::Mode::Search:
      this->clunit->exit_search(ev);
      DefeedCtx::shift_focus = true;
      cmd = "";
      break;
    case DefeedCtx::Mode::Normal:
      break;
    }
    DefeedCtx::mode = DefeedCtx::Mode::Normal;
    return true;
  }

  if (DefeedCtx::mode == DefeedCtx::Mode::Search) {
    ComponentBase::OnEvent(ev);
    this->clunit->search(cmd);
    return true;
  }

  return ComponentBase::OnEvent(ev);
}

bool Page::Focusable() const { return true; }
MenuOption Page::menu_style() {
  MenuOption option = MenuOption::Vertical();

  option.entries_option.transform = [](EntryState state) {
    Element e = state.active ? text("[ " + state.label + " ]")
                             : text("  " + state.label + "  ");
    if (state.active)
      e = e | bold | color(Color::Blue);
    return e;
  };
  return option;
}
int Page::GetCurIndex() const { return selected; }
int Page::GetMaxIndex() const { return max_entries; }
bool Page::OnEvent(Event ev) {
  if (Navigation::vim(ev, selected, max_entries)) {
    return true;
  }

  // TODO: Switch Request
  if (ev == Event::Escape || ev == Event::Character('-')) {
    DefeedCtx::switch_request = true;
    DefeedCtx::switch_type = DefeedCtx::SwitchPage::Backwards;
    return true;
  }

  if (ev == Event::Return) {
    DefeedCtx::switch_request = true;
    DefeedCtx::switch_type = DefeedCtx::SwitchPage::Forward;
    return true;
  }

  if (ev == Event::Character('+')) {
    DefeedCtx::switch_request = true;
    DefeedCtx::switch_type = DefeedCtx::SwitchPage::UndoBackwards;
    return true;
  }

  return ComponentBase::OnEvent(ev);
}
PageID Page::get_pid() { return this->pid; }

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

void App::begin_fetch() {
  if (DefeedCtx::task != DefeedCtx::Task::Fetch) {
    f = std::async(std::launch::async, &DefeedCtx::fetchRSS);
    DefeedCtx::task = DefeedCtx::Task::Fetch;
  }
}

void App::switch_page(PageID pid) {
  if (pid == -1 || active_pid == pid)
    return;

  std::shared_ptr<ListPage> lipg =
      std::static_pointer_cast<ListPage>(page_list);
  active_pid = pid;
  lipg->update_entries(pid);
  sb.append("Attached to existing page: " + std::to_string(pid));
}

void App::switch_page() {
  std::shared_ptr<ListPage> lipg =
      std::static_pointer_cast<ListPage>(page_list);
  int selected = lipg->GetCurIndex();
  PageCtx &ctx = PCB::getRefTo(active_pid);
  ctx.selected = selected;

#define SWITCH_EPILOGUE(new_active_pid)                                        \
  do {                                                                         \
    const PageID _new_active_pid = new_active_pid;                             \
    if (_new_active_pid != active_pid) {                                       \
      if (!call_history_back.empty()) {                                        \
        if (active_pid == call_history_back.top()) {                           \
          call_history_back.pop();                                             \
        } else {                                                               \
          call_history_back = {};                                              \
        }                                                                      \
      }                                                                        \
      call_history.push(active_pid);                                           \
      active_pid = *&_new_active_pid;                                          \
      s.pid = active_pid;                                                      \
    }                                                                          \
    DefeedCtx::switch_request = false;                                         \
  } while (0)

  if (DefeedCtx::switch_type == DefeedCtx::SwitchPage::Forward) {
    // FIXME: if active_pid doesnt have schema (instead it has xml root), this
    // crashes
    Schema &s = PCB::getSchemaAt(active_pid, selected);
    if (s.hasPage()) {
      if (DefeedCtx::refetch_rss && s.find("rss")) {
        if (DefeedCtx::refetch_rss_map.find(std::hash<std::string>{}(
                s.m["rss"])) == DefeedCtx::refetch_rss_map.end()) {
          // Rss feed was manually refetched and current entry hasn't been
          // reloaded
          // TODO: Destroy the current page?
          goto CREATE_RSS_PAGE;
        }
      }

      lipg->update_entries(s.pid);
      sb.append("Attached to existing page: " + std::to_string(s.pid));
      SWITCH_EPILOGUE(s.pid);

    } else if (s.find("file")) {
      // NOTE: filenames must be relative to DefeedCtx::defeed
      PageID pid = PCB::create(DefeedCtx::defeed + s.m.find("file")->second);
      lipg->update_entries(pid);
      SWITCH_EPILOGUE(pid);

    } else if (s.find("rss")) {
      // NOTE: Special case, we dont create Schema objects for xml that is not
      // used to define Defeed's behaviour. We also do not know what type of
      // layout will the rss xml have. The best we can do is guess!
    CREATE_RSS_PAGE:
      PageID pid = PCB::create(s.m["rss"], "title");
      lipg->update_entries(pid, "title");
      DefeedCtx::refetch_rss_map[std::hash<std::string>{}(s.m["rss"])] = 1;
      SWITCH_EPILOGUE(pid);

    } else if (s.find("link")) {
      //  chromium --app=link
      const std::string link{"--app=" + s.m["link"]};
      sb.append("Launching Chromium: " + link);
      pid_t child = fork();
      if (child == 0) {
        execlp("chromium", "chromium", link.c_str(), nullptr);
      } else if (child < 0) {
        sb.append("Failed to fork");
      }
      SWITCH_EPILOGUE(active_pid); // nop on switching page
    }

  } else if (DefeedCtx::switch_type == DefeedCtx::SwitchPage::Backwards) {
    if (!call_history.empty()) {
      PageID pid = call_history.top();
      lipg->update_entries(pid);
      // Don't keep temp pages in history
      if (!PCB::Temp::check(active_pid)) {
        call_history_back.push(active_pid);
      }
      active_pid = pid;
      call_history.pop();
      DefeedCtx::switch_request = false;
    }

  } else if (DefeedCtx::switch_type == DefeedCtx::SwitchPage::UndoBackwards) {
    if (!call_history_back.empty()) {
      PageID pid = call_history_back.top();
      lipg->update_entries(pid);
      call_history.push(active_pid);
      active_pid = pid;
      call_history_back.pop();
      DefeedCtx::switch_request = false;
    }
  }

#undef SWITCH_EPILOGUE
}

void App::update_on_event() {
  if (DefeedCtx::shift_focus) {
    active_page = 0;
    app->TakeFocus();
    DefeedCtx::shift_focus = false;
  }
}

bool App::OnEvent(Event ev) {
  update_on_event();

  if (DefeedCtx::mode != DefeedCtx::Mode::Normal)
    return false;

  // Allow children to capture event first,
  // ftxui by default transfers events down the dom tree from root component
  if (app->ComponentBase::OnEvent(ev)) {
    return true;
  }

  if (ev == Event::Character('q')) {
    screen.ExitLoopClosure()();
    return true;
  }

  if (ev == Event::Character('r')) {
    begin_fetch();
    return true;
  }

  if (ev == Event::Character('l')) {
    DefeedCtx::show_logs = !DefeedCtx::show_logs;
    if (DefeedCtx::show_logs) {
      log->TakeFocus();
    } else {
      DefeedCtx::shift_focus = true;
    }
    return true;
  }

  if (ev == Event::Character(':')) {
    DefeedCtx::mode = DefeedCtx::Mode::Cmd;
    cmd_line->TakeFocus(); // Forcefully give it to the input else we need to
                           // press down arrow key or `j`
    return true;
  }

  if (ev == Event::Character('/')) {
    DefeedCtx::mode = DefeedCtx::Mode::Search;
    // NOTE: We must inform the clunit about the PageID on which we want to call
    // the recursive search from, because the cureent pid will be replaced with
    // a temp Search Result list page
    cmd_exec_unit.set_search_pid(
        PCB::Temp::check(active_pid) ? call_history.top() : active_pid);
    cmd_line->TakeFocus();

    // Forcefully update the ctx entries
    int selected = std::static_pointer_cast<ListPage>(page_list)->GetCurIndex();
    PageCtx &ctx = PCB::getRefTo(active_pid);
    ctx.selected = selected;
    return true;
  }

  // NOTE: CtrlJ corresponds to "Return" in terminal
  // CtrlJ -> Line Feed
  // CtrlM -> Carriage Return
  if (ev == Event::CtrlL && DefeedCtx::show_logs) {
    log->TakeFocus();
    return true;
  }

  return false;
}

App::App()
    : screen(ftxui::App::Fullscreen()), cmd_exec_unit(this),
      on_event([this](Event ev) { return OnEvent(ev); }) {
  DefeedCtx::readToplevel();

  // clang-format off
  log          = std::make_shared<LogMenuComponent>();
  cmd_line     = std::make_shared<CmdLineComponent>(&cmd_exec_unit);
  status_line  = std::make_unique<StatusLineComponent>();
  page_list    = std::make_shared<ListPage>(DefeedCtx::toplevel);
  app          = Container::Vertical({
    page_list,

    Renderer([] { return filler(); }),

    status_line,
    Maybe(log, &DefeedCtx::show_logs)
      | size(ftxui::HEIGHT, ftxui::LESS_THAN, 10),
    Maybe(cmd_line, [] { return DefeedCtx::mode == DefeedCtx::Mode::Cmd ||
                                DefeedCtx::mode == DefeedCtx::Mode::Search; })
      | size(ftxui::HEIGHT, ftxui::EQUAL,     1),
  }, &active_page)
    | CatchEvent(on_event);
  // clang-format on
}

void App::event_loop() {
  Loop l(&screen, app);
  while (!l.HasQuitted() && !DefeedCtx::quit) {
    if (DefeedCtx::task == DefeedCtx::Task::Fetch) {
      switch (std::future_status status = f.wait_for(100ms); status) {
      case std::future_status::deferred:
        std::cerr << "main_thread: Illegal defer; Must not defer rss fetch"
                  << std::endl;
        std::abort();

      case std::future_status::ready:
        DefeedCtx::task = DefeedCtx::Task::Ready;
        DefeedCtx::refetch_rss = true;
        DefeedCtx::refetch_rss_map.clear();
        break;

      case std::future_status::timeout:
        break;
      };
    }

    if (DefeedCtx::switch_request) {
      switch_page();
    }

    screen.RequestAnimationFrame();

    l.RunOnce();

    // 60fps
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 60));
  }
}

bool CmdLine::exec(std::string &cmd) {
  // clean the stinky string
  cmd = Utils::strStrip(cmd);
  bool retval = true;

  if (cmd == "log") {
    if ((DefeedCtx::show_logs = !DefeedCtx::show_logs)) {
      parent->log->TakeFocus();
    } else {
      DefeedCtx::shift_focus = true;
    }
  } else if (cmd == "list" || cmd == "ls") {
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
  assert(parent && "CmdLine cannot have nullptr for parent");
}

void CmdLine::set_search_pid(const PageID pid) {
  sb.append("Updated pid");
  this->search_pid = pid;
}

void CmdLine::exit_search(Event ev) {
  if (ev == Event::Return) {
    parent->call_history.push(search_pid);
  } else if (ev == Event::Escape) {
    parent->switch_page(search_pid);
  }
  search_pid = -1;
  search_ss.clear();
}

Schemas CmdLine::search(const std::string &needle) {
  assert(search_pid != -1 && "Must set search pid before using Search method");

  if (needle.empty()) {
    parent->switch_page(search_pid);
    return {};
  }

  search_ss.clear();
  search_schemas(search_pid, Utils::strToLower(needle));

  std::shared_ptr<ListPage> lipg =
      std::static_pointer_cast<ListPage>(parent->page_list);
  PageID temp_pid = PCB::Temp::create(search_ss);
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

      if (!s.hasPage() && s.find("file")) {
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
