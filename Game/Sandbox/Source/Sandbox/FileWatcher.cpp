#include "FileWatcher.h"

#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <filesystem>
#include <functional>
#include <ctime>
#include <fstream>
#include <sstream>
#include <atomic>
#include <mutex>

using namespace std;

FileWatcher::FileWatcher()
    : filepath_(""), running_(false) {
}

    FileWatcher::FileWatcher(const string& filepath)
        : filepath_(filepath), running_(false) {

        const std::chrono::milliseconds delay(1000);

        delay_ = delay;
        // Check if the file exists and is a regular file
        if (std::filesystem::exists(filepath_) && std::filesystem::is_regular_file(filepath_)) {
            lock_guard<mutex> lock(cout_mutex_);
            // cout << "Initializing file watcher for file: " << filepath_ << " with delay: " << delay_.count() << "ms" << endl;
            // cout << "File found: " << filepath_ << endl;
            last_write_time_ = std::filesystem::last_write_time(filepath_);
        }
        else {
            lock_guard<mutex> lock(cout_mutex_);
            //cout << "File not found or is not a regular file: " << filepath_ << endl;
            //cout << "Exiting..." << endl;
            //exit(0);
        }
    }

    FileWatcher::~FileWatcher() {
        UE_LOG(LogTemp, Display, TEXT("Destructor called"));
        stop();
    }


    void FileWatcher::stop() {
        running_ = false;
        if (watcher_thread_.joinable()) {
            watcher_thread_.join();
        }
    }

    void FileWatcher::start(const Callback& callback) {
        if (running_) {
            return;
        }
        
        running_ = true;

        // Start monitoring in a separate thread
        watcher_thread_ = thread([this, callback]() {

            UE_LOG(LogTemp, Display, TEXT("Thread Began"));
            bool file_existed = std::filesystem::exists(filepath_);

            //Initial read if file exists /* TODO: Anlamsız ama başarılı */
            if (file_existed) {
                this_thread::sleep_for(delay_);
                callback(filepath_, FileStatus::FirstTime, "");
            }

            while (running_) {
                UE_LOG(LogTemp, Display, TEXT("Tread Loop"));

                // Check if file exists
                bool exists_now = std::filesystem::exists(filepath_);

                // File was deleted
                if (file_existed && !exists_now) {
                    callback(filepath_, FileStatus::Deleted, "");
                    file_existed = false;
                    last_write_time_ = std::filesystem::file_time_type::min();
                }
                else if (exists_now && std::filesystem::is_regular_file(filepath_)) {
                    auto current_write_time = std::filesystem::last_write_time(filepath_);

                    // If the last write time is different, file was modified
                    if (last_write_time_ != current_write_time) {
                        last_write_time_ = current_write_time;
                        //string content = readFileContent(filepath_);
                        // callback(filepath_, FileStatus::Modified, content);
                        callback(filepath_, FileStatus::Modified, "");
                    }
                }

                // Sleep for specified delay
                this_thread::sleep_for(delay_);
            }
            });
    }
