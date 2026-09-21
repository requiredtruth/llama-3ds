#pragma once
#include <string>
namespace llama3ds {
struct DownloadResult { bool ok=false; bool cancelled=false; std::string message; };
DownloadResult download_verified(const std::string& url,const std::string& final_path,const std::string& expected_sha256);
}
