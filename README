# Unified Engineering Workbench — Pass Science Works (R&D Internship 2026–2027)

**Candidate Repository**: [https://github.com/adharsh2006/Pass_science](https://github.com/adharsh2006/Pass_science)  
**Target Platform**: Windows 11 x64 (One-Click `Launch.exe` PE32+ Standalone GUI) & Cross-Platform C++17 (`CMake 3.16+`)  
**Recorded Effort**: 44.5 Hours across 12 calendar days

---

## 1. Prerequisites & One-Click Standalone Execution (Task A)

### Windows 11 x64 One-Click Launch (Zero Installation Required)
1. Extract `Adharsh_Workbench.zip` to any local folder (relative paths only; no administrator rights, PowerShell, terminal commands, internet connection, or package installation required).
2. Double-click **`Launch.exe`** in the root folder.
3. `Launch.exe` (a native Windows 11 x64 PE32+ GUI executable linked solely against standard Windows system libraries `SHELL32.dll` and `KERNEL32.dll`) immediately launches the **Unified Engineering Workbench** interface (`src/frontend/index.html`) in the system default browser — fully offline, pre-loaded with verified C++17 output data and interactive 3D/numerical modules.

> **Note on pre-built `bin/` binaries**: `bin/workbench_cli` and `bin/workbench_tests` are **Linux x86_64 ELF executables** compiled during development (GCC 12.2.0 on Debian 12). They are included as reference execution evidence alongside their verbatim stdout logs in `results/cli_execution_log.txt` and `results/test_execution_log.txt`. They **cannot** be run directly on Windows without WSL. For Windows execution, see the CMake source-build instructions in Section 4.

### VS Code / Antigravity One-Click Run Configuration
1. Open the extracted folder in **VS Code** or **Antigravity**.
2. Optional Extension Prerequisite for C++ source debugging: *C/C++ Extension (`ms-vscode.cpptools`)* or *CMake Tools (`ms-vscode.cmake-tools`)*. The standalone packaged launcher requires **no editor extensions**.
3. Open the **Run and Debug** panel (`Ctrl+Shift+D`), select **"1-Click Launch Unified Engineering Workbench (Packaged)"** (defined in `.vscode/launch.json`), and press **F5** (one action).
   - This configuration uses `"type": "shell"` to run `Launch.exe` directly on Windows without requiring Node.js.
   - The two additional configurations (`Run C++17 Backend` / `Run C++17 Test Suite`) target the Linux ELF binaries under GDB on Linux/WSL.

---

## 2. Folder Architecture & Separation of Concerns

```text
Adharsh_Workbench/
├── Launch.exe                        # Windows 11 x64 PE32+ GUI one-click standalone launcher
├── CMakeLists.txt                    # CMake >= 3.16 C++17 source build configuration
├── DEPENDENCIES.lock                 # Toolchain, compiler, and license lock manifest
├── README.md                         # Architecture, build/test steps, formats, and limitations
├── .vscode/
│   ├── launch.json                   # One-action VS Code / Antigravity run configurations
│   └── tasks.json                    # C++17 build tasks
├── bin/
│   ├── workbench_cli                 # Pre-built C++17 backend CLI & batch generator
│   └── workbench_tests               # Pre-built C++17 16-test verification suite
├── src/
│   ├── backend/
│   │   ├── include/                  # Pure C++17 headers (no UI/transport coupling)
│   │   │   ├── stl_csv_parser.hpp    # Task B: ASCII STL & ANSYS nodal CSV parser/validator
│   │   │   ├── thermal_fea.hpp       # Task C: 1D steady-state linear FEA solver & convergence
│   │   │   ├── multicore_batch.hpp   # Task D: Bounded worker pool & deterministic batch engine
│   │   │   ├── embedded_hal.hpp      # Task E: Modular MCU/FPGA/JTAG mock HAL & fault recovery
│   │   │   └── backprop.hpp          # Task F: Manual backpropagation & central finite diff
│   │   ├── core/                     # C++17 numerical, parsing, concurrency & HAL implementations
│   │   └── app/
│   │       └── workbench_cli.cpp     # Transport/CLI layer exporting CSVs & logs
│   └── frontend/
│       ├── index.html                # Self-contained offline Workbench UI (5 views)
│       ├── styles.css                # Responsive stylesheet
│       └── app.js                    # 3D canvas renderer, validation, async worker controller
├── data/
│   ├── heat_sink_fin.stl             # Self-created ASCII STL 3D heat sink (36 triangles, units: m)
│   ├── synthetic_ansys_nodal_temp.csv# Self-created SYNTHETIC nodal temperature CSV (240 nodes)
│   ├── malformed_nonfinite.csv       # Negative test fixture (NaN coordinate injection)
│   ├── empty_file.stl                # Negative test fixture (0-byte STL)
│   └── PROVENANCE.md                 # Provenance, CC0/MIT license, units, and schema notes
├── tests/
│   └── test_main.cpp                 # 16 positive, negative/malformed, and numerical C++17 tests
├── results/                          # Raw CSV outputs, execution logs, and labelled PNG plots
├── docs/                             # 6-page Technical Report, 2-page Handwritten Appendix, AI Log
└── git_evidence/                     # Git bundle (workbench.bundle) and readable commit log
```

---

## 3. Supported Data Formats & Tested File-Size Limits (Task B)

1. **ASCII STL Geometry (`*.stl`)**:
   - Must begin with `solid <name>`, define outward unit normal vectors (`facet normal nx ny nz`) and 3 counter-clockwise vertices (`vertex x y z` in **meters `m`**) inside `outer loop ... endloop`, and close with `endsolid`.
   - **Validation**: Rejects empty (0-byte/whitespace) files, binary STL null bytes, nested solids, non-finite coordinates (`NaN`/`Inf`), non-triangle loops, and collinear zero-area degenerate triangles ($	ext{Area} \le 10^{-18}	ext{ m}^2$).
   - **Tested File-Size Limit**: Enforced C++ memory guard at **50 MB** (`MAX_TESTED_STL_BYTES = 52,428,800` bytes; tested up to `250,000` facets).
2. **Nodal Temperature CSV (`*.csv`)**:
   - Required exact header: `node_id,x_m,y_m,z_m,temperature_C` (comment lines starting with `#` permitted).
   - **Synthetic Disclosure**: `data/synthetic_ansys_nodal_temp.csv` is explicitly labelled **SYNTHETIC** in file headers, documentation, and UI badges; it is not represented as a commercial ANSYS export.
   - **Validation**: Rejects missing/misnamed columns, duplicate or non-positive `node_id`, non-finite floats (`NaN`/`Inf`), and unphysical temperatures below absolute zero ($<-273.15^\circ	ext{C}$).
   - **Tested File-Size Limit**: Enforced C++ memory guard at **50 MB** (`MAX_TESTED_CSV_BYTES = 52,428,800` bytes; tested up to `500,000` nodes).

---

## 4. CMake Source-Build & Verification Instructions

To build the C++17 backend and test suite from source on Windows (MinGW-w64 / MSVC) or Linux:

```bash
# Option A: CMake Out-of-Source Build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
ctest --output-on-failure

# Option B: Direct Compiler Invocation (GCC / Clang / MinGW-w64)
g++ -std=c++17 -O3 -Wall -Wextra -pthread -Isrc/backend/include     src/backend/core/*.cpp src/backend/app/workbench_cli.cpp -o bin/workbench_cli

g++ -std=c++17 -O3 -Wall -Wextra -pthread -Isrc/backend/include     src/backend/core/*.cpp tests/test_main.cpp -o bin/workbench_tests

./bin/workbench_cli --data-dir data --out-dir results
./bin/workbench_tests --data-dir data --out-dir results
```

---

## 5. Known Limitations

1. **CAD/ANSYS Scope**: Supports ASCII STL and the specified 5-column nodal CSV schema only; binary STL, native CAD BREP solids (`.step`/`.iges`), and binary ANSYS `.rst` files are outside scope per brief. Mesh-to-point-cloud spatial registration is visual only.
2. **1D Thermal FEA**: Assumes steady-state 1D axial conduction with uniform cross-section $A$, constant isotropic conductivity $k$, uniform volumetric heat generation $q$, and adiabatic lateral surfaces.
3. **Multicore Hardware Roofline**: Parallel speedup is bounded by available physical cores on the host machine (measured $1.514x$ on a 2-vCPU VM; scaling beyond physical core count shows expected thread-scheduling saturation).
4. **Simulated Embedded Hardware**: All MCU (`STM32F407VG`), FPGA (`XC7A35T`), and JTAG (`FT2232H`) interactions in `embedded_hal.cpp` are deterministic software mocks clearly tagged `SIMULATED`.

5. **Pre-built Binaries Platform**: `bin/workbench_cli` and `bin/workbench_tests` are Linux x86_64 ELF executables (GCC 12.2.0, Debian 12). They cannot be run directly on Windows without WSL. Use the CMake source-build in Section 4 for native Windows binaries. Execution logs are preserved in `results/`.
