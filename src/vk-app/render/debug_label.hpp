#pragma once

#include <vulkan/vulkan.h>

#define GROVE_VK_DEBUG_LABELS_ENABLED (1)

namespace grove::vk::debug {

void initialize_debug_labels(VkInstance instance, VkDevice device);
void terminate_debug_labels();

void label_begin(VkCommandBuffer cmd, const char* label);
void label_end(VkCommandBuffer cmd);

struct LabelScopeHelper {
  explicit LabelScopeHelper(VkCommandBuffer cmd, const char* label) : cmd{cmd} {
    label_begin(cmd, label);
  }
  ~LabelScopeHelper() {
    label_end(cmd);
  }
  VkCommandBuffer cmd;
};

#if GROVE_VK_DEBUG_LABELS_ENABLED

#define GROVE_VK_SCOPED_DEBUG_LABEL(cmd, label) \
  grove::vk::debug::LabelScopeHelper((cmd), (label))

#else

#define GROVE_VK_SCOPED_DEBUG_LABEL(cmd, label) \
  0

#endif

}