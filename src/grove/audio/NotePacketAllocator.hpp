#pragma once

#include "types.hpp"
#include "grove/common/identifier.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/ArrayView.hpp"
#include <array>
#include <cstdint>
#include <unordered_map>

namespace grove {

struct NoteListHandle {
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(NoteListHandle, id)
  uint64_t id;
};

struct NotePacket {
  std::array<ClipNote, 32> notes;
  uint32_t num_notes;
};

struct NotePacketList {
  std::unique_ptr<NotePacketList> next;
  NotePacket packet;
};

struct NotePacketAllocator {
public:
  struct List {
    std::unique_ptr<NotePacketList> head;
    std::vector<NotePacketList*> sorted;
  };

  struct Item {
    NoteListHandle parent;
    NoteListHandle next;
    List list;
  };

public:
  std::unordered_map<uint64_t, Item> items;
  std::vector<std::unique_ptr<NotePacketList>> free_packets;
  uint64_t next_handle_id{1};
};

NoteListHandle create_note_list(NotePacketAllocator* alloc);
void destroy_note_list(NotePacketAllocator* alloc, NoteListHandle handle);
NoteListHandle clone_note_list(NotePacketAllocator* alloc, NoteListHandle handle);

void add_note(NotePacketAllocator* alloc, NoteListHandle handle, const ClipNote& note);
void remove_note(NotePacketAllocator* alloc, NoteListHandle handle, const ClipNote& note);

ArrayView<const ClipNote>
find_notes_intersecting_note(NotePacketAllocator* alloc, NoteListHandle handle,
                             const ClipNote& note, double beats_per_measure,
                             TemporaryView<ClipNote>& tmp);

int collect_notes_starting_in_region(NotePacketAllocator* alloc, NoteListHandle handle,
                                     ScoreCursor begin, ScoreCursor end,
                                     ClipNote* dst, int max_num_dst);
const ClipNote* find_note(NotePacketAllocator* alloc, NoteListHandle handle,
                          ScoreCursor begin, ScoreCursor end, MIDINote note);

uint32_t total_num_notes(NotePacketAllocator* alloc, NoteListHandle handle);

}