defeed

Feed your head with what you want, and not the garbage that algorithm wants you
to consume to keep you addicted

because you are the product [https://www.cbsnews.com/news/facebook-addictive-as-cigarettes-former-executive-says/]

---

Setup

defeed operates from $HOME/.defeed/ directory, an example .defeed directory is
present in the git repo itself, you can move this directory to $HOME for a
built-in setup.

Within the $HOME/.defeed/ directory, defeed tui expects a top level defeed.xml
This acts as the landing page of defeed.

Example:
```xml
<?xml version="1.0" encoding="utf-8" standalone="yes"?>
<defeed>
  <category>defeedtoplevel</category>
  <item>
    <display>rss</display>
    <file>rss.xml</file>
  </item>
  <item>
    <display>dm</display>
    <link>https://www.instagram.com/direct/inbox/</link>
  </item>
</defeed>
```

In the abv example, if user selects rss, the contents of file specified within
the file tags (path relative to $HOME/.defeed/ dir) is rendered. if a file tag
is not mentioned, defeed tries to find link tag, if it is present, it opens the
link in a focused `chromium --app`.

For some sites a focused stylus css is provided, do install stylus extension
and import the styles.

Defeed Prority Order: file > rss > link

For a more detailed example, see .defeed dir in git repo root.

---

defeed TUI Keybinds

- Navigate via vim keybinds

    l       ->   Toggle logs
    <c-k>   ->   Move focus to page vertically abv
    <c-l>   ->   Move focus to page (log menu) vertically down
    - / Esc ->   Move page back in history
    +       ->   Undo moving page back in history
