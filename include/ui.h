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

struct CmdLine {
  CmdLine(UI::App *parent);
  bool Exec(std::string &cmd);

private:
  UI::App *parent = nullptr;
};

struct LoadingComponent : public ComponentBase {
  Element OnRender() override;
};

struct LogMenuComponent : public ComponentBase {
  LogMenuComponent();
  Element OnRender() override;
  bool Focusable() const override;
  static MenuOption menuStyle();

private:
  int log_menu_idx = 0;
  std::vector<std::string_view> entries;
};

struct StatusLineComponent : public ComponentBase {
  StatusLineComponent();
  Element OnRender() override;
};

struct CmdLineComponent : public ComponentBase {
  CmdLineComponent(CmdLine *clunit);
  Element OnRender() override;
  bool Focusable() const override;

private:
  std::string cmd;
  CmdLine *clunit;
};

struct Page : public ComponentBase, public MenuOption {
  bool Focusable() const override;
  static MenuOption menuStyle();
};

struct LandingPage : public Page {
  explicit LandingPage(const std::vector<std::string> &entries);
  Element OnRender() override;
  int GetCurIndex() const;

private:
  int selected = 0;
};

struct ListPage : public Page {
  ListPage(const std::vector<std::string> &entries);
  Element OnRender() override;
  Component UpdateEntries(const std::vector<std::string> &entries);
  void Update(Component child);
  void Update(Component child, int selected);
  int GetCurIndex() const;

private:
  int selected = 0;
};

class App {
  friend class CmdLine;

  std::future<std::vector<std::shared_ptr<RssFeed>>> f;
  std::vector<std::shared_ptr<RssFeed>> feeds;

  ftxui::App screen;
  CmdLine cmd_exec_unit;
  Component log, status_line, cmd_line, app;
  Component page_landing, page_list;
  DefeedCtx::Page cur_page = DefeedCtx::Page::Landing;
  int cur_idx = 0;

  void init_app();
  void initialize_components();
  void begin_fetch();
  std::vector<std::string> make_entries_from_feed();

public:
  App();
  void EventLoop();
};

} // namespace UI
