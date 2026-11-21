#include <functional>
#include <future>
#include <stack>
#include <string>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>

#include "types.h"

extern SharedBuffer sb;

namespace UI {

using namespace ftxui;

class App;

struct CmdLine {
  CmdLine(UI::App *parent);
  bool exec(std::string &cmd);
  Schemas search(const std::string &needle);
  void set_search_pid(const PageID pid);
  void exit_search(Event ev);

private:
  void search_schemas(const PageID search_pid, const std::string l_needle);

private:
  UI::App *parent = nullptr;
  PageID search_pid = -1;
  Schemas search_ss{};
};

struct LoadingComponent : public ComponentBase {
  Element OnRender() override;
};

struct LogMenuComponent : public ComponentBase {
  LogMenuComponent();
  Element OnRender() override;
  bool Focusable() const override;
  static MenuOption menu_style();
  bool OnEvent(Event ev) override;

private:
  int log_menu_idx = 0;
  std::vector<std::string_view> entries{};
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
  std::string cmd{};
  CmdLine *clunit = nullptr;
};

struct Page : public ComponentBase, public MenuOption {
  bool Focusable() const override;
  static MenuOption menu_style();
  bool OnEvent(Event ev) override;
  int GetCurIndex() const;
  int GetMaxIndex() const;
  PageID get_pid();

protected:
  PageID pid = 0;
  int selected = 0;
  int max_entries = 0;
};

struct ListPage : public Page {
  explicit ListPage(const Schemas &ss);
  explicit ListPage(const Schemas &ss, const std::string &entry_key);
  Element OnRender() override;
  Component update_entries(const PageID pid,
                           const std::string &entry_key = "display");
  Component update_entries(const Schemas &ss,
                           const std::string &entry_key = "display");
  Component update_entries(const std::vector<std::string> &entries);
  void update(Component child);
  void update(Component child, int selected, int max_entries);

protected:
  std::vector<std::string> entries{};
};

class App {
  friend class CmdLine;

  std::future<void> f{};

  ftxui::App screen;
  CmdLine cmd_exec_unit;
  Component log{}, status_line{}, cmd_line{}, app{};
  Component page_landing{}, page_list{};
  int active_page = 0;
  int cur_idx = 0;

  bool OnEvent(Event ev);
  std::function<bool(Event)> on_event;

  void begin_fetch();
  void switch_page();
  void switch_page(PageID pid);
  void update_on_event();

  // Page impl
  PageID active_pid = 0;
  std::stack<PageID> call_history{}, call_history_back{};

public:
  App();
  void event_loop();
};

} // namespace UI
