#pragma once

#include "audio_parameters.hpp"
#include "AudioParameterWriteAccess.hpp"
#include "grove/common/identifier.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/common/Optional.hpp"

namespace grove {

struct AudioParameterSystem;
class Transport;

struct BreakPointSetHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(BreakPointSetHandle, id)
  uint32_t id;
};

struct AudioParameterSystemUpdateInfo {
  bool any_dropped_events{};
  ArrayView<uint32_t> connected_nodes{};
  ArrayView<uint32_t> deleted_nodes{};
};

}

namespace grove::param_system {

struct Stats {
  int num_newly_set_values;
  int num_newly_reverted_to_break_points;
  int num_need_resynchronize;
  int num_break_point_sets;
  int num_break_point_parameters;
  int total_num_break_points;
  int num_ui_values;
  int num_controlled_by_ui;
  int num_write_access_acquired_parameters;
};

AudioParameterSystem* get_global_audio_parameter_system();

void ui_initialize(AudioParameterSystem* sys, const Transport* transport);
void ui_end_update(AudioParameterSystem* sys, const AudioParameterSystemUpdateInfo& info);

AudioParameterWriteAccess* ui_get_write_access(AudioParameterSystem* sys);
const AudioParameterWriteAccess* ui_get_write_access(const AudioParameterSystem* sys);

void ui_set_value(AudioParameterSystem* sys, AudioParameterWriterID writer,
                  AudioParameterIDs ids, const AudioParameterValue& value);
bool ui_set_value_if_no_other_writer(
  AudioParameterSystem* sys, AudioParameterIDs ids, const AudioParameterValue& value);
AudioParameterValue ui_get_set_value_or_default(const AudioParameterSystem* sys,
                                                const AudioParameterDescriptor& desc);
bool ui_is_ui_controlled(const AudioParameterSystem* sys, AudioParameterIDs ids);
bool ui_has_break_points(const AudioParameterSystem* sys, AudioParameterIDs ids);
void ui_revert_to_break_points(AudioParameterSystem* sys, AudioParameterWriterID writer,
                               AudioParameterIDs ids);
void ui_remove_parent(AudioParameterSystem* sys, AudioParameterID id);

BreakPointSetHandle ui_create_break_point_set(AudioParameterSystem* sys, const ScoreRegion& span);
void ui_destroy_break_point_set(AudioParameterSystem* sys, BreakPointSetHandle handle);
const BreakPointSet* ui_read_break_point_set(const AudioParameterSystem* sys,
                                             BreakPointSetHandle set);
Optional<BreakPointSetHandle> ui_get_active_set_handle(const AudioParameterSystem* sys);
void ui_insert_break_point(AudioParameterSystem* sys, AudioParameterWriterID writer,
                           BreakPointSetHandle set, const AudioParameterDescriptor& param_desc,
                           const BreakPoint& point);
void ui_remove_break_point(AudioParameterSystem* sys, AudioParameterWriterID writer,
                           BreakPointSetHandle set, const AudioParameterDescriptor& param_desc,
                           const BreakPoint& point);
void ui_modify_break_point(AudioParameterSystem* sys, AudioParameterWriterID writer,
                           BreakPointSetHandle set, const AudioParameterDescriptor& param_desc,
                           const BreakPoint& point);

Stats ui_get_stats(const AudioParameterSystem* sys);
ScoreCursor ui_get_active_break_point_set_cursor_position(const AudioParameterSystem* sys);

void render_begin_process(AudioParameterSystem* sys, const AudioRenderInfo& info);
const AudioParameterChanges& render_read_changes(const AudioParameterSystem* sys);

}