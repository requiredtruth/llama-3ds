#!/usr/bin/env bash
set -euo pipefail

LLAMA_CPP_COMMIT="6f41ac59e0a49a00483a316a22ada6b04edd2950"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/.deps/llama.cpp"
BUILD="$ROOT/.deps/llama-build-3ds"

: "${DEVKITPRO:?DEVKITPRO must point to devkitPro}"
mkdir -p "$ROOT/.deps"

if [[ ! -d "$SRC/.git" ]]; then
  git clone --filter=blob:none https://github.com/ggml-org/llama.cpp.git "$SRC"
fi

git -C "$SRC" fetch --depth=1 origin "$LLAMA_CPP_COMMIT"
git -C "$SRC" checkout --detach "$LLAMA_CPP_COMMIT"
git -C "$SRC" reset --hard "$LLAMA_CPP_COMMIT"

python3 - "$SRC" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1])

def replace(path, old, new):
    p = root / path
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"3DS port anchor missing in {path}")
    p.write_text(text.replace(old, new, 1))

replace("ggml/src/ggml-backend-dl.h",
"""#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#   include <winevt.h>
#else
#    include <dlfcn.h>
#    include <unistd.h>
#endif""",
"""#if defined(__3DS__)
#   include <unistd.h>
#elif defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#   include <winevt.h>
#else
#    include <dlfcn.h>
#    include <unistd.h>
#endif""")

replace("ggml/src/ggml-backend-dl.h",
"""#ifdef _WIN32

using dl_handle = std::remove_pointer_t<HMODULE>;""",
"""#if defined(__3DS__)

using dl_handle = void;
struct dl_handle_deleter { void operator()(void *) {} };

#elif defined(_WIN32)

using dl_handle = std::remove_pointer_t<HMODULE>;""")

replace("ggml/src/ggml-backend-dl.cpp",
"""#ifdef _WIN32

dl_handle * dl_load_library""",
"""#if defined(__3DS__)

dl_handle * dl_load_library(const fs::path &) { return nullptr; }
void * dl_get_sym(dl_handle *, const char *) { return nullptr; }
const char * dl_error() { return "dynamic backend loading disabled on Nintendo 3DS"; }

#elif defined(_WIN32)

dl_handle * dl_load_library""")

replace("ggml/src/ggml-backend-reg.cpp",
"""#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN""",
"""#if defined(__3DS__)
#    include <unistd.h>
#elif defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN""")

# devkitARM defines uint32_t as unsigned long on this ABI, while this upstream
# model helper hard-codes unsigned int. Make the template type explicit.
replace("src/models/bailingmoe3.cpp",
"""std::max(1u, hparams.n_expert_shared)""",
"""std::max<decltype(hparams.n_expert_shared)>(1, hparams.n_expert_shared)""")

replace("src/llama-batch.cpp",
"""            int seq_id_max = 0;""",
"""            llama_seq_id seq_id_max = 0;""")

replace("src/llama-context.cpp",
"""    cparams.n_seq_max = std::max(1u, params.n_seq_max);""",
"""    cparams.n_seq_max = std::max<decltype(params.n_seq_max)>(1, params.n_seq_max);""")

replace("src/llama-context.cpp",
"""    data.n_p_eval    = std::max(1, n_p_eval);
    data.n_eval      = std::max(1, n_eval);
    data.n_reused    = std::max(0, n_reused);""",
"""    data.n_p_eval    = std::max<decltype(n_p_eval)>(1, n_p_eval);
    data.n_eval      = std::max<decltype(n_eval)>(1, n_eval);
    data.n_reused    = std::max<decltype(n_reused)>(0, n_reused);""")

replace("src/llama-grammar.cpp",
"""            n_prev_rules = std::max(1u, (uint32_t)symbol_ids.size() - n_rules_before);""",
"""            n_prev_rules = std::max<uint64_t>(1, static_cast<uint64_t>(symbol_ids.size()) - n_rules_before);""")

replace("src/llama-kv-cache.cpp",
"""    const uint32_t n_pad_cur = std::max(n_pad, 256u);""",
"""    const uint32_t n_pad_cur = std::max<uint32_t>(n_pad, 256);""")

replace("src/llama-kv-cache-msa.cpp",
"""    const uint32_t n_pad_cur = std::max(kv->get_n_pad(), 256u);""",
"""    const uint32_t n_pad_cur = std::max<uint32_t>(kv->get_n_pad(), 256);""")

# devkitARM's int32_t is long, so plain int is a distinct type. Upstream
# assumes they are identical in GGUF metadata and saver overload resolution.
replace("src/llama-model-loader.cpp",
"""    template<> struct GKV_Base<int32_t     >: GKV_Base_Type<int32_t,      GGUF_TYPE_INT32,   gguf_get_val_i32 > {};""",
"""    template<> struct GKV_Base<int32_t     >: GKV_Base_Type<int32_t,      GGUF_TYPE_INT32,   gguf_get_val_i32 > {};
#ifdef __3DS__
    template<> struct GKV_Base<int> {
        static constexpr gguf_type gt = GGUF_TYPE_INT32;
        static int getter(const gguf_context * ctx, const int kid) {
            return static_cast<int>(gguf_get_val_i32(ctx, kid));
        }
    };
#endif""")

