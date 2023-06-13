#pragma once

#include "common.hpp"
#include "grove/common/Optional.hpp"
#include <future>
#include <atomic>

namespace grove::bounds {

struct AccelInstanceHandle {
  GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, AccelInstanceHandle, id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(AccelInstanceHandle, id)
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  uint32_t id;
};

struct CreateAccelInstanceParams {
  float initial_span_size;
  float max_span_size_split;
};

struct BoundsSystem {
public:
  struct Instance {
    uint32_t id{};
    Accel accel;
    Optional<AccessorID> current_writer;
    std::vector<AccessorID> current_readers;
    AccessorID self_id{};

    //  Automatically rebuild if the proportion of inactive elements is greater than this threshold.
    float auto_rebuild_proportion_threshold{0.25f};
    bool need_check_auto_rebuild{};
    bool need_rebuild_accel{};
    bool rebuilding_accel{};
    bool deactivating{};
    CreateAccelInstanceParams rebuild_params{};
    std::vector<bounds::ElementID> pending_deactivation;
    std::future<void> async_future;
    std::atomic<bool> async_complete{};
  };

public:
  std::vector<std::unique_ptr<Instance>> instances;
  uint32_t next_instance_id{1};
  AccessorID self_accessor_id{AccessorID::create()};
};

AccelInstanceHandle create_instance(BoundsSystem* sys, const CreateAccelInstanceParams& params);
Accel* request_write(BoundsSystem* sys, AccelInstanceHandle instance, AccessorID id);
const Accel* request_read(BoundsSystem* sys, AccelInstanceHandle instance, AccessorID id);
void release_read(BoundsSystem* sys, AccelInstanceHandle instance, AccessorID id);
void release_write(BoundsSystem* sys, AccelInstanceHandle instance, AccessorID id);
void rebuild_accel(BoundsSystem* sys, AccelInstanceHandle instance,
                   const CreateAccelInstanceParams& params);
Accel* request_transient_write(BoundsSystem* sys, AccelInstanceHandle instance);
void release_transient_write(BoundsSystem* sys, AccelInstanceHandle instance);
void push_pending_deactivation(BoundsSystem* sys, AccelInstanceHandle instance,
                               const ElementID* ids, uint32_t num_ids);
void push_pending_deactivation(BoundsSystem* sys, AccelInstanceHandle instance,
                               std::vector<ElementID>&& ids);
size_t deactivate_element(bounds::Accel* accel, bounds::ElementID id);
void update(BoundsSystem* sys);

}