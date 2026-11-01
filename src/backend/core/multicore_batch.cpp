#include "multicore_batch.hpp"
#include <random>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace workbench {

namespace {

// Bounded worker pool with pre-spawned threads and atomic chunk index
class BoundedWorkerPool {
public:
    explicit BoundedWorkerPool(int num_workers)
        : stop_(false), active_tasks_(0), num_workers_(std::max(1, num_workers)) {
        workers_.reserve(num_workers_);
        for (int i = 0; i < num_workers_; ++i) {
            workers_.emplace_back([this]() { this->workerLoop(); });
        }
    }

    ~BoundedWorkerPool() {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            stop_ = true;
        }
        cv_task_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
    }

    void executeBatch(const std::vector<ThermalProblemParams>& cases,
                      std::vector<SingleCaseSummary>& out_results) {
        const std::size_t n = cases.size();
        out_results.resize(n);
        if (n == 0) return;

        {
            std::unique_lock<std::mutex> lock(mtx_);
            cases_ptr_ = &cases;
            results_ptr_ = &out_results;
            next_idx_.store(0, std::memory_order_relaxed);
            active_tasks_ = num_workers_;
            ++generation_;
        }
        cv_task_.notify_all();

        std::unique_lock<std::mutex> lock(mtx_);
        cv_done_.wait(lock, [this]() { return active_tasks_ == 0; });
    }

private:
    void workerLoop() {
        uint64_t seen_gen = 0;
        while (true) {
            const std::vector<ThermalProblemParams>* cases = nullptr;
            std::vector<SingleCaseSummary>* results = nullptr;
            {
                std::unique_lock<std::mutex> lock(mtx_);
                cv_task_.wait(lock, [this, &seen_gen]() {
                    return stop_ || generation_ > seen_gen;
                });
                if (stop_) return;
                seen_gen = generation_;
                cases = cases_ptr_;
                results = results_ptr_;
            }

            const std::size_t total = cases->size();
            constexpr std::size_t CHUNK_SIZE = 16;
            while (true) {
                std::size_t start = next_idx_.fetch_add(CHUNK_SIZE, std::memory_order_relaxed);
                if (start >= total) break;
                std::size_t end = std::min(total, start + CHUNK_SIZE);
                for (std::size_t idx = start; idx < end; ++idx) {
                    ThermalSolveResult sol = ThermalFeaSolver1D::solve((*cases)[idx]);
                    SingleCaseSummary sum{};
                    sum.case_index = static_cast<int>(idx);
                    sum.k_W_mK = (*cases)[idx].k_W_mK;
                    sum.q_W_m3 = (*cases)[idx].q_W_m3;
                    sum.midpoint_center_T_C = sol.midpoints[sol.midpoints.size() / 2].T_fe_mid_C;
                    double max_t = sol.node_T_fe_C[0];
                    for (double v : sol.node_T_fe_C) max_t = std::max(max_t, v);
                    sum.max_nodal_T_C = max_t;
                    // Deterministic output ordering at pre-allocated slot idx
                    (*results)[idx] = sum;
                }
            }

            {
                std::unique_lock<std::mutex> lock(mtx_);
                if (--active_tasks_ == 0) {
                    cv_done_.notify_one();
                }
            }
        }
    }

    std::vector<std::thread> workers_;
    std::mutex mtx_;
    std::condition_variable cv_task_;
    std::condition_variable cv_done_;
    bool stop_;
    int active_tasks_;
    int num_workers_;
    uint64_t generation_{0};
    std::atomic<std::size_t> next_idx_{0};
    const std::vector<ThermalProblemParams>* cases_ptr_{nullptr};
    std::vector<SingleCaseSummary>* results_ptr_{nullptr};
};

