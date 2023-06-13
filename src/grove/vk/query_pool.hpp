#pragma once

#include "common.hpp"

namespace grove::vk {

struct QueryPool {
  VkQueryPool handle{VK_NULL_HANDLE};
  VkQueryType query_type{};
  uint32_t max_num_queries{};
};

VkQueryPoolCreateInfo make_query_pool_create_info(VkQueryType query_type,
                                                  uint32_t query_count,
                                                  VkQueryPipelineStatisticFlags pipeline_stats);

Result<QueryPool> create_query_pool(VkDevice device, const VkQueryPoolCreateInfo* create_info);
void destroy_query_pool(QueryPool* pool, VkDevice device);

}