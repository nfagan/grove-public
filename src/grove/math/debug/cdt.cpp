#include "cdt.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include <fstream>

GROVE_NAMESPACE_BEGIN

bool cdt::debug::write_triangulation(const char* file_path,
                                     const Triangle* tris,
                                     uint32_t num_tris,
                                     const Point* points,
                                     uint32_t num_points) {
  std::fstream file(file_path, std::ios::binary | std::ios::out);
  if (!file.good()) {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to open file.", "cdt");
    return false;
  }
  file.write((char*) &num_tris, sizeof(uint32_t));
  file.write((char*) &num_points, sizeof(uint32_t));
  file.write((char*) tris, sizeof(Triangle) * num_tris);
  file.write((char*) points, sizeof(Point) * num_points);
  return true;
}

bool cdt::debug::read_triangulation(const char* file_path, std::vector<Triangle>* tris,
                                    std::vector<Point>* points) {
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file.good()) {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to open file.", "cdt");
    return false;
  }

  const auto size = uint64_t(file.tellg());
  file.seekg(0, std::ios::beg);
  if (size < sizeof(uint32_t) * 2) {
    GROVE_LOG_ERROR_CAPTURE_META("Invalid triangulation file format.", "cdt");
    return false;
  }

  uint32_t nt;
  file.read((char*) &nt, sizeof(uint32_t));
  uint32_t np;
  file.read((char*) &np, sizeof(uint32_t));
  if (np * sizeof(Point) + nt * sizeof(Triangle) + sizeof(uint32_t) * 2 != size) {
    GROVE_LOG_ERROR_CAPTURE_META("Invalid triangulation file format.", "cdt");
    return false;
  }

  *tris = std::vector<Triangle>(nt);
  *points = std::vector<Point>(np);

  file.read((char*) tris->data(), sizeof(Triangle) * nt);
  file.read((char*) points->data(), sizeof(Point) * np);
  return true;
}

bool cdt::debug::write_triangulation3(const char* file_path,
                                      const Triangle* tris,
                                      uint32_t num_tris,
                                      const Vec3f* points,
                                      uint32_t num_points) {
  std::fstream file(file_path, std::ios::binary | std::ios::out);
  if (!file.good()) {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to open file.", "cdt");
    return false;
  }
  file.write((char*) &num_tris, sizeof(uint32_t));
  file.write((char*) &num_points, sizeof(uint32_t));
  file.write((char*) tris, sizeof(Triangle) * num_tris);
  file.write((char*) points, sizeof(Vec3f) * num_points);
  return true;
}

bool cdt::debug::write_points_edges(const char* file_path,
                                    const Point* points, uint32_t num_points,
                                    const Edge* edges, uint32_t num_edges) {
  std::fstream file(file_path, std::ios::binary | std::ios::out);
  if (!file.good()) {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to open file.", "cdt");
    return false;
  }
  file.write((char*) &num_points, sizeof(uint32_t));
  file.write((char*) &num_edges, sizeof(uint32_t));
  file.write((char*) points, num_points * sizeof(Point));
  file.write((char*) edges, num_edges * sizeof(Edge));
  return true;
}

bool cdt::debug::read_points_edges(const char* file_path,
                                   std::vector<Point>* points,
                                   std::vector<Edge>* edges) {
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file.good()) {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to open file.", "cdt");
    return false;
  }

  const auto size = uint64_t(file.tellg());
  file.seekg(0, std::ios::beg);
  if (size < sizeof(uint32_t) * 2) {
    GROVE_LOG_ERROR_CAPTURE_META("Invalid point file format.", "cdt");
    return false;
  }

  uint32_t np;
  file.read((char*) &np, sizeof(uint32_t));
  uint32_t nc;
  file.read((char*) &nc, sizeof(uint32_t));
  if (np * sizeof(Point) + sizeof(uint32_t) * 2 + nc * sizeof(Edge) != size) {
    GROVE_LOG_ERROR_CAPTURE_META("Invalid point file format.", "cdt");
    return false;
  }

  *points = std::vector<Point>(np);
  *edges = std::vector<Edge>(nc);
  file.read((char*) points->data(), sizeof(Point) * np);
  file.read((char*) edges->data(), sizeof(Edge) * nc);
  return true;
}

GROVE_NAMESPACE_END
