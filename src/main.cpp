#include "Logger.h"
#include "gpu_reader.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>

int main() {
    // 1. 객체 생성
    Logger myLogger("system_stats");
    GpuReader gpu;

    // 2. GPU 모니터링 시작
    // 터미널 출력 대신 내부적으로 시작 여부를 체크
    gpu.start(1000); 

    // 실행 시작 시점에만 딱 한 번 터미널에 알려줌
    std::cout << "Monitoring started in the background. (logs/ folder)" << std::endl;

    // 3. 메인 루프
    while (true) {
        double cpu = myLogger.getCPUUsage();
        long ram = myLogger.getMemoryUsage() / 1024; // MB 변환
        auto snap = gpu.latest();

        std::stringstream ss;
        ss << "CPU: " << std::fixed << std::setprecision(2) << cpu << "%, "
           << "RAM: " << ram << " MB";

        if (snap) {
            ss << ", GR3D: " << snap->gr3d_util_pct << "%, "
               << "SYS_RAM: " << snap->ram_used_mb << "/" << snap->ram_total_mb << " MB";
        } else {
            ss << ", GR3D: N/A";
        }

        // 파일에만 기록 (std::cout 출력 삭제)
        myLogger.logLine(ss.str());

        // 5초 대기
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}