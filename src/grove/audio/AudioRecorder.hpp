#pragma once

#include "data_channel.hpp"
#include "grove/common/RingBuffer.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/common/identifier.hpp"
#include <memory>
#include <atomic>
#include <thread>

namespace grove {

class Transport;

/*
 * AudioRecordStreamHandle
 */

struct AudioRecordStreamHandle {
public:
  using Self = AudioRecordStreamHandle;

  bool is_valid() const {
    return id != 0;
  }

  GROVE_INTEGER_IDENTIFIER_EQUALITY(AudioRecordStreamHandle, id)
  GROVE_INTEGER_IDENTIFIER_INEQUALITIES(AudioRecordStreamHandle, id)

  static AudioRecordStreamHandle create();

public:
  uint64_t id{0};

private:
  static std::atomic<uint64_t> next_id;
};

using AudioRecordChannelSet = BufferChannelSet<4>;

/*
 * AudioRecordStreamBlocks
 */

struct AudioRecordStreamBlock {
  std::unique_ptr<unsigned char[]> data;
  std::size_t size{};

  int frame_offset{};
  int num_frames{};

  std::size_t recorded_offset{};
  std::size_t recorded_size{};
};

struct AudioRecordStreamBlocks {
public:
  static constexpr int ring_buffer_size = 32;

public:
  AudioRecordStreamBlocks(const AudioRecordChannelSet& layout, int num_frames);
  Optional<AudioRecordStreamBlock> reserve(const AudioRecordChannelSet& layout,
                                           int frame_offset,
                                           int num_frames);

public:
  RingBuffer<AudioRecordStreamBlock, ring_buffer_size> pending_read;
  RingBuffer<AudioRecordStreamBlock, ring_buffer_size> free;

  int frames_per_block{};
  std::size_t bytes_per_block{};
};

/*
 * AudioRecordStreamBackingStore
 */

struct AudioRecordStreamBackingStore {
  static constexpr std::size_t initial_allocation_size = 8192;

public:
  void update(AudioRecordStreamBlocks& blocks);

  AudioRecordStreamBlock store{};
  std::size_t size{};
};

/*
 * AudioRecordStream
 */

struct AudioRecordStream {
  enum class State {
    Idle,
    PendingRecordStart,
    Active,
    PendingRecordStop,
    AwaitingDataRetrieval,
  };

  enum class Status {
    Ok,
    ErrorFailedToReserveWriteBlock,
    ErrorBackingStoreFailedToProcessBlock
  };

public:
  AudioRecordStream(AudioRecordStreamHandle handle,
                    const Transport* transport,
                    AudioRecordChannelSet layout,
                    const AudioRenderInfo& info);

  bool is_compatible_with_layout(const BufferChannelDescriptors& descriptors,
                                 const ArrayView<int>& descriptor_indices) const;

  bool supports_accumulate(const BufferChannelDescriptors& descriptors,
                           const ArrayView<int>& descriptor_indices) const;

  bool supports_recording(const BufferChannelDescriptors& descriptors,
                          const ArrayView<int>& descriptor_indices,
                          const AudioRenderInfo& info) const;

  bool is_idle() const;
  bool is_ok() const;

  bool can_trigger_start_recording() const;
  bool can_trigger_stop_recording() const;

  bool reserve(int frame_offset, int num_frames);
  void submit_write_block();

  void backing_store_update();

  bool write(const AudioProcessData& data,
             const ArrayView<int>& descriptor_indices,
             const AudioRenderInfo& info);

  bool accumulate(const AudioProcessData& data,
                  const ArrayView<int>& descriptor_indices,
                  const AudioRenderInfo& info);

public:
  AudioRecordStreamHandle handle{};
  const Transport* transport{};
  AudioRecordChannelSet layout;
  double sample_rate{default_sample_rate()};

  AudioRecordStreamBlocks blocks;
  AudioRecordStreamBackingStore backing_store;

  AudioRecordStreamBlock write_block{};
  bool has_write_block{false};

  State state{State::Idle};
  Status status{Status::Ok};

  bool triggered_record_start{false};
  bool triggered_record_stop{false};

  bool is_recording{false};
};

struct AudioRecordStreamResult {
public:
  bool success() const {
    return status == AudioRecordStream::Status::Ok;
  }

public:
  AudioRecordStreamHandle handle{};
  AudioRecordStream::Status status{AudioRecordStream::Status::Ok};

  std::unique_ptr<unsigned char[]> data;
  std::size_t size{};

  AudioRecordChannelSet layout;
  double sample_rate{default_sample_rate()};
};

/*
 * AudioRecordStreamBackingStoreTask
 */

class AudioRecordStreamBackingStoreTask {
public:
  struct CreateStreamFuture {
    const Transport* transport{};
    AudioRecordChannelSet layout;
    AudioRenderInfo info{};
    AudioRecordStream* stream{};

