#pragma once

#include "components.hpp"

namespace grove::tree {

void consume_within_occupancy_zone(TreeID parent_id, const Bud& bud, AttractionPoints& points);
void sense_bud(const Bud& bud, AttractionPoints& points, SenseContext& context);
EnvironmentInputs compute_environment_input(const ClosestPointsToBuds& closest);

}