void detectSystemHardware(std::string& cpu_model, int& phys_cores, int& logical_cpus, double& ram_gb) {
    logical_cpus = static_cast<int>(std:: max(1u, std::thread::hardware_concurrency()));
    phys_cores = logical_cpus;
    cpu_model = "x86_64 Processor";
    ram_gb = 8.0;

#if defined(_WIN32)
    // Windows: use GetSystemInfo for logical CPUs and GlobalMemoryStatusEx for RAM
    // CPU model is read from registry HKLM\HARDWARE\DESCRIPTION\System\CentralProcessor\0
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buf[256] = {};
        DWORD len = sizeof(buf);
        if (RegQueryValueExA(hKey, "ProcessorNameString", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(buf), &len) == ERROR_SUCCESS) {
            cpu_model = std::string(buf);
            // trim leading spaces
            std::size_t p = 0;
            while (p < cpu_model.size() && cpu_model[p] == ' ') ++p;
            if (p) cpu_model = cpu_model.substr(p);
        }
        RegCloseKey(hKey);
    }
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        ram_gb = static_cast<double>(ms.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
    }
    // Physical cores: sum across processor groups via GetLogicalProcessorInformation
    DWORD bufLen = 0;
    GetLogicalProcessorInformation(nullptr, &bufLen);
    if (bufLen > 0) {
        std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> info(bufLen / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        if (GetLogicalProcessorInformation(info.data(), &bufLen)) {
            int phys = 0;
            for (const auto& entry : info) {
                if (entry.Relationship == RelationProcessorCore) ++phys;
            }
            if (phys > 0) phys_cores = phys;
        }
    }
#else
    // Linux: read /proc/cpuinfo and /proc/meminfo
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        int cores_found = 0;
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos) {
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    std::string val = line.substr(pos + 1);
                    while (!val.empty() && val.front() == ' ') val.erase(val.begin());
                    if (!val.empty() && val != "unknown") cpu_model = val;
                }
            } else if (line.find("cpu cores") != std::string::npos) {
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    cores_found = std::atoi(line.substr(pos + 1).c_str());
                }
            }
        }
        if (cores_found > 0) phys_cores = cores_found;
    }

    std::ifstream meminfo("/proc/meminfo");
    if (meminfo.is_open()) {
        std::string key;
        double kb = 0.0;
        std::string unit;
        if (meminfo >> key >> kb >> unit) {
            ram_gb = kb / (1024.0 * 1024.0);
        }
    }
#endif
}

} // namespace

std::vector<ThermalProblemParams> MulticoreBatchRunner::generateBatch(const BatchWorkloadConfig& cfg) {
    std::mt19937_64 rng(cfg.seed);
    std::uniform_real_distribution<double> dist_k(cfg.k_min, cfg.k_max);
    std::uniform_real_distribution<double> dist_q(cfg.q_min, cfg.q_max);

    std::vector<ThermalProblemParams> batch(cfg.num_cases);
    for (int i = 0; i < cfg.num_cases; ++i) {
        ThermalProblemParams p;
        p.length_m = 1.0;
        p.area_m2 = 1.0e-4;
        p.k_W_mK = dist_k(rng);
        p.q_W_m3 = dist_q(rng);
        p.T0_C = 100.0;
        p.TL_C = 20.0;
        p.num_elements = cfg.mesh_elements;
        batch[i] = p;
    }
    return batch;
}

std::vector<SingleCaseSummary> MulticoreBatchRunner::executeSerial(
    const std::vector<ThermalProblemParams>& cases) {
    std::vector<SingleCaseSummary> results(cases.size());
    for (std::size_t idx = 0; idx < cases.size(); ++idx) {
        ThermalSolveResult sol = ThermalFeaSolver1D::solve(cases[idx]);
        SingleCaseSummary sum{};
        sum.case_index = static_cast<int>(idx);
        sum.k_W_mK = cases[idx].k_W_mK;
        sum.q_W_m3 = cases[idx].q_W_m3;
        sum.midpoint_center_T_C = sol.midpoints[sol.midpoints.size() / 2].T_fe_mid_C;
        double max_t = sol.node_T_fe_C[0];
        for (double v : sol.node_T_fe_C) max_t = std::max(max_t, v);
        sum.max_nodal_T_C = max_t;
        results[idx] = sum;
    }
    return results;
}

std::vector<SingleCaseSummary> MulticoreBatchRunner::executeParallelPool(
    const std::vector<ThermalProblemParams>& cases,
    int num_workers) {
    BoundedWorkerPool pool(num_workers);
    std::vector<SingleCaseSummary> results;
    pool.executeBatch(cases, results);
    return results;
}

