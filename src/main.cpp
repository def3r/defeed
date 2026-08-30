#include <fstream>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <iostream>
#include <string>

#include <curl/curl.h>

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *stream) {
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

int main(int argc, char *argv[]) {
  CURL *curl;
  CURLcode result;

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

  int i = 0;
  result = curl_global_init(CURL_GLOBAL_ALL);
  if (result != CURLE_OK) {
    std::cerr << "Could not init curl" << std::endl;
    std::exit(1);
  }
  curl = curl_easy_init();
  if (curl) {
    while (!rss_feed_file.eof()) {
      std::string line{};
      std::string outname{"outfile" + std::to_string(i)};
      std::getline(rss_feed_file, line);
      if (!line.empty()) {
        std::cout << line << std::endl;

        curl_easy_setopt(curl, CURLOPT_URL, line.c_str());
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);

        FILE *pagefile = fopen(outname.c_str(), "wb");
        if (pagefile) {
          curl_easy_setopt(curl, CURLOPT_WRITEDATA, pagefile);
          result = curl_easy_perform(curl);
          if (result != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(result));
          }
          fclose(pagefile);
        }
      }
      std::cout << "Fetch: result=" << result << std::endl;
      i++;
    }
  }
  curl_easy_cleanup(curl);
  curl_global_cleanup();

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
}
