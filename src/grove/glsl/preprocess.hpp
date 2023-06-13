#pragma once

#include "parse/ast.hpp"
#include "parse/visitor.hpp"
#include "grove/common/Optional.hpp"
#include <string>

namespace grove::glsl {

/*
 * SourceLocationRange
 */

struct SourceLocationRange {
  const char* begin;
  const char* end;
};

/*
 * IncludeProcessError
 */

struct IncludeProcessError {
  enum class Type {
    None = 0,
    FileNotFound
  };

  Type type;
  std::string message;
};

/*
 * IncludeProcessResult
 */

struct IncludeProcessResult {
  bool success() const;
  void reset();

  std::vector<IncludeProcessError> errors;
  std::vector<SourceLocationRange> indices_to_erase;
  std::vector<std::string> resolved_includes;
  std::vector<std::string> resolved_files;
};

/*
 * IncludeProcessInstance
 */

struct IncludeProcessInstance {
  explicit IncludeProcessInstance(std::string invoking_directory);

  std::string invoking_directory;
  std::vector<std::string> search_directories;
  IncludeProcessResult result;
};

/*
 * IncludeProcessor
 */

class IncludeProcessor : public AstVisitor {
public:
  explicit IncludeProcessor(IncludeProcessInstance* instance);
  void compiler_directive(const CompilerDirective& directive) override;

private:
  void include_directive(const CompilerDirective& directive) const;

private:
  IncludeProcessInstance* instance;
};

/*
 * Defines
 */

struct PreprocessorDefinition {
  std::string identifier;
  std::string value;
  bool parenthesize_value{};
};

using PreprocessorDefinitions = std::vector<PreprocessorDefinition>;

inline PreprocessorDefinition make_define(std::string ident) {
  PreprocessorDefinition result;
  result.identifier = std::move(ident);
  return result;
}

inline PreprocessorDefinition make_integer_define(std::string ident, int value) {
  PreprocessorDefinition result;
  result.identifier = std::move(ident);
  result.value = std::to_string(value);
  result.parenthesize_value = true;
  return result;
}

/*
 * Pipelines
 */

std::string fill_in_includes(const std::string& source, const IncludeProcessResult& result);
Optional<std::string> fill_in_includes(const std::string& source,
                                       IncludeProcessInstance& process_instance);

std::string set_preprocessor_definitions(const std::string& source,
                                         const std::vector<PreprocessorDefinition>& defines);

}