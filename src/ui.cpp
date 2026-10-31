#include "ui.h"
#include "global.h"
#include "xml.h"

#include <cassert>
#include <future>
#include <iostream>
#include <memory>
#include <thread>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/terminal.hpp>

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
  entries = sb.getEntries();
  Component log_menu =
      Menu(&entries, &log_menu_idx, LogMenuComponent::menuStyle());
  Add(log_menu);
}
bool LogMenuComponent::Focusable() const { return DefeedCtx::show_logs; }
Element LogMenuComponent::OnRender() {
  entries = sb.getEntries();
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

  return ComponentBase::OnEvent(ev);
}
MenuOption LogMenuComponent::menuStyle() {
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
  // clang-format off
  if (DefeedCtx::mode == DefeedCtx::Mode::Normal) {
    switch (DefeedCtx::task) {
      case DefeedCtx::Task::Ready: status = "Normal"; break;
      case DefeedCtx::Task::Fetch: status = "Fetching"; break;
      case DefeedCtx::Task::Load:  status = "Loading"; break;
    }
  } else status = "Cmd";

  return hbox({
    text(" "),
    text(status),
    text(" "),
    Render(),
    filler() | yflex,
  }) | bgcolor(Color::Blue)
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
  if (DefeedCtx::mode != DefeedCtx::Mode::Cmd)
    return false;

  if (ev == Event::Escape) {
    DefeedCtx::mode = DefeedCtx::Mode::Normal;
    return true;
  }

  if (ev == Event::Return) {
    DefeedCtx::mode = DefeedCtx::Mode::Normal;
    if (this->clunit->Exec(cmd)) {
      cmd = "";
    }
    return true;
  }

  return ComponentBase::OnEvent(ev);
}

bool Page::Focusable() const { return true; }
MenuOption Page::menuStyle() {
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

  if (ev == Event::Escape || ev == Event::Character('-')) {
    DefeedCtx::PageUp();
    return true;
  }

  if (ev == Event::Return) {
    DefeedCtx::PageDown();
    return true;
  }

  return ComponentBase::OnEvent(ev);
}

LandingPage::LandingPage(const std::vector<std::string> &entries) {
  selected = 0;
  max_entries = entries.size();
  Component pg = Menu(entries, &this->selected, Page::menuStyle());
  Add(pg);
}
Element LandingPage::OnRender() {
  // clang-format off
  Element home = vbox({
    text("Home screen"),
    separator(),
    Render(),
  }) | flex;
  // clang-format on
  return home;
}
bool LandingPage::OnEvent(Event ev) { return Page::OnEvent(ev); }

ListPage::ListPage(const std::vector<std::string> &entries) {
  UpdateEntries(entries);
}
Element ListPage::OnRender() {
  Dimensions dims = ftxui::Terminal::Size();
  return Render() | vscroll_indicator | yframe |
         size(ftxui::HEIGHT, ftxui::LESS_THAN,
              dims.dimy - (DefeedCtx::show_logs ? 10 : 2));
  ;
}
Component ListPage::UpdateEntries(const std::vector<std::string> &entries) {
  selected = 0;
  max_entries = entries.size();
  Component pg = Menu(entries, &selected, Page::menuStyle());
  Update(pg);
  return pg;
}
void ListPage::Update(Component child) {
  selected = 0;
  DetachAllChildren();
  Add(std::move(child));
}
void ListPage::Update(Component child, int selected, int max_entries) {
  this->selected = selected;
  this->max_entries = max_entries;
  DetachAllChildren();
  Add(std::move(child));
}
bool ListPage::OnEvent(Event ev) { return Page::OnEvent(ev); }

// clang-format off
void App::initialize_components() {
  log          = std::make_shared<LogMenuComponent>();
  cmd_line     = std::make_shared<CmdLineComponent>(&cmd_exec_unit);
  status_line  = std::make_unique<StatusLineComponent>();
  page_list    = std::make_shared<ListPage>((std::vector<std::string>){});
  page_landing = std::make_shared<LandingPage>(make_entries_from_feed());
  app          = Container::Vertical({
    // pages
    Maybe(page_landing, [this] { return cur_page == DefeedCtx::Page::Landing; }),
    Maybe(page_list,    [this] { return cur_page == DefeedCtx::Page::List;    }),

    Renderer([] { return filler(); }),

    status_line,
    Maybe(log, &DefeedCtx::show_logs)
      | size(ftxui::HEIGHT, ftxui::LESS_THAN, 10),
    Maybe(cmd_line, [] { return DefeedCtx::mode == DefeedCtx::Mode::Cmd; })
      | size(ftxui::HEIGHT, ftxui::EQUAL,     1),
  }, &active_page)
    | CatchEvent(on_event);
}
// clang-format on

