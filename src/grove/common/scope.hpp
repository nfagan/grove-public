#pragma once

#include <utility>

namespace grove {

//  Via Andrei Alexandrescu, cpp con 2015.
namespace detail {
  enum class ScopeGuardOnExit {};
  template <typename Function>
  struct ScopeGuard {
    ScopeGuard(Function&& fun) : function(std::forward<Function>(fun)) {
      //
    }
    ~ScopeGuard() {
      function();
    }

    Function function;
  };

  template <typename Function>
  inline ScopeGuard<Function> operator+(ScopeGuardOnExit, Function&& fun) {
    return ScopeGuard<Function>(std::forward<Function>(fun));
  }
}

#define GROVE_CONCAT_IMPL(s1, s2) s1##s2
#define GROVE_CONCAT(s1, s2) GROVE_CONCAT_IMPL(s1, s2)
#ifdef __COUNTER__
#define GROVE_ANONYMOUS_VARIABLE(str) GROVE_CONCAT(str, __COUNTER__)
#else
#define GROVE_ANONYMOUS_VARIABLE(str) GROVE_CONCAT(str, __LINE__)
#endif

#define GROVE_SCOPE_EXIT \
  auto GROVE_ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) = detail::ScopeGuardOnExit() + [&]()

}