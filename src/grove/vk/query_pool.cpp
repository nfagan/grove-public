#include "query_pool.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

VkQueryPoolCreateInfo vk::make_query_pool_create_info(VkQueryType query_type,
                                                      uint32_t query_count,
                                                      VkQueryPipelineStatisticFlags pipeline_stats) {
  VkQueryPoolCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  result.queryCount = query_count;
  result.queryType = query_type;
  result.pipelineStatistics = pipeline_stats;
  return result;
}

Result<QueryPool>
vk::create_query_pool(VkDevice device, const VkQueryPoolCreateInfo* create_info) {
  VkQueryPool handle;
  auto res = vkCreateQueryPool(device, create_info, GROVE_VK_ALLOC, &handle);
  if (res != VK_SUCCESS) {
    return {res, "Failed to create query pool."};
  } else {
    QueryPool result{};
    result.handle = handle;
    result.query_type = create_info->queryType;
    result.max_num_queries = create_info->queryCount;
    return result;
  }
}

void vk::destroy_query_pool(QueryPool* pool, VkDevice device) {
  if (device != VK_NULL_HANDLE) {
    vkDestroyQueryPool(device, pool->handle, GROVE_VK_ALLOC);
    pool->handle = VK_NULL_HANDLE;
    pool->max_num_queries = 0;
    pool->query_type = {};
  } else {
    GROVE_ASSERT(pool->handle == VK_NULL_HANDLE);
  }
}

GROVE_NAMESPACE_END
