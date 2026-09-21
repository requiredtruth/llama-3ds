#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace llama3ds {
class Sha256 {
public:
    Sha256();
    void update(const void* data, std::size_t len);
    std::string final_hex();
private:
    void transform(const std::uint8_t block[64]);
    std::uint32_t state_[8];
    std::uint64_t bitlen_;
    std::uint8_t block_[64];
    std::size_t block_len_;
};
bool sha256_file(const std::string& path, std::string& hex, std::string& error);
}
