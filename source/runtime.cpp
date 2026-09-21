#include "runtime.hpp"
#include "llama.h"
#include <algorithm>
#include <climits>
#include <string>
#include <vector>

namespace llama3ds {
namespace {
bool g_backend_ready=false;

std::string prompt_for(llama_model* model,const std::string& user){
    const char* tmpl=llama_model_chat_template(model,nullptr);
    if(tmpl){
        llama_chat_message msgs[2]={{"system","You are a concise helpful assistant running locally on a Nintendo 3DS."},{"user",user.c_str()}};
        int32_t n=llama_chat_apply_template(tmpl,msgs,2,true,nullptr,0);
        if(n>0 && n<8192){
            std::vector<char> buf(static_cast<std::size_t>(n)+1,0);
            int32_t w=llama_chat_apply_template(tmpl,msgs,2,true,buf.data(),static_cast<int32_t>(buf.size()));
            if(w>0) return std::string(buf.data(),static_cast<std::size_t>(w));
        }
    }
    return "User: "+user+"\nAssistant:";
}
}

Runtime::Runtime():model_(nullptr),ctx_(nullptr),sampler_(nullptr),context_tokens_(128){
    if(!g_backend_ready){ llama_backend_init(); g_backend_ready=true; }
}
Runtime::~Runtime(){ unload(); }
bool Runtime::loaded() const { return model_ && ctx_ && sampler_; }

void Runtime::unload(){
    if(sampler_){ llama_sampler_free(sampler_); sampler_=nullptr; }
    if(ctx_){ llama_free(ctx_); ctx_=nullptr; }
    if(model_){ llama_model_free(model_); model_=nullptr; }
}

bool Runtime::load(const std::string& path,std::uint32_t context_tokens,std::string& error){
    unload();
    llama_model_params mp=llama_model_default_params();
    mp.n_gpu_layers=0;
    mp.load_mode=LLAMA_LOAD_MODE_NONE;
    mp.lazy_mode=LLAMA_LAZY_MODE_OFF;
    mp.check_tensors=true;
    mp.use_extra_bufts=false;
    model_=llama_model_load_from_file(path.c_str(),mp);
    if(!model_){ error="GGUF load failed (format or memory)."; return false; }

    context_tokens_=std::max<std::uint32_t>(64,std::min<std::uint32_t>(context_tokens,160));
    llama_context_params cp=llama_context_default_params();
    cp.n_ctx=context_tokens_; cp.n_batch=32; cp.n_ubatch=16; cp.n_threads=2; cp.n_threads_batch=2;
    cp.flash_attn_type=LLAMA_FLASH_ATTN_TYPE_DISABLED; cp.no_perf=true;
    ctx_=llama_init_from_model(model_,cp);
    if(!ctx_){ error="context allocation failed; try a smaller model"; llama_model_free(model_); model_=nullptr; return false; }

    llama_sampler_chain_params sp=llama_sampler_chain_default_params(); sp.no_perf=true;
    sampler_=llama_sampler_chain_init(sp);
    llama_sampler_chain_add(sampler_,llama_sampler_init_top_k(20));
    llama_sampler_chain_add(sampler_,llama_sampler_init_top_p(0.90f,1));
    llama_sampler_chain_add(sampler_,llama_sampler_init_temp(0.70f));
    llama_sampler_chain_add(sampler_,llama_sampler_init_dist(0x3d5u));
    return true;
}

bool Runtime::generate(const std::string& user,std::string& output,std::string& error){
    if(!loaded()){ error="load a model first"; return false; }
    llama_memory_clear(llama_get_memory(ctx_),true);
    llama_sampler_reset(sampler_);
    const std::string prompt=prompt_for(model_,user);
    const llama_vocab* vocab=llama_model_get_vocab(model_);
    int32_t n=llama_tokenize(vocab,prompt.c_str(),static_cast<int32_t>(prompt.size()),nullptr,0,true,true);
    if(n==INT32_MIN){ error="tokenizer overflow"; return false; }
    n=n<0?-n:n;
    constexpr int32_t max_new=40;
    if(n<=0 || n+max_new>=static_cast<int32_t>(context_tokens_)){ error="message too long for 3DS short context"; return false; }
    std::vector<llama_token> toks(static_cast<std::size_t>(n));
    int32_t got=llama_tokenize(vocab,prompt.c_str(),static_cast<int32_t>(prompt.size()),toks.data(),n,true,true);
    if(got<0){ error="tokenization failed"; return false; }
    llama_batch batch=llama_batch_get_one(toks.data(),got);
    if(llama_decode(ctx_,batch)!=0){ error="prompt decode failed"; return false; }

    output.clear();
    for(int i=0;i<max_new;i++){
        llama_token tok=llama_sampler_sample(sampler_,ctx_,-1);
        if(tok==LLAMA_TOKEN_NULL || llama_vocab_is_eog(vocab,tok)) break;
        char buf[256];
        int32_t piece=llama_token_to_piece(vocab,tok,buf,sizeof(buf),0,true);
        if(piece>0) output.append(buf,static_cast<std::size_t>(piece));
        batch=llama_batch_get_one(&tok,1);
        if(llama_decode(ctx_,batch)!=0){ error="generation decode failed"; return false; }
    }
    if(output.empty()) output="(model ended without printable text)";
    return true;
}
}
