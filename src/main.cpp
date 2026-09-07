#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <functional>
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

class DefeedCtx {
public:
  static inline std::string home{}, defeed{};
  static inline std::string rss{}, rss_txt{}, rss_info{};

  static void init() {
    const char *env_home = std::getenv("HOME");
    if (env_home == nullptr) {
      std::cerr << "Env var $HOME not set" << std::endl;
      std::abort();
    }
    home = env_home;
    defeed = home + "/.defeed";
    rss = defeed + "/rss";
    rss_txt = rss + ".txt";
    rss_info = rss + ".info";
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
    this->headers = other.headers;

    other.curl = nullptr;
    other.headers = nullptr;
  }
  // move assignment
  Fetcher &operator=(Fetcher &&other) noexcept {
    if (this != &other) {
      this->curl = other.curl;
      this->url = other.url;
      this->headers = other.headers;

      other.curl = nullptr;
      other.headers = nullptr;
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
    if (curl) {
      curl_easy_cleanup(curl);
    }
    if (headers) {
      curl_slist_free_all(headers);
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

  void append_headers(const std::string &header_str) {
    headers = curl_slist_append(headers, header_str.c_str());
  }
  void apply_headers() { curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); }

private:
  CURL *curl;
  CURLcode res;
  std::string url;
  std::string url_hash;
  struct curl_slist *headers;

  static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
  }

  static size_t header_callback(char *buffer, size_t size, size_t nitems,
                                void *userdata) {
    size_t n = size * nitems;

    if (userdata == nullptr)
      return n;
    size_t hash = *static_cast<size_t *>(userdata);

    char temp[n - 1]; // ignore \r\n
    std::memcpy(temp, buffer, n - 2);
    temp[n - 2] = '\0';
    std::string res{temp};

    size_t pos = res.find("etag: ");
    if (pos != std::string::npos) {
      std::filesystem::path url_path{DefeedCtx::rss + "/" +
                                     std::to_string(hash)};
      // At this point we assume all the dirs have been setup
      if (!std::filesystem::is_directory(url_path)) {
        std::abort();
      }
      std::ofstream etag_file{url_path.string() + "/etag"};
      etag_file << res.substr(pos + 6);
      std::cout << "For hash: " << hash << " \t" << res.substr(pos + 6)
                << std::endl;
      etag_file.close();
    }
    return n;
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
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

    this->headers = nullptr;
  }

  void update_headers(struct curl_slist *headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }
};

class MultiFetcher {
public:
  MultiFetcher() : multi(curl_multi_init()) {}
  ~MultiFetcher() {
    if (multi) {
      curl_multi_cleanup(multi);
    }
    if (headers) {
      curl_slist_free_all(headers);
    }
  }

  void add(const std::string &f_str, size_t hash) {
    fetchers.emplace_back(f_str);
    CURLMcode res = curl_multi_add_handle(multi, fetchers.back().curl);
    if (res != CURLM_OK) {
      std::cerr << "curl_multi_add_handle() failed, code " << res << std::endl;
      std::cerr << std::string(curl_multi_strerror(res)) << std::endl;
    }
    this->hash.push_back(hash);
  }
  void add(const std::string &f_str) {
    fetchers.emplace_back(f_str);
    CURLMcode res = curl_multi_add_handle(multi, fetchers.back().curl);
    if (res != CURLM_OK) {
      std::cerr << "curl_multi_add_handle() failed, code " << res << std::endl;
      std::cerr << std::string(curl_multi_strerror(res)) << std::endl;
    }
    this->hash.push_back(std::hash<std::string>{}(f_str));
  }
  void add(Fetcher &&f) {
    fetchers.push_back(std::move(f));
    CURLMcode res = curl_multi_add_handle(multi, fetchers.back().curl);
    if (res != CURLM_OK) {
      std::cerr << "curl_multi_add_handle() failed, code " << res << std::endl;
      std::cerr << std::string(curl_multi_strerror(res)) << std::endl;
    }
    this->hash.push_back(std::hash<std::string>{}(f.url));
  }

