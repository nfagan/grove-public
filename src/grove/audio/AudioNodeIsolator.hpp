#pragma once

#include <cstdint>

namespace grove {
struct AudioNodeIsolator;
struct AudioRenderInfo;
class AudioRenderable;
struct AudioProcessData;
class BufferChannelDescriptor;
}

namespace grove::ni {

struct AudioNodeIsolatorUpdateResult {
  uint32_t newly_will_activate;
  uint32_t newly_will_deactivate;
};

AudioNodeIsolator* get_global_audio_node_isolator();

void ui_init_audio_node_isolator(AudioNodeIsolator* isolator, const AudioRenderable* target_renderer);
void ui_set_solo_gain(AudioNodeIsolator* isolator, float g);
void ui_isolate_input(AudioNodeIsolator* isolator, uint32_t node);
void ui_isolate_output(AudioNodeIsolator* isolator, uint32_t node);
void ui_stop_isolating_input(AudioNodeIsolator* isolator, uint32_t node);
void ui_stop_isolating_output(AudioNodeIsolator* isolator, uint32_t node);
void ui_toggle_isolating(AudioNodeIsolator* isolator, uint32_t node, bool input);
bool ui_is_isolating(const AudioNodeIsolator* isolator, uint32_t node, bool input);
AudioNodeIsolatorUpdateResult ui_update(AudioNodeIsolator* isolator);

bool render_want_isolate_input(const AudioNodeIsolator* isolator, uint32_t node);
void render_isolate_input(
  AudioNodeIsolator* isolator, uint32_t node,
  const AudioProcessData& pd, const AudioRenderInfo& info);
void render_isolate_input(
  AudioNodeIsolator* isolator, uint32_t node,
  const unsigned char* in_data, const BufferChannelDescriptor* in_channel_descs,
  int num_channels, int num_frames);

bool render_want_isolate_output(const AudioNodeIsolator* isolator, uint32_t node);
void render_isolate_output(
  AudioNodeIsolator* isolator, uint32_t node,
  const unsigned char* in_data, const BufferChannelDescriptor* in_channel_descs,
  int num_channels, int num_frames);
void render_isolate_output(
  AudioNodeIsolator* isolator, uint32_t node,
  const AudioProcessData& pd, const AudioRenderInfo& info);

void begin_render(AudioNodeIsolator* isolator, const AudioRenderInfo& info);
void process(
  AudioNodeIsolator* isolator, const AudioRenderable* renderable,
  float* renderable_generated_samples, const AudioRenderInfo& info);
void end_render(AudioNodeIsolator* isolator);

#define GROVE_MAYBE_ISOLATE_INPUT(node, pd, info)                                                       \
  if (grove::ni::render_want_isolate_input(grove::ni::get_global_audio_node_isolator(), (node))) {      \
    grove::ni::render_isolate_input(grove::ni::get_global_audio_node_isolator(), (node), (pd), (info)); \
  }

#define GROVE_MAYBE_ISOLATE_OUTPUT(node, pd, info)                                                       \
  if (grove::ni::render_want_isolate_output(grove::ni::get_global_audio_node_isolator(), (node))) {      \
    grove::ni::render_isolate_output(grove::ni::get_global_audio_node_isolator(), (node), (pd), (info)); \
  }

}