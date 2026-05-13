#include <filesystem>
#include <vector>
#include <string>
#include <thread>
#include <unordered_set>
#include <atomic>
#include <mutex>
#include <chrono>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <csignal>
#include <nlohmann/json.hpp>
#include <httplib.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

static httplib::Server* g_server = nullptr;

class Scan {
public:
    Scan(const std::string& path, std::chrono::seconds interval, bool skipHidden)
        : interval_(interval), skipHidden_(skipHidden)
    {
        if (path.empty()) {
            const char* home = std::getenv("HOME");
            if (!home) throw std::runtime_error("HOME not set");
            path_ = home;
        } else {
            path_ = path;
        }
    }

    ~Scan() {
        stopWorker();
    }

    void runFileMode() {
        startWorker(&Scan::fileWorkerLoop);
        waitForSignal();
    }

    void runHttpMode() {
        startWorker(&Scan::httpWorkerLoop);
        httplib::Server svr;
        g_server = &svr;

        svr.Get("/media_files", [this](const httplib::Request&, httplib::Response& res) {
            res.set_content(getJSON(), "application/json");
        });

        svr.listen("localhost", 1234);

        g_server = nullptr;
        stopWorker();
    }

    std::string getJSON() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return jsonResult_.dump();
    }

    void stopWorker() {
        bool expected = false;
        if (!stopping_.compare_exchange_strong(expected, true)) {
            return;
        }
        running_ = false;
        cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
}

private:
    std::vector<std::string> audio_;
    std::vector<std::string> video_;
    std::vector<std::string> images_;
    std::chrono::seconds interval_;
    fs::path path_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};
    mutable std::mutex mutex_;
    std::mutex stopMutex_;
    json jsonResult_;
    std::thread worker_;
    bool skipHidden_ = false;

    const std::unordered_set<std::string> image_extensions_ = {
        ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp",
        ".tiff", ".tif", ".ico"
    };
    const std::unordered_set<std::string> audio_extensions_ = {
        ".mp3", ".wav", ".flac", ".aac", ".ogg", ".wma", ".m4a",
        ".opus", ".ape", ".aiff", ".aif"
    };
    const std::unordered_set<std::string> video_extensions_ = {
        ".mp4", ".mkv", ".webm", ".avi", ".mov", ".wmv", ".flv",
        ".mpg", ".mpeg", ".3gp", ".ogv"
    };

    void scanFileSystem(const fs::path& root) {
        audio_.clear(); video_.clear(); images_.clear();

        std::error_code ec;
        if (!fs::is_directory(root, ec) || ec) return;

        auto it = fs::recursive_directory_iterator(
            root,
            fs::directory_options::skip_permission_denied,
            ec
        );
        if (ec) return;

        auto end = fs::recursive_directory_iterator();

        while (it != end) {
            try {
                if (skipHidden_ && it->is_directory()) {
                    const auto& dirName = it->path().filename().string();
                    if (!dirName.empty() && dirName[0] == '.') {
                        it.disable_recursion_pending();
                        ++it;
                        continue;
                    }
                }

                if (it->is_regular_file()) {
                    std::string ext = it->path().extension().string();
                    if (audio_extensions_.count(ext)) {
                        audio_.push_back(it->path().filename().string());
                    } else if (video_extensions_.count(ext)) {
                        video_.push_back(it->path().filename().string());
                    } else if (image_extensions_.count(ext)) {
                        images_.push_back(it->path().filename().string());
                    }
                }
                ++it;
            } catch (const fs::filesystem_error&) {
                it.disable_recursion_pending();
                ++it;
            }
        }
    }

    void updateJSON() {
        json j;
        j["audio"] = audio_;
        j["video"] = video_;
        j["images"] = images_;

        std::lock_guard<std::mutex> lock(mutex_);
        jsonResult_ = std::move(j);
    }

    void writeToFile() {
        fs::path outPath = path_ / ".media_files";
        fs::path tmpPath = outPath;
        tmpPath += ".tmp";

        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::ofstream out(tmpPath);
            if (out) {
                out << jsonResult_.dump();
            }
        }

        if (fs::exists(tmpPath)) {
            fs::rename(tmpPath, outPath);
        }
    }

    void fileWorkerLoop() {
        while (running_) {
            scanFileSystem(path_);
            updateJSON();
            writeToFile();
        
            std::unique_lock<std::mutex> lock(stopMutex_);
            cv_.wait_for(lock, interval_, [this] { return !running_; });
        }
    }

    void httpWorkerLoop() {
        while (running_) {
            scanFileSystem(path_);
            updateJSON();
        
            std::unique_lock<std::mutex> lock(stopMutex_);
            cv_.wait_for(lock, interval_, [this] { return !running_; });
        }
    }

    void startWorker(void (Scan::*workerLoop)()) {
        running_ = true;
        worker_ = std::thread(workerLoop, this);
    }

    void waitForSignal() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};

static Scan* g_scanner = nullptr;

void signalHandler(int) {
    if (g_server) {
        g_server->stop();
    }
    if (g_scanner) {
        g_scanner->stopWorker();
    }
}