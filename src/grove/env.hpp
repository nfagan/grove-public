#pragma once

namespace grove::env {

void init_env(const char* asset_dir);
const char* get_asset_directory();
void terminate_env();

}

#define GROVE_ASSET_DIR grove::env::get_asset_directory()