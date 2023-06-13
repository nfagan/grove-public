#include "profiler.hpp"
#include "grove/vk/physical_device.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace globals {

Profiler* profiler{};

} //  globals

namespace {

uint64_t make_timestamp_mask(uint32_t num_valid_bits) {
  uint64_t res{};
  for (uint32_t i = 0; i < num_valid_bits; i++) {
    res |= (uint64_t(1u) << uint64_t(i));
  }
  return res;
}

void get_time_stamp_query_pool_results(VkDevice device,
                                       VkQueryPool pool,
                                       uint32_t query_ind,
                                       uint64_t* out) {
  const size_t data_size = sizeof(uint64_t) * 2;
  const size_t stride = sizeof(uint64_t);
  auto res = vkGetQueryPoolResults(
    device, pool, query_ind, 2, data_size, out, stride, VK_QUERY_RESULT_64_BIT);
  GROVE_ASSERT(res == VK_SUCCESS);
  (void) res;
}

} //  anon

void vk::Profiler::initialize(VkDevice device,
                              const PhysicalDevice& physical_device,
                              uint32_t queue_family,
                              uint32_t frame_queue_depth) {
  auto& queue_fam = physical_device.info.queue_families[queue_family];
  if (queue_fam.timestampValidBits == 0) {
    GROVE_ASSERT(false);
    return;
  } else {
    time_stamp_mask = make_timestamp_mask(queue_fam.timestampValidBits);
    time_stamp_period = physical_device.info.properties.limits.timestampPeriod;
  }
  for (uint32_t i = 0; i < frame_queue_depth; i++) {
    frame_data.emplace_back();
  }
  device_handle = device;
  initialized = true;
}

void vk::Profiler::terminate() {
  for (auto& fd : frame_data) {
    for (auto& pool : fd.query_pools) {
      vk::destroy_query_pool(&pool.pool, device_handle);
    }
  }
  frame_data.clear();
}

void vk::Profiler::begin_render(const BeginRenderInfo& info) {
  if (!initialized) {
    return;
  }

  if (change_enabled) {
    enabled = change_enabled.value();
    change_enabled = NullOpt{};
  }

  current_frame_info = info.frame_info;
  auto& fd = frame_data[info.frame_info.current_frame_index];

  for (auto& pend : fd.pending_read) {
    auto& entry = fd.entries.at(pend.id);
    auto& query_pool = fd.query_pools[entry.pool_index];
    VkQueryPool pool_handle = query_pool.pool.handle;
    const uint32_t query_ind = entry.tic_query_index;

    uint64_t time_stamps[2];
    get_time_stamp_query_pool_results(device_handle, pool_handle, query_ind, time_stamps);
    for (auto& ts : time_stamps) {
      ts &= time_stamp_mask;
    }

    QueryEntry* query_entry;
    if (auto query_it = query_entries.find(pend.id); query_it != query_entries.end()) {
      query_entry = &query_it->second;
    } else {
      query_entries[pend.id] = {};
      query_entry = &query_entries.at(pend.id);
    }

    GROVE_ASSERT(time_stamps[1] >= time_stamps[0]);
    auto elapsed_ms = 1e-6 * double(time_stamps[1] - time_stamps[0]) * double(time_stamp_period);
    query_entry->latest_samples.push(float(elapsed_ms));
  }

  fd.pending_read.clear();

  for (auto& pool : fd.query_pools) {
    if (pool.need_reset) {
      vkCmdResetQueryPool(info.cmd, pool.pool.handle, 0, pool.query_count);
      pool.need_reset = false;
    }
    pool.query_count = 0;
  }
}

Profiler::QueryHandle vk::Profiler::create_handle() {
  QueryHandle handle{next_handle_id++};
  return handle;
}

