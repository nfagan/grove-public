#include "NotePacketAllocator.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

struct NotePacketListIterator {
  NotePacketList* list;
  uint32_t note_index;
  uint32_t num_notes;
};

NotePacketListIterator begin(NotePacketList* list) {
  NotePacketListIterator result{};
  result.list = list;
  result.num_notes = list ? list->packet.num_notes : 0;
  return result;
}

bool is_valid(NotePacketListIterator it) {
  if (it.note_index < it.num_notes) {
    return true;
  } else {
    return it.list && it.list->next != nullptr && it.list->next->packet.num_notes > 0;
  }
}

NotePacketListIterator next(NotePacketListIterator it) {
  if (++it.note_index == it.num_notes) {
    assert(it.list);
    it.list = it.list->next.get();
    it.note_index = 0;
    it.num_notes = it.list ? it.list->packet.num_notes : 0;
  }
  return it;
}

const ClipNote& deref(NotePacketListIterator it) {
  assert(is_valid(it));
  return it.list->packet.notes[it.note_index];
}

bool less_by_span_begin(const ClipNote& a, const ClipNote& b) {
  return a.span.begin < b.span.begin;
}

[[maybe_unused]] std::vector<ClipNote> collect_notes(const NotePacketList* list) {
  std::vector<ClipNote> notes;
  while (list) {
    for (uint32_t i = 0; i < list->packet.num_notes; i++) {
      notes.push_back(list->packet.notes[i]);
    }
    list = list->next.get();
  }
  return notes;
}

bool has_parent(const NotePacketAllocator::Item& item) {
  return item.parent.is_valid();
}

bool has_next(const NotePacketAllocator::Item& item) {
  return item.next.is_valid();
}

bool full(const NotePacket& packet) {
  return packet.num_notes == uint32_t(packet.notes.size());
}

NoteListHandle head_handle(const NotePacketAllocator* alloc, NoteListHandle handle) {
  const auto* item = &alloc->items.at(handle.id);
  while (has_parent(*item)) {
    handle = item->parent;
    item = &alloc->items.at(handle.id);
  }
  return handle;
}

NotePacketAllocator::List* source_of(NotePacketAllocator* alloc, NoteListHandle handle) {
  while (true) {
    auto item_it = alloc->items.find(handle.id);
    if (item_it == alloc->items.end()) {
      return nullptr;

    } else if (!has_parent(item_it->second)) {
      return &item_it->second.list;

    } else {
      handle = item_it->second.parent;
    }
  }
  assert(false);
  return nullptr;
}

std::unique_ptr<NotePacketList> acquire_packet(NotePacketAllocator* alloc) {
  if (alloc->free_packets.empty()) {
    return std::make_unique<NotePacketList>();
  } else {
    auto res = std::move(alloc->free_packets.back());
    assert(!res->next);
    res->packet = {};
    alloc->free_packets.pop_back();
    return res;
  }
}

NotePacketAllocator::List create_list(NotePacketAllocator* alloc) {
  NotePacketAllocator::List result{};
  result.head = acquire_packet(alloc);
  result.sorted.push_back(result.head.get());
  return result;
}

NotePacketAllocator::List clone(NotePacketAllocator* alloc, NoteListHandle handle) {
  const auto& src_list = alloc->items.at(handle.id).list;
  if (!src_list.head) {
    return {};
  }

  const auto* src = src_list.head.get();
  auto dst_head = acquire_packet(alloc);
  auto* dst = dst_head.get();

  NotePacketAllocator::List result{};
  result.head = std::move(dst_head);
  result.sorted.resize(src_list.sorted.size());

  int pi{};
  while (true) {
    assert(pi < int(result.sorted.size()));

    dst->packet = src->packet;
    result.sorted[pi++] = dst;

    if (src->next) {
      dst->next = acquire_packet(alloc);
      src = src->next.get();
      dst = dst->next.get();
    } else {
      break;
    }
  }

  return result;
}

void deparent_next(NotePacketAllocator* alloc, const NotePacketAllocator::Item& item,
                   NoteListHandle handle) {
  auto& next = alloc->items.at(item.next.id);
  assert(next.parent == handle);
  next.list = clone(alloc, handle);
  next.parent = {};
}

void deparent_self(NotePacketAllocator* alloc, NotePacketAllocator::Item& item) {
  auto& par = alloc->items.at(item.parent.id);
  par.next = item.next;
  if (item.next.is_valid()) {
    alloc->items.at(item.next.id).parent = item.parent;
  }
  item.parent = {};
}

