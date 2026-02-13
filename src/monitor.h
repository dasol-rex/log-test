#ifndef MONITOR_H
#define MONITOR_H

#include "Logger.h"
#include "gpu_reader.h"
#include <string>
#include <atomic>

class Monitor {
public:
    Monitor(int pid, const std::string& logBaseName);
    ~Monitor();

    bool init();
    void run();
    void stop();

    bool termination_logged_ = false;
    bool reused_logged_ = false;

private:
    bool checkValidity();
    void collectAndLog();

    int pid_;
    long long base_start_time_;
    std::string logBaseName_;
    
    Logger logger_;
    GpuReader gpu_;
    
    // 1) 스레드 안전을 위한 atomic 변수 사용
    std::atomic<bool> keep_running_{false};
};

#endif