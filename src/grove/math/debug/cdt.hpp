#pragma once

#include "../cdt.hpp"
#include "../Vec3.hpp"

namespace grove::cdt::debug {

bool write_triangulation(const char* file_path, const Triangle* tri, uint32_t num_tris,
                         const Vec2<double>* points, uint32_t num_points);
bool read_triangulation(const char* file_path, std::vector<Triangle>* tris,
                        std::vector<Point>* points);
bool write_triangulation3(const char* file_path, const Triangle* tri, uint32_t num_tris,
                          const Vec3f* points, uint32_t num_points);
bool write_points_edges(const char* file_path, const Point* points, uint32_t num_points,
                        const Edge* edges, uint32_t num_edges);
bool read_points_edges(const char* file_path, std::vector<Point>* points, std::vector<Edge>* edges);

}