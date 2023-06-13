#pragma once

#include "common.hpp"

namespace grove::weather {

struct Status;

terrain::GlobalRenderParams terrain_render_params_from_status(const Status& status);

}