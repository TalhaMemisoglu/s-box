//// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <filesystem>
#include <functional>
#include <atomic>
#include <mutex>

enum class FileStatus {
    FirstTime,
    Modified,
    Deleted
};

class FileWatcher {
public:
    using Callback = std::function<void(const std::string& path, const FileStatus& status, const std::string& content)>;

    // Constructor and destructor
    FileWatcher();
    FileWatcher(const std::string& filepath);
    ~FileWatcher();

    // Core functionality
    void start(const Callback& callback);
    void stop();

private:
    std::string filepath_;
    std::chrono::duration<int, std::milli> delay_;
    std::filesystem::file_time_type last_write_time_;
    std::atomic<bool> running_;
    std::thread watcher_thread_;
    std::mutex cout_mutex_; // For thread-safe console output

    // Helper function to read file content
    std::string readFileContent(const std::string& filepath);
};