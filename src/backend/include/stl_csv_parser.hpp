#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace workbench {

struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

struct StlTriangle {
    Vec3 normal;
    Vec3 v0;
    Vec3 v1;
    Vec3 v2;
};

struct StlMeshResult {
    bool valid{false};
    std::string error_message;
    std::string solid_name;
    std::vector<StlTriangle> triangles;
    Vec3 bbox_min{0.0, 0.0, 0.0};
    Vec3 bbox_max{0.0, 0.0, 0.0};
    double total_surface_area_m2{0.0};
    std::size_t file_size_bytes{0};
};

struct NodalTemperature {
    int64_t node_id{0};
    double x_m{0.0};
    double y_m{0.0};
    double z_m{0.0};
    double temperature_C{0.0};
};

struct NodalCsvResult {
    bool valid{false};
    std::string error_message;
    std::vector<NodalTemperature> nodes;
    double min_temp_C{0.0};
    double max_temp_C{0.0};
    double mean_temp_C{0.0};
    Vec3 bbox_min{0.0, 0.0, 0.0};
    Vec3 bbox_max{0.0, 0.0, 0.0};
    std::size_t file_size_bytes{0};
};

class CadDataParser {
public:
    static constexpr std::size_t MAX_TESTED_STL_BYTES = 50ULL * 1024ULL * 1024ULL; // 50 MB
    static constexpr std::size_t MAX_TESTED_CSV_BYTES = 50ULL * 1024ULL * 1024ULL; // 50 MB

    static StlMeshResult parseAsciiStlString(const std::string& content, std::size_t file_bytes = 0);
    static StlMeshResult parseAsciiStlFile(const std::string& filepath);

    static NodalCsvResult parseAnsysCsvString(const std::string& content, std::size_t file_bytes = 0);
    static NodalCsvResult parseAnsysCsvFile(const std::string& filepath);
};

} // namespace workbench
