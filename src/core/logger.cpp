/**
 * @file logger.cpp
 * @brief Logger 类实现
 */

#include "logger.h"
#include <iostream>

namespace deb2pkg {

bool Logger::verbose_ = false;
std::mutex Logger::mutex_;

void Logger::Info(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << msg << std::endl;
}

void Logger::Success(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "✓ " << msg << std::endl;
}

void Logger::Warning(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "⚠ 警告：" << msg << std::endl;
}

void Logger::Error(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << "✗ 错误：" << msg << std::endl;
}

void Logger::Error(const std::string& msg, const std::string& solution) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << "\n✗ 错误：" << msg << std::endl;
    std::cerr << "  解决办法：" << solution << std::endl;
}

void Logger::Progress(const std::string& step, int current, int total) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "  [" << current << "/" << total << "] " << step << "..." << std::flush;
}

void Logger::Verbose(const std::string& msg) {
    if (verbose_) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "  [调试] " << msg << std::endl;
    }
}

void Logger::Separator() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "----------------------------------------" << std::endl;
}

} // namespace deb2pkg
