#include "monitor.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <vector>
#include <algorithm>
#include <vector>
#include <string>
// PID 검색을 위한 리눅스 시스템 헤더
#include <dirent.h>
#include <unistd.h>


namespace fs = std::filesystem;

Monitor::Monitor(int pid, const std::string& logBaseName)
    : pid_(pid), logBaseName_(logBaseName), logger_(logBaseName) {
}

Monitor::~Monitor() {
    stop();
}

static bool isDigits(const char* s) {
    for (; *s; ++s) if (*s < '0' || *s > '9') return false;
    return true;
}

static std::string readFirstLine(const std::string& path) {
    std::ifstream f(path);
    std::string line;
    std::getline(f, line);
    return line;
}

static std::vector<int> findPidsByName(const std::string& targetName) {
    std::vector<int> hits;
    DIR* dir = opendir("/proc");
    if (!dir) return hits;

    dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type != DT_DIR) continue;
        if (!isDigits(ent->d_name)) continue;

        int pid = std::stoi(ent->d_name);
        std::string comm = readFirstLine("/proc/" + std::to_string(pid) + "/comm");
        if (!comm.empty() && comm.back() == '\n') comm.pop_back();

        if (comm == targetName) hits.push_back(pid);
    }
    closedir(dir);
    return hits;
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

// Monitor.cpp 내부 수정 제안
bool Monitor::initByName(const std::string& targetName) {
    auto pids = findPidsByName(targetName);

    if (pids.empty()) {
        std::cerr << "[Error] '" << targetName << "' process not found.\n";
        return false;
    }
    if (pids.size() > 1) {
        std::cerr << "[Error] Duplicate processes for '" << targetName << "'. PIDs: ";
        for (auto p : pids) std::cerr << p << " ";
        std::cerr << "\n";
        return false;
    }

    pid_ = pids[0];
    return init();
}

void Monitor::run() {
    keep_running_ = true;
    std::cout << "Monitoring started for PID [" << pid_ << "]. Press Ctrl+C to stop.\n";

    while (keep_running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
         if (!keep_running_) break; 

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
    
    // 1) 시스템 전체 GPU 점유율 (기존)
    auto snap = gpu_.latest();
    int totalGpuUtil = snap ? snap->gr3d_util_pct : 0;

    // 2) 특정 프로세스의 GPU 메모리 점유량 
    // 보통 /proc/[pid]/maps나 nvmap 정보를 통해 추출
    long procGpuMemKb = getSpecificProcessGpuMem(pid_);
    double procGpuMemMB = procGpuMemKb / 1024.0;

    std::stringstream ss;
    ss << "[PID:" << pid_ << "] "
        << "CPU: " << std::fixed << std::setprecision(2) << cpu << "%"
        << ", RAM: " << procRam / 1024 << " MB"
        << ", SYS_RAM: " << sysRam / 1024 << " MB"
        << ", GPU_MEM: " << std::fixed << std::setprecision(1) << procGpuMemMB << " MB"
        << ", SYS_GPU: " << totalGpuUtil << "%";


    logger_.logLine(ss.str());
}

// 특정 프로세스의 GPU 사용 메모리를 가져오는 헬퍼 함수 
long Monitor::getSpecificProcessGpuMem(int pid) {
    const std::string path = "/sys/kernel/debug/nvmap/iovmm/clients";
    std::ifstream file(path);

    if (!file.is_open()) {
        static bool warned = false;
        if (!warned) {
            std::cerr << "[Warning] Cannot read " << path
                      << " (need sudo/root). GPU_MEM will be 0.\n";
            warned = true;
        }
        return 0;
    }

    std::string line;

    // 첫 줄(헤더) 스킵
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.rfind("total", 0) == 0) continue; // "total" 라인 무시

        std::stringstream ss(line);
        std::string client, process;
        int linePid = -1;
        std::string sizeTok;

        // 각 줄은: CLIENT PROCESS PID SIZE
        // 예: user gnome-shell 1625 5384K
        if (!(ss >> client >> process >> linePid >> sizeTok)) continue;

        if (linePid != pid) continue;

        // SIZE 토큰 예: "5384K" 또는 "0K"
        long sizeKB = 0;
        // 숫자만 추출
        std::string digits;
        for (char c : sizeTok) if (c >= '0' && c <= '9') digits.push_back(c);
        if (!digits.empty()) {
            sizeKB = std::stol(digits);
        }
        return sizeKB; // KB
    }

    return 0;
}
