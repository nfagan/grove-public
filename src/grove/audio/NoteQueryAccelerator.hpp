#pragma once

#include "types.hpp"
#include "ScoreRegionTree.hpp"
#include "grove/common/identifier.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/ArrayView.hpp"
#include <unordered_map>

namespace grove {

struct NoteQueryAcceleratorInstanceHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(NoteQueryAcceleratorInstanceHandle, id)
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  uint32_t id;
};

using NoteTreeDataAlloc = ScoreRegionTreeDataAllocator<ClipNote>;
using NoteTreeIndexPacketAlloc = ScoreRegionTreeDataIndexAllocator<1>;
using NoteQueryTree = ScoreRegionTree<ClipNote>;
using NoteQueryTraversalStack = ScoreRegionTreeStaticIndexStack<512>;

struct NoteQueryAccelerator {
public:
  struct Item {
    NoteQueryAcceleratorInstanceHandle parent{};
    NoteQueryAcceleratorInstanceHandle next{};
    NoteQueryTree tree;
  };

public:
  std::unordered_map<uint32_t, Item> items;
  NoteTreeDataAlloc data_alloc;
  NoteTreeIndexPacketAlloc index_alloc;

  uint32_t next_instance_id{1};
};

NoteQueryAcceleratorInstanceHandle
create_note_query_accelerator_instance(NoteQueryAccelerator* accel);

NoteQueryAcceleratorInstanceHandle
clone_note_query_accelerator_instance(NoteQueryAccelerator* accel,
                                      NoteQueryAcceleratorInstanceHandle handle);

void destroy_note_query_accelerator_instance(NoteQueryAccelerator* accel,
                                             NoteQueryAcceleratorInstanceHandle handle);

void insert_note(NoteQueryAccelerator* accel, NoteQueryAcceleratorInstanceHandle handle,
                 ClipNote note);
void remove_note(NoteQueryAccelerator* accel, NoteQueryAcceleratorInstanceHandle handle,
                 ClipNote note);
void remove_all_notes(NoteQueryAccelerator* accel, NoteQueryAcceleratorInstanceHandle handle);
const NoteQueryTree* read_note_query_tree(NoteQueryAccelerator* accel,
                                          NoteQueryAcceleratorInstanceHandle handle);
const ClipNote* find_cursor_strictly_within_note(NoteQueryAccelerator* accel,
                                                 const NoteQueryTree* tree, const ScoreCursor& cursor,
                                                 MIDINote note);
const ClipNote* find_note(NoteQueryAccelerator* accel, const NoteQueryTree* tree,
                          ScoreCursor begin, MIDINote note);

int collect_notes_starting_in_region(NoteQueryAccelerator* accel, const NoteQueryTree* tree,
                                     const ScoreRegion& region, uint32_t* dst_indices,
                                     ClipNote* dst, int max_num_dst);

int collect_notes_intersecting_region(NoteQueryAccelerator* accel, const NoteQueryTree* tree,
                                      const ScoreRegion& region, uint32_t* dst_indices,
                                      ClipNote* dst, int max_num_dst);
int collect_notes_intersecting_note(NoteQueryAccelerator* accel, const NoteQueryTree* tree,
                                    const ScoreRegion& region, MIDINote note,
                                    uint32_t* dst_indices, ClipNote* dst, int max_num_dst);
ArrayView<const ClipNote>
collect_notes_intersecting_note(NoteQueryAccelerator* accel, const NoteQueryTree* tree,
                                const ScoreRegion& region, MIDINote note,
                                TemporaryView<uint32_t>& dst_indices,
                                TemporaryView<ClipNote>& dst_notes);

}