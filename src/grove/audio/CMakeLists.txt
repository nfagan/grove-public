function(configure_audio)

set(sources
  audio_buffer.hpp
  audio_buffer.cpp
  AudioBufferStore.hpp
  AudioBufferStore.cpp
  audio_callback.hpp
  audio_callback.cpp
  audio_config.hpp
  audio_device.hpp
  audio_device.cpp
  AudioEffect.hpp
  AudioEffect.cpp
  audio_effects.hpp
  audio_events.hpp
  audio_events.cpp
  arpeggio.hpp
  AudioEventSystem.hpp
  AudioEventSystem.cpp
  AudioRenderBufferSystem.hpp
  AudioRenderBufferSystem.cpp
  AudioRenderable.hpp
  AudioRecorder.hpp
  AudioRecorder.cpp
  AudioParameterSystem.hpp
  AudioParameterSystem.cpp
  AudioParameterWriteAccess.hpp
  AudioParameterWriteAccess.cpp
  AudioScaleSystem.hpp
  AudioScaleSystem.cpp
  AudioNodeIsolator.hpp
  AudioNodeIsolator.cpp
  audio_node.hpp
  audio_node.cpp
  audio_nodes.hpp
  audio_parameters.hpp
  audio_parameters.cpp
  cursor.hpp
  Metronome.hpp
  Metronome.cpp
  NoteClipSystem.hpp
  NoteClipSystem.cpp
  NoteClipStateMachineSystem.hpp
  NoteClipStateMachineSystem.cpp
  NoteNumberSet.hpp
  NotePacketAllocator.hpp
  NotePacketAllocator.cpp
  NoteQueryAccelerator.hpp
  NoteQueryAccelerator.cpp
  NoteQueue.hpp
  AudioCore.hpp
  AudioCore.cpp

  AudioGraph.hpp
  AudioGraph.cpp
  AudioGraphProxy.hpp
  AudioGraphProxy.cpp
  AudioGraphRenderer.hpp
  AudioGraphRenderer.cpp
  AudioGraphRenderData.hpp
  AudioGraphRenderData.cpp
  AudioMemoryArena.hpp
  AudioMemoryArena.cpp
  AudioScale.hpp
  AudioScale.cpp
  AudioRenderer.hpp
  AudioRenderer.cpp
  ArpeggiatorSystem.hpp
  ArpeggiatorSystem.cpp

  AudioStream.hpp
  AudioStream.cpp
  AudioThread.hpp
  AudioThread.cpp

  data_channel.hpp
  data_channel.cpp

  dft.hpp
  fdft.hpp
  fdft.cpp
  delay.hpp
  filter.hpp
  DoubleBuffer.hpp

  audio_processor_nodes/MIDINoteToPitchCV.hpp
  audio_processor_nodes/MIDINoteToPitchCV.cpp
  audio_processor_nodes/OscillatorNode.hpp
  audio_processor_nodes/OscillatorNode.cpp
  audio_processor_nodes/ModulatedOscillatorNode.hpp
  audio_processor_nodes/ModulatedOscillatorNode.cpp
  audio_processor_nodes/GainNode.hpp
  audio_processor_nodes/SplitterNode.hpp
  audio_processor_nodes/SplitterNode.cpp
  audio_processor_nodes/MergerNode.hpp
  audio_processor_nodes/MergerNode.cpp
  audio_processor_nodes/MultiplyNode.hpp
  audio_processor_nodes/SumNode.hpp
  audio_processor_nodes/DuplicatorNode.hpp
  audio_processor_nodes/DelayNode.hpp
  audio_processor_nodes/DelayNode.cpp
  audio_processor_nodes/DestinationNode.hpp
  audio_processor_nodes/DestinationNode.cpp
  audio_processor_nodes/ConstantNode.hpp
  audio_processor_nodes/ADSRNode.hpp
  audio_processor_nodes/ADSRNode.cpp
  audio_processor_nodes/RandomizedEnvelopeNode.hpp
  audio_processor_nodes/RandomizedEnvelopeNode.cpp
  audio_processor_nodes/MIDIMessageStreamNode.hpp
  audio_processor_nodes/MIDIMessageStreamNode.cpp

  audio_effects/SpectrumAnalyzer.hpp
  audio_effects/SpectrumAnalyzer.cpp
  audio_effects/UtilityEffect.hpp
  audio_effects/UtilityEffect.cpp

  MIDIMessageStreamSystem.hpp
  MIDIMessageStreamSystem.cpp

  envelope.hpp
  envelope.cpp

  io.hpp
  io.cpp

  PitchSamplingSystem.hpp
  PitchSamplingSystem.cpp

  scales.hpp
  oscillator.hpp
  oscillator.cpp
  ScoreRegionTree.hpp
  Transport.hpp
  Transport.cpp
  TriggeredBufferRenderer.hpp
  TriggeredBufferRenderer.cpp
  TriggeredNotes.hpp
  TriggeredNotes.cpp
  QuantizedTriggeredNotes.hpp
  QuantizedTriggeredNotes.cpp
  TimelineSystem.hpp
  TimelineSystem.cpp
  tuning.hpp
  tuning.cpp
  types.hpp
  types.cpp
  voice_allocation.hpp
)

foreach(source ${sources})
  target_sources(${PROJECT_NAME} PRIVATE ${GROVE_MODULE_SOURCE_DIR}/audio/${source})
endforeach()

endfunction(configure_audio)

configure_audio()