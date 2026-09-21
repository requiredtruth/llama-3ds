#include "downloader.hpp"
#include "sha256.hpp"
#include <3ds.h>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace llama3ds {
namespace {
std::uint64_t size_of(const std::string& p){ struct stat s{}; return stat(p.c_str(),&s)==0?static_cast<std::uint64_t>(s.st_size):0; }
bool verify(const std::string& p,const std::string& expected,std::string& why){ std::string got; if(!sha256_file(p,got,why)) return false; if(got!=expected){ why="SHA-256 mismatch"; return false; } return true; }
void progress(std::uint64_t done,std::uint64_t total){
    std::printf("\x1b[18;0H");
    if(total) std::printf("%llu / %llu bytes (%llu%%)   ",(unsigned long long)done,(unsigned long long)total,(unsigned long long)(done*100/total));
    else std::printf("%llu bytes   ",(unsigned long long)done);
    std::printf("\nB: pause download (resume later)\n");
    gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
}
}

DownloadResult download_verified(const std::string& url,const std::string& final_path,const std::string& expected){
    DownloadResult out; std::string why;
    if(size_of(final_path) && verify(final_path,expected,why)){ out.ok=true; out.message="already downloaded and verified"; return out; }
    if(size_of(final_path)) std::remove(final_path.c_str());

    const std::string part=final_path+".part";
    std::uint64_t resume=size_of(part);
    std::string current=url;
    httpcContext ctx{};
    bool opened=false;
    u32 status=0;

    for(int redirects=0;redirects<8;redirects++){
        Result rc=httpcOpenContext(&ctx,HTTPC_METHOD_GET,current.c_str(),1);
        if(R_FAILED(rc)){ out.message="HTTP open failed"; return out; }
        opened=true;
        httpcSetSSLOpt(&ctx,SSLCOPT_DisableVerify);
        httpcAddRequestHeaderField(&ctx,"User-Agent","llama-3ds/0.2.2");
        httpcAddRequestHeaderField(&ctx,"Accept","application/octet-stream");
        httpcAddRequestHeaderField(&ctx,"Connection","close");
        if(resume){ char range[64]; std::snprintf(range,sizeof(range),"bytes=%llu-",(unsigned long long)resume); httpcAddRequestHeaderField(&ctx,"Range",range); }
        rc=httpcBeginRequest(&ctx);
        if(R_FAILED(rc)){ httpcCloseContext(&ctx); out.message="HTTP begin failed"; return out; }
        rc=httpcGetResponseStatusCode(&ctx,&status);
        if(R_FAILED(rc)){
            httpcCancelConnection(&ctx); httpcCloseContext(&ctx); opened=false;
            char detail[64]; std::snprintf(detail,sizeof(detail),"HTTP status failed: 0x%08lX",(unsigned long)rc);
            out.message=detail; return out;
        }
        if((status>=301&&status<=303)||status==307||status==308){
            char location[4096]{};
            rc=httpcGetResponseHeader(&ctx,"Location",location,sizeof(location));
            httpcCancelConnection(&ctx); httpcCloseContext(&ctx); opened=false;
            if(R_FAILED(rc)||!location[0]){ out.message="bad redirect"; return out; }
            current=location; continue;
        }
        break;
    }
    if(!opened){ out.message="too many redirects"; return out; }
    const bool append=resume>0 && status==206;
    if(status!=200 && status!=206){ httpcCancelConnection(&ctx); httpcCloseContext(&ctx); out.message="HTTP "+std::to_string(status); return out; }
    if(!append) resume=0;

    FILE* f=std::fopen(part.c_str(),append?"ab":"wb");
    if(!f){ httpcCancelConnection(&ctx); httpcCloseContext(&ctx); out.message="SD write open failed"; return out; }
    u32 content=0; httpcGetDownloadSizeState(&ctx,nullptr,&content);
    std::uint64_t done=resume,total=content?resume+content:0;
    std::vector<u8> buf(32768);
    Result rc=0;
    do {
        hidScanInput();
        if(hidKeysDown()&KEY_B){ httpcCancelConnection(&ctx); std::fclose(f); httpcCloseContext(&ctx); out.cancelled=true; out.message="paused; select again to resume"; return out; }
        u32 got=0; rc=httpcDownloadData(&ctx,buf.data(),static_cast<u32>(buf.size()),&got);
        if(got && std::fwrite(buf.data(),1,got,f)!=got){ httpcCancelConnection(&ctx); std::fclose(f); httpcCloseContext(&ctx); out.message="SD write failed; partial kept"; return out; }
        done+=got; progress(done,total);
    } while(rc==(Result)HTTPC_RESULTCODE_DOWNLOADPENDING);
    std::fclose(f); httpcCloseContext(&ctx);
    if(R_FAILED(rc)){ out.message="network read failed; partial kept"; return out; }

    std::printf("\nVerifying SHA-256...\n");
    if(!verify(part,expected,why)){ std::remove(part.c_str()); out.message=why+"; bad file deleted"; return out; }
    std::remove(final_path.c_str());
    if(std::rename(part.c_str(),final_path.c_str())!=0){ out.message="verified but rename failed"; return out; }
    out.ok=true; out.message="download complete and verified"; return out;
}
}
