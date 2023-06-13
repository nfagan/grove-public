#pragma once

#include "grove/common/Optional.hpp"
#include "grove/common/common.hpp"
#include <string>
#include <vector>
#include <functional>
#include <array>

GROVE_NAMESPACE_BEGIN

namespace cmd {

struct MatchResult {
  bool success;
  int increment;
};

using MatchCallback = std::function<MatchResult(int, int, char**)>;

struct ParameterName {
  ParameterName();
  ParameterName(const char* full, const char* alias);
  ParameterName(const char* single);

  bool matches(const char* arg) const;
  std::string to_string() const;

  std::array<const char*, 2> alternates;
  int num_alternates;
};

struct Argument {
  Argument(const ParameterName& param, std::string description, MatchCallback cb);
  Argument(const ParameterName& param, const ParameterName& args,
           std::string description, MatchCallback cb);

  std::string to_string() const;

  ParameterName param;
  Optional<ParameterName> arguments;
  std::string description;
  MatchCallback match_callback;
};

struct Arguments {
  Arguments();
  bool parse(int argc, char** argv);
  void show_help() const;
  void show_usage() const;

private:
  bool evaluate() const;
  void build_parse_spec();

private:
  std::vector<Argument> arguments;

public:
  bool had_parse_error{false};
  bool show_help_text{false};

  int window_width{1280};
  int window_height{720};
  bool full_screen{false};
  bool enable_vsync{true};
  int msaa_samples{4};
  int num_trees{-1};
  bool prefer_high_dpi_framebuffer{false};
  bool initialize_default_audio_stream{true};

//  std::string root_resource_directory{GROVE_PLAYGROUND_RES_DIR};
  std::string root_resource_directory;
  std::string root_shader_directory;
};

}

GROVE_NAMESPACE_END