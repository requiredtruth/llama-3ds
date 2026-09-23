#include <3ds.h>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <vector>
#include "app.hpp"
#include "downloader.hpp"
#include "runtime.hpp"
#include "sha256.hpp"

namespace llama3ds {
struct ModelOption { const char* title; const char* filename; const char* url; const char* sha256; std::uint32_t context; const char* note; };
static const ModelOption catalog[]={
{"TinyStories 260K (1.2 MB)","stories260K.gguf","https://huggingface.co/ggml-org/tiny-llamas/resolve/main/stories260K.gguf?download=true","047bf46455a544931cff6fef14d7910154c56afbc23ab1c5e56a72e69912c04b",128,"Fastest native llama.cpp sanity model; not instruction tuned."},
{"TinyStories 15M Q4_0 (19 MB)","stories15M-q4_0.gguf","https://huggingface.co/ggml-org/tiny-llamas/resolve/main/stories15M-q4_0.gguf?download=true","6151b1929d7f5aa3385d9ddef3393e55587c0a55de661562322bc51dfda93a04",128,"Small local generation model; not a modern chat assistant."},
{"SmolLM2 135M Instruct Q4_0 (78 MB)","SmolLM2-135M-Instruct-Q4_0.gguf","https://huggingface.co/mukel/SmolLM2-135M-Instruct-GGUF/resolve/main/SmolLM2-135M-Instruct-Q4_0.gguf?download=true","bb0933fe5d39b971ec7550e6718138830a90b980d139d813f7137fd3982b3460",96,"Actual instruct model; extremely tight on New 3DS memory."}
};

void ensure_directories(){ mkdir("sdmc:/3ds",0777); mkdir(kRoot,0777); mkdir(kModels,0777); mkdir("sdmc:/3ds/llama-3ds/config",0777); mkdir("sdmc:/3ds/llama-3ds/logs",0777); }
std::uint32_t app_memory_total(){ return osGetMemRegionSize(MEMREGION_APPLICATION); }
std::uint32_t app_memory_free(){ return osGetMemRegionFree(MEMREGION_APPLICATION); }

static bool exists(const std::string& p){ struct stat s{}; return stat(p.c_str(),&s)==0 && s.st_size>0; }
static void frame(){ gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank(); }
static void wait_back(){ while(aptMainLoop()){ hidScanInput(); if(hidKeysDown()&(KEY_A|KEY_B|KEY_START)) return; frame(); } }
static void wrapped(const std::string& s,int width=48,int maxlines=5){ int col=0,lines=0; for(char c:s){ if(lines>=maxlines) break; if(c=='\n'||col>=width){ std::putchar('\n'); lines++; col=0; if(c=='\n') continue; if(lines>=maxlines) break; } std::putchar(c); col++; } std::putchar('\n'); }

static void home(int sel,const Runtime& rt,bool net){
    consoleClear(); std::printf("llama-3ds v0.2.4\nNew 3DS / New 2DS XL local LLM\n\n");
    const char* items[]={"Models / Download","Chat","System / Memory","About"};
    for(int i=0;i<4;i++) std::printf("%c %s\n",sel==i?'>':' ',items[i]);
    std::printf("\nRuntime: %s\nNetwork: %s\n\nD-Pad move  A select  START exit\n",rt.loaded()?"MODEL LOADED":"no model",net?"HTTP ready":"unavailable");
}
static void system_screen(){
    consoleClear(); auto total=app_memory_total(),free=app_memory_free();
    std::printf("System / Memory\n\nApplication: %.1f MiB\nFree now:    %.1f MiB\nUsed now:    %.1f MiB\n\n",(double)total/1048576.0,(double)free/1048576.0,(double)(total-free)/1048576.0);
    std::printf("CPU only / 2 threads\n96-128 token contexts\nNo GPU offload\n\nA/B return\n"); wait_back();
}
static void about(){
    consoleClear(); std::printf("About\n\nNative .3dsx local inference.\nPinned upstream llama.cpp core.\n");
    std::printf("Catalog files are SHA-256 pinned.\n\nNo real-device tok/s claim until measured.\nOld 3DS is not the target.\n\nA/B return\n"); wait_back();
}
static bool verify_file(const ModelOption& m,const std::string& p){
    consoleClear(); std::printf("Verifying %s...\n",m.filename); std::string got,err;
    if(!sha256_file(p,got,err)){ std::printf("\n%s\n",err.c_str()); wait_back(); return false; }
    if(got!=m.sha256){ std::printf("\nSHA-256 mismatch. Not loading.\n"); wait_back(); return false; }
    return true;
}
static void models(Runtime& rt,bool net){
    int sel=0; const int count=sizeof(catalog)/sizeof(catalog[0]); bool redraw=true;
    while(aptMainLoop()){
        if(redraw){
            consoleClear(); std::printf("Models / Download\nSD: /3ds/llama-3ds/models\n\n");
            for(int i=0;i<count;i++){ std::string p=std::string(kModels)+"/"+catalog[i].filename; std::printf("%c [%c] %s\n",sel==i?'>':' ',exists(p)?'x':' ',catalog[i].title); }
            std::printf("\n"); wrapped(catalog[sel].note,48,3); std::printf("\nA download/verify/load  Y LAN URL\nX delete  B back\n");
            redraw=false;
        }
        frame(); hidScanInput(); u32 d=hidKeysDown(); if(d&KEY_B) return;
        if(d&KEY_DUP){ sel=(sel+count-1)%count; redraw=true; }
        if(d&KEY_DDOWN){ sel=(sel+1)%count; redraw=true; }
        const auto& m=catalog[sel]; std::string path=std::string(kModels)+"/"+m.filename;
        if(d&KEY_X){ rt.unload(); std::remove(path.c_str()); std::remove((path+".part").c_str()); redraw=true; }
        if(d&KEY_Y){
            SwkbdState kb; char address[256]{};
            swkbdInit(&kb,SWKBD_TYPE_NORMAL,2,255);
            swkbdSetHintText(&kb,"http://192.168.1.10:8000/model.gguf");
            swkbdSetButton(&kb,SWKBD_BUTTON_LEFT,"Cancel",false);
            swkbdSetButton(&kb,SWKBD_BUTTON_RIGHT,"Download",true);
            if(swkbdInputText(&kb,address,sizeof(address))==SWKBD_BUTTON_RIGHT){
                std::string url(address);
                consoleClear();
                if(url.rfind("http://",0)!=0) std::printf("Use an http:// LAN address.\n");
                else if(!net) std::printf("HTTP service unavailable.\n");
                else {
                    std::remove((path+".part").c_str());
                    auto r=download_verified(url,path,m.sha256);
                    std::printf("\n%s\n",r.message.c_str());
                }
                wait_back();
            }
            redraw=true; continue;
        }
        if(d&KEY_A){
            if(!exists(path)){
                consoleClear(); std::printf("Download\n%s\n\n",m.title); frame();
                if(!net){ std::printf("HTTP service unavailable.\n"); wait_back(); redraw=true; continue; }
                auto r=download_verified(m.url,path,m.sha256); std::printf("\n%s\n",r.message.c_str()); if(!r.ok){ wait_back(); redraw=true; continue; }
            } else if(!verify_file(m,path)){ redraw=true; continue; }
            consoleClear(); std::printf("Loading with llama.cpp...\nFree before: %.1f MiB\n",(double)app_memory_free()/1048576.0); frame();
            std::string err; if(rt.load(path,m.context,err)) std::printf("\nLoaded. Free now: %.1f MiB\n",(double)app_memory_free()/1048576.0);
            else { std::printf("\nLOAD FAILED\n"); wrapped(err); }
            std::printf("\nA/B return\n"); wait_back(); redraw=true;
        }
    }
}
static void chat(Runtime& rt){
    std::vector<std::string> log; bool redraw=true;
    while(aptMainLoop()){
        if(redraw){
            consoleClear(); std::printf("Chat\nA type/send  X clear  B back\n\n");
            if(!rt.loaded()) std::printf("No model loaded. Open Models first.\n");
            else { std::size_t start=log.size()>4?log.size()-4:0; for(std::size_t i=start;i<log.size();i++){ wrapped(log[i],48,4); std::printf("\n"); } }
            redraw=false;
        }
        frame(); hidScanInput(); u32 d=hidKeysDown(); if(d&KEY_B) return; if(d&KEY_X){ log.clear(); redraw=true; } if(!(d&KEY_A)||!rt.loaded()) continue;
        SwkbdState kb; char input[192]{}; swkbdInit(&kb,SWKBD_TYPE_NORMAL,2,180); swkbdSetValidation(&kb,SWKBD_NOTEMPTY_NOTBLANK,0,0); swkbdSetHintText(&kb,"Message to local model"); swkbdSetButton(&kb,SWKBD_BUTTON_LEFT,"Cancel",false); swkbdSetButton(&kb,SWKBD_BUTTON_RIGHT,"Send",true);
        if(swkbdInputText(&kb,input,sizeof(input))!=SWKBD_BUTTON_RIGHT || !input[0]){ redraw=true; continue; }
        log.push_back(std::string("You: ")+input); consoleClear(); std::printf("Generating locally...\n"); frame();
        std::string answer,err; if(rt.generate(input,answer,err)) log.push_back("LLM: "+answer); else log.push_back("ERROR: "+err); redraw=true;
    }
}
}

int main(int,char**){
    gfxInitDefault(); consoleInit(GFX_TOP,nullptr); osSetSpeedupEnable(true); llama3ds::ensure_directories();
    Result h=httpcInit(0x4000); bool net=R_SUCCEEDED(h); llama3ds::Runtime runtime; int sel=0;
    llama3ds::home(sel,runtime,net);
    while(aptMainLoop()){
        llama3ds::frame(); hidScanInput(); u32 d=hidKeysDown();
        if(d&KEY_START) break;
        if(d&KEY_DUP){ sel=(sel+3)%4; llama3ds::home(sel,runtime,net); }
        if(d&KEY_DDOWN){ sel=(sel+1)%4; llama3ds::home(sel,runtime,net); }
        if(d&KEY_A){
            if(sel==0) llama3ds::models(runtime,net); else if(sel==1) llama3ds::chat(runtime); else if(sel==2) llama3ds::system_screen(); else llama3ds::about();
            llama3ds::home(sel,runtime,net);
        }
    }
    runtime.unload(); if(net) httpcExit(); osSetSpeedupEnable(false); gfxExit(); return 0;
}
