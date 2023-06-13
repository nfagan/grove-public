function(configure_common)

set(sources
  algorithm.hpp
  ArrayView.hpp
  BuddyAllocator.hpp
  common.hpp
  config.hpp
  ContiguousElementGroupAllocator.hpp
  ContiguousElementGroupAllocator.cpp
  DistinctRanges.hpp
  DynamicArray.hpp
  Either.hpp
  Future.hpp
  fs.hpp
  fs.cpp
  Handshake.hpp
  History.hpp
  identifier.hpp
  intrin.hpp
  intrin.cpp
  logging.hpp
  logging.cpp
  memory.hpp
  memory.cpp
  profile.hpp
  profile.cpp
  RingBuffer.hpp
  StatStopwatch.hpp
  StatStopwatch.cpp
  Stopwatch.hpp
  platform.hpp
  pack.hpp
  pack.cpp
  scope.hpp
  stats.hpp
  SimulationTimer.hpp
  SlotLists.hpp
  Temporary.hpp
  Unique.hpp
  Optional.hpp
  QueuedRingBuffer.hpp
  vector_util.hpp
)

foreach(source ${sources})
  target_sources(${PROJECT_NAME} PRIVATE ${GROVE_MODULE_SOURCE_DIR}/common/${source})
endforeach()

endfunction(configure_common)

configure_common()