#include "monitor.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

Monitor::Monitor(int pid, const std::string& logBaseName)
    : pid_(pid), logBaseName_(logBaseName), logger_(logBaseName) {
}

Monitor::~Monitor() {
    stop();
}

bool Monitor::init() {
    // 2) init() 실패 이유를 구체적으로 출력
    if (!fs::exists("/proc/" + std::to_string(pid_))) {
        std::cerr << "[Error] PID " << pid_ << " does not exist in /proc.\n";
        return false;
    }

    base_start_time_ = logger_.getProcessStartTime(pid_);
    if (base_start_time_ < 0) {
        std::cerr << "[Error] Failed to read start time for PID " << pid_ << ".\n";
        return false;
    }

    if (!gpu_.start(1000)) {
        std::cerr << "[Error] Failed to start GPU reader (tegrastats).\n";
        return false;
    }

    logger_.getProcessCPUUsage(pid_);
    return true;
}

void Monitor::run() {
    keep_running_ = true;
    std::cout << "Monitoring started for PID [" << pid_ << "]. Press Ctrl+C to stop.\n";

    while (keep_running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (!checkValidity()) {
            // 3) 종료 원인은 checkValidity 내부에서 상세히 출력함
            keep_running_ = false;
            break;
        }

        collectAndLog();
    }
    
    std::cout << "Monitoring loop finished.\n";
}

void Monitor::stop() {
    if (!keep_running_) return;
    keep_running_ = false;
    logger_.logLine("[PID:" + std::to_string(pid_) + "] Stopped by user (signal).");
    gpu_.stop();
}

bool Monitor::checkValidity() {
    // 프로세스 종료 확인
    if (!fs::exists("/proc/" + std::to_string(pid_))) {
        if (!termination_logged_) {
            logger_.logLine("[PID:" + std::to_string(pid_) + "] Process terminated.");
            termination_logged_ = true;
        }
        return false;
    }

    // PID 재사용 확인
    if (logger_.getProcessStartTime(pid_) != base_start_time_) {
        if (!reused_logged_) {
            logger_.logLine("[PID:" + std::to_string(pid_) + "] PID reused by another process.");
            reused_logged_ = true;
        }
        return false;
    }

    return true;
}

void Monitor::collectAndLog() {
    double cpu = logger_.getProcessCPUUsage(pid_);
    long procRam = logger_.getProcessMemoryUsage(pid_);
    long sysRam = logger_.getMemoryUsage();
    
    auto snap = gpu_.latest();
    int gpuUtil = snap ? snap->gr3d_util_pct : 0;

    std::stringstream ss;
    ss << "[PID:" << pid_ << "] "
       << "CPU: " << std::fixed << std::setprecision(2) << cpu << "%"
       << ", PROC_RAM: " << procRam / 1024 << " MB"
       << ", SYS_RAM: " << sysRam / 1024 << " MB"
       << ", GPU: " << gpuUtil << "%";

    logger_.logLine(ss.str());
}