void on_write(NotePacketAllocator* alloc, NotePacketAllocator::Item& item, NoteListHandle handle) {
  if (has_parent(item)) {
    assert(!item.list.head);
    item.list = clone(alloc, head_handle(alloc, handle));
    deparent_self(alloc, item);

  } else if (has_next(item)) {
    deparent_next(alloc, item, handle);
  }

  item.next = {};
}

NotePacketAllocator::Item* lookup(NotePacketAllocator* alloc, NoteListHandle handle) {
  auto item_it = alloc->items.find(handle.id);
  if (item_it == alloc->items.end()) {
    return nullptr;
  } else {
    return &item_it->second;
  }
}

void sort_notes(NotePacket& packet) {
  std::sort(packet.notes.data(), packet.notes.data() + packet.num_notes, less_by_span_begin);
}

void add_note(NotePacket& packet, const ClipNote& note) {
  assert(packet.num_notes < packet.notes.size());
  packet.notes[packet.num_notes++] = note;
  sort_notes(packet);
}

void right_shift_notes(NotePacket& packet) {
  std::rotate(
    packet.notes.data(),
    packet.notes.data() + packet.num_notes - 1,
    packet.notes.data() + packet.num_notes);
}

void erase_note(NotePacket& packet, uint32_t note_ind) {
  assert(packet.num_notes > 0);
  std::rotate(
    packet.notes.data() + note_ind,
    packet.notes.data() + note_ind + 1,
    packet.notes.data() + packet.num_notes);
  packet.num_notes--;
}

void shift_list_insert_note(NotePacketAllocator* alloc, NotePacketList* at,
                            NotePacketAllocator::Item& item, ClipNote note) {
  assert(full(at->packet));

  ClipNote leftover = at->packet.notes[at->packet.num_notes - 1];
  if (less_by_span_begin(leftover, note)) {
    std::swap(leftover, note);
  }
  right_shift_notes(at->packet);
  at->packet.notes[0] = note;
  sort_notes(at->packet);

  while (true) {
    if (!at->next) {
      at->next = acquire_packet(alloc);
      item.list.sorted.push_back(at->next.get());
    }

    auto& dst_packet = at->next->packet;
    if (!full(dst_packet)) {
      add_note(dst_packet, leftover);
      return;

    } else {
      auto tmp = leftover;
      leftover = dst_packet.notes[dst_packet.num_notes - 1];
      right_shift_notes(dst_packet);
      dst_packet.notes[0] = tmp;
      at = at->next.get();
    }
  }
}

const ClipNote* latest_note(const NotePacket& packet) {
  assert(packet.num_notes > 0);
  return &packet.notes[packet.num_notes - 1];
}

[[maybe_unused]] const ClipNote* earliest_note(const NotePacket& packet) {
  assert(packet.num_notes > 0);
  return &packet.notes[0];
}

NotePacketList make_lookup_list(ScoreCursor begin) {
  NotePacketList result{};
  result.packet.notes[0].span.begin = begin;
  result.packet.num_notes = 1;
  return result;
}

NotePacketList* lower_bound_on_list(NotePacketAllocator::List& list, ScoreCursor begin, int* index) {
  NotePacketList lookup_list = make_lookup_list(begin);

  auto it = std::lower_bound(
    list.sorted.begin(),
    list.sorted.end(),
    &lookup_list, [](NotePacketList* a, NotePacketList* b) {
      return latest_note(a->packet)->span.begin < latest_note(b->packet)->span.begin;
    });

  if (it == list.sorted.end()) {
    return nullptr;
  } else {
    *index = int(it - list.sorted.begin());
    return *it;
  }
}

NotePacketList* lower_bound_on_list(NotePacketAllocator::List& list, ScoreCursor begin) {
  int ignore;
  return lower_bound_on_list(list, begin, &ignore);
}

int lower_bound_on_note(const NotePacket& packet, const ClipNote& note) {
  auto it = std::lower_bound(
    packet.notes.data(),
    packet.notes.data() + packet.num_notes,
    note, less_by_span_begin);

  if (it == packet.notes.data() + packet.num_notes) {
    return -1;
  } else {
    return int(it - packet.notes.data());
  }
}

