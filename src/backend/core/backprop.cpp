#include "backprop.hpp"
#include <cmath>
#include <algorithm>

namespace workbench {

double ManualBackpropEngine::computeLossOnly(const BackpropParams& p, double x, double y) {
    const double z = p.w * x + p.b;
    const double h = std::tanh(z);
    const double y_hat = p.v * h + p.c;
    const double err = y_hat - y;
    return 0.5 * err * err;
}

BackpropStepRecord ManualBackpropEngine::computeSingleStep(
    const BackpropParams& p, double x, double y, double lr, int step_idx) {
    BackpropStepRecord rec;
    rec.step = step_idx;
    rec.params_before = p;

    // 1. Forward pass:
    // z = w*x + b
    // h = tanh(z)
    // y_hat = v*h + c
    // J = 0.5 * (y_hat - y)^2
    rec.z = p.w * x + p.b;
    rec.h = std::tanh(rec.z);
    rec.y_hat = p.v * rec.h + p.c;
    rec.error = rec.y_hat - y;
    rec.loss_J = 0.5 * rec.error * rec.error;

    // 2. Manual Chain-Rule Backward Pass (zero automatic differentiation):
    // dJ/dy_hat = y_hat - y
    // dJ/dv = (dJ/dy_hat) * (dy_hat/dv) = (y_hat - y) * h
    // dJ/dc = (dJ/dy_hat) * (dy_hat/dc) = (y_hat - y) * 1
    // dh/dz = sech^2(z) = 1 - tanh^2(z) = 1 - h^2
    // dJ/dz = (dJ/dy_hat) * (dy_hat/dh) * (dh/dz) = (y_hat - y) * v * (1 - h^2)
    // dJ/dw = (dJ/dz) * (dz/dw) = (y_hat - y) * v * (1 - h^2) * x
    // dJ/db = (dJ/dz) * (dz/db) = (y_hat - y) * v * (1 - h^2) * 1
    const double dJ_dyhat = rec.error;
    const double dh_dz = 1.0 - rec.h * rec.h;
    const double dJ_dz = dJ_dyhat * p.v * dh_dz;

    rec.analytical_grad.dJ_dv = dJ_dyhat * rec.h;
    rec.analytical_grad.dJ_dc = dJ_dyhat;
    rec.analytical_grad.dJ_dw = dJ_dz * x;
    rec.analytical_grad.dJ_db = dJ_dz;

    // 3. Simultaneous gradient-descent update using gradients at current parameters:
    rec.params_after.w = p.w - lr * rec.analytical_grad.dJ_dw;
    rec.params_after.b = p.b - lr * rec.analytical_grad.dJ_db;
    rec.params_after.v = p.v - lr * rec.analytical_grad.dJ_dv;
    rec.params_after.c = p.c - lr * rec.analytical_grad.dJ_dc;

    return rec;
}

FiniteDifferenceCheck ManualBackpropEngine::verifyCentralFiniteDifferences(
    const BackpropParams& p, double x, double y, double eps) {
    FiniteDifferenceCheck chk;
    chk.epsilon = eps;

    BackpropStepRecord step0 = computeSingleStep(p, x, y, 0.1, 0);
    chk.analytical = step0.analytical_grad;

    // Central finite difference: (J(theta + eps) - J(theta - eps)) / (2 * eps)
    {
        BackpropParams pp = p, pm = p;
        pp.w += eps; pm.w -= eps;
        chk.numerical_central.dJ_dw = (computeLossOnly(pp, x, y) - computeLossOnly(pm, x, y)) / (2.0 * eps);
    }
    {
        BackpropParams pp = p, pm = p;
        pp.b += eps; pm.b -= eps;
        chk.numerical_central.dJ_db = (computeLossOnly(pp, x, y) - computeLossOnly(pm, x, y)) / (2.0 * eps);
    }
    {
        BackpropParams pp = p, pm = p;
        pp.v += eps; pm.v -= eps;
        chk.numerical_central.dJ_dv = (computeLossOnly(pp, x, y) - computeLossOnly(pm, x, y)) / (2.0 * eps);
    }
    {
        BackpropParams pp = p, pm = p;
        pp.c += eps; pm.c -= eps;
        chk.numerical_central.dJ_dc = (computeLossOnly(pp, x, y) - computeLossOnly(pm, x, y)) / (2.0 * eps);
    }

    chk.abs_err_w = std::abs(chk.analytical.dJ_dw - chk.numerical_central.dJ_dw);
    chk.abs_err_b = std::abs(chk.analytical.dJ_db - chk.numerical_central.dJ_db);
    chk.abs_err_v = std::abs(chk.analytical.dJ_dv - chk.numerical_central.dJ_dv);
    chk.abs_err_c = std::abs(chk.analytical.dJ_dc - chk.numerical_central.dJ_dc);

    chk.max_abs_error = std::max({chk.abs_err_w, chk.abs_err_b, chk.abs_err_v, chk.abs_err_c});
    chk.passed = (chk.max_abs_error <= 1.0e-5);
    return chk;
}

BackpropExperimentResult ManualBackpropEngine::runTaskFExperiment(
    double x, double y, double lr, int num_subsequent_steps) {
    BackpropExperimentResult res;
    res.x_sample = x;
    res.y_target = y;
    res.learning_rate = lr;

    BackpropParams current{0.5, 0.0, 1.0, 0.0};
    res.grad_check = verifyCentralFiniteDifferences(current, x, y, 1.0e-6);

    // Step 0 is the initial state + first update; followed by 100 subsequent steps (total 101 records: steps 0..100)
    res.trajectory.reserve(num_subsequent_steps + 1);
    for (int s = 0; s <= num_subsequent_steps; ++s) {
        BackpropStepRecord step_rec = computeSingleStep(current, x, y, lr, s);
        res.trajectory.push_back(step_rec);
        current = step_rec.params_after;
    }

    return res;
}

} // namespace workbench
