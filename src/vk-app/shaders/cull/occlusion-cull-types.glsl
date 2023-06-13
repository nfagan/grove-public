struct OcclusionCullResult {
  uint status;
};

#ifndef OCCLUSION_CULL_RESULT_OCCLUDED
#error "missing OCCLUSION_CULL_RESULT_OCCLUDED define"
#endif

#ifndef OCCLUSION_CULL_RESULT_VISIBLE
#error "missing OCCLUSION_CULL_RESULT_VISIBLE define"
#endif