MulticoreBenchmarkReport MulticoreBatchRunner::runBenchmarkSuite(const BatchWorkloadConfig& cfg) {
    auto t_init_start = std::chrono::steady_clock::now();
    MulticoreBenchmarkReport report;
    report.config = cfg;
    detectSystemHardware(report.cpu_model, report.physical_cores, report.logical_cpus, report.total_ram_gb);

#if defined(__GNUC__)
    report.compiler_version = "GCC/G++ " + std::to_string(__GNUC__) + "." +
                              std::to_string(__GNUC_MINOR__) + "." +
                              std::to_string(__GNUC_PATCHLEVEL__);
#else
    report.compiler_version = "C++17 Standard Compiler";
#endif
    report.optimization_flags = "-std=c++17 -O3 -pthread";

    // Generate batch outside timed compute section
    std::vector<ThermalProblemParams> cases = generateBatch(cfg);
    auto t_init_end = std::chrono::steady_clock::now();
    report.launch_to_ready_ms = std::chrono::duration<double, std::milli>(t_init_end - t_init_start).count();

    // Baseline serial reference for agreement verification
    std::vector<SingleCaseSummary> serial_ref;

    // Evaluate worker counts: 1, 2, and 4 (noting logical CPU cap in report)
    std::vector<int> worker_counts = {1, 2, 4};
    double serial_median_ms = 0.0;

    for (int p : worker_counts) {
        WorkerTimingRecord rec;
        rec.worker_count = p;

        if (p == 1) {
            // Warm-up run
            auto w0 = std::chrono::steady_clock::now();
            serial_ref = executeSerial(cases);
            auto w1 = std::chrono::steady_clock::now();
            rec.warmup_time_ms = std::chrono::duration<double, std::milli>(w1 - w0).count();

            for (int r = 0; r < cfg.repetitions; ++r) {
                auto t0 = std::chrono::steady_clock::now();
                auto out = executeSerial(cases);
                auto t1 = std::chrono::steady_clock::now();
                rec.rep_times_ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
            }
            rec.max_abs_temp_diff_C = 0.0;
            rec.agreement_verified = true;
        } else {
            BoundedWorkerPool pool(p);
            std::vector<SingleCaseSummary> par_out;

            // Warm-up run
            auto w0 = std::chrono::steady_clock::now();
            pool.executeBatch(cases, par_out);
            auto w1 = std::chrono::steady_clock::now();
            rec.warmup_time_ms = std::chrono::duration<double, std::milli>(w1 - w0).count();

            for (int r = 0; r < cfg.repetitions; ++r) {
                auto t0 = std::chrono::steady_clock::now();
                pool.executeBatch(cases, par_out);
                auto t1 = std::chrono::steady_clock::now();
                rec.rep_times_ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
            }

            // Verify agreement against serial_ref across all cases
            double max_diff = 0.0;
            for (std::size_t idx = 0; idx < cases.size(); ++idx) {
                double d1 = std::abs(par_out[idx].max_nodal_T_C - serial_ref[idx].max_nodal_T_C);
                double d2 = std::abs(par_out[idx].midpoint_center_T_C - serial_ref[idx].midpoint_center_T_C);
                max_diff = std::max({max_diff, d1, d2});
            }
            rec.max_abs_temp_diff_C = max_diff;
            rec.agreement_verified = (max_diff <= 1.0e-8);
        }

        std::vector<double> sorted_times = rec.rep_times_ms;
        std::sort(sorted_times.begin(), sorted_times.end());
        rec.median_time_ms = sorted_times[sorted_times.size() / 2];
        rec.min_time_ms = sorted_times.front();
        rec.max_time_ms = sorted_times.back();
        double sum = std::accumulate(sorted_times.begin(), sorted_times.end(), 0.0);
        rec.mean_time_ms = sum / static_cast<double>(sorted_times.size());

        if (p == 1) {
            serial_median_ms = rec.median_time_ms;
            rec.speedup_vs_serial = 1.0;
            rec.efficiency = 1.0;
        } else {
            rec.speedup_vs_serial = serial_median_ms / rec.median_time_ms;
            rec.efficiency = rec.speedup_vs_serial / static_cast<double>(p);
        }

        report.timings.push_back(rec);
    }

    return report;
}

} // namespace workbench
