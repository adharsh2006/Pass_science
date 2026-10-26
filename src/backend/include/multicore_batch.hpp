#pragma once
#include "thermal_fea.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace workbench {

struct BatchWorkloadConfig {
    uint64_t seed{20260415ULL};
    int num_cases{2500};
    int mesh_elements{200};
    int repetitions{5};
    double k_min{5.0};
    double k_max{250.0};
    double q_min{5000.0};
    double q_max{50000.0};
};

struct SingleCaseSummary {
    int case_index{0};
    double k_W_mK{0.0};
    double q_W_m3{0.0};
    double midpoint_center_T_C{0.0};
    double max_nodal_T_C{0.0};
};

struct WorkerTimingRecord {
    int worker_count{1};
    std::vector<double> rep_times_ms;
    double warmup_time_ms{0.0};
    double median_time_ms{0.0};
    double mean_time_ms{0.0};
    double min_time_ms{0.0};
    double max_time_ms{0.0};
    double speedup_vs_serial{1.0};
    double efficiency{1.0};
    double max_abs_temp_diff_C{0.0};
    bool agreement_verified{false};
};

struct MulticoreBenchmarkReport {
    BatchWorkloadConfig config;
    std::string cpu_model;
    int physical_cores{1};
    int logical_cpus{1};
    double total_ram_gb{0.0};
    std::string compiler_version;
    std::string optimization_flags;
    double launch_to_ready_ms{0.0};
    std::vector<WorkerTimingRecord> timings;
};

class MulticoreBatchRunner {
public:
    static std::vector<ThermalProblemParams> generateBatch(const BatchWorkloadConfig& cfg);

    static std::vector<SingleCaseSummary> executeSerial(
        const std::vector<ThermalProblemParams>& cases);

    static std::vector<SingleCaseSummary> executeParallelPool(
        const std::vector<ThermalProblemParams>& cases,
        int num_workers);

    static MulticoreBenchmarkReport runBenchmarkSuite(const BatchWorkloadConfig& cfg);
};

} // namespace workbench
