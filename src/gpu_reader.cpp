#include "gpu_reader.h"

#include <regex>
#include <iostream>
#include <cstring>

bool GpuReader::start(int interval_ms) {
    if (running_.load()) return true;

    interval_ms_ = interval_ms;
    // tegrastats 실행 시 불필요한 출력을 줄이고 인터벌을 정확히 명시
    std::string cmd = "tegrastats --interval " + std::to_string(interval_ms_);

    pipe_ = popen(cmd.c_str(), "r");
    if (!pipe_) {
        return false;
    }

    running_.store(true);
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
            if(running_.load()) {
                std::cerr << "[Warning] Failed to read from tegrastats pipe."<<std::endl;
            }
            break; // 읽기 실패 (예: tegrastats 종료)
        }
    

        std::string line(buf);

        TegrastatsSnapshot snap;
        snap.raw_line = line;
        //데이터가 섞이지 않도록 파싱 후 갱신
        if (parseLine(line, snap)) {
            std::lock_guard<std::mutex> lock(mtx_);
            latest_ = snap;
        }
        else {
            std::cerr << "[Warning] Failed to parse tegrastats line: " << line << std::endl;
        }
    }
    // 여기 도달하면 tegrastats가 끊긴 상태
    running_.store(false);
}

bool GpuReader::parseLine(const std::string& line, TegrastatsSnapshot& out) {
    // RAM,GR3D_FREQ 정보을 정규식으로 추출
    static const std::regex ram_re(R"(RAM\s+(\d+)\/(\d+)MB)");
    static const std::regex gr3d_re(R"(GR3D_FREQ\s+(\d+)%)");

    std::smatch m;
    //ram_ok, gpu_ok 플래그로 각각의 정보가 성공적으로 추출됐는지 추적
    bool ram_ok = false;
    bool gpu_ok = false;

    // RAM 정보 추출
    if (std::regex_search(line, m, ram_re) && m.size() >= 3) {
        out.ram_used_mb = std::stoi(m[1].str());
        out.ram_total_mb = std::stoi(m[2].str());
        ram_ok = true;
    }
    else {
        std::cerr << "[Warning] Failed to parse RAM info from line: " << line << "\n";
    }

    // GPU 정보 추출 (GR3D_FREQ 뒤의 숫자만 깔끔하게 가져옴)
    if (std::regex_search(line, m, gr3d_re) && m.size() >= 2) {
        out.gr3d_util_pct = std::stoi(m[1].str());
        gpu_ok = true;
    }
    else {
        std::cerr << "[Warning] Failed to parse GR3D_FREQ info from line: " << line << "\n";
    }

    // 둘 중 하나라도 성공하면 데이터가 업데이트된 것으로 간주
    return ram_ok || gpu_ok;
}
