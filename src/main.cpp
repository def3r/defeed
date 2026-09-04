#include <fstream>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <iostream>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <curl/multi.h>

// TODO: curl_multi: https://curl.se/libcurl/c/libcurl-multi.html

class CurlGlobal {
private:
  CurlGlobal() {}

public:
  static void init() {
    static bool cinit = false;
    if (cinit == true) {
      return;
    }
    CURLcode result = curl_global_init(CURL_GLOBAL_ALL);
    if (result != CURLE_OK) {
      std::cerr << "Could not init curl" << std::endl;
      std::exit(1);
    }
    cinit = true;
  }

  static void cleanup() {
    static bool ccleanup = false;
    if (ccleanup == true) {
      return;
    }
    curl_global_cleanup();
    ccleanup = true;
  }
};

class MultiFetcher;

class Fetcher {
  friend class MultiFetcher;

public:
  Fetcher() { init_common(); }

  // RAII obj, steal on move, delete on copy
  // move constructor
  Fetcher(Fetcher &&other) noexcept {
    this->curl = other.curl;
    this->url = other.url;
    other.curl = nullptr;
  }
  // move assignment
  Fetcher &operator=(Fetcher &&other) noexcept {
    if (this != &other) {
      this->curl = other.curl;
      this->url = other.url;
      other.curl = nullptr;
    }
    return *this;
  }
  Fetcher(const Fetcher &) = delete;
  Fetcher &operator=(const Fetcher &) = delete;

  Fetcher(const std::string &url) : url(url) {
    init_common();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  }

  ~Fetcher() {
    if (curl != nullptr) {
      curl_easy_cleanup(curl);
    }
  }

  CURLcode perform_write(const std::string &fname) {
    FILE *pagefile = fopen(fname.c_str(), "wb");
    if (!pagefile) {
    }
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, pagefile);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
      return res;
    }
    fclose(pagefile);
    return res;
  }

private:
  CURL *curl;
  CURLcode res;
  std::string url;

  static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
  }

  void init_common() {
    CurlGlobal::init();
    curl = curl_easy_init();
    if (!curl) {
      std::cerr << "Could not init easy curl" << std::endl;
      std::exit(1);
    }
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Fetcher::write_cb);
  }
};

class MultiFetcher {
public:
  MultiFetcher() : multi(curl_multi_init()) {}
  ~MultiFetcher() {
    if (multi) {
      curl_multi_cleanup(multi);
    }
  }

  void add(const std::string &f_str) {
    fetchers.emplace_back(f_str);
    CURLMcode res = curl_multi_add_handle(multi, fetchers.back().curl);
    if (res != CURLM_OK) {
      std::cerr << "curl_multi_add_handle() failed, code " << res << std::endl;
      std::cerr << std::string(curl_multi_strerror(res)) << std::endl;
    }
  }

  bool perform_write(const std::string &prefix) {
    int still_running;
    std::vector<FILE *> fps{};
    fps.reserve(fetchers.size());

    for (size_t i = 0; i < fetchers.size(); i++) {
      std::string fname{prefix + std::to_string(i)};
      FILE *pagefile = fopen(fname.c_str(), "wb");
      if (!pagefile) {
        std::cerr << "Unable to open file: " << fname << std::endl;
        return false;
      }
      curl_easy_setopt(fetchers[i].curl, CURLOPT_WRITEDATA, pagefile);
      fps.push_back(pagefile);
    }

    do {
      CURLMcode mresult = curl_multi_perform(multi, &still_running);
      if (mresult != CURLM_OK) {
        std::cerr << "curl_multi_perform() failed, code" << mresult
                  << std::endl;
        break;
      }

      mresult = curl_multi_poll(multi, NULL, 0, 1000, NULL);
      if (mresult != CURLM_OK) {
        std::cerr << "curl_multi_poll() failed, code " << mresult << std::endl;
        break;
      }
    } while (still_running);

    for (size_t i = 0; i < fetchers.size(); i++) {
      fclose(fps[i]);
    }

    return true;
  }

private:
  CURLM *multi;
  std::vector<Fetcher> fetchers;
};

int main(int argc, char *argv[]) {
  using namespace ftxui;

  std::string rss_file = "rss.txt";

  if (argc > 1) {
    rss_file = argv[1];
  }

  std::ifstream rss_feed_file(rss_file);
  if (!rss_feed_file.is_open()) {
    std::cerr << "Cant open file " << rss_file << std::endl;
    std::exit(1);
  }

  MultiFetcher mf;

  int i = 0;
  while (!rss_feed_file.eof()) {
    std::string url{};
    std::getline(rss_feed_file, url);
    if (!url.empty()) {
      std::cout << url << std::endl;
      mf.add(url);
    }
    i++;
  }

  mf.perform_write("outfile_");

  // Create a simple document with three text elements.
  Element document = hbox({
      text("left") | border,
      text("middle") | border | flex,
      text("right") | border,
  });

  // Create a screen with full width and height fitting the document.
  auto screen = Screen::Create(Dimension::Full(),       // Width
                               Dimension::Fit(document) // Height
  );

  // Render the document onto the screen.
  Render(screen, document);

  // Print the screen to the console.
  screen.Print();

  CurlGlobal::cleanup();
}
