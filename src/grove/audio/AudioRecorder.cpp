#include "AudioRecorder.hpp"
#include "Transport.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/vector_util.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

GROVE_NAMESPACE_BEGIN

#if 0
#define GROVE_SANITY_CHECK_SIZE(name)             \
  {                                               \
    static int last_size__{0};                    \
    if (size() != last_size__) {                  \
      std::cout << (name) << size() << std::endl; \
      last_size__ = size();                       \
    }                                             \
  }
#else
#define GROVE_SANITY_CHECK_SIZE(name) (void) (name);
#endif

namespace {

void stream_backing_store_task(AudioRecordStreamBackingStoreTask* task) {
  while (task->proceed()) {
    task->update();
    task->sleep();
  }
}

AudioRecordStreamResult make_audio_record_stream_result(const AudioRecordStream* stream) {
  AudioRecordStreamResult result{};

  result.handle = stream->handle;
  result.layout = stream->layout;

  if (stream->is_ok()) {
    auto& store = stream->backing_store;
    result.size = store.size;
    result.sample_rate = stream->sample_rate;

    if (store.size > 0) {
      result.data = std::make_unique<unsigned char[]>(store.size);
      std::memcpy(result.data.get(), store.store.data.get(), store.size);
    }

  } else {
    result.status = stream->status;
  }

  return result;
}

bool await_pending_blocks(AudioRecordStream* stream, double timeout_ms) {
  using Clock = std::chrono::high_resolution_clock;
  using Duration = std::chrono::duration<double>;

  const auto make_timeout = [timeout_ms](Clock::time_point t0) {
    return [t0, timeout_ms]() {
      return Duration(Clock::now() - t0).count() * 1e3 >= timeout_ms;
    };
  };

  const auto check_timeout = make_timeout(Clock::now());

  while (stream->blocks.pending_read.size() > 0 && !check_timeout()) {
    //  Potentially retrieve last block.
    stream->backing_store_update();
  }

  return stream->blocks.pending_read.size() == 0;
}

template <typename T>
auto find_stream(T&& streams, AudioRecordStreamHandle handle) {
  return std::find_if(streams.begin(), streams.end(), [handle](auto& stream) {
    return stream->handle == handle;
  });
}

void trigger_start_recording(AudioRecorder& recorder, AudioRecorder::StartStreamFuture* future) {
  auto maybe_stream = find_stream(recorder.streams, future->handle);
  if (maybe_stream == recorder.streams.end()) {
    future->success = false;

  } else {
    auto& stream = **maybe_stream;
    if (stream.can_trigger_start_recording()) {
      stream.triggered_record_start = true;
      stream.state = AudioRecordStream::State::PendingRecordStart;

    } else {
      future->success = false;
    }
  }

  future->is_ready.store(true);
}

void trigger_stop_recording(AudioRecorder& recorder, AudioRecorder::StopStreamFuture* future) {
  auto maybe_stream = find_stream(recorder.streams, future->handle);

  if (maybe_stream == recorder.streams.end()) {
    //  If we can't locate the stream, it could be because recording was previously stopped by
    //  the transport. If that's the case, push the future to the queue of futures pending data
    //  retrieval. Otherwise, it's an error.
    auto& stopped = recorder.streams_stopped_not_by_trigger;
    auto maybe_stopped = std::find(stopped.begin(), stopped.end(), future->handle);

    if (maybe_stopped == stopped.end()) {
      future->success = false;
      future->is_ready.store(true);

    } else {
      stopped.erase(maybe_stopped);
      recorder.pending_ui_data_retrieval.push_back(future);
    }

  } else {
    auto& stream = **maybe_stream;
    if (stream.can_trigger_stop_recording()) {
      stream.triggered_record_stop = true;

      if (stream.state != AudioRecordStream::State::AwaitingDataRetrieval) {
        stream.state = AudioRecordStream::State::PendingRecordStop;
      }

      //  Await data from backing store task.
      recorder.pending_ui_data_retrieval.push_back(future);

    } else {
      future->success = false;
      future->is_ready.store(true);
    }
  }
}

void process_record_trigger_commands(AudioRecorder& recorder) {
  auto& streams_pending_record_start = recorder.streams_pending_record_start;
  auto& streams_pending_record_stop = recorder.streams_pending_record_stop;

  int num_to_start_recording = streams_pending_record_start.size();
  for (int i = 0; i < num_to_start_recording; i++) {
    trigger_start_recording(recorder, streams_pending_record_start.read());
  }

  int num_to_stop_recording = streams_pending_record_stop.size();
  for (int i = 0; i < num_to_stop_recording; i++) {
    trigger_stop_recording(recorder, streams_pending_record_stop.read());
  }
}

void process_create_stream_commands(AudioRecorder& recorder, const AudioRenderInfo& info) {
  {
    //  Read commands from ui-buffer and push to queue.
    int num_to_create = recorder.ui_create_stream_commands.size();
    for (int i = 0; i < num_to_create; i++) {
      auto* future = recorder.ui_create_stream_commands.read();
      recorder.queued_create_stream_commands.push_back(future);
    }
  }

  {
    //  For each queued command, try to submit an async request to the backing store task
    //  to actually create the stream. If this fails, then break and try again later. Otherwise,
    //  remove the command from the queue.
    auto& queued_create_stream_commands = recorder.queued_create_stream_commands;
    auto& streams_awaiting_creation = recorder.pending_creation_from_backing_store;
    auto& backing_store_task = recorder.backing_store_task;

    DynamicArray<int, 8> erase_pending_create_streams;

    for (int i = 0; i < int(queued_create_stream_commands.size()); i++) {
      auto* ui_future = queued_create_stream_commands[i];
      auto maybe_future =
        backing_store_task.create_stream(ui_future->layout, info, ui_future->transport);

      if (maybe_future) {
        AudioRecorder::PendingCreatedStream pending_stream{ui_future, std::move(maybe_future)};
        streams_awaiting_creation.push_back(std::move(pending_stream));
        erase_pending_create_streams.push_back(i);

      } else {
        //  No room left in the backing_store_task command buffer; try again later.
        break;
      }
    }

    erase_set(queued_create_stream_commands, erase_pending_create_streams);
  }

  {
    //  For each request submitted to the backing store task, check whether it's been fulfilled.
    //  If it has, notify the ui of the result. If the stream was created successfully, add it to
    //  the set of streams.
    auto& streams_awaiting_creation = recorder.pending_creation_from_backing_store;
    auto& streams = recorder.streams;

    DynamicArray<int, 8> erase_created_streams;

    for (int i = 0; i < int(streams_awaiting_creation.size()); i++) {
      auto& future = streams_awaiting_creation[i];
      auto& [ui_future, task_future] = future;

      if (task_future->is_ready.load()) {
        if (task_future->success) {
          streams.push_back(task_future->stream);
          ui_future->result_handle = task_future->stream->handle;

        } else {
          ui_future->success = false;
        }

        ui_future->is_ready.store(true);
        erase_created_streams.push_back(i);
      }
    }

    erase_set(streams_awaiting_creation, erase_created_streams);
  }
}

bool process_stream(AudioRecordStream* stream, const AudioRenderInfo& info) {
  using State = AudioRecordStream::State;

  if (stream->is_idle()) {
    return false;
  }

  const auto* transport = stream->transport;
  const auto scheduling_info = transport->render_get_scheduling_info();
  const auto quantum_start_frame = scheduling_info.next_quantum_render_frame_index_start;
  const bool has_new_quantum = quantum_start_frame >= 0;

  int frame_offset{0};
  int num_frames_alloc{info.num_frames};
  bool stopped_not_by_trigger{false};

  if (stream->state != State::AwaitingDataRetrieval && transport->just_stopped()) {
    //  Recording stopped by stopping the transport, rather than via a ui-trigger.
    stream->is_recording = false;
    stream->state = State::AwaitingDataRetrieval;
    stopped_not_by_trigger = true;

  } else if (!transport->render_is_playing()) {
    return stopped_not_by_trigger;

  } else if (stream->state == State::PendingRecordStart) {
    if (transport->just_played()) {
      stream->state = State::Active;
      stream->is_recording = true;

    } else if (has_new_quantum) {
      stream->state = State::Active;
      stream->is_recording = true;

      frame_offset = quantum_start_frame;
      num_frames_alloc = info.num_frames - quantum_start_frame;
    }

  } else if (stream->state == State::PendingRecordStop && has_new_quantum) {
    stream->state = State::AwaitingDataRetrieval;
    num_frames_alloc = quantum_start_frame;
  }

  if (stream->is_recording) {
    if (!stream->reserve(frame_offset, num_frames_alloc)) {
      GROVE_LOG_ERROR_CAPTURE_META("Failed to reserve space for recording.", "AudioRecorder");

      stream->is_recording = false;
      stream->state = AudioRecordStream::State::AwaitingDataRetrieval;
      //  Stopped because of error.
      stopped_not_by_trigger = true;
    }
  }

  return stopped_not_by_trigger;
}

} //  anon

