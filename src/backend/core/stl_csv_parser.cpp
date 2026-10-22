#include "stl_csv_parser.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <cctype>

namespace workbench {

namespace {

std::string trim(const std::string& s) {
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

bool parseStrictDouble(const std::string& token, double& out) {
    std::string t = trim(token);
    if (t.empty()) return false;
    // Reject explicit NaN / Inf strings immediately
    std::string lower = t;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower.find("nan") != std::string::npos || lower.find("inf") != std::string::npos) {
        return false;
    }
    char* endptr = nullptr;
    out = std::strtod(t.c_str(), &endptr);
    if (endptr == t.c_str() || *endptr != '\0') {
        return false;
    }
    return std::isfinite(out);
}

bool parseStrictInt64(const std::string& token, int64_t& out) {
    std::string t = trim(token);
    if (t.empty()) return false;
    char* endptr = nullptr;
    out = std::strtoll(t.c_str(), &endptr, 10);
    if (endptr == t.c_str() || *endptr != '\0') {
        return false;
    }
    return true;
}

double triangleArea(const Vec3& a, const Vec3& b, const Vec3& c) {
    double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
    double vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    double cx = uy * vz - uz * vy;
    double cy = uz * vx - ux * vz;
    double cz = ux * vy - uy * vx;
    return 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
}

} // namespace

StlMeshResult CadDataParser::parseAsciiStlFile(const std::string& filepath) {
    std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        StlMeshResult res;
        res.error_message = "Unable to open STL file: '" + filepath + "'. Verify relative path and read permissions.";
        return res;
    }
    std::streamsize sz = ifs.tellg();
    if (sz < 0) sz = 0;
    if (static_cast<std::size_t>(sz) == 0) {
        StlMeshResult res;
        res.error_message = "Empty STL file ('" + filepath + "', 0 bytes). Expected an ASCII STL starting with 'solid <name>'.";
        return res;
    }
    if (static_cast<std::size_t>(sz) > MAX_TESTED_STL_BYTES) {
        StlMeshResult res;
        res.error_message = "STL file exceeds tested 50 MB memory safety limit (" + std::to_string(sz) + " bytes).";
        return res;
    }
    ifs.seekg(0, std::ios::beg);
    std::string content(static_cast<std::size_t>(sz), '\0');
    ifs.read(&content[0], sz);
    return parseAsciiStlString(content, static_cast<std::size_t>(sz));
}

