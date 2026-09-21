# llama-3ds

Native local-LLM homebrew for **New Nintendo 3DS / New Nintendo 2DS XL** running under Luma CFW through the Homebrew Launcher.

## v0.2 target

- Native `.3dsx` application.
- Pinned upstream `llama.cpp` core cross-compiled for ARM11.
- Static CPU backend; no desktop dynamic-loader dependency.
- SD model bank: `sdmc:/3ds/llama-3ds/models/`.
- On-device model catalog/downloads with resume support.
- SHA-256 verification before a catalog model is accepted.
- Software-keyboard chat UI with local token generation.
- CPU-only, two inference threads, intentionally short 96–128 token contexts.
- Live application-memory display.
- Every release is compiled in GitHub Actions and includes the `.3dsx`, SMDH and SHA-256 checksums.

## Built-in catalog

| Model | Size | Use |
|---|---:|---|
| TinyStories 260K | 1.19 MB | Smallest native llama.cpp sanity model |
| TinyStories 15M Q4_0 | 19.1 MB | Small local generation test |
| SmolLM2-135M-Instruct Q4_0 | 77.6 MB | Actual instruction/chat model; memory is extremely tight |

The repository does **not** redistribute model weights. The app downloads the catalog files from their upstream Hugging Face repositories and pins the published SHA-256 digests.

The SmolLM2 option is deliberately marked experimental on hardware: the model file alone consumes most of a New 3DS application's practical memory budget, and llama.cpp still needs executable, context/KV and scratch memory. The smaller TinyStories models make it possible to validate the native inference path independently.

No real-device tokens/second number is claimed until it has been measured on actual hardware.

## Install

1. Open the newest GitHub Release.
2. Download `llama-3ds.3dsx`.
3. Put it under `sd:/3ds/llama-3ds/`.
4. Launch it with the Homebrew Launcher.
5. Choose **Models / Download**, download/load a model, then open **Chat**.

## Controls

- D-Pad: move
- A: select / type / send
- B: back; while downloading, pause and keep the partial file
- X: delete selected model or clear chat
- START: exit

## Build

Requires devkitPro/devkitARM/libctru plus CMake and Git:

```bash
make -j2
```

The build pins llama.cpp commit `6f41ac59e0a49a00483a316a22ada6b04edd2950` and applies only the small Nintendo-3DS portability shim in `tools/build_llama_cpp_3ds.sh`.

## Releases

`main` is the release branch. CI compiles the binary first, verifies the produced `.3dsx` and SMDH, then publishes those artifacts and a SHA-256 checksum file using the version in `VERSION`.

See [SUPPORT.md](SUPPORT.md) for the project's voluntary support information.

## License

Repository code is MIT. llama.cpp and downloaded models retain their own licenses; model weights are not included here.
