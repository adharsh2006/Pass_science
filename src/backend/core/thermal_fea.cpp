#include "thermal_fea.hpp"
#include <cmath>
#include <algorithm>

namespace workbench {

double ThermalFeaSolver1D::exactTemperature(double x_m, const ThermalProblemParams& p) {
    // General exact solution for constant k, A, q with Dirichlet BCs T(0)=T0, T(L)=TL:
    // T_exact(x) = T0 + ((TL - T0) / L) * x + (q / (2 * k)) * x * (L - x)
    // For L=1, T0=100, TL=20: T_exact(x) = 100 - 80*x/L + (q/(2*k))*x*(L - x)
    const double linear_term = p.T0_C + ((p.TL_C - p.T0_C) / p.length_m) * x_m;
    const double source_term = (p.q_W_m3 / (2.0 * p.k_W_mK)) * x_m * (p.length_m - x_m);
    return linear_term + source_term;
}

ThermalSolveResult ThermalFeaSolver1D::solve(const ThermalProblemParams& p) {
    ThermalSolveResult res;
    res.params = p;

    // Parameter validation
    if (!std::isfinite(p.length_m) || p.length_m <= 0.0) {
        res.error_message = "Rod length L must be finite and strictly positive (m).";
        return res;
    }
    if (!std::isfinite(p.area_m2) || p.area_m2 <= 0.0) {
        res.error_message = "Cross-sectional area A must be finite and strictly positive (m^2).";
        return res;
    }
    if (!std::isfinite(p.k_W_mK) || p.k_W_mK <= 0.0) {
        res.error_message = "Thermal conductivity k must be finite and strictly positive (W/(m K)).";
        return res;
    }
    if (!std::isfinite(p.q_W_m3) || !std::isfinite(p.T0_C) || !std::isfinite(p.TL_C)) {
        res.error_message = "Volumetric heat generation q and boundary temperatures T(0), T(L) must be finite.";
        return res;
    }
    if (p.num_elements < 2 || p.num_elements > 100000) {
        res.error_message = "Element count N must be in [2, 100000].";
        return res;
    }

    const int N = p.num_elements;
    const int num_nodes = N + 1;
    const double h = p.length_m / static_cast<double>(N);

    // Tridiagonal global stiffness matrix storage:
    // lower diagonal a[0..num_nodes-1], main diagonal d[0..num_nodes-1], upper diagonal c[0..num_nodes-1]
    std::vector<double> a(num_nodes, 0.0);
    std::vector<double> d(num_nodes, 0.0);
    std::vector<double> c(num_nodes, 0.0);
    std::vector<double> F(num_nodes, 0.0);

    // Local 2-node linear element stiffness and load contributions:
    // K^(e) = (k * A / h) * [ 1  -1 ]
    //                       [-1   1 ]
    // f^(e) = (q * A * h / 2) * [ 1 ]
    //                           [ 1 ]
    const double ke = (p.k_W_mK * p.area_m2) / h;
    const double fe = 0.5 * p.q_W_m3 * p.area_m2 * h;

    for (int e = 0; e < N; ++e) {
        const int i = e;
        const int j = e + 1;
        d[i] += ke;
        c[i] += -ke;
        a[j] += -ke;
        d[j] += ke;

        F[i] += fe;
        F[j] += fe;
    }

    // Impose Dirichlet boundary conditions at x = 0 (node 0) and x = L (node N)
    // lifting known boundary temperatures cleanly to the RHS before zeroing off-diagonals:
    // Node 0: T_0 = T0_C
    F[1] -= a[1] * p.T0_C;
    a[1] = 0.0;
    d[0] = 1.0;
    c[0] = 0.0;
    F[0] = p.T0_C;

    // Node N: T_N = TL_C
    F[N - 1] -= c[N - 1] * p.TL_C;
    c[N - 1] = 0.0;
    a[N] = 0.0;
    d[N] = 1.0;
    F[N] = p.TL_C;

    // Solve tridiagonal linear system via Thomas Algorithm (O(N) complexity)
    std::vector<double> c_star(num_nodes, 0.0);
    std::vector<double> d_star(num_nodes, 0.0);

    c_star[0] = c[0] / d[0];
    d_star[0] = F[0] / d[0];

    for (int i = 1; i < num_nodes; ++i) {
        const double denom = d[i] - a[i] * c_star[i - 1];
        c_star[i] = (i < num_nodes - 1) ? (c[i] / denom) : 0.0;
        d_star[i] = (F[i] - a[i] * d_star[i - 1]) / denom;
    }

    res.node_T_fe_C.resize(num_nodes, 0.0);
    res.node_T_fe_C[num_nodes - 1] = d_star[num_nodes - 1];
    for (int i = num_nodes - 2; i >= 0; --i) {
        res.node_T_fe_C[i] = d_star[i] - c_star[i] * res.node_T_fe_C[i + 1];
    }

    // Evaluate nodal coordinates, analytical solution, and nodal errors
    res.node_x_m.resize(num_nodes);
    res.node_T_exact_C.resize(num_nodes);
    res.node_abs_error_C.resize(num_nodes);
    res.max_nodal_abs_error_C = 0.0;

    for (int i = 0; i < num_nodes; ++i) {
        const double x = static_cast<double>(i) * h;
        res.node_x_m[i] = x;
        res.node_T_exact_C[i] = exactTemperature(x, p);
        res.node_abs_error_C[i] = std::abs(res.node_T_fe_C[i] - res.node_T_exact_C[i]);
        res.max_nodal_abs_error_C = std::max(res.max_nodal_abs_error_C, res.node_abs_error_C[i]);
    }

    // Evaluate FE linear interpolation at element midpoints:
    // T_FE(x_mid) = 0.5 * (T_i + T_{i+1}) using linear shape functions N1(0.5)=0.5, N2(0.5)=0.5
    res.midpoints.resize(N);
    res.max_midpoint_abs_error_C = 0.0;
    double sq_err_sum = 0.0;

    for (int e = 0; e < N; ++e) {
        ElementMidpointSample sample{};
        sample.element_index = e;
        sample.x_mid_m = (static_cast<double>(e) + 0.5) * h;
        sample.T_fe_mid_C = 0.5 * (res.node_T_fe_C[e] + res.node_T_fe_C[e + 1]);
        sample.T_exact_mid_C = exactTemperature(sample.x_mid_m, p);
        sample.abs_error_mid_C = std::abs(sample.T_fe_mid_C - sample.T_exact_mid_C);

        res.midpoints[e] = sample;
        res.max_midpoint_abs_error_C = std::max(res.max_midpoint_abs_error_C, sample.abs_error_mid_C);
        sq_err_sum += sample.abs_error_mid_C * sample.abs_error_mid_C;
    }
    res.l2_midpoint_error_C = std::sqrt(sq_err_sum / static_cast<double>(N));
    res.valid = true;
    return res;
}

std::vector<ThermalSolveResult> ThermalFeaSolver1D::runConvergenceStudy(
    const std::vector<int>& element_counts) {
    std::vector<ThermalSolveResult> results;
    results.reserve(element_counts.size());
    for (int n : element_counts) {
        ThermalProblemParams p;
        p.num_elements = n;
        results.push_back(solve(p));
    }
    return results;
}

} // namespace workbench
