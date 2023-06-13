#include "text.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

struct MarkTextResult {
  MarkTextResult() : success(false) {
    //
  }

  MarkTextResult(std::vector<std::string_view>&& lines, int num_spaces_to_start) :
    lines(std::move(lines)),
    num_spaces_to_start(num_spaces_to_start) {
    //
  }

  std::vector<std::string_view> lines;
  int num_spaces_to_start{-1};
  bool success{true};
};

template <typename T>
std::vector<T> split(const char* str, int64_t len, char delim) {
  std::vector<T> result;

  int64_t offset = 0;
  int64_t index = 0;

  for (int64_t i = 0; i < len; i++) {
    const auto c = str[i];

    if (c == delim) {
      T slice(str + offset, index - offset);
      result.emplace_back(slice);
      offset = index + 1;
    }

    index++;
  }

  result.emplace_back(T(str + offset, len - offset));
  return result;
}

std::vector<int64_t> find_characters(const char* str, int64_t len, char look_for) {
  std::vector<int64_t> result;
  for (int64_t i = 0; i < len; i++) {
    auto c = str[i];

    if (c == look_for) {
      result.push_back(i);
    }
  }

  return result;
}

MarkTextResult mark_text(std::string_view text, int64_t start,
                         int64_t stop, int64_t context_amount) {
  int64_t begin_ind = std::max(int64_t(0), start - context_amount);
  int64_t stop_ind = std::min(int64_t(text.size()), stop + context_amount);
  int64_t len = stop_ind - begin_ind;

  int64_t subset_start_ind = start - begin_ind;
  std::string_view subset_text(text.data() + begin_ind, len);

  auto lines = split<std::string_view>(subset_text.data(), subset_text.size(), '\n');
  auto new_line_inds = find_characters(subset_text.data(), subset_text.size(), '\n');

  auto cumulative_inds = new_line_inds;
  cumulative_inds.insert(cumulative_inds.begin(), -1);
  new_line_inds.insert(new_line_inds.begin(), std::numeric_limits<int64_t>::min());

  uint64_t interval_ptr = 0;
  while (interval_ptr < new_line_inds.size() && subset_start_ind >= new_line_inds[interval_ptr]) {
    interval_ptr++;
  }

  if (interval_ptr == 0) {
    return MarkTextResult();
  }

  auto interval_ind = interval_ptr - 1;
  auto num_spaces = subset_start_ind - cumulative_inds[interval_ind] - 1;

  lines.erase(lines.begin() + interval_ind + 1, lines.end());

  return MarkTextResult(std::move(lines), int(num_spaces));
}

std::string spaces(int num) {
  std::string spaces;
  for (int i = 0; i < num; i++) {
    spaces += " ";
  }
  return spaces;
}

template <typename T>
std::string join(const std::vector<T>& strs, const std::string& by, int64_t n) {
  std::string result;

  for (int64_t i = 0; i < n; i++) {
    result += strs[i];

    if (i < n-1) {
      result += by;
    }
  }

  return result;
}

} //  anon

std::string io::mark_text_with_message_and_context(std::string_view text,
                                                   int64_t start,
                                                   int64_t stop,
                                                   int64_t context_amount,
                                                   const std::string& message) {
  //
  auto mark_text_res = mark_text(text, start, stop, context_amount);
  if (!mark_text_res.success) {
    return "";
  }

  auto& lines = mark_text_res.lines;
  auto msg = spaces(mark_text_res.num_spaces_to_start);

  msg += "^\n";
  msg += message;

  lines.push_back(msg);
  return join(lines, "\n", lines.size());
}

GROVE_NAMESPACE_END
