#include "env.hpp"
#include <string>
#include <atomic>
#include <cassert>

namespace grove {

namespace {

std::atomic<bool> did_init{false};
std::string store_asset_dir;

} //  anon

void env::init_env(const char* asset_dir) {
  bool expect_init{};
  if (!did_init.compare_exchange_strong(expect_init, true)) {
    assert(false && "This function should only be called once at startup.");
  }
  store_asset_dir = asset_dir;
}

const char* env::get_asset_directory() {
  assert(did_init.load() && "Environment not yet initialized.");
  return store_asset_dir.c_str();
}

void env::terminate_env() {
  did_init.store(false);
  store_asset_dir = {};
}

}