    bool success{true};
    std::atomic<bool> is_ready{false};
  };

  struct RetrieveDataFuture {
    AudioRecordStreamHandle handle{};
    AudioRecordStreamResult stream_result{};

    bool success{true};
    std::atomic<bool> is_ready{false};
  };

  using BoxedCreateStreamFuture = std::unique_ptr<CreateStreamFuture>;
  using BoxedRetrieveDataFuture = std::unique_ptr<RetrieveDataFuture>;

private:
  static constexpr int num_ms_sleep = 10;
  static constexpr double num_ms_await_stream_block = 1.0;

public:
  ~AudioRecordStreamBackingStoreTask();

  void initialize();
  void terminate();
  void update();
  void sleep() const;
  bool proceed() const {
    return keep_processing.load();
  }
  int size() const;

  std::unique_ptr<CreateStreamFuture> create_stream(AudioRecordChannelSet layout,
                                                    const AudioRenderInfo& info,
                                                    const Transport* transport);

  std::unique_ptr<RetrieveDataFuture> retrieve_data(AudioRecordStreamHandle handle);

private:
  void process_create_stream_commands();
  void process_retrieve_data_commands();

private:
  std::thread task;

  bool initialized{false};
  std::atomic<bool> keep_processing{false};

  DynamicArray<std::unique_ptr<AudioRecordStream>, 32> streams;

  RingBuffer<CreateStreamFuture*, 4> pending_created_streams;
  RingBuffer<RetrieveDataFuture*, 4> streams_pending_data_retrieval;
};

/*
 * AudioRecorder
 */

class AudioRecorder {
public:
  struct CreateStreamFuture {
    const Transport* transport{};
    AudioRecordChannelSet layout;
    AudioRecordStreamHandle result_handle{};

    std::atomic<bool> is_ready{false};
    bool success{true};
  };

  struct StartStreamFuture {
    AudioRecordStreamHandle handle{};

    std::atomic<bool> is_ready{false};
    bool success{true};
  };

  struct StopStreamFuture {
    AudioRecordStreamHandle handle{};
    AudioRecordStreamResult stream_result{};

    std::atomic<bool> is_ready{false};
    bool success{true};
  };

  struct PendingCreatedStream {
    CreateStreamFuture* ui_future{};
    AudioRecordStreamBackingStoreTask::BoxedCreateStreamFuture task_future;
  };

  using BoxedCreateStreamFuture = std::unique_ptr<CreateStreamFuture>;
  using BoxedStartStreamFuture = std::unique_ptr<StartStreamFuture>;
  using BoxedStopStreamFuture = std::unique_ptr<StopStreamFuture>;
  using BoxedRetrieveDataFuture = AudioRecordStreamBackingStoreTask::BoxedRetrieveDataFuture;

public:
  ~AudioRecorder();

  void initialize();
  void terminate();

  void begin_render(const AudioRenderInfo& info);
  void end_render(const AudioRenderInfo& info);

  bool write(AudioRecordStreamHandle to_stream,
             const AudioProcessData& data,
             const ArrayView<int>& descriptor_indices,
             const AudioRenderInfo& info);

  bool accumulate(AudioRecordStreamHandle to_stream,
                  const AudioProcessData& data,
                  const ArrayView<int>& descriptor_indices,
                  const AudioRenderInfo& info);

  bool is_recording(AudioRecordStreamHandle handle) const;

  std::unique_ptr<CreateStreamFuture> create_stream(AudioRecordChannelSet layout,
                                                    const Transport* transport);

  std::unique_ptr<StartStreamFuture> start_recording(AudioRecordStreamHandle handle);
  std::unique_ptr<StopStreamFuture> stop_recording(AudioRecordStreamHandle handle);

  int size() const;

public:
  AudioRecordStreamBackingStoreTask backing_store_task;
  DynamicArray<AudioRecordStream*, 32> streams;

  DynamicArray<BoxedRetrieveDataFuture, 16> pending_data_retrieval_from_backing_store;
  DynamicArray<StopStreamFuture*, 4> pending_ui_data_retrieval;

  RingBuffer<CreateStreamFuture*, 4> ui_create_stream_commands;
  DynamicArray<CreateStreamFuture*, 4> queued_create_stream_commands;
  DynamicArray<PendingCreatedStream, 4> pending_creation_from_backing_store;

  RingBuffer<StartStreamFuture*, 4> streams_pending_record_start;
  RingBuffer<StopStreamFuture*, 4> streams_pending_record_stop;
  DynamicArray<AudioRecordStreamHandle, 8> streams_stopped_not_by_trigger;
};

}