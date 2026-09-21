#pragma once

namespace llama3ds {
constexpr const char* kRoot = "sdmc:/3ds/llama-3ds";
constexpr const char* kModels = "sdmc:/3ds/llama-3ds/models";
void ensure_directories();
void draw_home(int selected);
}
