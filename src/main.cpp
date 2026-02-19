#include "monitor.h"
#include <iostream>
#include <csignal>

static Monitor* g_mon = nullptr;

static void handleSignal(int) {
    if (g_mon) g_mon->stop();   // Ctrl+C 들어오면 동일한 stop 루틴 실행
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <process_name> [logBaseName]\n";
        return 1;
    }

    std::string procName = argv[1];
    std::string logBaseName = (argc >= 3) ? argv[2] : procName;

    // pid는 더 이상 main에서 받지 않고, Monitor가 이름으로 찾게
    Monitor mon(/*pid*/ -1, logBaseName);
    g_mon = &mon;

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    if (!mon.initByName(procName)) {
        std::cerr << "Failed to init monitor by name: " << procName << "\n";
        return 1;
    }

    mon.run();

    g_mon = nullptr;
    return 0;
}