StlMeshResult CadDataParser::parseAsciiStlString(const std::string& content, std::size_t file_bytes) {
    StlMeshResult res;
    res.file_size_bytes = file_bytes > 0 ? file_bytes : content.size();

    std::string trimmed_all = trim(content);
    if (trimmed_all.empty()) {
        res.error_message = "Empty STL input (contains only whitespace or 0 bytes). Expected ASCII STL format.";
        return res;
    }

    // Check for binary STL null bytes
    if (content.find('\0') != std::string::npos) {
        res.error_message = "Binary STL or non-ASCII characters detected. Only ASCII STL files are supported in Task B.";
        return res;
    }

    std::istringstream iss(content);
    std::string line;
    std::size_t line_num = 0;
    bool in_solid = false;
    bool in_facet = false;
    bool in_loop = false;
    bool saw_endsolid = false;
    StlTriangle current_tri{};
    int vertex_count = 0;

    while (std::getline(iss, line)) {
        ++line_num;
        std::string t = trim(line);
        if (t.empty()) continue;

        std::istringstream ls(t);
        std::string keyword;
        ls >> keyword;

        if (keyword == "solid") {
            if (in_solid) {
                res.error_message = "Line " + std::to_string(line_num) + ": Nested 'solid' declaration encountered.";
                return res;
            }
            in_solid = true;
            std::string rest;
            std::getline(ls, rest);
            res.solid_name = trim(rest);
            if (res.solid_name.empty()) res.solid_name = "unnamed_solid";
        } else if (keyword == "facet") {
            if (!in_solid || in_facet) {
                res.error_message = "Line " + std::to_string(line_num) + ": Unexpected 'facet' outside solid or inside another facet.";
                return res;
            }
            std::string norm_kw, nx_s, ny_s, nz_s;
            ls >> norm_kw >> nx_s >> ny_s >> nz_s;
            if (norm_kw != "normal" || !parseStrictDouble(nx_s, current_tri.normal.x) ||
                !parseStrictDouble(ny_s, current_tri.normal.y) || !parseStrictDouble(nz_s, current_tri.normal.z)) {
                res.error_message = "Line " + std::to_string(line_num) + ": Malformed or non-finite facet normal vector.";
                return res;
            }
            in_facet = true;
            vertex_count = 0;
        } else if (keyword == "outer") {
            std::string loop_kw;
            ls >> loop_kw;
            if (!in_facet || in_loop || loop_kw != "loop") {
                res.error_message = "Line " + std::to_string(line_num) + ": Expected 'outer loop' inside facet.";
                return res;
            }
            in_loop = true;
        } else if (keyword == "vertex") {
            if (!in_loop || vertex_count >= 3) {
                res.error_message = "Line " + std::to_string(line_num) + ": Unexpected 'vertex' statement.";
                return res;
            }
            std::string vx_s, vy_s, vz_s, extra;
            ls >> vx_s >> vy_s >> vz_s;
            if ((ls >> extra) || ! ( [&]() {
                Vec3 v;
                if (!parseStrictDouble(vx_s, v.x) || !parseStrictDouble(vy_s, v.y) || !parseStrictDouble(vz_s, v.z))
                    return false;
                if (vertex_count == 0) current_tri.v0 = v;
                else if (vertex_count == 1) current_tri.v1 = v;
                else current_tri.v2 = v;
                return true;
            }() )) {
                res.error_message = "Line " + std::to_string(line_num) + ": Malformed or non-finite vertex coordinates.";
                return res;
            }
            ++vertex_count;
        } else if (keyword == "endloop") {
            if (!in_loop || vertex_count != 3) {
                res.error_message = "Line " + std::to_string(line_num) + ": 'endloop' requires exactly 3 vertices.";
                return res;
            }
            in_loop = false;
        } else if (keyword == "endfacet") {
            if (!in_facet || in_loop) {
                res.error_message = "Line " + std::to_string(line_num) + ": 'endfacet' without matching closed loop.";
                return res;
            }
            double area = triangleArea(current_tri.v0, current_tri.v1, current_tri.v2);
            if (area <= 1e-18) {
                res.error_message = "Line " + std::to_string(line_num) + ": Degenerate zero-area triangle detected.";
                return res;
            }
            res.total_surface_area_m2 += area;
            res.triangles.push_back(current_tri);
            in_facet = false;
        } else if (keyword == "endsolid") {
            if (!in_solid || in_facet) {
                res.error_message = "Line " + std::to_string(line_num) + ": Unexpected 'endsolid' while facet still open.";
                return res;
            }
            saw_endsolid = true;
            in_solid = false;
            break;
        } else {
            res.error_message = "Line " + std::to_string(line_num) + ": Unrecognized STL keyword '" + keyword + "'.";
            return res;
        }
    }

    if (!saw_endsolid) {
        res.error_message = "Incomplete ASCII STL file: missing closing 'endsolid' statement.";
        return res;
    }
    if (res.triangles.empty()) {
        res.error_message = "STL solid contains zero triangles.";
        return res;
    }

    // Compute bounding box
    res.bbox_min = res.triangles[0].v0;
    res.bbox_max = res.triangles[0].v0;
    auto update_bbox = [&](const Vec3& v) {
        res.bbox_min.x = std::min(res.bbox_min.x, v.x);
        res.bbox_min.y = std::min(res.bbox_min.y, v.y);
        res.bbox_min.z = std::min(res.bbox_min.z, v.z);
        res.bbox_max.x = std::max(res.bbox_max.x, v.x);
        res.bbox_max.y = std::max(res.bbox_max.y, v.y);
        res.bbox_max.z = std::max(res.bbox_max.z, v.z);
    };
    for (const auto& tri : res.triangles) {
        update_bbox(tri.v0);
        update_bbox(tri.v1);
        update_bbox(tri.v2);
    }

    res.valid = true;
    return res;
}

NodalCsvResult CadDataParser::parseAnsysCsvFile(const std::string& filepath) {
    std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        NodalCsvResult res;
        res.error_message = "Unable to open CSV file: '" + filepath + "'. Verify relative path and read permissions.";
        return res;
    }
    std::streamsize sz = ifs.tellg();
    if (sz < 0) sz = 0;
    if (static_cast<std::size_t>(sz) == 0) {
        NodalCsvResult res;
        res.error_message = "Empty CSV file ('" + filepath + "', 0 bytes). Expected header 'node_id,x_m,y_m,z_m,temperature_C'.";
        return res;
    }
    if (static_cast<std::size_t>(sz) > MAX_TESTED_CSV_BYTES) {
        NodalCsvResult res;
        res.error_message = "CSV file exceeds tested 50 MB memory safety limit (" + std::to_string(sz) + " bytes).";
        return res;
    }
    ifs.seekg(0, std::ios::beg);
    std::string content(static_cast<std::size_t>(sz), '\0');
    ifs.read(&content[0], sz);
    return parseAnsysCsvString(content, static_cast<std::size_t>(sz));
}