/*
 * RecordStreamHandle
 */

std::atomic<uint64_t> AudioRecordStreamHandle::next_id{1};

AudioRecordStreamHandle AudioRecordStreamHandle::create() {
  return AudioRecordStreamHandle{next_id++};
}

/*
 * AudioRecordStream
 */

AudioRecordStream::AudioRecordStream(AudioRecordStreamHandle handle,
                                     const Transport* transport,
                                     AudioRecordChannelSet layout,
                                     const AudioRenderInfo& info) :
                                     handle{handle},
                                     transport{transport},
                                     layout{std::move(layout)},
                                     sample_rate{info.sample_rate},
                                     blocks{this->layout, info.num_frames} {
  //
}

bool AudioRecordStream::supports_recording(const BufferChannelDescriptors& descriptors,
                                           const ArrayView<int>& descriptor_indices,
                                           const AudioRenderInfo& info) const {
  return is_recording &&
         has_write_block &&
         is_compatible_with_layout(descriptors, descriptor_indices) &&
         write_block.frame_offset + write_block.num_frames <= info.num_frames;
}

bool AudioRecordStream::supports_accumulate(const BufferChannelDescriptors& descriptors,
                                            const ArrayView<int>& descriptor_indices) const {
  return std::all_of(descriptor_indices.begin(), descriptor_indices.end(), [&](auto& ind) {
    return descriptors[ind].is_float();
  });
}