void App::begin_fetch() {
  if (DefeedCtx::task != DefeedCtx::Task::Fetch) {
    f = std::async(std::launch::async, &DefeedCtx::fetchRSS);
    DefeedCtx::task = DefeedCtx::Task::Fetch;
  }
}

std::vector<std::string> App::make_entries_from_feed() {
  std::vector<std::string> entries{};
  entries.reserve(feeds.size());
  for (int i = 0; i < feeds.size(); i++) {
    entries.push_back(feeds[i].get()->url);
  }
  return entries;
}

void App::switch_page(DefeedCtx::Page from, DefeedCtx::Page to) {
  std::shared_ptr<LandingPage> lpg =
      std::static_pointer_cast<LandingPage>(page_landing);
  std::shared_ptr<ListPage> lipg =
      std::static_pointer_cast<ListPage>(page_list);
  assert(lpg != nullptr && "Landing page must not be nullptr");
  int selected = lpg->GetCurIndex();
  std::shared_ptr<RssFeed> feed = feeds[selected];
  Component &target = to == DefeedCtx::Page::Landing ? page_landing : page_list;

  if (from == DefeedCtx::Page::Landing && to == DefeedCtx::Page::List) {
    // Creating the page for the first time
    if (feed->list_page_menu == nullptr) {
      XML::Internal root = XML_CAST_INTERNAL(feed->root);
      XML::Siblings &items =
          (XML_CAST_INTERNALBASE(root->children["channel"][0]))
              ->children["item"];

      std::vector<std::string> entries{};
      for (int i = 0; i < items.size(); i++) {
        XML::LeafBase *title = XML_CAST_LEAFBASE(
            XML_CAST_INTERNALBASE(items[i])->children["title"][0]);
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
      feed->list_page_menu = lipg->UpdateEntries(entries);
      feed->list_page_max_entries = entries.size();

    } else {
      lipg->Update(feed->list_page_menu, feed->list_page_selected,
                   feed->list_page_max_entries);
    }

    // Walker aboba(feed.get());
  } else if (from == DefeedCtx::Page::List && to == DefeedCtx::Page::Landing) {
    // list_page_menu is populated only at alloc time, but
    // list_page_selected needs to be updated every page switch
    feed->list_page_selected = lipg->GetCurIndex();
  }

  target->TakeFocus();
  cur_page = to;
}

void App::update_on_msg() {
  if (DefeedCtx::shift_focus) {
    active_page = static_cast<int>(DefeedCtx::page);
    app->TakeFocus();
    DefeedCtx::shift_focus = false;
  }
}

bool App::OnEvent(Event ev) {
  update_on_msg();

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

  return false;
}

App::App()
    : screen(ftxui::App::Fullscreen()), cmd_exec_unit(this),
      on_event([this](Event ev) { return OnEvent(ev); }) {
  // TODO: set DefeedCtx::task to Task::Load when we move to async
  feeds = DefeedCtx::loadRSS();
  initialize_components();
}

void App::EventLoop() {
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
        break;

      case std::future_status::timeout:
        break;
      };
    }

    if (cur_page != DefeedCtx::page) {
      switch_page(cur_page, DefeedCtx::page);
    }

    screen.RequestAnimationFrame();

    l.RunOnce();

    // 60fps
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 60));
  }
}

bool CmdLine::Exec(std::string &cmd) {
  // clean the stinky string
  cmd.erase(cmd.find_last_not_of(" \t") + 1);
  cmd.erase(0, cmd.find_first_not_of(" \t"));

  bool retval = true;

  if (cmd == "log") {
    if ((DefeedCtx::show_logs = !DefeedCtx::show_logs)) {
      parent->log->TakeFocus();
    } else {
      DefeedCtx::shift_focus = true;
    }
  } else if (cmd == "q") {
    DefeedCtx::quit = true;
  } else if (cmd == "fetch") {
    parent->begin_fetch();
  } else {
    retval = false;
  }

  return retval;
}

CmdLine::CmdLine(UI::App *parent) : parent(parent) {
  assert(parent && "CmdLine cannot have nullptr for parent");
}

} // namespace UI
