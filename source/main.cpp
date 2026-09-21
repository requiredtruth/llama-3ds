#include <3ds.h>
#include <cstdio>
#include <sys/stat.h>
#include "app.hpp"

namespace llama3ds {

static void mkdir_if_needed(const char* path) {
    mkdir(path, 0777);
}

void ensure_directories() {
    mkdir_if_needed("sdmc:/3ds");
    mkdir_if_needed(kRoot);
    mkdir_if_needed(kModels);
    mkdir_if_needed("sdmc:/3ds/llama-3ds/config");
    mkdir_if_needed("sdmc:/3ds/llama-3ds/logs");
}

void draw_home(int selected) {
    consoleClear();
    std::printf("llama-3ds\n");
    std::printf("New 3DS / New 2DS XL local LLM\n\n");
    const char* items[] = {"Models", "Chat", "System / Memory", "About"};
    for (int i = 0; i < 4; ++i) {
        std::printf("%c %s\n", selected == i ? '>' : ' ', items[i]);
    }
    std::printf("\nA select   B back   START exit\n");
    std::printf("\nBaseline snapshot: runtime comes next.\n");
}

} // namespace llama3ds

int main(int, char**) {
    gfxInitDefault();
    consoleInit(GFX_TOP, nullptr);
    osSetSpeedupEnable(true);
    llama3ds::ensure_directories();

    int selected = 0;
    llama3ds::draw_home(selected);

    while (aptMainLoop()) {
        hidScanInput();
        const u32 down = hidKeysDown();
        if (down & KEY_START) break;
        if (down & KEY_DUP) {
            selected = (selected + 3) % 4;
            llama3ds::draw_home(selected);
        }
        if (down & KEY_DDOWN) {
            selected = (selected + 1) % 4;
            llama3ds::draw_home(selected);
        }
        if (down & KEY_A) {
            consoleClear();
            std::printf("%s\n\n", selected == 0 ? "Models" : selected == 1 ? "Chat" : selected == 2 ? "System / Memory" : "About");
            std::printf("Baseline import placeholder.\n\nB to return.\n");
        }
        if (down & KEY_B) llama3ds::draw_home(selected);
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    osSetSpeedupEnable(false);
    gfxExit();
    return 0;
}