bool AudioRecordStream::is_compatible_with_layout(const BufferChannelDescriptors& descriptors,
                                                  const ArrayView<int>& descriptor_indices) const {
  const auto num_channels = int(layout.num_channels());

  if (descriptor_indices.size() != num_channels) {
    return false;
  }

  for (int i = 0; i < num_channels; i++) {
    auto& query_descriptor = descriptors[descriptor_indices[i]];
    auto own_descriptor = layout.channel_descriptor(i);

    if (query_descriptor.type != own_descriptor.type) {
      return false;
    }
  }

  return true;
}

bool AudioRecordStream::is_idle() const {
  return state == State::Idle;
}

bool AudioRecordStream::is_ok() const {
  return status == Status::Ok;
}

bool AudioRecordStream::can_trigger_start_recording() const {
  return !triggered_record_start;
}

bool AudioRecordStream::can_trigger_stop_recording() const {
  return !triggered_record_stop;
}

bool AudioRecordStream::reserve(int frame_offset, int num_frames) {
  if (auto maybe_next_block = blocks.reserve(layout, frame_offset, num_frames)) {
    write_block = std::move(maybe_next_block.value());
    has_write_block = true;
    return true;

  } else {
    write_block = {};
    has_write_block = false;
    status = Status::ErrorFailedToReserveWriteBlock;
    return false;
  }
}

void AudioRecordStream::submit_write_block() {
  blocks.pending_read.write(std::move(write_block));
  has_write_block = false;
  write_block = {};
}

bool AudioRecordStream::accumulate(const AudioProcessData& data,
                                   const ArrayView<int>& descriptor_indices,
                                   const AudioRenderInfo& info) {
  if (!supports_recording(data.descriptors, descriptor_indices, info) ||
      !supports_accumulate(data.descriptors, descriptor_indices)) {
    return false;
  }

  assert(layout.num_channels() == descriptor_indices.size());

  for (int i = 0; i < write_block.num_frames; i++) {
    auto off = i + write_block.frame_offset;

    for (int j = 0; j < int(layout.num_channels()); j++) {
      auto write_descriptor = layout.channel_descriptor(j);
      auto& read_descriptor = data.descriptors[descriptor_indices[j]];
      assert(write_descriptor.is_float() && read_descriptor.is_float());

      auto* read = data.buffer.data + read_descriptor.ptr_offset(off);
      auto* write = write_block.data.get() + write_descriptor.ptr_offset(off);

      auto size = write_descriptor.size();
      assert((write + size) - write_block.data.get() <= int64_t(write_block.size));
      assert(size == read_descriptor.size() && size == sizeof(float));

      float current{};
      std::memcpy(&current, write, size);

      float new_value{};
      std::memcpy(&new_value, read, size);
      new_value += current;

      assert(std::isfinite(new_value));
      std::memcpy(write, &new_value, size);
    }
  }

  return true;
}

