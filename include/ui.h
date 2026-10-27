#include "global.h"

#include <functional>
#include <future>
#include <memory>
#include <string>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>

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
  bool OnEvent(Event ev) override;

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
  bool OnEvent(Event ev) override;

private:
  std::string cmd;
  CmdLine *clunit;
};

struct Page : public ComponentBase, public MenuOption {
  bool Focusable() const override;
  static MenuOption menuStyle();
  bool OnEvent(Event ev) override;
  int GetCurIndex() const;
  int GetMaxIndex() const;

protected:
  int selected = 0;
  int max_entries = 0;
};

struct LandingPage : public Page {
  explicit LandingPage(const std::vector<std::string> &entries);
  Element OnRender() override;
  bool OnEvent(Event ev) override;
};

struct ListPage : public Page {
  explicit ListPage(const std::vector<std::string> &entries);
  Element OnRender() override;
  bool OnEvent(Event ev) override;
  Component UpdateEntries(const std::vector<std::string> &entries);
  void Update(Component child);
  void Update(Component child, int selected, int max_entries);
};

class App {
  friend class CmdLine;

  std::future<std::vector<std::shared_ptr<RssFeed>>> f;
  std::vector<std::shared_ptr<RssFeed>> feeds;

  ftxui::App screen;
  CmdLine cmd_exec_unit;
  Component log, status_line, cmd_line, app;
  Component page_landing, page_list;
  int active_page = 0;
  DefeedCtx::Page cur_page = DefeedCtx::Page::Landing;
  int cur_idx = 0;

  bool OnEvent(Event ev);
  std::function<bool(Event)> on_event;

  void initialize_components();
  void begin_fetch();
  std::vector<std::string> make_entries_from_feed();
  void switch_page(DefeedCtx::Page from, DefeedCtx::Page to);
  void update_on_msg();

public:
  App();
  void EventLoop();
};

} // namespace UI
