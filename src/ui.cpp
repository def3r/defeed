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

using namespace std::chrono_literals;

namespace UI {

using namespace ftxui;

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
  log_menu |= CatchEvent([this](Event ev) {
    static int g_count = 0;
    if (ev == Event::Character("g")) {
      if (++g_count == 2) {
        log_menu_idx = 0;
      }
      return true;
    }
    g_count = 0;

    if (ev == Event::Character("G")) {
      log_menu_idx = entries.size();
      return true;
    }

    return false;
  });
  Add(log_menu);
}
bool LogMenuComponent::Focusable() const { return true; }
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

  return Render() | vscroll_indicator | yframe |
         size(ftxui::HEIGHT, ftxui::EQUAL, 10);
}
MenuOption LogMenuComponent::menuStyle() {
  MenuOption option = MenuOption::Vertical();
  option.entries_option.transform = [](EntryState state) {
    Element e = text(" " + state.label + " ");
    if (state.focused)
      e = e | bold;
    if (state.focused)
      e = e | color(Color::LightCoral);

    if (state.active)
      e = e | bold;
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
  input |= CatchEvent([this](Event event) {
    if (DefeedCtx::mode != DefeedCtx::Mode::Cmd)
      return false;

    if (event == Event::Escape) {
      DefeedCtx::mode = DefeedCtx::Mode::Normal;
      return true;
    }

    if (event == Event::Return) {
      DefeedCtx::mode = DefeedCtx::Mode::Normal;
      if (this->clunit->Exec(cmd)) {
        cmd = "";
      }
      return true;
    }

    return false;
  });
  Add(input);
}
bool CmdLineComponent::Focusable() const { return true; }
Element CmdLineComponent::OnRender() { return Render(); }

bool Page::Focusable() const { return true; }

MenuOption Page::menuStyle() {
  MenuOption option = MenuOption::Vertical();

  option.entries_option.transform = [](EntryState state) {
    Element e = state.active ? text("[ " + state.label + " ]")
                             : text("  " + state.label + "  ");
    if (state.focused)
      e = e | bold;
    if (state.focused)
      e = e | color(Color::Blue);

    if (state.active)
      e = e | bold;
    return e;
  };
  return option;
}

LandingPage::LandingPage(const std::vector<std::string> &entries) {
  selected = 0;
  Component pg = Menu(entries, &this->selected, Page::menuStyle());
  pg |= CatchEvent([this](Event ev) {
    if (ev == Event::Return) {
      DefeedCtx::page = DefeedCtx::Page::List;
    }
    return false;
  });
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
int LandingPage::GetCurIndex() const { return selected; }

ListPage::ListPage(const std::vector<std::string> &entries) {
  UpdateEntries(entries);
}
Element ListPage::OnRender() { return Render(); }
Component ListPage::UpdateEntries(const std::vector<std::string> &entries) {
  selected = 0;
  Component pg = Menu(entries, &selected, Page::menuStyle());
  pg |= CatchEvent([](Event ev) {
    if (ev == Event::Escape || ev == Event::Character('-')) {
      DefeedCtx::page = DefeedCtx::Page::Landing;
    }
    return false;
  });
  Update(pg);
  return pg;
}
void ListPage::Update(Component child) {
  selected = 0;
  DetachAllChildren();
  Add(std::move(child));
}
void ListPage::Update(Component child, int selected) {
  this->selected = selected;
  DetachAllChildren();
  Add(std::move(child));
}
int ListPage::GetCurIndex() const { return selected; }

void App::init_app() {
  // clang-format off
  app = Container::Vertical({
    // pages
    Maybe(page_landing, [this] { return cur_page == DefeedCtx::Page::Landing; }),
    Maybe(page_list, [this] { return cur_page == DefeedCtx::Page::List; }),
    Renderer([] { return filler(); }),

    status_line,
    Maybe(log, &DefeedCtx::show_logs),
    Maybe(cmd_line, [] { return DefeedCtx::mode == DefeedCtx::Mode::Cmd; }),
  });
  // clang-format on

  app |= CatchEvent([this](Event event) {
    if (DefeedCtx::mode != DefeedCtx::Mode::Normal)
      return false;

    if (event == Event::Character('q')) {
      if (DefeedCtx::show_logs)
        DefeedCtx::show_logs = false;
      else
        screen.ExitLoopClosure()();
      return true;
    }

    if (event == Event::Character('r')) {
      begin_fetch();
      return true;
    }

    if (event == Event::Character('l')) {
      DefeedCtx::show_logs = !DefeedCtx::show_logs;
      if (DefeedCtx::show_logs)
        log->TakeFocus();
      return true;
    }

    if (event == Event::Character(':')) {
      DefeedCtx::mode = DefeedCtx::Mode::Cmd;
      cmd_line->TakeFocus(); // Forcefully give it to the input else we need to
                             // press down arrow key or `j`
      return true;
    }

    return false;
  });
}

// clang-format off
void App::initialize_components() {
  log          = std::make_shared<LogMenuComponent>();
  cmd_line     = std::make_shared<CmdLineComponent>(&cmd_exec_unit);
  status_line  = std::make_unique<StatusLineComponent>();
  page_list    = std::make_shared<ListPage>((std::vector<std::string>){});
  page_landing = std::make_shared<LandingPage>(make_entries_from_feed());
  init_app();
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

App::App() : screen(ftxui::App::Fullscreen()), cmd_exec_unit(this) {
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
      if (cur_page == DefeedCtx::Page::Landing) {
        std::shared_ptr<LandingPage> lpg =
            std::static_pointer_cast<LandingPage>(page_landing);
        assert(lpg != nullptr && "Landing page must not be nullptr");
        int selected = lpg->GetCurIndex();
        std::shared_ptr<RssFeed> feed = feeds[selected];
        std::shared_ptr<ListPage> lipg =
            std::static_pointer_cast<ListPage>(page_list);

        if (feed->list_page_menu == nullptr) {
          std::shared_ptr<XML::Node> root =
              std::static_pointer_cast<XML::Node>(feed->root);
          std::vector<std::unique_ptr<XML::NodeBase>> &items =
              static_cast<XML::Node *>(root->children["channel"][0].get())
                  ->children["item"];

          std::vector<std::string> entries{};
          for (int i = 0; i < items.size(); i++) {
            XML::Leaf *title = static_cast<XML::Leaf *>(
                static_cast<XML::Node *>(items[i].get())
                    ->children["title"][0]
                    .get());
            if (title == nullptr) {
              sb.append("Title is nullptr");
              continue;
            }
            if (title->content_idx != 0) {
              sb.append("Title is HTML data");
              continue;
            }
            std::string temp = std::get<0>(title->content);
            entries.push_back(temp);
          }
          feed->list_page_menu = lipg->UpdateEntries(entries);
        } else {
          lipg->Update(feed->list_page_menu, feed->list_page_selected);
        }

        page_list->TakeFocus();

        // DBGer aboba(feed);
      } else if (DefeedCtx::page == DefeedCtx::Page::Landing) {
        if (cur_page == DefeedCtx::Page::List) {
          std::shared_ptr<LandingPage> lpg =
              std::static_pointer_cast<LandingPage>(page_landing);
          std::shared_ptr<ListPage> lipg =
              std::static_pointer_cast<ListPage>(page_list);

          // list_page_menu is populated only at alloc time, but
          // list_page_selected needs to be updated every page switch
          int selected = lpg->GetCurIndex();
          feeds[selected]->list_page_selected = lipg->GetCurIndex();
        }
        page_landing->TakeFocus();
      }
      cur_page = DefeedCtx::page;
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
    DefeedCtx::show_logs = !DefeedCtx::show_logs;
    parent->log->TakeFocus();
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
