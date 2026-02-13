#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
// 로그 기록 및 시스템 정보 추출을 위한 클래스 정의
class Logger {
public:
    Logger(const std::string& baseName);
    void logStatus();
    void logLine(const std::string& line);
    int getPIDByName(const std::string& procName);
    double getProcessCPUUsage(int pid);
    long getProcessMemoryUsage(int pid);
    long long getProcessStartTime(int pid);
    
    // 시스템 정보 추출 함수
    double getCPUUsage();
    long getMemoryUsage(); // KB 단위
    std::string getCurrentTime();

private:
    std::string filename;

    std::string baseName;        // 예: "system_stats"
    std::string currentDate;     // 예: "2026-02-09"
    std::string currentFilename; // 예: "system_stats_2026-02-09.log"

    std::string getCurrentDate();      // YYYY-MM-DD
    void refreshLogFileIfNeeded();     // 날짜 바뀌면 파일명 갱신
    
};

#endif