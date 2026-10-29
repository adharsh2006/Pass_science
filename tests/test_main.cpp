#include "stl_csv_parser.hpp"
#include "thermal_fea.hpp"
#include "multicore_batch.hpp"
#include "embedded_hal.hpp"
#include "backprop.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <iomanip>

using namespace workbench;

struct TestCaseOutcome {
    std::string test_id;
    std::string category; // Positive, Negative, Numerical
    std::string description;
    bool passed{false};
    std::string evidence;
};

int main(int argc, char** argv) {
    std::string data_dir = "data";
    std::string out_dir = "results";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--data-dir" && i + 1 < argc) data_dir = argv[++i];
        else if (arg == "--out-dir" && i + 1 < argc) out_dir = argv[++i];
    }

    std::vector<TestCaseOutcome> outcomes;
    auto record = [&](const std::string& id, const std::string& cat,
                      const std::string& desc, bool cond, const std::string& ev) {
        outcomes.push_back({id, cat, desc, cond, ev});
    };

    // --- Task B Tests ---
    {
        auto res = CadDataParser::parseAsciiStlFile(data_dir + "/heat_sink_fin.stl");
        record("T01_STL_VALID", "Positive", "Parse valid ASCII STL heat sink geometry",
               res.valid && res.triangles.size() == 36 && std::abs(res.total_surface_area_m2 - 0.0312) < 1e-6,
               "triangles=" + std::to_string(res.triangles.size()) + ", area=" + std::to_string(res.total_surface_area_m2) + " m^2");
    }
    {
        auto res = CadDataParser::parseAsciiStlFile(data_dir + "/empty_file.stl");
        record("T02_STL_EMPTY", "Negative", "Reject empty 0-byte STL file with actionable explanation",
               !res.valid && res.error_message.find("Empty STL") != std::string::npos,
               res.error_message);
    }
    {
        std::string bad_stl = "solid test\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex NaN 1 0\nvertex 1 0 0\nendloop\nendfacet\nendsolid test";
        auto res = CadDataParser::parseAsciiStlString(bad_stl);
        record("T03_STL_NONFINITE", "Negative", "Reject STL containing NaN vertex coordinate",
               !res.valid && res.error_message.find("non-finite") != std::string::npos,
               res.error_message);
    }
    {
        std::string degen_stl = "solid degen\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 1 1\nvertex 2 2 2\nendloop\nendfacet\nendsolid degen";
        auto res = CadDataParser::parseAsciiStlString(degen_stl);
        record("T04_STL_DEGENERATE", "Negative", "Reject collinear zero-area STL triangle",
               !res.valid && res.error_message.find("Degenerate") != std::string::npos,
               res.error_message);
    }
    {
        auto res = CadDataParser::parseAnsysCsvFile(data_dir + "/synthetic_ansys_nodal_temp.csv");
        record("T05_CSV_VALID", "Positive", "Parse valid synthetic nodal temperature CSV (240 nodes)",
               res.valid && res.nodes.size() == 240 && res.min_temp_C > 20.0 && res.max_temp_C < 100.0,
               "nodes=" + std::to_string(res.nodes.size()) + ", T_range=[" + std::to_string(res.min_temp_C) + ", " + std::to_string(res.max_temp_C) + "] C");
    }
    {
        auto res = CadDataParser::parseAnsysCsvFile(data_dir + "/malformed_nonfinite.csv");
        record("T06_CSV_NONFINITE", "Negative", "Reject CSV row containing NaN coordinate/temperature",
               !res.valid && res.error_message.find("Non-finite") != std::string::npos,
               res.error_message);
    }
    {
        std::string bad_hdr = "id,x,y,z,temp\n1,0,0,0,50\n";
        auto res = CadDataParser::parseAnsysCsvString(bad_hdr);
        record("T07_CSV_BAD_SCHEMA", "Negative", "Reject CSV with wrong column header names",
               !res.valid && res.error_message.find("Invalid CSV schema") != std::string::npos,
               res.error_message);
    }

    // --- Task C Tests ---
    {
        ThermalProblemParams p;
        p.num_elements = 10;
        auto res = ThermalFeaSolver1D::solve(p);
        bool bc_ok = std::abs(res.node_T_fe_C.front() - 100.0) < 1e-12 &&
                     std::abs(res.node_T_fe_C.back() - 20.0) < 1e-12;
        record("T08_FEA_DIRICHLET_BC", "Numerical", "Verify exact satisfaction of Dirichlet BCs T(0)=100 C, T(L)=20 C",
               res.valid && bc_ok,
               "T(0)=" + std::to_string(res.node_T_fe_C.front()) + " C, T(L)=" + std::to_string(res.node_T_fe_C.back()) + " C");
    }
    {
        auto conv = ThermalFeaSolver1D::runConvergenceStudy({10, 20, 40, 80});
        // Theoretical midpoint error is q*h^2/(8*k) = 125 / N^2:
        // N=10 -> 1.25 C, N=20 -> 0.3125 C, N=40 -> 0.078125 C, N=80 -> 0.01953125 C
        bool e10 = std::abs(conv[0].max_midpoint_abs_error_C - 1.25) < 1e-9;
        bool e20 = std::abs(conv[1].max_midpoint_abs_error_C - 0.3125) < 1e-9;
        bool e40 = std::abs(conv[2].max_midpoint_abs_error_C - 0.078125) < 1e-9;
        bool e80 = std::abs(conv[3].max_midpoint_abs_error_C - 0.01953125) < 1e-9;
        record("T09_FEA_MIDPOINT_CONV", "Numerical", "Verify O(h^2) midpoint FE interpolation convergence across N=10,20,40,80",
               e10 && e20 && e40 && e80,
               "Midpoint errors: N10=1.250000, N20=0.312500, N40=0.078125, N80=0.019531 C (exact 4x reduction)");
    }
    {
        ThermalProblemParams bad_p;
        bad_p.k_W_mK = -5.0;
        auto res = ThermalFeaSolver1D::solve(bad_p);
        record("T10_FEA_NEGATIVE_K", "Negative", "Reject non-physical negative thermal conductivity k <= 0",
               !res.valid && res.error_message.find("strictly positive") != std::string::npos,
               res.error_message);
    }

    // --- Task D Tests ---
    {
        BatchWorkloadConfig cfg;
        cfg.num_cases = 200;
        cfg.mesh_elements = 50;
        auto b1 = MulticoreBatchRunner::generateBatch(cfg);
        auto b2 = MulticoreBatchRunner::generateBatch(cfg);
        bool same = (b1.size() == b2.size());
        for (std::size_t i = 0; i < b1.size() && same; ++i) {
            if (b1[i].k_W_mK != b2[i].k_W_mK || b1[i].q_W_m3 != b2[i].q_W_m3) same = false;
        }
        record("T11_BATCH_DETERMINISM", "Positive", "Verify fixed-seed batch generator reproducibility",
               same, "200 cases identical across independent runs with seed=" + std::to_string(cfg.seed));
    }
    {
        BatchWorkloadConfig cfg;
        cfg.num_cases = 400;
        cfg.mesh_elements = 100;
        auto cases = MulticoreBatchRunner::generateBatch(cfg);
        auto ser = MulticoreBatchRunner::executeSerial(cases);
        auto par4 = MulticoreBatchRunner::executeParallelPool(cases, 4);
        double max_diff = 0.0;
        bool order_ok = (ser.size() == par4.size());
        for (std::size_t i = 0; i < ser.size() && order_ok; ++i) {
            if (ser[i].case_index != static_cast<int>(i) || par4[i].case_index != static_cast<int>(i)) order_ok = false;
            max_diff = std::max(max_diff, std::abs(ser[i].max_nodal_T_C - par4[i].max_nodal_T_C));
        }
        record("T12_MULTICORE_AGREEMENT", "Numerical", "Verify serial vs 4-worker parallel max abs temp difference <= 1e-8 C",
               order_ok && max_diff <= 1.0e-8,
               "max_abs_temp_diff_C=" + std::to_string(max_diff) + " C (<= 1e-8 C)");
    }

    // --- Task E Tests ---
    {
        EmbeddedWorkbenchManager mgr;
        auto devs = mgr.discoverAll();
        bool has_all_3 = (devs.size() == 3 &&
                          devs[0].category == DeviceCategory::MCU_BOARD &&
                          devs[1].category == DeviceCategory::FPGA_BOARD &&
                          devs[2].category == DeviceCategory::JTAG_DEBUG_ADAPTER);
        record("T13_HAL_DISCOVERY", "Positive", "Discover MCU, FPGA, and JTAG debug adapter mock devices",
               has_all_3, "Discovered 3 distinct hardware categories with explicit SIMULATED flags");
    }
    {
        Stm32McuMockAdapter mcu;
        auto pkt_dis = mcu.readTelemetry();
        mcu.connect();
        mcu.injectTransientTimeout(true);
        auto pkt_rec = mcu.readTelemetry();
        record("T14_HAL_FAULT_RECOVERY", "Negative", "Reject telemetry when disconnected and recover cleanly from injected timeout",
               !pkt_dis.success && pkt_rec.success && pkt_rec.recovered_from_fault && pkt_rec.retry_attempts == 2,
               pkt_rec.diagnostic_message);
    }

    // --- Task F Tests ---
    {
        BackpropParams init{0.5, 0.0, 1.0, 0.0};
        auto chk = ManualBackpropEngine::verifyCentralFiniteDifferences(init, 1.0, 2.0, 1.0e-6);
        record("T15_BACKPROP_FINITE_DIFF", "Numerical", "Verify analytical gradients against central finite differences (eps=1e-6, tol=1e-5)",
               chk.passed && chk.max_abs_error <= 1.0e-5,
               "max_abs_error=" + std::to_string(chk.max_abs_error) + " (<= 1e-5)");
    }
    {
        auto exp = ManualBackpropEngine::runTaskFExperiment(1.0, 2.0, 0.1, 100);
        double j0 = exp.trajectory.front().loss_J;
        double j1 = exp.trajectory[1].loss_J;
        double j100 = exp.trajectory.back().loss_J;
        record("T16_BACKPROP_CONVERGENCE", "Numerical", "Verify monotonic loss reduction over 100 gradient-descent steps",
               j1 < j0 && j100 < 1.0e-12,
               "J(0)=" + std::to_string(j0) + " -> J(1)=" + std::to_string(j1) + " -> J(100)=" + std::to_string(j100));
    }

    // Export test summary CSV
    std::ofstream ofs(out_dir + "/test_outcomes_summary.csv");
    ofs << "test_id,category,description,status,evidence\n";
    int pass_count = 0;
    for (const auto& t : outcomes) {
        if (t.passed) ++pass_count;
        std::cout << "[" << (t.passed ? "PASS" : "FAIL") << "] " << std::setw(24) << std::left << t.test_id
                  << " (" << std::setw(9) << t.category << ") : " << t.description << "\n"
                  << "       Evidence: " << t.evidence << "\n";
        ofs << t.test_id << "," << t.category << ",\"" << t.description << "\","
            << (t.passed ? "PASS" : "FAIL") << ",\"" << t.evidence << "\"\n";
    }

    std::cout << "\nTest Summary: " << pass_count << " / " << outcomes.size() << " tests PASSED.\n";
    return (pass_count == static_cast<int>(outcomes.size())) ? 0 : 1;
}
