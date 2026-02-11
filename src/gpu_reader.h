#pragma once
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <cstdio>

struct TegrastatsSnapshot {
    int ram_used_mb = -1;
    int ram_total_mb = -1;
    int gr3d_util_pct = -1; // GPU util (%)
    std::string raw_line;
};

class GpuReader {
public:
    GpuReader() = default;
    ~GpuReader() { stop(); }

    // interval_ms: tegrastats 출력 주기(밀리초)
    bool start(int interval_ms = 1000);
    void stop();

    // 가장 최근 파싱된 값 반환 (없으면 nullopt)
    std::optional<TegrastatsSnapshot> latest();

private:
    void readerLoop();
    bool parseLine(const std::string& line, TegrastatsSnapshot& out);

    std::mutex mtx_;
    std::optional<TegrastatsSnapshot> latest_;
    std::atomic<bool> running_{false};

    FILE* pipe_ = nullptr;
    std::thread reader_thread_;

    int interval_ms_ = 1000;
};
