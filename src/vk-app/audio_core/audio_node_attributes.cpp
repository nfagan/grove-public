#include "audio_node_attributes.hpp"
#include "../ui/ui_util.hpp"
#include "grove/gl/debug/debug_draw.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

Vec3f color_for_data_type(AudioNodeStorage::DataType type) {
  using Type = AudioNodeStorage::DataType;

  switch (type) {
    case Type::MIDIPlusAudio:
      return colors::midi_instrument_input_output;
    case Type::MIDIMessage:
      return colors::midi_message;
    case Type::MIDINote:
      return colors::midi_note;
    case Type::Float:
      return colors::float_data;
    case Type::Audio:
      return colors::mid_gray;
    case Type::Sample2:
      return colors::sample2;
    default:
      assert(false);
      return colors::red;
  }
}

void debug_draw_port(const AudioNodeStorage::PortInfo& port_info,
                     const Camera& camera,
                     const Vec3f& position,
                     const Vec3f& scale,
                     const Vec3f& input_scale,
                     bool selected) {
  //
  auto model = make_translation_scale(position, scale);
  auto color = color_for_data_type(port_info.descriptor.data_type);
  color *= selected ? 0.5f : 1.0f;
  debug::draw_cube(model, camera, color);

  if (port_info.descriptor.is_input()) {
    auto input_model = make_translation_scale(position, scale * input_scale);
    debug::draw_cube(input_model, camera, Vec3f{1.0f});
  }
}

GROVE_NAMESPACE_END
