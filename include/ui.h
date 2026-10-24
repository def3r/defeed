#include "ftxui/component/component_base.hpp"
#include "global.h"

#include <future>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <memory>
#include <string>

using namespace std::chrono_literals;

extern SharedBuffer sb;

namespace UI {

using namespace ftxui;

class App;

class CmdLine {
  UI::App *parent = nullptr;

public:
  CmdLine(UI::App *parent);
  bool Exec(std::string &cmd);
};

struct LoadingComponent : public ComponentBase {
  Element OnRender() override;
};
struct LogMenuComponent : public ComponentBase {
  LogMenuComponent();
  Element OnRender() override;
  bool Focusable() const override;

private:
  int log_menu_idx = 0;
  Component log_menu;
};

struct StatusLineComponent : public ComponentBase {
  StatusLineComponent();
  Element OnRender() override;

private:
  Component loading;
};

struct CmdLineComponent : public ComponentBase {
  CmdLineComponent(CmdLine *clunit);
  Element OnRender() override;
  bool Focusable() const override;

private:
  std::string cmd;
  Component input;
  CmdLine *clunit;
};

struct Page : public ComponentBase {
  bool Focusable() const override;
  static MenuOption menuStyle();
};

struct LandingPage : public Page {
  LandingPage(const std::vector<std::string> &entries);
  Element OnRender() override;

private:
  int selected = 0;
  std::vector<std::string> entries;
  Component renderer, home_menu;
};

class App {
  friend class CmdLine;

  std::future<std::vector<std::unique_ptr<RssFeed>>> f;
  std::vector<std::unique_ptr<RssFeed>> feeds;

  ftxui::App screen;
  CmdLine cmd_exec_unit;
  Component log, status_line, cmd_line, page, app;

  void init_app();
  void initialize_components();
  void begin_fetch();
  std::vector<std::string> make_entries_from_feed();

public:
  App();
  void EventLoop();
};

} // namespace UI
