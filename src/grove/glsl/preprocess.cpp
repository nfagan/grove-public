#include "preprocess.hpp"
#include "scan/scan.hpp"
#include "parse/parse.hpp"
#include "grove/common/common.hpp"
#include "grove/common/fs.hpp"
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace {

glsl::IncludeProcessError make_error(glsl::IncludeProcessError::Type type,
                                     const std::string& message) {
  return {type, message};
}

bool maybe_read_file(const std::string& file_path, std::string& swap_with) {
  if (fs::file_exists(file_path)) {
    bool read_file_success;
    auto file_contents =
      grove::read_text_file(file_path.c_str(), &read_file_success);

    if (read_file_success) {
      std::swap(file_contents, swap_with);
      return true;
    }
  }

  return false;
}

std::string require_terminal_file_separator(const std::string& a) {
  if (a.empty()) {
    return std::string{};

  } else if (a.back() != fs::file_separator) {
    auto b = a;
    b.push_back(fs::file_separator);
    return b;

  } else {
    return a;
  }
}

std::string join_path(const std::string& d, const std::string& f) {
  return require_terminal_file_separator(d) + f;
}

} //  anon

/*
 * IncludeProcessResult
 */

bool glsl::IncludeProcessResult::success() const {
  return errors.empty();
}

void glsl::IncludeProcessResult::reset() {
  errors.clear();
  indices_to_erase.clear();
  resolved_includes.clear();
  resolved_files.clear();
}

/*
 * IncludeProcessorInstance
 */

glsl::IncludeProcessInstance::IncludeProcessInstance(std::string invoking_directory) :
  invoking_directory(std::move(invoking_directory)) {
  //
}

/*
 * IncludeProcessor
 */

glsl::IncludeProcessor::IncludeProcessor(IncludeProcessInstance* instance) :
  instance(instance) {
  //
}

void glsl::IncludeProcessor::include_directive(const CompilerDirective& directive) const {
  using Err = IncludeProcessError::Type;
  const auto& include_path = std::string(directive.source_token.lexeme);
  assert(!include_path.empty());

  bool found_file = false;
  std::string src_file_contents;
  std::string src_file_path;

  if (include_path[0] == fs::file_separator) {
    //  Absolute path.
    if (maybe_read_file(include_path, src_file_contents)) {
      found_file = true;
      src_file_path = include_path;
    }
  } else {
    //  Relative path.
    auto invoking_dir_path = join_path(instance->invoking_directory, include_path);
    if (maybe_read_file(invoking_dir_path, src_file_contents)) {
      found_file = true;
      src_file_path = invoking_dir_path;
    } else {
      for (const auto& search_dir : instance->search_directories) {
        const auto search_file = join_path(search_dir, include_path);
        if (maybe_read_file(search_file, src_file_contents)) {
          found_file = true;
          src_file_path = search_file;
          break;
        }
      }
    }
  }

  if (found_file) {
    instance->result.resolved_includes.push_back(std::move(src_file_contents));
    instance->result.resolved_files.push_back(std::move(src_file_path));
    instance->result.indices_to_erase.push_back({directive.begin, directive.end});

  } else {
    auto message = "No such file: " + include_path;
    instance->result.errors.push_back(make_error(Err::FileNotFound, message));
  }
}

void glsl::IncludeProcessor::compiler_directive(const CompilerDirective& directive) {
  switch (directive.type) {
    case CompilerDirective::Type::Include:
      include_directive(directive);
  }
}

/*
 *
 */

Optional<std::string> glsl::fill_in_includes(const std::string& source,
                                             IncludeProcessInstance& process_instance) {
  auto tok_res = glsl::scan(source.c_str(), int(source.size()));
  if (!tok_res.success()) {
    return NullOpt{};
  }

  auto parse_res = glsl::parse(tok_res.tokens);
  if (!parse_res.success()) {
    return NullOpt{};
  }

  IncludeProcessor processor{&process_instance};
  for (auto& node : parse_res.nodes) {
    node->accept_const(processor);
  }

  if (process_instance.result.success()) {
    return Optional<std::string>(fill_in_includes(source, process_instance.result));
  } else {
    return NullOpt{};
  }
}

std::string glsl::fill_in_includes(const std::string& source,
                                   const IncludeProcessResult& result) {
  assert(result.resolved_includes.size() == result.indices_to_erase.size());

  auto* begin = source.data();
  int off = 0;
  std::string filled_in_source;

  for (int i = 0; i < int(result.indices_to_erase.size()); i++) {
    const auto& range = result.indices_to_erase[i];
    const auto& fill_in = result.resolved_includes[i];

    const auto num_copy = range.begin - (begin + off);
    auto substr = source.substr(off, num_copy);

    filled_in_source += substr;
    filled_in_source += fill_in;

    off = int(range.end - begin);
  }

  if (off < int(source.size())) {
    filled_in_source += source.substr(off, source.size() - off);
  }

  return filled_in_source;
}

std::string glsl::set_preprocessor_definitions(const std::string& source,
                                               const std::vector<PreprocessorDefinition>& defines) {
  std::string define_str;

  for (auto& define : defines) {
    assert(!define.identifier.empty());
    define_str += "#define ";
    define_str += define.identifier;

    if (!define.value.empty()) {
      define_str += ' ';
      if (define.parenthesize_value) {
        define_str += '(';
      }
      define_str += define.value;
      if (define.parenthesize_value) {
        define_str += ')';
      }
    }

    define_str += '\n';
  }

  const auto first_newline = source.find('\n');
  if (first_newline != std::string::npos && first_newline + 1 < source.size()) {
    //  @Hack: Assume the first line of `source` is a version string "#version ..." and place all
    //  defines after this line.
    auto tmp = define_str;
    define_str = source;
    define_str.insert(first_newline + 1, tmp);

  } else {
    define_str += source;
  }

  return define_str;
}

GROVE_NAMESPACE_END
