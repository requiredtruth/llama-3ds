# llama-3ds

Experimental local-LLM homebrew for **New Nintendo 3DS / New Nintendo 2DS XL** running through the Homebrew Launcher under Luma CFW.

This repository starts from the original project scope: a native `.3dsx` application with a model menu, SD-card model storage, on-device download path, and local chat UI. The device target is ARM11 and devkitARM/libctru.

## Baseline snapshot

This first revision intentionally preserves the project shell before the runtime is completed. It already builds as a native 3DS homebrew application and establishes the UI/storage contract that the completed revision builds on.

### Controls

- `A` — open the selected action
- `B` — go back
- `D-Pad` — move selection
- `START` — exit

### SD layout

```text
sdmc:/3ds/llama-3ds/
  models/
  config/
  logs/
```

## Build

Install devkitPro with devkitARM and the 3DS libraries, then:

```bash
make -j
```

Output:

```text
llama-3ds.3dsx
```

The GitHub Actions build uses the official `devkitpro/devkitarm` environment.

## Status

Baseline import only. The next revision adds model download, selection, chat state, and the local inference adapter.

## License

MIT for the code in this repository. Third-party model/runtime components keep their own licenses.
