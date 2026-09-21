#pragma once
#include <cstdint>
#include <string>
struct llama_model;
struct llama_context;
struct llama_sampler;

namespace llama3ds {
class Runtime {
public:
    Runtime();
    ~Runtime();
    bool load(const std::string& path, std::uint32_t context_tokens, std::string& error);
    void unload();
    bool loaded() const;
    bool generate(const std::string& user_text, std::string& output, std::string& error);
private:
    llama_model* model_;
    llama_context* ctx_;
    llama_sampler* sampler_;
    std::uint32_t context_tokens_;
};
}