  bool perform_write() {
    int still_running;
    std::vector<FILE *> fps{};
    fps.reserve(fetchers.size());

    for (size_t h : hash) {
      std::filesystem::path url_dir{DefeedCtx::rss + "/" + std::to_string(h)};
      if (!std::filesystem::is_directory(url_dir)) {
        if (!std::filesystem::create_directory(url_dir)) {
          std::cerr << "Unable to mkdir " << url_dir.string() << std::endl;
          std::abort();
        }
      }
    }

    for (size_t i = 0; i < fetchers.size(); i++) {
      std::string fname{DefeedCtx::rss + "/" + std::to_string(hash[i]) +
                        "/rssfeed.txt"};
      FILE *pagefile = override_file ? std::fopen(fname.c_str(), "wb")
                                     : std::fopen(fname.c_str(), "ab");
      if (!pagefile) {
        std::cerr << "Unable to open file: " << fname << std::endl;
        return false;
      }
      curl_easy_setopt(fetchers[i].curl, CURLOPT_WRITEDATA, pagefile);
      fps.push_back(pagefile);
      curl_easy_setopt(fetchers[i].curl, CURLOPT_HEADERDATA, &hash[i]);
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

  void append_headers(const std::string &header_str) {
    this->headers = curl_slist_append(headers, header_str.c_str());
  }

  void update_fetcher_headers() {
    for (Fetcher &fetcher : fetchers) {
      fetcher.update_headers(headers);
    }
  }

  void reset_override_file() { override_file = false; }

private:
  CURLM *multi;
  std::vector<Fetcher> fetchers;
  std::vector<size_t> hash;
  struct curl_slist *headers = NULL;
  bool override_file = true;
};

void defeed_setup() {
  namespace fs = std::filesystem;
  fs::path defeed_dir{DefeedCtx::defeed};
  if (!fs::is_directory(defeed_dir)) {
    std::cout << "defeed dir not found, initializing defeed." << std::endl;
    if (!fs::create_directory(defeed_dir)) {
      std::cerr << "Unable to create dir: " << defeed_dir << std::endl;
      std::exit(1);
    }
    if (!fs::create_directory(DefeedCtx::rss)) {
      std::cerr << "Unable to create dir: " << defeed_dir << std::endl;
      std::exit(1);
    }
    std::ofstream rss_file{DefeedCtx::rss_txt};
    rss_file.close();
  }
}

int main(int argc, char *argv[]) {
  using namespace ftxui;

  DefeedCtx::init();
  defeed_setup();

  std::ifstream rss_info{DefeedCtx::rss_info};
  std::ifstream rss_txt{DefeedCtx::rss_txt};
  if (!rss_txt.is_open()) {
    std::cerr << "Cant open file " << DefeedCtx::rss_txt << std::endl;
    std::exit(1);
  }

  std::vector<std::pair<std::string, size_t>> rss_urls{};
  while (!rss_txt.eof()) {
    std::string url{};
    std::getline(rss_txt, url);
    if (!url.empty()) {
      std::cout << url << "\t\t" << std::hash<std::string>{}(url) << std::endl;
      rss_urls.push_back({url, std::hash<std::string>{}(url)});
      // mf.add(url);
    }
  }
  rss_txt.close();

  MultiFetcher fetch_rss{};
  MultiFetcher cond_fetch_rss{};
  cond_fetch_rss.reset_override_file();
  for (auto [url, hash] : rss_urls) {
    std::filesystem::path url_path{DefeedCtx::rss + "/" + std::to_string(hash)};
    if (!std::filesystem::is_directory(url_path)) {
      fetch_rss.add(url);
      continue;
    }
    std::ifstream etag_file{url_path.string() + "/etag"};
    if (!etag_file.is_open()) {
      std::cout << "Can't find etag file for " << hash << " (" << url << ")"
                << std::endl;
      fetch_rss.add(url);
      continue;
    }
    std::string etag{};
    etag_file >> etag;
    etag_file.close();

    Fetcher f{url};
    f.append_headers("If-None-Match: " + etag);
    f.apply_headers();
    cond_fetch_rss.add(std::move(f));
  }

  fetch_rss.perform_write();
  cond_fetch_rss.perform_write();

  // mf.perform_write("outfile_");

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