bool find_note(NotePacketAllocator::List& list, const ClipNote& note,
               NotePacketList** in_list, int* note_index) {
  int list_index;
  auto* target = lower_bound_on_list(list, note.span.begin, &list_index);
  if (!target) {
    return false;
  }

  int note_ind = lower_bound_on_note(target->packet, note);
  if (note_ind == -1) {
    return false;
  }

  while (true) {
    if (target->packet.notes[note_ind] == note) {
      *in_list = target;
      *note_index = note_ind;
      return true;
    } else {
      if (++note_ind == int(target->packet.num_notes)) {
        if (++list_index < int(list.sorted.size())) {
          assert(target->next && target->next.get() == list.sorted[list_index]);
          target = list.sorted[list_index];
          note_ind = 0;
        } else {
          return false;
        }
      }
    }
  }

  assert(false);
  return false;
}

int collect_notes_intersecting_note(NotePacketAllocator* alloc, NoteListHandle handle,
                                    const ClipNote& src, double beats_per_measure,
                                    ClipNote* dst, int max_num_dst) {
  auto* list = source_of(alloc, handle);
  if (!list) {
    return 0;
  }

  int collect_ind{};
  for (auto it = grove::begin(list->head.get()); is_valid(it); it = next(it)) {
    const auto& note = deref(it);
    if (note.intersects(src, beats_per_measure)) {
      if (collect_ind < max_num_dst) {
        dst[collect_ind] = note;
      }
      collect_ind++;
    }
  }

  return collect_ind;
}

[[maybe_unused]] bool is_sorted(const std::vector<ClipNote>& notes) {
  return std::is_sorted(notes.begin(), notes.end(), less_by_span_begin);
}

[[maybe_unused]] bool is_consistent(const NotePacketList* list,
                                    const std::vector<NotePacketList*>& sorted) {
  const NotePacketList* parent{};
  int node_ind{};
  while (list) {
    if (node_ind >= int(sorted.size()) || sorted[node_ind] != list) {
      return false;
    }
    if (parent && less_by_span_begin(*earliest_note(list->packet), *latest_note(parent->packet))) {
      return false;
    }
    parent = list;
    list = list->next.get();
    node_ind++;
  }
  return node_ind == int(sorted.size());
}

} //  anon

NoteListHandle create_note_list(NotePacketAllocator* alloc) {
  NotePacketAllocator::Item item{};
  NoteListHandle handle{alloc->next_handle_id++};
  alloc->items[handle.id] = std::move(item);
  return handle;
}

void destroy_note_list(NotePacketAllocator* alloc, NoteListHandle handle) {
  auto& item = *lookup(alloc, handle);

  if (has_parent(item)) {
    deparent_self(alloc, item);

  } else if (has_next(item)) {
    deparent_next(alloc, item, handle);
  }

  if (item.list.head) {
    auto* list = item.list.head.get();
    while (list->next) {
      auto* tmp = list->next.get();
      alloc->free_packets.push_back(std::move(list->next));
      list = tmp;
    }
    alloc->free_packets.push_back(std::move(item.list.head));
  }

  alloc->items.erase(handle.id);
}

NoteListHandle clone_note_list(NotePacketAllocator* alloc, NoteListHandle src) {
  auto* item = lookup(alloc, src);
  assert(item);

  while (item->next.is_valid()) {
    src = item->next;
    item = lookup(alloc, item->next);
  }

  assert(!has_next(*item));
  NoteListHandle dst{alloc->next_handle_id++};
  NotePacketAllocator::Item dst_item{};
  dst_item.parent = src;
  item->next = dst;
  alloc->items[dst.id] = std::move(dst_item);
  return dst;
}

void add_note(NotePacketAllocator* alloc, NoteListHandle handle, const ClipNote& note) {
  auto& item = *lookup(alloc, handle);
  on_write(alloc, item, handle);

  if (!item.list.head) {
    assert(item.list.sorted.empty());
    item.list = create_list(alloc);
    add_note(item.list.head->packet, note);
    return;
  }

  auto* target = lower_bound_on_list(item.list, note.span.begin);
  if (!target) {
    //  Note is later than the current latest note.
    target = item.list.sorted.back();
  }

  if (!full(target->packet)) {
    add_note(target->packet, note);
  } else {
    shift_list_insert_note(alloc, target, item, note);
  }

#ifdef GROVE_DEBUG
  {
    auto notes = collect_notes(item.list.head.get());
    assert(is_sorted(notes));
    assert(is_consistent(item.list.head.get(), item.list.sorted));
  }
#endif
}

