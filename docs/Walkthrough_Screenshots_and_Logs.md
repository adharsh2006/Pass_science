# Timestamped Walkthrough & Execution Logs (3–5 Minute Demo Equivalent)

Per Section 8 of the brief (*"Include a 3–5 minute demonstration video, or an equivalent timestamped screenshot walkthrough with logs"*), this document provides the complete timestamped walkthrough alongside verbatim execution logs and screenshots.

---

## 00:00 – 00:45 | One-Click Standalone Launch & Startup Verification (Task A)
- **Action**: Extract `Adharsh_Workbench.zip` on a clean machine and double-click `Launch.exe` (or run `"1-Click Launch Unified Engineering Workbench (Packaged)"` in VS Code via `F5`).
- **Observed Behaviour**: `Launch.exe` invokes the packaged workbench immediately without terminal commands, PowerShell, or package installation. Launch-to-ready time is **4.044 ms**.
- **Screenshot Reference**: `results/ui_walkthrough_geometry.png`

## 00:45 – 01:40 | CAD STL & Synthetic ANSYS Nodal CSV Visualization & Error Handling (Task B)
- **Action**:
  1. Inspect default `data/heat_sink_fin.stl` (`36` triangles, surface area `0.031200 m^2`, units: `m`) and `data/synthetic_ansys_nodal_temp.csv` (`240` nodes, `35.14 °C` to `93.40 °C`, units: `m, °C`, explicitly badged `SYNTHETIC CSV DATA`).
  2. Rotate (left-drag), pan (Shift-drag), zoom (scroll/buttons), and click Fit-to-View. Inspect Node `125` at `(0.0500, 0.0300, 0.0020) m` reading `93.3955 °C`.
  3. Click **"Test Empty STL Error"** and **"Test NaN CSV Error"** to verify malformed file rejection.
- **Observed Log**:
  - `[Task B] STL Validation: PASS | Solid: pass_science_heatsink_m | Triangles: 36 | Area: 0.031200 m^2`
  - `[Task B] CSV Validation: PASS | Nodes: 240 | T_min: 35.1428 C | T_max: 93.3955 C | T_mean: 61.4107 C`
  - `[Validation Rejected Empty STL] Empty STL file (0 bytes or whitespace only). Expected ASCII STL starting with 'solid <name>'.`
  - `[Validation Rejected Non-Finite CSV] Line 2: Non-finite (NaN/Inf) or malformed value rejected by validator.`

## 01:40 – 02:35 | 1D Steady-State Thermal FEA & Mesh Convergence (Task C)
- **Action**: Switch to Tab 2 (**Thermal FEA Solver**). Solve $-d/dx(kA dT/dx) = qA$ with $L=1	ext{ m}, A=10^{-4}	ext{ m}^2, k=10	ext{ W/(m}\cdot	ext{K)}, q=10,000	ext{ W/m}^3, T(0)=100^\circ	ext{C}, T(L)=20^\circ	ext{C}$ across $N \in \{10, 20, 40, 80\}$.
- **Observed Log & Figures**: `results/ui_walkthrough_thermal.png`, `results/task_c_temperature_profiles.png`, `results/task_c_mesh_convergence.png`
  - `N = 10 (h = 0.1000 m) | Max Nodal Err: 2.842e-14 C | Max Midpoint FE Err: 1.250000 C | Order p: 0.00`
  - `N = 20 (h = 0.0500 m) | Max Nodal Err: 1.990e-13 C | Max Midpoint FE Err: 0.312500 C | Order p: 2.00`
  - `N = 40 (h = 0.0250 m) | Max Nodal Err: 8.811e-13 C | Max Midpoint FE Err: 0.078125 C | Order p: 2.00`
  - `N = 80 (h = 0.0125 m) | Max Nodal Err: 1.222e-12 C | Max Midpoint FE Err: 0.019531 C | Order p: 2.00`

## 02:35 – 03:25 | Multicore Worker Pool Execution & Safe Cancellation (Task D)
- **Action**: Switch to Tab 3 (**Multicore Performance**). Run `4,000` independent thermal solves ($N=250$, fixed seed `20260415`) across $p = 1, 2, 4$ workers with warm-up and 5 measured repetitions. Test **Cancel Workload** mid-run to verify UI responsiveness and non-corrupted state.
- **Observed Log & Figure**: `results/task_d_multicore_scaling.png`
  - `Workers p = 1 | Median: 36.788 ms | Speedup: 1.000x | Efficiency: 100.0% | Max Diff: 0.00e+00 C (PASS)`
  - `Workers p = 2 | Median: 24.295 ms | Speedup: 1.514x | Efficiency: 75.7% | Max Diff: 0.00e+00 C (PASS)`
  - `Workers p = 4 | Median: 24.562 ms | Speedup: 1.498x | Efficiency: 37.4% | Max Diff: 0.00e+00 C (PASS)`

## 03:25 – 04:05 | Embedded-System HAL Discovery & Fault Recovery (Task E)
- **Action**: Switch to Tab 4 (**Embedded Hardware HAL**). Discover all three simulated adapters (`MCU-STM32F407VG-01`, `FPGA-XC7A35T-01`, `JTAG-FT2232H-PROBE-01`), read nominal telemetry, and inject a transient bus timeout.
- **Observed Log & Diagram**: `results/task_e_block_diagram.png`
  - `[Task E] Telemetry #4 [FPGA-XC7A35T-01] RECOVERED_AFTER_TIMEOUT | Retries: 2 | [SIMULATED] Transient SPI CS_N framing timeout detected; re-synchronized state machine on retry #2.`
  - `[Task E] Telemetry #5 [JTAG-FT2232H-PROBE-01] RECOVERED_TAP_RESET | Retries: 2 | [SIMULATED] JTAG TDO CRC mismatch/timeout on scan; issued 5x TMS=1 Test-Logic-Reset recovery sequence and restored chain.`

## 04:05 – 04:45 | Manual Backpropagation & Automated 16-Test Suite (Task F)
- **Action**: Switch to Tab 5 (**Backpropagation & Verification**) and execute `./bin/workbench_tests`.
- **Observed Log & Figure**: `results/task_f_loss_curve.png`, `results/test_execution_log.txt`
  - `Step 0 Forward Pass: z = 0.50000000, h = 0.46211716, y_hat = 0.46211716, Loss J(0) = 1.18254182`
  - `Step 0 Analytical Gradients: dJ/dw = -1.20946448, dJ/db = -1.20946448, dJ/dv = -0.71068205, dJ/dc = -1.53788284`
  - `Central Finite-Diff Check (eps=1e-6): Max Abs Error = 1.1175e-10 <= 1e-5 -> PASS`
  - `Step 1 Updated Params: w1 = 0.62094645, b1 = 0.12094645, v1 = 1.07106820, c1 = 0.15378828`
  - `After 100 Subsequent Steps (Step 100): y_hat = 2.00000000, Loss J(100) = 1.702622e-20`
  - `Test Summary: 16 / 16 tests PASSED.`
