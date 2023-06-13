#include "NoteQueryAccelerator.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

bool has_next(const NoteQueryAccelerator::Item& item) {
  return item.next.is_valid();
}

bool has_parent(const NoteQueryAccelerator::Item& item) {
  return item.parent.is_valid();
}

NoteQueryTree* source_of(NoteQueryAccelerator* accel, NoteQueryAcceleratorInstanceHandle handle) {
  while (true) {
    auto item_it = accel->items.find(handle.id);
    if (item_it == accel->items.end()) {
      return nullptr;
    } else if (!has_parent(item_it->second)) {
      return &item_it->second.tree;
    } else {
      handle = item_it->second.parent;
    }
  }
  assert(false);
  return nullptr;
}

NoteQueryTree clone(NoteQueryAccelerator* accel, NoteQueryAcceleratorInstanceHandle src) {
  const auto& src_tree = accel->items.at(src.id).tree;
  return grove::clone(&src_tree, &accel->index_alloc, &accel->data_alloc);
}

NoteQueryAcceleratorInstanceHandle head_handle(const NoteQueryAccelerator* accel,
                                               NoteQueryAcceleratorInstanceHandle handle) {
  const auto* item = &accel->items.at(handle.id);
  while (has_parent(*item)) {
    handle = item->parent;
    item = &accel->items.at(handle.id);
  }
  return handle;
}

void deparent_next(NoteQueryAccelerator* accel, const NoteQueryAccelerator::Item& item,
                   NoteQueryAcceleratorInstanceHandle handle) {
  auto& next = accel->items.at(item.next.id);
  assert(next.parent == handle);
  next.tree = clone(accel, handle);
  next.parent = {};
}

void deparent_self(NoteQueryAccelerator* accel, NoteQueryAccelerator::Item& item) {
  auto& par = accel->items.at(item.parent.id);
  par.next = item.next;
  if (item.next.is_valid()) {
    accel->items.at(item.next.id).parent = item.parent;
  }
  item.parent = {};
}

void on_write(NoteQueryAccelerator* accel, NoteQueryAccelerator::Item& item,
              NoteQueryAcceleratorInstanceHandle handle) {
  if (has_parent(item)) {
    item.tree = clone(accel, head_handle(accel, handle));
    deparent_self(accel, item);

  } else if (has_next(item)) {
    deparent_next(accel, item, handle);
  }

  item.next = {};
}

NoteQueryAccelerator::Item* lookup(NoteQueryAccelerator* accel,
                                   NoteQueryAcceleratorInstanceHandle handle) {
  auto item_it = accel->items.find(handle.id);
  if (item_it == accel->items.end()) {
    return nullptr;
  } else {
    return &item_it->second;
  }
}

template <typename F>
int collect_notes(NoteQueryAccelerator* accel, const NoteQueryTree* tree, const F& f,
                  const ScoreRegion& span, uint32_t* dst_indices, ClipNote* dst, int max_num_dst) {
  NoteQueryTraversalStack stack;
  stack.size = 0;

  auto res = collect_unique_if(
    tree, span, f, &accel->index_alloc, &accel->data_alloc, stack, dst_indices, max_num_dst);

  if (!res.traversed) {
    assert(false);
    return 0;
  } else {
    for (uint32_t i = 0; i < res.num_collected; i++) {
      dst[i] = accel->data_alloc.items[dst_indices[i]];
    }
    return int(res.num_would_collect);
  }
}

} //  anon

NoteQueryAcceleratorInstanceHandle
create_note_query_accelerator_instance(NoteQueryAccelerator* accel) {
  NoteQueryAcceleratorInstanceHandle result{accel->next_instance_id++};
  NoteQueryAccelerator::Item item{};
  item.tree = NoteQueryTree{};
  accel->items[result.id] = std::move(item);
  return result;
}

NoteQueryAcceleratorInstanceHandle
clone_note_query_accelerator_instance(NoteQueryAccelerator* accel,
                                      NoteQueryAcceleratorInstanceHandle src) {
  auto* item = lookup(accel, src);
  assert(item);

  while (item->next.is_valid()) {
    src = item->next;
    item = lookup(accel, item->next);
  }

  assert(!has_next(*item));
  NoteQueryAcceleratorInstanceHandle dst{accel->next_instance_id++};
  NoteQueryAccelerator::Item dst_item{};
  dst_item.parent = src;
  item->next = dst;
  accel->items[dst.id] = std::move(dst_item);
  return dst;
}

void destroy_note_query_accelerator_instance(NoteQueryAccelerator* accel,
                                             NoteQueryAcceleratorInstanceHandle handle) {
  auto& item = *lookup(accel, handle);
  bool owns_tree = !has_parent(item);

  if (has_parent(item)) {
    deparent_self(accel, item);

  } else if (has_next(item)) {
    deparent_next(accel, item, handle);
  }

  if (owns_tree) {
    clear_contents(&item.tree, &accel->index_alloc, &accel->data_alloc);
  }

  accel->items.erase(handle.id);
}

