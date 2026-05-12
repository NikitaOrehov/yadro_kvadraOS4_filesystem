#include "scan.h"

Scan* g_scanner = nullptr;

void signalHandler(int) {
    if (g_scanner) {
        g_scanner->stopWorker();
    }
    std::exit(0);
}

int main(int argc, char* argv[]) {
    std::string path;
    std::chrono::seconds interval(30);
    bool skipHidden = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--path" && i + 1 < argc) {
            path = argv[++i];
        } else if (arg == "--interval" && i + 1 < argc) {
            int sec = std::stoi(argv[++i]);
            if (sec > 0) interval = std::chrono::seconds(sec);
        } else if (arg == "--skip-hidden") {
            skipHidden = true;
        }
    }

    try {
        Scan scanner(path, interval, skipHidden);
        g_scanner = &scanner;

        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        scanner.runServer();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}