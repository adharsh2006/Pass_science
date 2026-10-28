#pragma once
#include <vector>
#include <string>

namespace workbench {

struct BackpropParams {
    double w{0.5};
    double b{0.0};
    double v{1.0};
    double c{0.0};
};

struct BackpropGradients {
    double dJ_dw{0.0};
    double dJ_db{0.0};
    double dJ_dv{0.0};
    double dJ_dc{0.0};
};

struct BackpropStepRecord {
    int step{0};
    BackpropParams params_before;
    double z{0.0};
    double h{0.0};
    double y_hat{0.0};
    double error{0.0};
    double loss_J{0.0};
    BackpropGradients analytical_grad;
    BackpropParams params_after;
};

struct FiniteDifferenceCheck {
    double epsilon{1.0e-6};
    BackpropGradients analytical;
    BackpropGradients numerical_central;
    double abs_err_w{0.0};
    double abs_err_b{0.0};
    double abs_err_v{0.0};
    double abs_err_c{0.0};
    double max_abs_error{0.0};
    bool passed{false}; // requires max_abs_error <= 1e-5
};

struct BackpropExperimentResult {
    double x_sample{1.0};
    double y_target{2.0};
    double learning_rate{0.1};
    FiniteDifferenceCheck grad_check;
    std::vector<BackpropStepRecord> trajectory; // Step 0 (initial) + 100 training steps
};

class ManualBackpropEngine {
public:
    static double computeLossOnly(const BackpropParams& p, double x, double y);
    static BackpropStepRecord computeSingleStep(const BackpropParams& p, double x, double y, double lr, int step_idx = 0);
    static FiniteDifferenceCheck verifyCentralFiniteDifferences(
        const BackpropParams& p, double x = 1.0, double y = 2.0, double eps = 1.0e-6);
    static BackpropExperimentResult runTaskFExperiment(
        double x = 1.0, double y = 2.0, double lr = 0.1, int num_subsequent_steps = 100);
};

} // namespace workbench