void remove_note(NotePacketAllocator* alloc, NoteListHandle handle, const ClipNote& note) {
  auto& item = *lookup(alloc, handle);
  on_write(alloc, item, handle);

  NotePacketList* target;
  int note_ind;
  if (!find_note(item.list, note, &target, &note_ind)) {
    assert(false && "No such note.");
    return;
  }

  erase_note(target->packet, uint32_t(note_ind));

  if (target->packet.num_notes == 0) {
    NotePacketList* parent = nullptr;
    NotePacketList* self = item.list.head.get();
    int node_index{};

    while (true) {
      assert(node_index < int(item.list.sorted.size()));
      if (self == target) {
        assert(item.list.sorted[node_index] == target);
        if (parent) {
          auto to_return = std::move(parent->next);
          parent->next = std::move(self->next);
          alloc->free_packets.push_back(std::move(to_return));
          item.list.sorted.erase(item.list.sorted.begin() + node_index);
        } else {
          assert(self == item.list.head.get());
          auto new_head = std::move(self->next);
          alloc->free_packets.push_back(std::move(item.list.head));
          item.list.sorted.erase(item.list.sorted.begin());
          item.list.head = std::move(new_head);
          if (item.list.head) {
            assert(!item.list.sorted.empty() && item.list.sorted[0] == item.list.head.get());
          }
        }
        break;
      } else {
        assert(self->next);
        parent = self;
        self = self->next.get();
        node_index++;
      }
    }
  }

#ifdef GROVE_DEBUG
  {
    auto notes = collect_notes(item.list.head.get());
    assert(is_sorted(notes));
    assert(is_consistent(item.list.head.get(), item.list.sorted));
  }
#endif
}

ArrayView<const ClipNote>
find_notes_intersecting_note(NotePacketAllocator* alloc, NoteListHandle handle,
                             const ClipNote& note, double beats_per_measure,
                             TemporaryView<ClipNote>& tmp) {
  auto* res = tmp.require(tmp.stack_size);
  const int num_intersecting = collect_notes_intersecting_note(
    alloc, handle, note, beats_per_measure, res, tmp.stack_size);

  if (num_intersecting > tmp.stack_size) {
    //  Allocate and reacquire.
    res = tmp.require(num_intersecting);
    collect_notes_intersecting_note(
      alloc, handle, note, beats_per_measure, res, num_intersecting);
  }

  return {res, res + num_intersecting};
}

uint32_t total_num_notes(NotePacketAllocator* alloc, NoteListHandle handle) {
  auto* list = source_of(alloc, handle);
  if (!list) {
    return 0;
  }
  uint32_t s{};
  auto* h = list->head.get();
  while (h) {
    s += h->packet.num_notes;
    h = h->next.get();
  }
  return s;
}

int collect_notes_starting_in_region(NotePacketAllocator* alloc, NoteListHandle handle,
                                     ScoreCursor begin, ScoreCursor end,
                                     ClipNote* dst, int max_num_dst) {
  auto* list = source_of(alloc, handle);
  if (!list) {
    return 0;
  }

  auto* target = lower_bound_on_list(*list, begin);
  if (!target) {
    return 0;
  }

  int collect_ind{};
  for (auto it = grove::begin(target); is_valid(it); it = next(it)) {
    const auto& note = deref(it);
    if (note.span.begin >= end) {
      break;

    } else if (note.span.begin >= begin) {
      if (collect_ind < max_num_dst) {
        assert(note.span.begin >= begin && note.span.begin < end);
        dst[collect_ind] = note;
      }
      collect_ind++;
    }
  }

  return collect_ind;
}

const ClipNote* find_note(NotePacketAllocator* alloc, NoteListHandle handle,
                          ScoreCursor begin, ScoreCursor end, MIDINote search_note) {
  auto* list = source_of(alloc, handle);
  if (!list) {
    return nullptr;
  }

  auto* target = lower_bound_on_list(*list, begin);
  if (!target) {
    return nullptr;
  }

  for (auto it = grove::begin(target); is_valid(it); it = next(it)) {
    const auto& note = deref(it);
    if (note.span.begin >= end) {
      break;

    } else if (note.span.begin == begin &&
               note.note.matches_pitch_class_and_octave(search_note)) {
      return &note;
    }
  }

  return nullptr;
}

GROVE_NAMESPACE_END
