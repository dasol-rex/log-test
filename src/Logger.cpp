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