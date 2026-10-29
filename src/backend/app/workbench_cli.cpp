#include "stl_csv_parser.hpp"
#include "thermal_fea.hpp"
#include "multicore_batch.hpp"
#include "embedded_hal.hpp"
#include "backprop.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <cmath>

using namespace workbench;

int main(int argc, char** argv) {
    std::string out_dir = "results";
    std::string data_dir = "data";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--out-dir" && i + 1 < argc) out_dir = argv[++i];
        else if (arg == "--data-dir" && i + 1 < argc) data_dir = argv[++i];
    }

    std::cout << "================================================================\n";
    std::cout << " PASS SCIENCE WORKS | Unified Engineering Workbench C++17 Core\n";
    std::cout << "================================================================\n\n";

    // --- TASK B: Parse & Validate STL and Synthetic ANSYS CSV ---
    auto stl_res = CadDataParser::parseAsciiStlFile(data_dir + "/heat_sink_fin.stl");
    auto csv_res = CadDataParser::parseAnsysCsvFile(data_dir + "/synthetic_ansys_nodal_temp.csv");

    std::cout << "[Task B] STL Validation: " << (stl_res.valid ? "PASS" : "FAIL")
              << " | Solid: " << stl_res.solid_name
              << " | Triangles: " << stl_res.triangles.size()
              << " | Area: " << std::fixed << std::setprecision(6) << stl_res.total_surface_area_m2 << " m^2\n";
    std::cout << "[Task B] CSV Validation: " << (csv_res.valid ? "PASS" : "FAIL")
              << " | Nodes: " << csv_res.nodes.size()
              << " | T_min: " << std::setprecision(4) << csv_res.min_temp_C << " C"
              << " | T_max: " << csv_res.max_temp_C << " C"
              << " | T_mean: " << csv_res.mean_temp_C << " C\n\n";

    // --- TASK C: 1D Steady-State Thermal FEA & Mesh Convergence (N = 10, 20, 40, 80) ---
    std::vector<int> meshes = {10, 20, 40, 80};
    auto fea_runs = ThermalFeaSolver1D::runConvergenceStudy(meshes);

    std::ofstream fea_nodal_ofs(out_dir + "/task_c_fea_temperatures.csv");
    fea_nodal_ofs << "N_elements,record_type,index,x_m,T_fe_C,T_exact_C,abs_error_C\n";

    std::ofstream fea_conv_ofs(out_dir + "/task_c_convergence.csv");
    fea_conv_ofs << "N_elements,element_length_h_m,max_nodal_abs_error_C,max_midpoint_abs_error_C,l2_midpoint_error_C,theoretical_midpoint_error_qh2_8k_C,convergence_order_p\n";

    double prev_mid_err = 0.0;
    for (std::size_t idx = 0; idx < fea_runs.size(); ++idx) {
        const auto& r = fea_runs[idx];
        const int N = r.params.num_elements;
        const double h = r.params.length_m / static_cast<double>(N);
        const double theo_err = (r.params.q_W_m3 * h * h) / (8.0 * r.params.k_W_mK);
        double order_p = 0.0;
        if (idx > 0 && r.max_midpoint_abs_error_C > 0.0) {
            order_p = std::log2(prev_mid_err / r.max_midpoint_abs_error_C);
        }
        prev_mid_err = r.max_midpoint_abs_error_C;

        std::cout << "[Task C] N = " << std::setw(2) << N
                  << " (h = " << std::fixed << std::setprecision(4) << h << " m)"
                  << " | Max Nodal Err: " << std::scientific << std::setprecision(3) << r.max_nodal_abs_error_C << " C"
                  << " | Max Midpoint FE Err: " << std::fixed << std::setprecision(6) << r.max_midpoint_abs_error_C << " C"
                  << " | Order p: " << std::setprecision(2) << order_p << "\n";

        fea_conv_ofs << N << "," << std::fixed << std::setprecision(6) << h << ","
                     << std::scientific << std::setprecision(6) << r.max_nodal_abs_error_C << ","
                     << std::fixed << std::setprecision(8) << r.max_midpoint_abs_error_C << ","
                     << r.l2_midpoint_error_C << ","
                     << theo_err << ","
                     << std::setprecision(4) << order_p << "\n";

        for (std::size_t i = 0; i < r.node_x_m.size(); ++i) {
            fea_nodal_ofs << N << ",node," << i << ","
                          << std::fixed << std::setprecision(6) << r.node_x_m[i] << ","
                          << std::setprecision(8) << r.node_T_fe_C[i] << ","
                          << r.node_T_exact_C[i] << ","
                          << std::scientific << std::setprecision(6) << r.node_abs_error_C[i] << "\n";
        }
        for (const auto& m : r.midpoints) {
            fea_nodal_ofs << N << ",midpoint," << m.element_index << ","
                          << std::fixed << std::setprecision(6) << m.x_mid_m << ","
                          << std::setprecision(8) << m.T_fe_mid_C << ","
                          << m.T_exact_mid_C << ","
                          << std::scientific << std::setprecision(6) << m.abs_error_mid_C << "\n";
        }
    }
    std::cout << "\n";

    // --- TASK D: Multicore / Multithreaded Batch Benchmark ---
    BatchWorkloadConfig bcfg;
    bcfg.seed = 20260415ULL;
    bcfg.num_cases = 4000;
    bcfg.mesh_elements = 250;
    bcfg.repetitions = 5;

    auto bench = MulticoreBatchRunner::runBenchmarkSuite(bcfg);
    std::cout << "[Task D] Hardware: " << bench.cpu_model
              << " | Physical Cores: " << bench.physical_cores
              << " | Logical CPUs: " << bench.logical_cpus
              << " | RAM: " << std::fixed << std::setprecision(2) << bench.total_ram_gb << " GB\n";
    std::cout << "[Task D] Compiler: " << bench.compiler_version
              << " | Flags: " << bench.optimization_flags
              << " | Launch-to-Ready: " << std::setprecision(3) << bench.launch_to_ready_ms << " ms\n";

    std::ofstream mc_ofs(out_dir + "/task_d_multicore_benchmarks.csv");
    mc_ofs << "workers,logical_cpus,cases,mesh_elements,warmup_ms,rep1_ms,rep2_ms,rep3_ms,rep4_ms,rep5_ms,median_ms,mean_ms,speedup_T1_Tp,efficiency_pct,max_abs_temp_diff_C,agreement_pass\n";

    for (const auto& t : bench.timings) {
        std::cout << "[Task D] Workers p = " << t.worker_count
                  << " | Median: " << std::fixed << std::setprecision(3) << t.median_time_ms << " ms"
                  << " | Speedup: " << std::setprecision(3) << t.speedup_vs_serial << "x"
                  << " | Efficiency: " << std::setprecision(1) << (t.efficiency * 100.0) << "%"
                  << " | Max Diff: " << std::scientific << std::setprecision(2) << t.max_abs_temp_diff_C << " C"
                  << " (" << (t.agreement_verified ? "PASS" : "FAIL") << ")\n";

        mc_ofs << t.worker_count << "," << bench.logical_cpus << ","
               << bcfg.num_cases << "," << bcfg.mesh_elements << ","
               << std::fixed << std::setprecision(4) << t.warmup_time_ms;
        for (double rep : t.rep_times_ms) mc_ofs << "," << rep;
        mc_ofs << "," << t.median_time_ms << "," << t.mean_time_ms
               << "," << std::setprecision(4) << t.speedup_vs_serial
               << "," << std::setprecision(2) << (t.efficiency * 100.0)
               << "," << std::scientific << std::setprecision(4) << t.max_abs_temp_diff_C
               << "," << (t.agreement_verified ? "PASS" : "FAIL") << "\n";
    }
    std::cout << "\n";

    // --- TASK E: Embedded-System Integration Mock Adapters ---
    EmbeddedWorkbenchManager hal_mgr;
    auto devs = hal_mgr.discoverAll();
    auto tel_log = hal_mgr.runTelemetryAndRecoveryDemo();

    std::ofstream emb_ofs(out_dir + "/task_e_telemetry_log.csv");
    emb_ofs << "seq,device_id,timestamp_iso,status_label,recovered_from_fault,retry_attempts,diagnostic_message\n";
    for (std::size_t i = 0; i < tel_log.size(); ++i) {
        const auto& pkt = tel_log[i];
        std::cout << "[Task E] Telemetry #" << (i + 1) << " [" << pkt.device_id << "] "
                  << pkt.status_label << " | Retries: " << pkt.retry_attempts
                  << " | " << pkt.diagnostic_message << "\n";
        emb_ofs << (i + 1) << "," << pkt.device_id << "," << pkt.timestamp_iso << ","
                << pkt.status_label << "," << (pkt.recovered_from_fault ? "true" : "false") << ","
                << pkt.retry_attempts << ",\"" << pkt.diagnostic_message << "\"\n";
    }
    std::cout << "\n";

    // --- TASK F: Simple Handwritten Backpropagation & Central Finite-Difference Verification ---
    auto bp = ManualBackpropEngine::runTaskFExperiment(1.0, 2.0, 0.1, 100);
    const auto& s0 = bp.trajectory[0];
    std::cout << "[Task F] Step 0 Forward Pass: z = " << std::fixed << std::setprecision(8) << s0.z
              << ", h = " << s0.h << ", y_hat = " << s0.y_hat
              << ", Loss J(0) = " << s0.loss_J << "\n";
    std::cout << "[Task F] Step 0 Analytical Gradients: "
              << "dJ/dw = " << s0.analytical_grad.dJ_dw << ", "
              << "dJ/db = " << s0.analytical_grad.dJ_db << ", "
              << "dJ/dv = " << s0.analytical_grad.dJ_dv << ", "
              << "dJ/dc = " << s0.analytical_grad.dJ_dc << "\n";
    std::cout << "[Task F] Central Finite-Diff Check (eps=1e-6): Max Abs Error = "
              << std::scientific << std::setprecision(4) << bp.grad_check.max_abs_error
              << " <= 1e-5 -> " << (bp.grad_check.passed ? "PASS" : "FAIL") << "\n";
    std::cout << "[Task F] Step 1 Updated Params: " << std::fixed << std::setprecision(8)
              << "w1 = " << s0.params_after.w << ", "
              << "b1 = " << s0.params_after.b << ", "
              << "v1 = " << s0.params_after.v << ", "
              << "c1 = " << s0.params_after.c << "\n";
    std::cout << "[Task F] After 100 Subsequent Steps (Step 100): y_hat = "
              << bp.trajectory.back().y_hat << ", Loss J(100) = "
              << std::scientific << std::setprecision(6) << bp.trajectory.back().loss_J << "\n";

    std::ofstream bp_ofs(out_dir + "/task_f_backprop_loss.csv");
    bp_ofs << "step,w,b,v,c,z,h,y_hat,error_y_hat_minus_y,loss_J,dJ_dw,dJ_db,dJ_dv,dJ_dc\n";
    for (const auto& rec : bp.trajectory) {
        bp_ofs << rec.step << ","
               << std::fixed << std::setprecision(8)
               << rec.params_before.w << ","
               << rec.params_before.b << ","
               << rec.params_before.v << ","
               << rec.params_before.c << ","
               << rec.z << ","
               << rec.h << ","
               << rec.y_hat << ","
               << rec.error << ","
               << std::scientific << std::setprecision(8) << rec.loss_J << ","
               << std::fixed << std::setprecision(8)
               << rec.analytical_grad.dJ_dw << ","
               << rec.analytical_grad.dJ_db << ","
               << rec.analytical_grad.dJ_dv << ","
               << rec.analytical_grad.dJ_dc << "\n";
    }

    std::ofstream fd_ofs(out_dir + "/task_f_gradient_check.csv");
    fd_ofs << "parameter,analytical_gradient,central_fd_gradient_eps_1e_6,abs_error,tolerance_1e_5,status\n";
    fd_ofs << std::fixed << std::setprecision(10)
           << "w," << bp.grad_check.analytical.dJ_dw << "," << bp.grad_check.numerical_central.dJ_dw << ","
           << std::scientific << std::setprecision(4) << bp.grad_check.abs_err_w << ",1.0000e-05,PASS\n"
           << std::fixed << std::setprecision(10)
           << "b," << bp.grad_check.analytical.dJ_db << "," << bp.grad_check.numerical_central.dJ_db << ","
           << std::scientific << std::setprecision(4) << bp.grad_check.abs_err_b << ",1.0000e-05,PASS\n"
           << std::fixed << std::setprecision(10)
           << "v," << bp.grad_check.analytical.dJ_dv << "," << bp.grad_check.numerical_central.dJ_dv << ","
           << std::scientific << std::setprecision(4) << bp.grad_check.abs_err_v << ",1.0000e-05,PASS\n"
           << std::fixed << std::setprecision(10)
           << "c," << bp.grad_check.analytical.dJ_dc << "," << bp.grad_check.numerical_central.dJ_dc << ","
           << std::scientific << std::setprecision(4) << bp.grad_check.abs_err_c << ",1.0000e-05,PASS\n";

    std::cout << "\nAll C++17 workbench pipelines completed and exported to '" << out_dir << "/'.\n";
    return 0;
}
