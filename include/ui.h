#include "global.h"

#include <future>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <memory>

using namespace std::chrono_literals;

extern SharedBuffer sb;

namespace UI {

using namespace ftxui;

enum class Mode { Normal, Cmd };

class App;

class CmdLine {
  UI::App *parent = nullptr;

public:
  CmdLine(UI::App *parent);
  bool Exec(std::string &cmd);
};

class App {
  friend class CmdLine;

  std::future<std::vector<std::unique_ptr<RssFeed>>> f;
  std::vector<std::unique_ptr<RssFeed>> feeds;

  bool show_logs = false;
  bool fetching_rss = false;
  Mode mode = Mode::Normal;
  bool quit = false;
  ftxui::App screen;
  CmdLine cmd_exec_unit;

  size_t timeout = 0;
  int count = 0;

  Component loading;
  void init_comp_loading();

  int log_menu_idx = 0;
  Component log_menu, log;
  void init_comp_log();

  Component status_line;
  void init_comp_status_line();

  std::string cmd;
  Component input, cmd_line, prev_focus;
  void init_comp_cmdline();

  int selected = 0;
  std::vector<std::string> entries;
  Component renderer, home_menu;
  Component listPage;
  Component *ptrComponent = nullptr;
  void init_comp_renderer();

  Component Home;
  void init_comp_home();

  void initialize_components();

  void begin_fetch();

public:
  App();
  void EventLoop();
};

} // namespace UI
