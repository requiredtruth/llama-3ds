// devkitARM's static archive omits the CPU registry translation unit.
// Compile the pinned llama.cpp CPU registry implementation into the app.
#include "../.deps/llama.cpp/ggml/src/ggml-cpu/ggml-cpu.cpp"
