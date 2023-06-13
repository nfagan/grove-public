#pragma once

#define GROVE_NUM_SUN_SHADOW_CASCADES (3)
#define GROVE_DEFAULT_NUM_SUN_SHADOW_SAMPLES (4)

#include "grove/math/vector.hpp"
#include "grove/glsl/preprocess.hpp"

namespace grove::csm {

struct CSMDescriptor;

struct SunCSMSampleData {
  Vec4f shadow_cascade_extents;
  Vec4f shadow_cascade_uv_scales[GROVE_NUM_SUN_SHADOW_CASCADES];
  Vec4f shadow_cascade_uv_offsets[GROVE_NUM_SUN_SHADOW_CASCADES];
};

static_assert(alignof(SunCSMSampleData) == 4);

SunCSMSampleData make_sun_csm_sample_data(const csm::CSMDescriptor& descr);
glsl::PreprocessorDefinition make_num_sun_shadow_cascades_preprocessor_definition();
glsl::PreprocessorDefinition make_default_num_sun_shadow_samples_preprocessor_definition();
glsl::PreprocessorDefinitions make_default_sample_shadow_preprocessor_definitions();

}