NodalCsvResult CadDataParser::parseAnsysCsvString(const std::string& content, std::size_t file_bytes) {
    NodalCsvResult res;
    res.file_size_bytes = file_bytes > 0 ? file_bytes : content.size();

    std::string trimmed_all = trim(content);
    if (trimmed_all.empty()) {
        res.error_message = "Empty CSV input (contains only whitespace or 0 bytes). Expected header 'node_id,x_m,y_m,z_m,temperature_C'.";
        return res;
    }

    std::istringstream iss(content);
    std::string line;
    std::size_t line_num = 0;
    bool header_parsed = false;
    std::unordered_set<int64_t> seen_ids;

    while (std::getline(iss, line)) {
        ++line_num;
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;

        if (!header_parsed) {
            // Validate required schema: node_id,x_m,y_m,z_m,temperature_C
            std::string normalized;
            for (char c : t) {
                if (!std::isspace(static_cast<unsigned char>(c))) normalized.push_back(c);
            }
            if (normalized != "node_id,x_m,y_m,z_m,temperature_C") {
                res.error_message = "Line " + std::to_string(line_num) +
                    ": Invalid CSV schema header '" + t +
                    "'. Required exact columns: node_id,x_m,y_m,z_m,temperature_C";
                return res;
            }
            header_parsed = true;
            continue;
        }

        // Split by comma
        std::vector<std::string> cols;
        std::stringstream ss(t);
        std::string item;
        while (std::getline(ss, item, ',')) {
            cols.push_back(item);
        }
        if (!t.empty() && t.back() == ',') cols.push_back("");

        if (cols.size() != 5) {
            res.error_message = "Line " + std::to_string(line_num) +
                ": Expected 5 columns (node_id,x_m,y_m,z_m,temperature_C), found " +
                std::to_string(cols.size()) + ".";
            return res;
        }

        NodalTemperature node{};
        if (!parseStrictInt64(cols[0], node.node_id) || node.node_id <= 0) {
            res.error_message = "Line " + std::to_string(line_num) +
                ": Invalid or non-positive node_id '" + cols[0] + "'.";
            return res;
        }
        if (seen_ids.count(node.node_id)) {
            res.error_message = "Line " + std::to_string(line_num) +
                ": Duplicate node_id " + std::to_string(node.node_id) + " detected.";
            return res;
        }
        if (!parseStrictDouble(cols[1], node.x_m) ||
            !parseStrictDouble(cols[2], node.y_m) ||
            !parseStrictDouble(cols[3], node.z_m) ||
            !parseStrictDouble(cols[4], node.temperature_C)) {
            res.error_message = "Line " + std::to_string(line_num) +
                ": Non-finite (NaN/Inf) or malformed numeric value in row.";
            return res;
        }
        // Absolute zero physical sanity check (-273.15 C)
        if (node.temperature_C < -273.15 || node.temperature_C > 10000.0) {
            res.error_message = "Line " + std::to_string(line_num) +
                ": Physically out-of-range temperature (" + std::to_string(node.temperature_C) + " C).";
            return res;
        }

        seen_ids.insert(node.node_id);
        res.nodes.push_back(node);
    }

    if (!header_parsed || res.nodes.empty()) {
        res.error_message = "CSV file contains header or comments only — zero valid nodal records found.";
        return res;
    }

    res.min_temp_C = res.nodes[0].temperature_C;
    res.max_temp_C = res.nodes[0].temperature_C;
    double sum_temp = 0.0;
    res.bbox_min = {res.nodes[0].x_m, res.nodes[0].y_m, res.nodes[0].z_m};
    res.bbox_max = res.bbox_min;

    for (const auto& n : res.nodes) {
        res.min_temp_C = std::min(res.min_temp_C, n.temperature_C);
        res.max_temp_C = std::max(res.max_temp_C, n.temperature_C);
        sum_temp += n.temperature_C;
        res.bbox_min.x = std::min(res.bbox_min.x, n.x_m);
        res.bbox_min.y = std::min(res.bbox_min.y, n.y_m);
        res.bbox_min.z = std::min(res.bbox_min.z, n.z_m);
        res.bbox_max.x = std::max(res.bbox_max.x, n.x_m);
        res.bbox_max.y = std::max(res.bbox_max.y, n.y_m);
        res.bbox_max.z = std::max(res.bbox_max.z, n.z_m);
    }
    res.mean_temp_C = sum_temp / static_cast<double>(res.nodes.size());
    res.valid = true;
    return res;
}

} // namespace workbench
