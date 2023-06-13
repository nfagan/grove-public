#include "instancing.hpp"
#include "FrustumGrid.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/random.hpp"
#include "grove/math/vector.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

int calculate_num_instances(const Vec2f& cell_size, float density,
                            int num_cells, int max_num_instances) {
  const float area = cell_size.x * cell_size.y;
  int num_instances = int(area * density * float(num_cells));

  if (num_instances > max_num_instances) {
    const int num_per_cell = max_num_instances / num_cells;
    num_instances = num_per_cell * num_cells;
  }

  return num_instances;
}

inline void random_policy(std::vector<float>& instance_data, int index, int vertex_size) {
  instance_data[index * vertex_size] = grove::rand();
  instance_data[index * vertex_size + 1] = grove::rand();
  //  blade rotation
  instance_data[index * vertex_size + 3] = grove::rand() * grove::pi();
}

inline void alternating_offsets_policy(std::vector<float>& instance_data,
                                       int index, int j, int num_per_dim,
                                       int vertex_size, float placement_offset) {
  const int num_per_coord = 1;
  const int denom = num_per_dim < num_per_coord ? 1 : num_per_dim / num_per_coord;

  const float x = float((j / num_per_coord) % denom) / float(denom) + placement_offset;
  const float y = float((j / num_per_coord) / denom) / float(denom) + placement_offset;

  const float rand_x = (grove::urand_closed() - 0.5f) * 2.0f * 0.2f;
  const float rand_y = (grove::urand_closed() - 0.5f) * 2.0f * 0.2f;

  instance_data[index * vertex_size] = x + rand_x;
  instance_data[index * vertex_size + 1] = y + rand_y;

  const float frac_denom = num_per_coord == 1 ? 1.0f : float(num_per_coord-1);
  const float theta =
    grove::pi()/1.5f * float(j % num_per_coord) / frac_denom + (grove::rand() * grove::pi());

  instance_data[index * vertex_size + 3] = theta;
}

inline void alternating_offsets_policy2(std::vector<float>& instance_data,
                                        int index, int j, int num_per_dim,
                                        int vertex_size, float placement_offset,
                                        float displacement_magnitude) {
  const int num_per_coord = 2;
  const int denom = num_per_dim < num_per_coord ? 1 : num_per_dim / num_per_coord;

  const float x = float((j / num_per_coord) % denom) / float(denom) + placement_offset;
  const float y = float((j / num_per_coord) / denom) / float(denom) + placement_offset;

  const float rand_x = (grove::urand_closed() - 0.5f) * 2.0f * displacement_magnitude;
  const float rand_y = (grove::urand_closed() - 0.5f) * 2.0f * displacement_magnitude;

  instance_data[index * vertex_size] = x + rand_x;
  instance_data[index * vertex_size + 1] = y + rand_y;

  const float theta =
    index % num_per_coord == 0 ? grove::rand() * 0.5f : 1.0f - grove::rand() * 0.5f;
  instance_data[index * vertex_size + 3] = theta;
}

inline void golden_ratio_policy(std::vector<float>& instance_data,
                                float displacement_magnitude,
                                Vec2f& gr_offsets,
                                int index,
                                int vertex_size) {
  auto x = std::fmod(gr_offsets.x + grove::golden_ratio(), 1.0);
  auto z = std::fmod(gr_offsets.y + grove::golden_ratio(), 1.0);

  gr_offsets.x = x;
  gr_offsets.y = z;

  auto rand_x = (grove::urand_closed() - 0.5f) * 2.0f * displacement_magnitude;
  auto rand_z = (grove::urand_closed() - 0.5f) * 2.0f * displacement_magnitude;

  instance_data[index * vertex_size] = x + rand_x;
  instance_data[index * vertex_size + 1] = z + rand_z;

  const float theta = grove::urand() * 0.5f;
  instance_data[index * vertex_size + 3] = theta;
}

std::vector<float> make_instance_data(int num_instances,
                                      int num_cells,
                                      int texture_width,
                                      const GrassInstanceOptions& options) {
  (void) texture_width;
  const auto placement_policy = options.placement_policy;

  int index = 0;
  const int vertex_size = 4;
  std::vector<float> instance_data(num_instances * vertex_size);

  const int num_instances_per_cell = num_instances / num_cells;
  const int num_per_dim = std::ceil(std::sqrt(float(num_instances_per_cell)));

  auto gr_start = Vec2f(grove::urand(), grove::urand());

  for (int i = 0; i < num_cells; i++) {
    for (int j = 0; j < num_instances_per_cell; j++) {

      if (placement_policy == InstancePlacementPolicy::Random) {
        random_policy(instance_data, index, vertex_size);

      } else if (placement_policy == InstancePlacementPolicy::AlternatingOffsets) {
        alternating_offsets_policy(instance_data, index, j,
          num_per_dim, vertex_size, options.placement_offset);

      } else if (placement_policy == InstancePlacementPolicy::AlternatingOffsets2) {
        alternating_offsets_policy2(instance_data, index, j,
          num_per_dim, vertex_size, options.placement_offset, options.displacement_magnitude);

      } else if (placement_policy == InstancePlacementPolicy::GoldenRatio) {
        golden_ratio_policy(instance_data, options.displacement_magnitude,
          gr_start, index, vertex_size);
      }

      //  grid index
      instance_data[index * vertex_size + 2] = float(i);

      index++;
    }
  }

  return instance_data;
}

}

grove::FrustumGridInstanceData
make_frustum_grid_instance_data(const FrustumGrid& grid, const GrassInstanceOptions& options) {
  FrustumGridInstanceData result;
  
  const Vec2f& cell_size = grid.get_cell_size();
  int num_cells = grid.get_num_cells();
  
  result.num_instances =
    calculate_num_instances(cell_size, options.density, num_cells, options.max_num_instances);
  result.data =
    make_instance_data(result.num_instances, num_cells, std::sqrt(grid.get_data_size()), options);
  
  result.buffer_descriptor.add_attribute(AttributeDescriptor::float2(0, 1));
  result.buffer_descriptor.add_attribute(AttributeDescriptor::float1(1, 1));
  result.buffer_descriptor.add_attribute(AttributeDescriptor::float1(2, 1));
  
  return result;
}


GROVE_NAMESPACE_END