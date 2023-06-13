#include "shadow.hpp"
#include "csm.hpp"
#include "grove/glsl/preprocess.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

csm::SunCSMSampleData csm::make_sun_csm_sample_data(const csm::CSMDescriptor& descr) {
  static_assert(GROVE_NUM_SUN_SHADOW_CASCADES <= 4);
  SunCSMSampleData result{};
  for (int i = 0; i < GROVE_NUM_SUN_SHADOW_CASCADES; i++) {
    result.shadow_cascade_extents[i] = descr.ith_cascade_extent(i);
    result.shadow_cascade_uv_scales[i] = Vec4f{descr.uv_transforms[i].scale, 0.0f};
    result.shadow_cascade_uv_offsets[i] = Vec4f{descr.uv_transforms[i].offset, 0.0f};
  }
  return result;
}

glsl::PreprocessorDefinition csm::make_num_sun_shadow_cascades_preprocessor_definition() {
  return glsl::PreprocessorDefinition{
    "NUM_SUN_SHADOW_CASCADES", std::to_string(GROVE_NUM_SUN_SHADOW_CASCADES), true
  };
}

glsl::PreprocessorDefinition csm::make_default_num_sun_shadow_samples_preprocessor_definition() {
  return glsl::PreprocessorDefinition{
    "NUM_SHADOW_SAMPLES", std::to_string(GROVE_DEFAULT_NUM_SUN_SHADOW_SAMPLES), true
  };
}

glsl::PreprocessorDefinitions csm::make_default_sample_shadow_preprocessor_definitions() {
  glsl::PreprocessorDefinitions result;
  result.push_back(make_num_sun_shadow_cascades_preprocessor_definition());
  result.push_back(make_default_num_sun_shadow_samples_preprocessor_definition());
  return result;
}

GROVE_NAMESPACE_END