void insert_note(NoteQueryAccelerator* accel, NoteQueryAcceleratorInstanceHandle handle,
                 ClipNote note) {
  auto& item = *lookup(accel, handle);
  on_write(accel, item, handle);
  insert(&item.tree, note.span, std::move(note), &accel->index_alloc, &accel->data_alloc);
}

void remove_note(NoteQueryAccelerator* accel, NoteQueryAcceleratorInstanceHandle handle,
                 ClipNote note) {
  auto& item = *lookup(accel, handle);
  on_write(accel, item, handle);
  auto f = [note](const ClipNote& src) { return src == note; };
  bool removed = remove_if(&item.tree, note.span, f, &accel->index_alloc, &accel->data_alloc);
  assert(removed && "No such note.");
  (void) removed;
}

void remove_all_notes(NoteQueryAccelerator* accel, NoteQueryAcceleratorInstanceHandle handle) {
  auto& item = *lookup(accel, handle);
  on_write(accel, item, handle);
  clear_contents(&item.tree, &accel->index_alloc, &accel->data_alloc);
}

const NoteQueryTree* read_note_query_tree(NoteQueryAccelerator* accel,
                                          NoteQueryAcceleratorInstanceHandle handle) {
  return source_of(accel, handle);
}

const ClipNote*
find_cursor_strictly_within_note(NoteQueryAccelerator* accel,
                                 const NoteQueryTree* tree, const ScoreCursor& cursor, MIDINote note) {
  NoteQueryTraversalStack stack;
  stack.size = 0;

  const ClipNote* dst{};
  auto f = [note, cursor, &dst](const ClipNote& src) {
    auto beg = src.span.begin;
    auto end = src.span.end(NoteQueryTree::modulus);
    if (src.note == note && cursor > beg && cursor < end) {
      dst = &src;
      return true;
    } else {
      return false;
    }
  };
  auto res = test_cursor(tree, cursor, f, &accel->index_alloc, &accel->data_alloc, stack);
  if (!res.traversed) {
    assert(false);
    return nullptr;
  } else {
    return dst;
  }
}

const ClipNote* find_note(NoteQueryAccelerator* accel, const NoteQueryTree* tree,
                          ScoreCursor begin, MIDINote note) {
  const ClipNote* dst{};
  auto f = [begin, note, &dst](const ClipNote& src) {
    bool match = src.note.matches_pitch_class_and_octave(note) && src.span.begin == begin;
    if (match) {
      dst = &src;
      return true;
    } else {
      return false;
    }
  };

  NoteQueryTraversalStack stack;
  stack.size = 0;
  auto res = test_cursor(tree, begin, f, &accel->index_alloc, &accel->data_alloc, stack);
  if (!res.traversed) {
    assert(false);
    return nullptr;
  } else {
    return dst;
  }
}

int collect_notes_starting_in_region(NoteQueryAccelerator* accel, const NoteQueryTree* tree,
                                     const ScoreRegion& span, uint32_t* dst_indices,
                                     ClipNote* dst, int max_num_dst) {
  auto begin = span.begin;
  auto end = span.end(NoteQueryTree::modulus);
  auto f = [begin, end](const ClipNote& note) {
    return note.span.begin >= begin && note.span.begin < end;
  };

  return collect_notes(accel, tree, f, span, dst_indices, dst, max_num_dst);
}

int collect_notes_intersecting_region(NoteQueryAccelerator* accel, const NoteQueryTree* tree,
                                      const ScoreRegion& span, uint32_t* dst_indices,
                                      ClipNote* dst, int max_num_dst) {
  auto f = [span](const ClipNote& note) {
    return note.span.intersects(span, NoteQueryTree::modulus);
  };

  return collect_notes(accel, tree, f, span, dst_indices, dst, max_num_dst);
}

int collect_notes_intersecting_note(NoteQueryAccelerator* accel, const NoteQueryTree* tree,
                                    const ScoreRegion& span, MIDINote note,
                                    uint32_t* dst_indices, ClipNote* dst, int max_num_dst) {
  auto f = [span, note](const ClipNote& src) {
    return src.note.matches_pitch_class_and_octave(note) &&
           src.span.intersects(span, NoteQueryTree::modulus);
  };

  return collect_notes(accel, tree, f, span, dst_indices, dst, max_num_dst);
}

ArrayView<const ClipNote>
collect_notes_intersecting_note(NoteQueryAccelerator* accel, const NoteQueryTree* tree,
                                const ScoreRegion& region, MIDINote note,
                                TemporaryView<uint32_t>& dst_indices,
                                TemporaryView<ClipNote>& dst_notes) {
  const int stack_size = dst_notes.stack_size;
  assert(stack_size == dst_indices.stack_size);

  auto* notes = dst_notes.require(stack_size);
  auto* inds = dst_indices.require(stack_size);

  int num_required = collect_notes_intersecting_note(
    accel, tree, region, note, inds, notes, stack_size);

  if (num_required > stack_size) {
    notes = dst_notes.require(num_required);
    inds = dst_indices.require(num_required);
    collect_notes_intersecting_note(accel, tree, region, note, inds, notes, stack_size);
  }

  return {notes, notes + num_required};
}

GROVE_NAMESPACE_END
