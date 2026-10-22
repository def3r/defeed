#include "global.h"

#include <future>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>

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

  std::future<std::vector<std::unique_ptr<XML::NodeBase>>> f;

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
  Component input, cmd_line;
  void init_comp_cmdline();

  Component renderer;
  void init_comp_renderer();

  Component Home;
  void init_comp_home();

  void initialize_components();

public:
  App();
  void EventLoop();
};

} // namespace UI
