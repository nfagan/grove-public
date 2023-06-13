#pragma once

#include <vector>
#include <string_view>
#include <string>

namespace grove::io {

std::string mark_text_with_message_and_context(std::string_view text,
                                               int64_t start,
                                               int64_t stop,
                                               int64_t context_amount,
                                               const std::string& message);

}