#pragma once

#include "grove/vk/common.hpp"
#include "grove/vk/command_pool.hpp"
#include "grove/vk/sync.hpp"
#include "grove/common/Future.hpp"
#include <functional>
#include <array>
#include <unordered_set>
#include <unordered_map>

namespace grove::vk {

struct Core;

class CommandProcessor {
public:
  static constexpr int command_pool_size = 8;
  struct CommandContext {
    VkQueue queue{};
    vk::Fence fence;
    VkCommandBuffer cmd{};
    bool began{};
    bool complete{};
    bool submitted{};
  };

  struct PoolContext {
    vk::CommandPool command_pool;
    std::array<CommandContext, command_pool_size> contexts;
    uint32_t num_submitted{};
    uint32_t num_complete{};
  };

  struct ContextIndices {
    uint32_t pool;
    uint32_t command;
  };

  struct HashContextIndices {
    std::size_t operator()(const ContextIndices& inds) const noexcept {
      uint64_t v{inds.pool};
      v |= (uint64_t(inds.command) << 32u);
      return std::hash<uint64_t>{}(v);
    }
  };

  struct EqualContextIndices {
    bool operator()(const ContextIndices& a, const ContextIndices& b) const noexcept {
      return a.pool == b.pool && a.command == b.command;
    }
  };

  using Command = std::function<void(VkCommandBuffer)>;
  using FutureError = Future<Error>;
  using CommandFuture = std::shared_ptr<FutureError>;

  struct PendingFuture {
    CommandFuture future;
    ContextIndices indices;
  };

public:
  void destroy(VkDevice device);
  void begin_frame(VkDevice device);
  void end_frame(VkDevice device);

  [[nodiscard]] Error sync(VkDevice device, VkQueue queue, uint32_t queue_family, Command&& command);
  Result<CommandFuture> async(VkDevice device, VkQueue queue, uint32_t queue_family, Command&& command);

  [[nodiscard]] Error sync_graphics_queue(const Core& core, Command&& cmd, uint32_t ith_queue = 0);
  Result<CommandFuture> async_graphics_queue(const Core& core, Command&& cmd, uint32_t ith_queue = 0);

private:
  bool require_context(VkDevice device, uint32_t queue_family, VkQueue queue, ContextIndices* indices);
  void on_context_begin(VkDevice device, VkQueue queue, uint32_t pool_ind, uint32_t command_ind);
  void on_context_submit(VkDevice device, uint32_t pool_ind, uint32_t command_ind);
  void on_context_complete(VkDevice device, uint32_t pool_ind, uint32_t command_ind);

private:
  using IndexSet = std::unordered_set<ContextIndices, HashContextIndices, EqualContextIndices>;
  template <typename T>
  using IndexMap = std::unordered_map<ContextIndices, T, HashContextIndices, EqualContextIndices>;

  std::vector<PoolContext> pool_contexts;
  std::vector<PendingFuture> pending_futures;
  IndexSet pending_submit;
  IndexMap<bool> contexts_examined_this_frame;
};

}