void vk::Profiler::tic(QueryHandle handle, VkCommandBuffer cmd, VkPipelineStageFlagBits stage) {
  if (!initialized || !enabled) {
    return;
  }
  auto& fd = frame_data[current_frame_info.current_frame_index];
  auto& query_pools = fd.query_pools;
  auto& entries = fd.entries;
  TimestampQueryPool* dst_pool{};
  for (auto& pool : fd.query_pools) {
    if (pool.query_count + 1 < query_pool_size) {
      //  Reserve 2 queries - tic and toc
      dst_pool = &pool;
      break;
    }
  }
  if (!dst_pool) {
    auto create_info = make_query_pool_create_info(
      VK_QUERY_TYPE_TIMESTAMP, query_pool_size, 0);
    if (auto create_res = create_query_pool(device_handle, &create_info)) {
      VkQueryPool pool_handle = create_res.value.handle;
      //  Reset before first use.
      vkCmdResetQueryPool(cmd, pool_handle, 0, query_pool_size);
      TimestampQueryPool new_pool{};
      new_pool.pool = create_res.value;
      query_pools.push_back(new_pool);
      dst_pool = &query_pools.back();
    } else {
      GROVE_ASSERT(false);
      return;
    }
  }

  GROVE_ASSERT(dst_pool->query_count + 1 < query_pool_size);
  const auto pool_ind = uint32_t(dst_pool - query_pools.data());
  const uint32_t query_ind = dst_pool->query_count;
  VkQueryPool pool_handle = dst_pool->pool.handle;
  vkCmdWriteTimestamp(cmd, stage, pool_handle, query_ind);

  dst_pool->query_count += 2; //  tic + toc
  dst_pool->need_reset = true;

  TicEntry entry{};
  entry.pool_index = pool_ind;
  entry.tic_query_index = query_ind;
  entry.expect_toc = true;

  if (auto it = entries.find(handle.id); it != entries.end()) {
    GROVE_ASSERT(!it->second.expect_toc);
    it->second = entry;
  } else {
    entries[handle.id] = entry;
  }
}

void vk::Profiler::toc(QueryHandle handle, VkCommandBuffer cmd, VkPipelineStageFlagBits stage) {
  if (!initialized || !enabled) {
    return;
  }
  auto& fd = frame_data[current_frame_info.current_frame_index];
  auto& query_pools = fd.query_pools;
  auto& entries = fd.entries;
  auto& pending = fd.pending_read;
  auto entry_it = entries.find(handle.id);
  if (entry_it == entries.end()) {
    GROVE_ASSERT(false);
    return;
  }
  auto& tic_entry = entry_it->second;
  GROVE_ASSERT(tic_entry.expect_toc);
  tic_entry.expect_toc = false;

  auto& pool = query_pools[tic_entry.pool_index];
  VkQueryPool pool_handle = pool.pool.handle;
  const uint32_t query_ind = tic_entry.tic_query_index + 1; //  @NOTE

  vkCmdWriteTimestamp(cmd, stage, pool_handle, query_ind);
  pending.push_back(handle);
}

void vk::Profiler::tic(std::string_view id, VkCommandBuffer cmd, VkPipelineStageFlagBits stage) {
  if (auto it = string_to_handle.find(id); it != string_to_handle.end()) {
    tic(it->second, cmd, stage);
  } else {
    auto handle = create_handle();
    string_to_handle[id] = handle;
    tic(handle, cmd, stage);
  }
}

void vk::Profiler::toc(std::string_view id, VkCommandBuffer cmd, VkPipelineStageFlagBits stage) {
  if (auto it = string_to_handle.find(id); it != string_to_handle.end()) {
    toc(it->second, cmd, stage);
  } else {
    //  Should get handle from tic first.
    GROVE_ASSERT(false);
  }
}

const Profiler::QueryEntry* vk::Profiler::get(std::string_view id) const {
  if (auto handle_it = string_to_handle.find(id); handle_it != string_to_handle.end()) {
    return get(handle_it->second);
  } else {
    return nullptr;
  }
}

const Profiler::QueryEntry* vk::Profiler::get(QueryHandle handle) const {
  if (auto it = query_entries.find(handle.id); it != query_entries.end()) {
    return &it->second;
  } else {
    return nullptr;
  }
}

std::string vk::Profiler::QueryEntry::stat_str() const {
  constexpr int data_size = 1024;
  char data[data_size];
  auto num_written = std::snprintf(
    data, data_size, "mean: %0.2fms, min: %0.2fms, max: %0.2fms, last: %0.2fms",
    latest_samples.mean_or_default(0.0f),
    latest_samples.min_or_default(0.0f),
    latest_samples.max_or_default(0.0f),
    latest_samples.latest()
  );
  if (num_written < data_size && num_written > 0) {
    return std::string{data};
  } else {
    return {};
  }
}

void vk::Profiler::set_global_profiler(Profiler* profiler) {
  globals::profiler = profiler;
}

void vk::Profiler::tic_global(std::string_view id,
                              VkCommandBuffer cmd,
                              VkPipelineStageFlagBits stage) {
  if (globals::profiler) {
    globals::profiler->tic(id, cmd, stage);
  }
}

void vk::Profiler::toc_global(std::string_view id,
                              VkCommandBuffer cmd,
                              VkPipelineStageFlagBits stage) {
  if (globals::profiler) {
    globals::profiler->toc(id, cmd, stage);
  }
}

GROVE_NAMESPACE_END
