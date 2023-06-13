#include "audio_events.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

/*
 * AudioEventIDStore
 */

std::atomic<uint32_t> AudioEventIDStore::next_id{0};

/*
 * AudioEvent
 */

static_assert(std::is_trivial<AudioEvent>::value,
  "Expected AudioEvent to be trivial.");
static_assert(std::is_standard_layout<AudioEvent>::value,
  "Expected AudioEvent to be of standard layout.");

GROVE_NAMESPACE_END
