#include "monitor.h"
#include <iostream>
#include <csignal>

static Monitor* g_mon = nullptr;

static void handleSignal(int) {
    if (g_mon) g_mon->stop();   // Ctrl+C 들어오면 동일한 stop 루틴 실행
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <pid> [logBaseName]\n";
        return 1;
    }

    int pid = 0;
    try {
        pid = std::stoi(argv[1]);
    } catch (...) {
        std::cerr << "Invalid PID: " << argv[1] << "\n";
        return 1;
    }

    std::string logBaseName = (argc >= 3) ? argv[2] : "system_log";

    Monitor mon(pid, logBaseName);
    g_mon = &mon;

    // Ctrl+C / kill(terminate) 처리
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    if (!mon.init()) {
        std::cerr << "Failed to init monitor. PID=" << pid << "\n";
        return 1;
    }

    mon.run();

    g_mon = nullptr;
    return 0;
}