replace("src/llama-model-saver.h",
"""    void add_kv(enum llm_kv key, int32_t      value);""",
"""    void add_kv(enum llm_kv key, int32_t      value);
#ifdef __3DS__
    void add_kv(enum llm_kv key, int          value);
#endif""")

replace("src/llama-model-saver.cpp",
"""void llama_model_saver::add_kv(const enum llm_kv key, const int32_t value) {
    gguf_set_val_i32(gguf_ctx, llm_kv(key).c_str(), value);
}""",
"""void llama_model_saver::add_kv(const enum llm_kv key, const int32_t value) {
    gguf_set_val_i32(gguf_ctx, llm_kv(key).c_str(), value);
}
#ifdef __3DS__
void llama_model_saver::add_kv(const enum llm_kv key, const int value) {
    gguf_set_val_i32(gguf_ctx, llm_kv(key).c_str(), static_cast<int32_t>(value));
}
#endif""")

replace("src/llama-model.cpp",
"""                            return -1;
                        };""",
"""                            return static_cast<decltype(hparams.n_layer_kv_from_start)>(-1);
                        };""")

replace("src/llama-sampler.cpp",
"""    k = std::min(k, (int) cur_p->size);""",
"""    k = std::min<int32_t>(k, static_cast<int32_t>(cur_p->size));""")

replace("src/llama-sampler.cpp",
"""    penalty_last_n = std::max(penalty_last_n, 0);""",
"""    penalty_last_n = std::max<int32_t>(penalty_last_n, 0);""")

replace("src/llama-sampler.cpp",
"""    int last_n_repeat = std::min((int) ctx->last_tokens.size(), ctx->dry_penalty_last_n);""",
"""    int last_n_repeat = std::min<int>(static_cast<int>(ctx->last_tokens.size()), static_cast<int>(ctx->dry_penalty_last_n));""")

replace("src/llama-sampler.cpp",
"""    dry_penalty_last_n = std::max(dry_penalty_last_n, 0);""",
"""    dry_penalty_last_n = std::max<int32_t>(dry_penalty_last_n, 0);""")

replace("src/llama-sampler.cpp",
"""    data.n_sample    = std::max(0, ctx->n_sample);""",
"""    data.n_sample    = std::max<int32_t>(0, ctx->n_sample);""")

# llama_token is int32_t (long on devkitARM), not plain int.
replace("src/llama-vocab.cpp",
"""        const std::vector<int> ids = tokenize("\\n", false);""",
"""        const std::vector<llama_token> ids = tokenize("\\n", false);""")
replace("src/llama-vocab.cpp",
"""        const std::vector<int> ids = tokenize("\\n", false);""",
"""        const std::vector<llama_token> ids = tokenize("\\n", false);""")

# Force the registry entrypoint to retain C linkage with devkitARM's headers.
replace("ggml/src/ggml-cpu/ggml-cpu.cpp",
"""ggml_backend_reg_t ggml_backend_cpu_reg(void) {""",
"""extern "C" ggml_backend_reg_t ggml_backend_cpu_reg(void) {""")
PY

rm -rf "$BUILD"
cmake -S "$SRC" -B "$BUILD"   -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/3DS.cmake"   -DCMAKE_BUILD_TYPE=MinSizeRel   -DBUILD_SHARED_LIBS=OFF   -DGGML_STATIC=ON   -DGGML_BACKEND_DL=OFF   -DGGML_NATIVE=OFF   -DGGML_OPENMP=OFF   -DGGML_LLAMAFILE=OFF   -DGGML_BLAS=OFF   -DGGML_ACCELERATE=OFF   -DGGML_CPU_REPACK=OFF   -DGGML_CPU_KLEIDIAI=OFF   -DGGML_CPU_ARM_ARCH=armv6k   -DLLAMA_BUILD_COMMON=OFF   -DLLAMA_BUILD_TESTS=OFF   -DLLAMA_BUILD_TOOLS=OFF   -DLLAMA_BUILD_EXAMPLES=OFF   -DLLAMA_BUILD_SERVER=OFF   -DLLAMA_BUILD_APP=OFF   -DLLAMA_TOOLS_INSTALL=OFF   -DLLAMA_TESTS_INSTALL=OFF   -DLLAMA_OPENSSL=OFF   -DLLAMA_SUBPROCESS=OFF

cmake --build "$BUILD" --target llama -j2
CPU_ARCHIVE="$(find "$BUILD" -name 'libggml-cpu.a' -print -quit)"
test -n "$CPU_ARCHIVE"
echo "CPU archive: $CPU_ARCHIVE"
arm-none-eabi-nm -g "$CPU_ARCHIVE" | grep 'ggml_backend_cpu_reg\|ggml_backend_init' || true