bool AudioRecordStream::write(const AudioProcessData& data,
                              const ArrayView<int>& descriptor_indices,
                              const AudioRenderInfo& info) {
  if (!supports_recording(data.descriptors, descriptor_indices, info)) {
    return false;
  }

  assert(layout.num_channels() == descriptor_indices.size());

  for (int i = 0; i < write_block.num_frames; i++) {
    auto off = i + write_block.frame_offset;

    for (int j = 0; j < int(layout.num_channels()); j++) {
      auto write_descriptor = layout.channel_descriptor(j);
      auto& read_descriptor = data.descriptors[descriptor_indices[j]];
      assert(write_descriptor.type == read_descriptor.type);

      auto* read = data.buffer.data + read_descriptor.ptr_offset(off);
      auto* write = write_block.data.get() + write_descriptor.ptr_offset(off);

      auto size = write_descriptor.size();
      assert((write + size) - write_block.data.get() <= int64_t(write_block.size));
      assert(size == read_descriptor.size());

      std::memcpy(write, read, size);
    }
  }

  return true;
}

void AudioRecordStream::backing_store_update() {
  backing_store.update(blocks);
}

/*
 * AudioRecordStreamBackingStore
 */

void AudioRecordStreamBackingStore::update(AudioRecordStreamBlocks& blocks) {
  int num_pending_read = blocks.pending_read.size();

  for (int i = 0; i < num_pending_read; i++) {
    auto block = blocks.pending_read.read();
    assert(block.recorded_size + block.recorded_offset <= block.size);

    while (store.size - size < block.recorded_size) {
      const auto num_alloc =
        store.size == 0 ? initial_allocation_size : store.size * 2;

      auto new_data = std::make_unique<unsigned char[]>(num_alloc);

      if (store.size > 0) {
        std::memcpy(new_data.get(), store.data.get(), store.size);
      }

      store.data = std::move(new_data);
      store.size = num_alloc;
    }

    auto* write = store.data.get() + size;
    auto* read = block.data.get() + block.recorded_offset;

    if (block.recorded_size > 0) {
      std::memcpy(write, read, block.recorded_size);
    }

    size += block.recorded_size;

    assert(!blocks.free.full());
    blocks.free.write(std::move(block));
  }
}

/*
 * AudioRecordStreamBlocks
 */

AudioRecordStreamBlocks::AudioRecordStreamBlocks(const AudioRecordChannelSet& layout, int num_frames) {
  const auto num_bytes = layout.frame_bytes(num_frames);
  auto num_blocks_process = free.write_capacity();

  for (int i = 0; i < num_blocks_process; i++) {
    AudioRecordStreamBlock block{};

    if (num_bytes > 0) {
      block.data = std::make_unique<unsigned char[]>(num_bytes);
    }

    block.size = num_bytes;
    free.write(std::move(block));
  }

  frames_per_block = num_frames;
  bytes_per_block = num_bytes;
}

Optional<AudioRecordStreamBlock>
AudioRecordStreamBlocks::reserve(const AudioRecordChannelSet& layout,
                                 int frame_offset, int num_frames) {

  const auto bytes_to_allocate = layout.frame_bytes(num_frames);
  const auto byte_offset = layout.frame_bytes(frame_offset);

  if (frame_offset + num_frames > frames_per_block ||
      byte_offset + bytes_to_allocate > bytes_per_block ||
      free.size() == 0) {
    return NullOpt{};
  }

  auto block = free.read();
  if (block.size > 0) {
    std::memset(block.data.get(), 0, block.size);
  }

  block.frame_offset = frame_offset;
  block.num_frames = num_frames;

  block.recorded_offset = byte_offset;
  block.recorded_size = bytes_to_allocate;

  assert(bytes_to_allocate + byte_offset <= block.size &&
         block.size == bytes_per_block);

  return Optional<AudioRecordStreamBlock>(std::move(block));
}

/*
 * AudioRecorder
 */

AudioRecorder::~AudioRecorder() {
  terminate();
}

void AudioRecorder::initialize() {
  backing_store_task.initialize();
}

void AudioRecorder::terminate() {
  backing_store_task.terminate();
}

