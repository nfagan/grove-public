#include "growth.hpp"
#include "environment_sample.hpp"
#include "environment_input.hpp"
#include "bud_fate.hpp"
#include "render.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Stopwatch.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

bool finished_growing(const GrowableTree& tree, int num_internodes) {
  return num_internodes >= tree.max_num_internodes ||
         num_internodes == tree.last_num_internodes;
}

int check_trees_finished_growing(GrowthContext* context) {
  int num_growing{};
  for (auto& tree : context->trees) {
    if (!tree.finished_growing) {
      const auto num_internodes = int(tree.nodes->internodes.size());
      if (finished_growing(tree, num_internodes)) {
        tree.finished_growing = true;
      } else {
        num_growing++;
      }
      tree.last_num_internodes = num_internodes;
    }
  }
  return num_growing;
}

void initialize_growth_cycle(GrowthContext* context) {
  context->environment_input->clear();
  context->sense_context->clear();
}

void consume(GrowthContext* context) {
  for (auto& tree : context->trees) {
    if (!tree.finished_growing) {
      const tree::TreeID id = tree.nodes->id;
      for (auto& bud : tree.nodes->buds) {
        consume_within_occupancy_zone(id, bud, *context->attraction_points);
      }
    }
  }
}

void sense(GrowthContext* context) {
  for (auto& tree : context->trees) {
    if (!tree.finished_growing) {
      for (auto& bud : tree.nodes->buds) {
        sense_bud(bud, *context->attraction_points, *context->sense_context);
      }
    }
  }
  *context->environment_input = compute_environment_input(
    context->sense_context->closest_points_to_buds);
}

void apply_environment_input(GrowthContext* context) {
  for (auto& tree : context->trees) {
    if (!tree.finished_growing) {
      apply_environment_input(*tree.nodes, *context->environment_input, *tree.bud_q_params);
    }
  }
}

void compute_bud_fate(GrowthContext* context) {
  for (auto& tree : context->trees) {
    if (!tree.finished_growing) {
      bud_fate(*tree.nodes, *context->environment_input, *tree.spawn_params);
    }
  }
}

void set_render_positions(GrowthContext* context) {
  for (auto& tree : context->trees) {
    if (!tree.finished_growing) {
      set_render_position(tree.nodes->internodes, 0);
    }
  }
}

int growth_cycle(GrowthContext* context) {
  consume(context);
  sense(context);
  apply_environment_input(context);
  compute_bud_fate(context);
  set_render_positions(context);
  const int num_still_growing = check_trees_finished_growing(context);
  return num_still_growing;
}

void insert_attraction_points(GrowthContext* context) {
  Vec3f* points_buff = context->attraction_points_buffer.begin();
  const auto max_num_points = int(context->attraction_points_buffer.size());

  for (auto& tree : context->trees) {
    const int num_added = (*tree.make_attraction_points)(points_buff, max_num_points);
    const uint32_t id = tree.nodes->id.id;
    for (int j = 0; j < num_added; j++) {
      auto& p = points_buff[j];
      context->attraction_points->insert(p, tree::make_attraction_point(p, id));
    }
  }
}

void start_growing(GrowthContext* context) {
  insert_attraction_points(context);
}

} //  anon

GrowableTree tree::make_growable_tree(TreeNodeStore* nodes,
                                      const SpawnInternodeParams* spawn_params,
                                      const DistributeBudQParams* bud_q_params,
                                      const MakeAttractionPoints* make_attraction_points,
                                      int max_num_internodes) {
  GrowableTree result{};
  result.nodes = nodes;
  result.spawn_params = spawn_params;
  result.bud_q_params = bud_q_params;
  result.make_attraction_points = make_attraction_points;
  result.max_num_internodes = max_num_internodes;
  result.last_num_internodes = int(nodes->internodes.size());
  return result;
}

GrowthResult tree::grow(GrowthContext* context) {
  Stopwatch stopwatch;

  start_growing(context);
  while (true) {
    initialize_growth_cycle(context);
    if (int num_growing = growth_cycle(context); num_growing == 0) {
      break;
    }
  }

  GrowthResult result{};
  result.elapsed_time = stopwatch.delta().count();
  return result;
}

GROVE_NAMESPACE_END
