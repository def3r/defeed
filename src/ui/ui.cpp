#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <ftxui/component/loop.hpp>

#include "defeed.h"
#include "global.h"
#include "page.h"
#include "sb.h"
#include "ui.h"

#include <unistd.h>

using namespace std::chrono_literals;

namespace UI {

using namespace ftxui;

Element LoadingComponent::OnRender() {
  static int img;
  constexpr int idx = 15;
  img = (img + 1) % 200;
  return spinner(idx, img);
}

void App::begin_fetch() {
  if (DefeedCtx::task != DefeedCtx::Task::Fetch) {
    f = std::async(std::launch::async, &DefeedCtx::fetchRSS);
    DefeedCtx::task = DefeedCtx::Task::Fetch;
  }
}

void App::switch_page(PageID pid) {
  if (pid == -1 or active_pid == pid)
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

  // The macro of context switch completion:
  // - a lil dereferencing an explicit reference to a local var (*&) hack is
  // used to suppress warning in case of new_active_pid == active_pid
  //
  // - var _new_active_pid is used to suppress warning for != comparison (maybe
  // redundant now that we have _active_pid var)
  //
  // - var _active_pid is used to handle auto page switch when temp search
  // result page has 1 entry, and user presses Event::Return to exit the search
  // mode. In case of a temp page, use parent pid to store call history
#define SWITCH_EPILOGUE(new_active_pid)                                        \
  do {                                                                         \
    const PageID _new_active_pid = new_active_pid;                             \
    const PageID _active_pid = PCB::Temp::check(active_pid)                    \
                                   ? PCB::getRefTo(active_pid).ppid            \
                                   : active_pid;                               \
    if (_new_active_pid != _active_pid) {                                      \
      if (not call_history_back.empty()) {                                     \
        if (_active_pid == call_history_back.top()) {                          \
          call_history_back.pop();                                             \
        } else {                                                               \
          call_history_back = {};                                              \
        }                                                                      \
      }                                                                        \
      call_history.push(_active_pid);                                          \
      active_pid = *&_new_active_pid;                                          \
      s.pid = _active_pid;                                                     \
    }                                                                          \
    DefeedCtx::switch_request = false;                                         \
  } while (0)

  if (DefeedCtx::switch_type == DefeedCtx::SwitchPage::Forward) {
    // FIXME: if active_pid doesnt have schema (instead it has xml root), this
    // crashes
    Schema &s = PCB::getSchemaAt(active_pid, selected);
    if (s.hasPage()) {
      if (DefeedCtx::refetch_rss and s.find("rss")) {
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
        // TODO: to force x11 platform, works for wsl, will fail on wayland
        // (probably) Update: doesn't work for wayland execlp("chromium",
        // "chromium", link.c_str(), "--ozone-platform=x11",
        // 		">/dev/null", "2>&1",  nullptr);
        execlp("chromium", "chromium", link.c_str(), ">/dev/null", "2>&1",
               nullptr);
      } else if (child < 0) {
        sb.append("Failed to fork");
      }
      SWITCH_EPILOGUE(active_pid); // nop on switching page
    }

  } else if (DefeedCtx::switch_type == DefeedCtx::SwitchPage::Backwards) {
    if (not call_history.empty()) {
      PageID pid = call_history.top();
      lipg->update_entries(pid);
      // Don't keep temp pages in history
      if (not PCB::Temp::check(active_pid)) {
        call_history_back.push(active_pid);
      }
      active_pid = pid;
      call_history.pop();
      DefeedCtx::switch_request = false;
    }

  } else if (DefeedCtx::switch_type == DefeedCtx::SwitchPage::UndoBackwards) {
    if (not call_history_back.empty()) {
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

  EVENT_HANDLER_PROLOGUE;

  if (not DefeedCtx::isModeNormal())
    return false;

  // Allow children to capture event first,
  // ftxui by default transfers events down the dom tree from root component
  if (app->ComponentBase::OnEvent(ev)) {
    return true;
  }

  REGISTER_CHAR("quit", q) { screen.ExitLoopClosure()(); }

  REGISTER_CHAR("refetch rss", r) { begin_fetch(); }

  REGISTER_CHAR("toggle logs", l) {
    DefeedCtx::show_logs = not DefeedCtx::show_logs;
    if (DefeedCtx::show_logs) {
      log->TakeFocus();
    } else {
      DefeedCtx::shift_focus = true;
    }
  }

  REGISTER_CHAR("switch to cmd line mode", :) {
    DefeedCtx::mode = DefeedCtx::Mode::Cmd;
    cmd_line->TakeFocus(); // Forcefully give it to the input else we need to
                           // press down arrow key or `j`
  }

  REGISTER_CHAR("switch to search mode", /) {
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
  }

  // NOTE: CtrlJ corresponds to "Return" in terminal
  // CtrlJ -> Line Feed
  // CtrlM -> Carriage Return
  REGISTER("switch focus from logs", EVENT_IS(CtrlL) and DefeedCtx::show_logs) {
    log->TakeFocus();
  }

  EVENT_HANDLER_EPILOGUE;

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
    Maybe(cmd_line, [] { return DefeedCtx::isModeCmd() or
                                DefeedCtx::isModeSearch(); })
      | size(ftxui::HEIGHT, ftxui::EQUAL,     1),
  }, &active_page)
    | CatchEvent(on_event);
  // clang-format on
}

void App::event_loop() {
  Loop l(&screen, app);
  while (not l.HasQuitted() and not DefeedCtx::quit) {
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

} // namespace UI
