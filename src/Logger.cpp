#include "Logger.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <dirent.h> // 폴더를 뒤지기 위한 헤더
#include <unistd.h>

namespace fs = std::filesystem;

Logger::Logger(const std::string& b) : baseName(b) {
    currentDate = "";
    // 로그 폴더가 없으면 생성
    if (!fs::exists("logs")) {
        fs::create_directory("logs");
    }
    refreshLogFileIfNeeded();
}

std::string Logger::getCurrentTime() {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// CPU 사용량 추출 함수
double Logger::getCPUUsage() {
    std::ifstream file("/proc/stat");
    std::string line, cpu;
    long user, nice, system, idle, iowait, irq, softirq, steal;

    if (std::getline(file, line)) {
        std::stringstream ss(line);
        ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    }

    //전체 시간 중에서 노는 시간의 비율을 뺀 나머지를 사용량으로 계산
    long total = user + nice + system + idle + iowait + irq + softirq + steal;
    return 100.0 * (1.0 - (double)idle / total);
}

// 로그 파일에 상태 정보 기록
void Logger::logLine(const std::string& line) {
    refreshLogFileIfNeeded();
    std::ofstream file(currentFilename, std::ios::app);
    if (file.is_open()) {
        file << "[" << getCurrentTime() << "] " << line << "\n";
    }
}

std::string Logger::getCurrentDate() {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now), "%Y-%m-%d");
    return ss.str();
}
// 날짜가 바뀌었는지 확인하고, 바뀌었으면 파일명 갱신
void Logger::refreshLogFileIfNeeded() {
    std::string today = getCurrentDate();
    if (today != currentDate) {
        currentDate = today;
        // 경로를 logs/ 폴더 내부로 지정
        currentFilename = "logs/" + baseName + "_" + currentDate + ".log";
    }
}

// 메모리 사용량 추출 함수 (KB 단위)
long Logger::getMemoryUsage() {
    std::ifstream file("/proc/meminfo");
    std::string label;
    long memFree, memTotal;

    file >> label >> memTotal >> label; // MemTotal
    file >> label >> memFree >> label;  // MemFree
    
    return memTotal - memFree; // 사용 중인 메모리 (KB)
}

void Logger::logStatus() {
    refreshLogFileIfNeeded(); // 날짜 체크 추가
    std::ofstream outFile(currentFilename, std::ios::app); // filename -> currentFilename
    if (outFile.is_open()) {
        outFile << "[" << getCurrentTime() << "] "
                << "CPU: " << std::fixed << std::setprecision(2) << getCPUUsage() << "%, "
                << "RAM: " << getMemoryUsage() / 1024 << " MB" << std::endl;
    }
}

// 프로세스 이름으로 PID 찾기
int Logger::getPIDByName(const std::string& procName) {
    DIR* dir = opendir("/proc");
    if (!dir) return -1;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // 폴더 이름이 숫자인 것만 확인 (PID 폴더들)
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            std::string pid = entry->d_name;
            std::ifstream cmdline("/proc/" + pid + "/comm"); // 프로세스 이름이 적힌 파일
            std::string name;
            if (std::getline(cmdline, name)) {
                if (name == procName) {
                    closedir(dir);
                    return std::stoi(pid); // 찾았다!
                }
            }
        }
    }
    closedir(dir);
    return -1; // 못 찾음
}

double Logger::getProcessCPUUsage(int pid) {
    static long last_proc = 0;
    static auto last_time = std::chrono::steady_clock::now();

    // proc utime+stime (jiffies)
    std::ifstream pstat("/proc/" + std::to_string(pid) + "/stat");
    if (!pstat.is_open()) return 0.0;

    std::string tmp;
    long utime, stime;
    for (int i = 0; i < 13; ++i) pstat >> tmp;
    pstat >> utime >> stime;

    long proc = utime + stime;
    auto now = std::chrono::steady_clock::now();

    // first call: baseline
    if (last_proc == 0) {
        last_proc = proc;
        last_time = now;
        return 0.0;
    }

    long diff_proc = proc - last_proc;
    std::chrono::duration<double> diff_wall = now - last_time;

    last_proc = proc;
    last_time = now;

    long ticks = sysconf(_SC_CLK_TCK); // jiffies per second
    double diff_proc_sec = (double)diff_proc / (double)ticks;

    if (diff_wall.count() <= 0.0) return 0.0;

    // %CPU (top-style: 1 core full = ~100)
    return 100.0 * (diff_proc_sec / diff_wall.count());
}

long Logger::getProcessMemoryUsage(int pid) {
    std::ifstream file("/proc/" + std::to_string(pid) + "/status");
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("VmRSS:") != std::string::npos) {
            std::stringstream ss(line);
            std::string label;
            long mem;
            std::string unit;
            ss >> label >> mem >> unit;
            return mem;  // KB
        }
    }
    return 0;
}

long long Logger::getProcessStartTime(int pid) {
    std::ifstream pstat("/proc/" + std::to_string(pid) + "/stat");
    if (!pstat.is_open()) return -1;

    std::string line;
    std::getline(pstat, line);

    auto rparen = line.rfind(')');
    if (rparen == std::string::npos) return -1;

    std::stringstream ss(line.substr(rparen + 2));

    // starttime은 원래 stat의 22번째 필드
    // rparen 이후 substring에서는 20번째 토큰
    std::string tmp;
    long long starttime = 0;

    for (int i = 0; i < 19; ++i) ss >> tmp;
    ss >> starttime;

    return starttime;
}
