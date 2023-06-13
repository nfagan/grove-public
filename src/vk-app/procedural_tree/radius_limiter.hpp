#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/OBB3.hpp"
#include "grove/common/identifier.hpp"
#include <vector>

namespace grove::bounds {

struct RadiusLimiter;

struct RadiusLimiterStats {
  int num_elements;
  int num_free_elements;
  int num_cells;
  int num_cell_indices;
  int num_free_cell_indices;
  int num_element_indices;
  int num_free_element_indices;
};

struct RadiusLimiterAggregateID {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(RadiusLimiterAggregateID, id)
  static RadiusLimiterAggregateID create();
  uint32_t id;
};

struct RadiusLimiterElementTag {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(RadiusLimiterElementTag, tag)
  GROVE_INTEGER_IDENTIFIER_IS_VALID(tag)
  static RadiusLimiterElementTag create();
  uint16_t tag;
};

struct RadiusLimiterElement {
  static RadiusLimiterElement create_enclosing_obb3(
    const OBB3f& bounds, RadiusLimiterAggregateID id, RadiusLimiterElementTag tag);

  OBB3f to_obb(float r) const {
    return OBB3f{i, j, k, p, Vec3f{r, half_length, r}};
  }

  Vec3f i;
  Vec3f j;
  Vec3f k;
  Vec3f p;
  float half_length;
  float radius;
  bool reached_maximum_radius;
  RadiusLimiterAggregateID aggregate_id;
  RadiusLimiterElementTag tag;
};

struct RadiusLimiterElementHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(RadiusLimiterElementHandle, index)

  static RadiusLimiterElementHandle invalid() {
    return RadiusLimiterElementHandle{-1};
  }

  int index;
};

RadiusLimiter* create_radius_limiter();
void destroy_radius_limiter(RadiusLimiter** lim);

[[nodiscard]] RadiusLimiterElementHandle insert(RadiusLimiter* lim, RadiusLimiterElement el,
                                                bool pad_radius = true);
void remove(RadiusLimiter* lim, RadiusLimiterElementHandle el);

float expand(RadiusLimiter* lim, RadiusLimiterElementHandle element, float target_radius);
const RadiusLimiterElement* read_element(const RadiusLimiter* lim, RadiusLimiterElementHandle elem);

//  @NOTE: Ignores intersections between elements with the same aggregate id.
bool intersects_other(const RadiusLimiter* lim, RadiusLimiterElement el);
//  @NOTE: Does not consider aggregate id.
bool intersects_other_tag(const RadiusLimiter* lim, const OBB3f& obb, RadiusLimiterElementTag tag);

int gather_intersecting(const RadiusLimiter* lim, RadiusLimiterElement el,
                        std::vector<RadiusLimiterElement>& out);
int gather_intersecting(const RadiusLimiter* lim, const OBB3f& bounds,
                        std::vector<RadiusLimiterElement>& out);

int gather_intersecting_line(const RadiusLimiter* lim, const Vec3f& p0, const Vec3f& p1,
                             std::vector<const RadiusLimiterElement*>& out);

RadiusLimiterStats get_stats(const RadiusLimiter* lim);
void validate(const RadiusLimiter* lim);

void histogram(const RadiusLimiter* lim, const Vec3<int16_t>& ori,
               const Vec3<int16_t>& pow2_cell_size, const Vec3<int16_t>& cell_counts,
               uint32_t aggregate, int* freqs);
void filter_histogram(const int* freqs, const Vec3<int16_t>& cell_counts, float* tmp, float* out);
Vec3f mean_gradient(const float* hist, const Vec3<int16_t>& cell_counts);

}