void AudioRecorder::begin_render(const AudioRenderInfo& info) {
  process_create_stream_commands(*this, info);
  process_record_trigger_commands(*this);

  for (auto& stream : streams) {
    const bool stopped_not_by_trigger = process_stream(stream, info);

    if (stopped_not_by_trigger) {
      streams_stopped_not_by_trigger.push_back(stream->handle);
    }
  }

  GROVE_SANITY_CHECK_SIZE("Render: ")
}

void AudioRecorder::end_render(const AudioRenderInfo&) {
  {
    //  For each stream that was written-to, submit its write_block to the backing store task thread
    //  to append to the complete stream. If the stream stopped during this render epoch, then it is
    //  expired, and we attempt to submit a request to retrieve the data from the backing store.
    using State = AudioRecordStream::State;
    DynamicArray<int, 16> erase_streams;

    for (int i = 0; i < int(streams.size()); i++) {
      auto& stream = streams[i];

      if (stream->has_write_block) {
        stream->submit_write_block();
      }

      if (stream->state == State::AwaitingDataRetrieval) {
        auto maybe_retrieve_future =
          backing_store_task.retrieve_data(stream->handle);

        if (maybe_retrieve_future) {
          //  Now wait for data to arrive; remove the stream from the set of active streams.
          pending_data_retrieval_from_backing_store.push_back(
            std::move(maybe_retrieve_future));

          erase_streams.push_back(i);
        }
      }
    }

    erase_set(streams, erase_streams);
  }

  {
    DynamicArray<int, 16> erase_pending_uis;

    for (int i = 0; i < int(pending_ui_data_retrieval.size()); i++) {
      auto& ui_future = pending_ui_data_retrieval[i];

      for (auto& retrieved : pending_data_retrieval_from_backing_store) {
        if (!retrieved->is_ready.load() || retrieved->handle != ui_future->handle) {
          continue;
        }

        if (retrieved->success) {
          ui_future->stream_result = std::move(retrieved->stream_result);

        } else {
          ui_future->success = false;
        }

        pending_data_retrieval_from_backing_store.erase(&retrieved);
        erase_pending_uis.push_back(i);
        ui_future->is_ready.store(true);
        break;
      }
    }

    erase_set(pending_ui_data_retrieval, erase_pending_uis);
  }
}

bool AudioRecorder::is_recording(AudioRecordStreamHandle handle) const {
  auto maybe_stream = find_stream(streams, handle);
  if (maybe_stream == streams.end()) {
    return false;
  } else {
    return (*maybe_stream)->is_recording;
  }
}

bool AudioRecorder::write(AudioRecordStreamHandle to_stream,
                          const AudioProcessData& data,
                          const ArrayView<int>& descriptor_indices,
                          const AudioRenderInfo& info) {
  auto maybe_stream = find_stream(streams, to_stream);
  if (maybe_stream == streams.end()) {
    return false;

  } else {
    return (*maybe_stream)->write(data, descriptor_indices, info);
  }
}

bool AudioRecorder::accumulate(AudioRecordStreamHandle to_stream,
                               const AudioProcessData& data,
                               const ArrayView<int>& descriptor_indices,
                               const AudioRenderInfo& info) {
  auto maybe_stream = find_stream(streams, to_stream);
  if (maybe_stream == streams.end()) {
    return false;

  } else {
    return (*maybe_stream)->accumulate(data, descriptor_indices, info);
  }
}

std::unique_ptr<AudioRecorder::CreateStreamFuture>
AudioRecorder::create_stream(AudioRecordChannelSet layout, const Transport* transport) {
  if (ui_create_stream_commands.full()) {
    return nullptr;

  } else {
    auto future = std::make_unique<AudioRecorder::CreateStreamFuture>();
    future->layout = std::move(layout);
    future->transport = transport;
    ui_create_stream_commands.write(future.get());
    return future;
  }
}

std::unique_ptr<AudioRecorder::StartStreamFuture>
AudioRecorder::start_recording(AudioRecordStreamHandle handle) {
  if (streams_pending_record_start.full()) {
    return nullptr;

  } else {
    auto future = std::make_unique<AudioRecorder::StartStreamFuture>();
    future->handle = handle;
    streams_pending_record_start.write(future.get());

    return future;
  }
}

std::unique_ptr<AudioRecorder::StopStreamFuture>
AudioRecorder::stop_recording(AudioRecordStreamHandle handle) {
  if (streams_pending_record_stop.full()) {
    return nullptr;

  } else {
    auto future = std::make_unique<AudioRecorder::StopStreamFuture>();
    future->handle = handle;
    streams_pending_record_stop.write(future.get());

    return future;
  }
}

