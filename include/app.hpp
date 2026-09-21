#pragma once
#include <cstdint>
namespace llama3ds {
constexpr const char* kRoot="sdmc:/3ds/llama-3ds";
constexpr const char* kModels="sdmc:/3ds/llama-3ds/models";
void ensure_directories();
std::uint32_t app_memory_total();
std::uint32_t app_memory_free();
}
