#include "command_line.hpp"
#include <iostream>

GROVE_NAMESPACE_BEGIN

namespace cmd {

namespace {
  bool matches(const char* a, const char* b) {
    return std::strcmp(a, b) == 0;
  }

  bool is_argument(const char* a) {
    return std::strlen(a) > 0 && a[0] == '-';
  }

  Optional<int> parse_int(const char* arg) {
    try {
      return Optional<int>(std::stoi(arg));
    } catch (...) {
      return NullOpt{};
    }
  }

  int parse_or_default(const char* arg, int dflt) {
    auto v = parse_int(arg);
    return v ? v.value() : dflt;
  }
}

/*
* ParameterName
*/

ParameterName::ParameterName() : num_alternates(0) {
  //
}
ParameterName::ParameterName(const char* full, const char* alias) :
  alternates{{full, alias}}, num_alternates(2) {
  //
}
ParameterName::ParameterName(const char* single) :
  alternates{{single}}, num_alternates(1) {
  //
}

bool ParameterName::matches(const char* arg) const {
  for (int i = 0; i < num_alternates; i++) {
    if (grove::cmd::matches(arg, alternates[i])) {
      return true;
    }
  }
  return false;
}

std::string ParameterName::to_string() const {
  std::string res;
  for (int i = 0; i < num_alternates; i++) {
    res += alternates[i];
    if (i < num_alternates-1) {
      res += ", ";
    }
  }
  return res;
}

/*
* Argument
*/

Argument::Argument(const ParameterName& param,
                   std::string description,
                   MatchCallback cb) :
  param(param),
  description(std::move(description)),
  match_callback(std::move(cb)) {
  //
}
Argument::Argument(const ParameterName& param,
                   const ParameterName& args,
                   std::string description,
                   MatchCallback cb) :
  param(param),
  arguments(Optional<ParameterName>(args)),
  description(std::move(description)),
  match_callback(std::move(cb)) {
  //
}

std::string Argument::to_string() const {
  std::string str = "  " + param.to_string();
  if (arguments) {
    str += " ";
    str += arguments.value().to_string();
  }

  str += ": ";
  str += "\n      " + description;

  return str;
}

namespace {
  MatchResult string_param(std::string* out, const char* arg) {
    *out = arg;
    return MatchResult{true, 2};
  }

  MatchResult true_param(bool* out) {
    *out = true;
    return MatchResult{true, 1};
  }
  MatchResult false_param(bool* out) {
    *out = false;
    return MatchResult{true, 1};
  }
}

/*
* Arguments
*/

Arguments::Arguments() : root_shader_directory{GROVE_PROJECT_SOURCE_DIR} {
  root_shader_directory += "/shaders";
  root_resource_directory = std::string{GROVE_PROJECT_SOURCE_DIR} + "/../../assets";
}

void Arguments::show_help() const {
  std::cout << std::endl;
  show_usage();
  std::cout << std::endl << "options: " << std::endl;

  for (const auto& arg : arguments) {
    std::cout << arg.to_string() << std::endl;
  }

  std::cout << std::endl;
}

void Arguments::show_usage() const {
  std::cout << "Usage: [options]" << std::endl;
}

void Arguments::build_parse_spec() {
  arguments.emplace_back(ParameterName("--help", "-h"), "Show this text.",
    [this](int, int, char**) {
    return true_param(&show_help_text);
  });
  arguments.emplace_back(ParameterName("--width", "-w"), "Window width.",
    [this](int i, int argc, char** argv) {
    if (i >= argc-1) {
      return MatchResult{false, 1};
    } else {
      window_width = parse_or_default(argv[i+1], window_width);
      return MatchResult{true, 2};
    }
  });
  arguments.emplace_back(ParameterName("--height", "-he"), "Window height.",
    [this](int i, int argc, char** argv) {
    if (i >= argc-1) {
      return MatchResult{false, 1};
    } else {
      window_height = parse_or_default(argv[i+1], window_height);
      return MatchResult{true, 2};
    }
  });
  arguments.emplace_back(ParameterName("--msaa", "-s"), "MSAA samples.",
    [this](int i, int argc, char** argv) {
    if (i >= argc-1) {
      return MatchResult{false, 1};
    } else {
      msaa_samples = parse_or_default(argv[i+1], msaa_samples);
      return MatchResult{true, 2};
    }
  });
  arguments.emplace_back(ParameterName("--trees", "-nt"), "Num initial trees.",
   [this](int i, int argc, char** argv) {
   if (i >= argc-1) {
     return MatchResult{false, 1};
   } else {
     num_trees = parse_or_default(argv[i+1], num_trees);
     return MatchResult{true, 2};
   }
  });
  arguments.emplace_back(ParameterName("--high-dpi", "-hdpi"), "Prefer high-DPI framebuffer.",
    [this](int, int, char**) {
    return true_param(&prefer_high_dpi_framebuffer);
  });
  arguments.emplace_back(ParameterName("--fullscreen", "-f"), "Full-screen mode.",
    [this](int, int, char**) {
    return true_param(&full_screen);
  });
  arguments.emplace_back(ParameterName("--no-vsync", "-nv"), "Disable V-sync.",
    [this](int, int, char**) {
    return false_param(&enable_vsync);
  });
  arguments.emplace_back(ParameterName("--no-stream", "-ns"), "Don't attempt to initialize an audio stream.",
    [this](int, int, char**) {
    return false_param(&initialize_default_audio_stream);
  });
  arguments.emplace_back(ParameterName("--res-dir", "-rd"), "Set resource directory.",
    [this](int i, int argc, char** argv) {
    if (i >= argc-1) {
      return MatchResult{false, 1};
    } else {
      return string_param(&root_resource_directory, argv[i+1]);
    }
  });
  arguments.emplace_back(ParameterName("--shader-dir", "-sd"), "Set shader directory.",
    [this](int i, int argc, char** argv) {
    if (i >= argc-1) {
      return MatchResult{false, 1};
    } else {
      return string_param(&root_shader_directory, argv[i+1]);
    }
  });
}

bool Arguments::parse(int argc, char** argv) {
  build_parse_spec();

  int i = 1;  //  skip executable.

  while (i < argc) {
    int incr = 1;
    const char* arg = argv[i];
    const bool is_arg = is_argument(arg);
    bool any_matched = false;
    bool parse_success = true;

    for (const auto& to_match : arguments) {
      if (to_match.param.matches(arg)) {
        auto res = to_match.match_callback(i, argc, argv);
        incr = res.increment;
        any_matched = true;
        had_parse_error = had_parse_error || !res.success;
        parse_success = res.success;
        break;
      }
    }

    if (!any_matched && is_arg) {
      std::cout << "Unrecognized or invalid argument: " << arg << ".";
      std::cout << " Try --help." << std::endl;
      had_parse_error = true;

    } else if (!parse_success) {
      std::cout << "Invalid value for argument: " << arg << ".";
      std::cout << " Try --help." << std::endl;
    }

    i += incr;
  }

  return evaluate();
}

bool Arguments::evaluate() const {
  if (had_parse_error) {
    return false;

  } else if (show_help_text) {
    show_help();
    return false;
  }

  return true;
}

}

GROVE_NAMESPACE_END
