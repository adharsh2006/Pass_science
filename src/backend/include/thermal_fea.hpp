#pragma once
#include <vector>
#include <string>

namespace workbench {

struct ThermalProblemParams {
    double length_m{1.0};          // L = 1 m
    double area_m2{1.0e-4};        // A = 1e-4 m^2
    double k_W_mK{10.0};           // k = 10 W/(m K)
    double q_W_m3{10000.0};        // q = 10,000 W/m^3
    double T0_C{100.0};            // T(0) = 100 C
    double TL_C{20.0};             // T(L) = 20 C
    int num_elements{10};          // N elements (N+1 nodes)
};

struct ElementMidpointSample {
    int element_index{0};
    double x_mid_m{0.0};
    double T_fe_mid_C{0.0};
    double T_exact_mid_C{0.0};
    double abs_error_mid_C{0.0};
};

struct ThermalSolveResult {
    bool valid{false};
    std::string error_message;
    ThermalProblemParams params;
    std::vector<double> node_x_m;
    std::vector<double> node_T_fe_C;
    std::vector<double> node_T_exact_C;
    std::vector<double> node_abs_error_C;
    std::vector<ElementMidpointSample> midpoints;
    double max_nodal_abs_error_C{0.0};
    double max_midpoint_abs_error_C{0.0};
    double l2_midpoint_error_C{0.0};
};

class ThermalFeaSolver1D {
public:
    // Exact analytical solution: T_exact(x) = T0 + ((TL - T0)/L)*x + (q / (2*k))*x*(L - x)
    static double exactTemperature(double x_m, const ThermalProblemParams& p);

    // Solve 1D steady-state linear finite element system -d/dx(kA dT/dx) = qA
    static ThermalSolveResult solve(const ThermalProblemParams& p);

    // Run mesh convergence study across N = {10, 20, 40, 80}
    static std::vector<ThermalSolveResult> runConvergenceStudy(
        const std::vector<int>& element_counts = {10, 20, 40, 80});
};

} // namespace workbench
