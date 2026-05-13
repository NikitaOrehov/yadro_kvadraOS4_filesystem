#include "scan.h"

int main(int argc, char* argv[]) {
    std::string path;
    std::chrono::seconds interval(30);
    bool skipHidden = false;
    bool httpMode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--path" && i + 1 < argc) {
            path = argv[++i];
        } else if (arg == "--interval" && i + 1 < argc) {
            int sec = std::stoi(argv[++i]);
            if (sec > 0) interval = std::chrono::seconds(sec);
        } else if (arg == "--skip-hidden") {
            skipHidden = true;
        } else if (arg == "--http") {
            httpMode = true;
        }
    }

    try {
        Scan scanner(path, interval, skipHidden);
        g_scanner = &scanner;

        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        if (httpMode) {
            scanner.runHttpMode();
        } else {
            scanner.runFileMode();
        }
    } catch (const std::exception& e) {
        return 1;
    }
    return 0;
}