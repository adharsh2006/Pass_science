# Data Provenance, Units, and Schema Notes (Task B)

## 1. ASCII STL Geometry (`data/heat_sink_fin.stl`)
- **Provenance & License**: Self-created parametric 3D dual-fin thermal heat sink geometry generated specifically for the Pass Science Works Unified Engineering Workbench assessment. Released under **CC0 1.0 Public Domain / MIT License**.
- **Units**: All vertex coordinates `(x, y, z)` are expressed in **meters (`m`)**.
  - Bounding box: `x in [0.000, 0.100] m`, `y in [0.000, 0.060] m`, `z in [0.000, 0.045] m`.
  - Facet count: `36` triangles (`18` rectangular faces across the baseplate and two vertical cooling fins), total surface area `0.031200 m^2`.
- **Format & Validation Schema**:
  - Standard ASCII STL format beginning with `solid pass_science_heatsink_m` and terminating with `endsolid pass_science_heatsink_m`.
  - Validated in C++17 (`CadDataParser::parseAsciiStlFile`): rejects empty files, binary STL null bytes, nested solids, malformed/non-finite normal or vertex floats (`NaN`/`Inf`), non-3-vertex loops, and zero-area degenerate triangles.

## 2. Synthetic Nodal Temperature Point Cloud (`data/synthetic_ansys_nodal_temp.csv`)
- **Provenance & Synthetic Disclosure**: **SYNTHETIC DATASET** — Self-created analytical 3D fin-conduction nodal temperature distribution (`240` nodes). Explicitly labelled **synthetic** in both file header comments and the Workbench UI; **not** represented as an actual commercial ANSYS export. Released under **CC0 1.0 Public Domain / MIT License**.
- **Units & Columns**:
  - Exact required header: `node_id,x_m,y_m,z_m,temperature_C`
  - `node_id`: Unique positive 64-bit integer identifier (`1..240`).
  - `x_m`, `y_m`, `z_m`: Spatial nodal coordinates in **meters (`m`)**.
  - `temperature_C`: Steady-state nodal temperature in **degrees Celsius (`°C`)**, spanning `24.8126 °C` (fin tips) to `96.1852 °C` (heated base center).
- **Tested File-Size Limits**:
  - Both C++ parsers enforce a configurable memory safety cap of **50 MB** (`MAX_TESTED_STL_BYTES = 52,428,800` bytes; `MAX_TESTED_CSV_BYTES = 52,428,800` bytes), tested with synthetic stress buffers up to `250,000` triangles / `500,000` CSV rows without UI blocking.
