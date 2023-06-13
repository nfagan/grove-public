#pragma once

#include "common.hpp"
#include "grove/vk/query_pool.hpp"
#include "grove/common/identifier.hpp"
#include "grove/common/History.hpp"
#include <vector>
#include <unordered_map>
#include <string_view>
#include <string>

namespace grove::vk {

struct PhysicalDevice;

class Profiler {
private:
  static constexpr int query_pool_size = 8;

  struct TicEntry {
    uint32_t pool_index;
    uint32_t tic_query_index;
    bool expect_toc;
  };

  struct TimestampQueryPool {
    QueryPool pool;
    uint32_t query_count{};
    bool need_reset{};
  };

public:
  struct QueryHandle {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(QueryHandle, id)
    uint32_t id;
  };

  struct QueryEntry {
    std::string stat_str() const;
    int num_samples() const {
      return latest_samples.num_samples();
    }

    History<float, 32> latest_samples{};
  };

  struct BeginRenderInfo {
    VkCommandBuffer cmd;
    const RenderFrameInfo& frame_info;
  };

public:
  void initialize(VkDevice device_handle,
                  const PhysicalDevice& physical_device,
                  uint32_t queue_family,
                  uint32_t frame_queue_depth);
  void set_enabled(bool v) {
    change_enabled = v;
  }
  bool is_enabled() const {
    return enabled;
  }
  void terminate();
  void begin_render(const BeginRenderInfo& info);
  void tic(QueryHandle handle, VkCommandBuffer cmd, VkPipelineStageFlagBits stage);
  void toc(QueryHandle handle, VkCommandBuffer cmd, VkPipelineStageFlagBits stage);
  void tic(std::string_view id, VkCommandBuffer cmd, VkPipelineStageFlagBits stage);
  void toc(std::string_view id, VkCommandBuffer cmd, VkPipelineStageFlagBits stage);
  const QueryEntry* get(QueryHandle handle) const;
  const QueryEntry* get(std::string_view id) const;
  QueryHandle create_handle();

  static void set_global_profiler(Profiler* profiler);
  static void tic_global(std::string_view id, VkCommandBuffer cmd, VkPipelineStageFlagBits stage);
  static void toc_global(std::string_view id, VkCommandBuffer cmd, VkPipelineStageFlagBits stage);

private:
  struct FrameData {
    std::vector<TimestampQueryPool> query_pools;
    std::unordered_map<uint32_t, TicEntry> entries;
    std::vector<QueryHandle> pending_read;
  };

  VkDevice device_handle{};
  DynamicArray<FrameData, 2> frame_data;
  RenderFrameInfo current_frame_info;
  std::unordered_map<std::string_view, QueryHandle> string_to_handle;
  std::unordered_map<uint32_t, QueryEntry> query_entries;
  uint32_t next_handle_id{1};

  uint64_t time_stamp_mask{};
  float time_stamp_period{};
  bool initialized{};
  bool enabled{};
  Optional<bool> change_enabled;
};

namespace profile {

struct GlobalScopeHelper {
  GlobalScopeHelper(std::string_view id,
                    VkCommandBuffer cmd,
                    VkPipelineStageFlagBits tic_stage,
                    VkPipelineStageFlagBits toc_stage) :
                    id{id},
                    cmd{cmd},
                    tic_stage{tic_stage},
                    toc_stage{toc_stage} {
    vk::Profiler::tic_global(id, cmd, tic_stage);
  }
  ~GlobalScopeHelper() {
    vk::Profiler::toc_global(id, cmd, toc_stage);
  }

  std::string_view id;
  VkCommandBuffer cmd;
  VkPipelineStageFlagBits tic_stage;
  VkPipelineStageFlagBits toc_stage;
};

#define GROVE_VK_PROFILING_ENABLED (1)

#if GROVE_VK_PROFILING_ENABLED

#define GROVE_VK_PROFILE_SCOPE(id, cmd) \
 grove::vk::profile::GlobalScopeHelper((id), (cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT)

#else

#define GROVE_VK_PROFILE_SCOPE(...) 0;

#endif

} //  profile

}