int AudioRecorder::size() const {
  auto sz = pending_data_retrieval_from_backing_store.size() +
            pending_ui_data_retrieval.size() +
            ui_create_stream_commands.size() +
            queued_create_stream_commands.size() +
            pending_creation_from_backing_store.size() +
            streams_pending_record_start.size() +
            streams_pending_record_stop.size() +
            streams_stopped_not_by_trigger.size();
  return int(sz);
}

/*
 * AudioRecordStreamBackingStoreTask
 */

AudioRecordStreamBackingStoreTask::~AudioRecordStreamBackingStoreTask() {
  terminate();
}

void AudioRecordStreamBackingStoreTask::process_create_stream_commands() {
  int num_pending_creation = pending_created_streams.size();

  for (int i = 0; i < num_pending_creation; i++) {
    auto future = pending_created_streams.read();

    auto stream_handle = AudioRecordStreamHandle::create();
    auto stream =
      std::make_unique<AudioRecordStream>(
        stream_handle, future->transport, std::move(future->layout), future->info);

    auto* stream_ptr = stream.get();
    streams.push_back(std::move(stream));

    future->stream = stream_ptr;
    future->is_ready.store(true);
  }
}

void AudioRecordStreamBackingStoreTask::process_retrieve_data_commands() {
  int num_pending_retrieval = streams_pending_data_retrieval.size();

  for (int i = 0; i < num_pending_retrieval; i++) {
    auto* future = streams_pending_data_retrieval.read();
    auto maybe_stream = find_stream(streams, future->handle);

    if (maybe_stream == streams.end()) {
      future->success = false;

    } else {
      auto* stream = maybe_stream->get();
      if (!await_pending_blocks(stream, num_ms_await_stream_block)) {
        //  Failed to acquire all pending blocks from the stream in time.
        future->success = false;

      } else {
        //  All data were received from stream. The stream might still have errored, e.g. in the
        //  case that, at some point during recording, a block could not be reserved for the stream.
        future->stream_result = make_audio_record_stream_result(stream);
      }

      streams.erase(maybe_stream);
    }

    future->is_ready.store(true);
  }
}

void AudioRecordStreamBackingStoreTask::update() {
  process_create_stream_commands();

  for (auto& stream : streams) {
    stream->backing_store_update();
  }

  process_retrieve_data_commands();

  GROVE_SANITY_CHECK_SIZE("Task: ")
}

void AudioRecordStreamBackingStoreTask::sleep() const {
  std::this_thread::sleep_for(std::chrono::milliseconds(num_ms_sleep));
}

void AudioRecordStreamBackingStoreTask::initialize() {
  assert(!initialized);
  initialized = true;
  keep_processing.store(true);

  task = std::thread([this]() {
    stream_backing_store_task(this);
  });
}

void AudioRecordStreamBackingStoreTask::terminate() {
  keep_processing.store(false);
  
  if (task.joinable()) {
    task.join();
  }

  initialized = false;
}

std::unique_ptr<AudioRecordStreamBackingStoreTask::CreateStreamFuture>
AudioRecordStreamBackingStoreTask::create_stream(AudioRecordChannelSet layout,
                                                 const AudioRenderInfo& info,
                                                 const Transport* transport) {
  if (pending_created_streams.full()) {
    return nullptr;

  } else {
    auto future = std::make_unique<AudioRecordStreamBackingStoreTask::CreateStreamFuture>();
    future->transport = transport;
    future->layout = std::move(layout);
    future->info = info;

    pending_created_streams.write(future.get());
    return future;
  }
}

std::unique_ptr<AudioRecordStreamBackingStoreTask::RetrieveDataFuture>
AudioRecordStreamBackingStoreTask::retrieve_data(AudioRecordStreamHandle for_stream) {
  if (streams_pending_data_retrieval.full()) {
    return nullptr;

  } else {
    auto future = std::make_unique<AudioRecordStreamBackingStoreTask::RetrieveDataFuture>();
    future->handle = for_stream;

    streams_pending_data_retrieval.write(future.get());
    return future;
  }
}

int AudioRecordStreamBackingStoreTask::size() const {
  return int(
    streams.size() + pending_created_streams.size() + streams_pending_data_retrieval.size());
}

#undef GROVE_SANITY_CHECK_SIZE

GROVE_NAMESPACE_END