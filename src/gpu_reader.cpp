#include "gpu_reader.h"

#include <regex>
#include <iostream>
#include <cstring>

bool GpuReader::start(int interval_ms) {
    if (running_.load()) return true;

    interval_ms_ = interval_ms;

    // tegrastats는 보통 root 권한에서 잘 동작
    std::string cmd = "tegrastats --interval " + std::to_string(interval_ms_);

    pipe_ = popen(cmd.c_str(), "r");
    if (!pipe_) {
        std::cerr << "Failed to start tegrastats via popen(): " << std::strerror(errno) << "\n";
        return false;
    }

    running_.store(true);
    //메인 프로그램이 멈추지 않도록 별도 스레드에서 읽기
    reader_thread_ = std::thread(&GpuReader::readerLoop, this);
    return true;
}

void GpuReader::stop() {
    if (!running_.exchange(false)) return;

    // popen은 blocking read라서, pipe를 닫아줘야 thread가 끝나기 쉬움
    if (pipe_) {
        pclose(pipe_);
        pipe_ = nullptr;
    }

    //스레드 종료 대기
    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }
}

std::optional<TegrastatsSnapshot> GpuReader::latest() {
    std::lock_guard<std::mutex> lock(mtx_);
    return latest_;
}
// 프로그램이 끝날 때까지 무한 반복
void GpuReader::readerLoop() {
    // tegrastats 한 줄이 길어서 충분히 큰 버퍼 필요
    char buf[4096];

    while (running_.load()) {
        if (!pipe_) break;

        if (!fgets(buf, sizeof(buf), pipe_)) {
            // EOF or error: tegrastats가 종료됐거나 파이프 문제
            break;
        }

        std::string line(buf);

        TegrastatsSnapshot snap;
        snap.raw_line = line;
        //데이터가 섞이지 않도록 파싱 후 갱신
        if (parseLine(line, snap)) {
            std::lock_guard<std::mutex> lock(mtx_);
            latest_ = snap;
        }
    }

    // 여기 도달하면 tegrastats가 끊긴 상태
    running_.store(false);
}

bool GpuReader::parseLine(const std::string& line, TegrastatsSnapshot& out) {
    // RAM 3099/7471MB ... GR3D_FREQ 0%@[764,0] ...
    static const std::regex ram_re(R"(RAM\s+(\d+)\/(\d+)MB)");
    static const std::regex gr3d_re(R"(GR3D_FREQ\s+(\d+)%\@)");

    std::smatch m;

    bool ok = false;

    if (std::regex_search(line, m, ram_re) && m.size() >= 3) {
        out.ram_used_mb = std::stoi(m[1].str());
        out.ram_total_mb = std::stoi(m[2].str());
        ok = true;
    }

    if (std::regex_search(line, m, gr3d_re) && m.size() >= 2) {
        out.gr3d_util_pct = std::stoi(m[1].str());
        ok = true;
    }

    return ok;
}
