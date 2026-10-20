#ifndef BYB_SCORE_OUTBOX_H
#define BYB_SCORE_OUTBOX_H
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <stdint.h>
#include "PlayerName.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace BackyardBrains { namespace Rankings {
#ifdef _WIN32
inline std::wstring wide(const std::string &s) {
    const int count=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,NULL,0);
    if(count<=0) return std::wstring();
    std::wstring result(count,L'\0'); MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,&result[0],count);
    result.resize(count-1); return result;
}
inline std::string utf8(const std::wstring &s) {
    const int count=WideCharToMultiByte(CP_UTF8,0,s.c_str(),-1,NULL,0,NULL,NULL);
    if(count<=0) return std::string();
    std::string result(count,'\0'); WideCharToMultiByte(CP_UTF8,0,s.c_str(),-1,&result[0],count,NULL,NULL);
    result.resize(count-1); return result;
}
#endif
inline std::string outboxDirectory() {
#ifdef _WIN32
    wchar_t value[32768];
    DWORD count=GetEnvironmentVariableW(L"LOTTOL_SCORE_OUTBOX",value,32768);
    if(count>0 && count<32768) return utf8(value);
    count=GetModuleFileNameW(NULL,value,32768);
    if(count==0 || count>=32768) return std::string();
    const std::wstring module(value,count);
    return utf8(module.substr(0,module.find_last_of(L"\\/"))+L"\\..\\..\\..\\server\\outbox");
#else
    const char *configured=std::getenv("LOTTOL_SCORE_OUTBOX");
    return configured?configured:"../../../server/outbox";
#endif
}
inline bool validGame(const std::string &id) {
    if(id.empty() || id.size()>64) return false;
    for(char c:id) if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_')) return false;
    return true;
}
// A completed round writes one tiny, atomic event. No network call or server
// timeout ever runs on the recording/UI thread. The server imports the durable
// inbox with an idempotent SQLite transaction, including after an offline run.
inline bool queueTo(const std::string &directory,const std::string &game,int64_t score,bool practice=false,
                    const std::string &playerName=std::string()) {
    if(practice) return true;
    if(directory.empty() || !validGame(game) || score<0 || score>1000000000LL) return false;
    std::string name;
    if(!playerName.empty() && !normalizePlayerName(playerName,name)) return false;
#ifdef _WIN32
    _wmkdir(wide(directory).c_str());
    const unsigned long pid=GetCurrentProcessId();
#else
    mkdir(directory.c_str(),0700); const unsigned long pid=getpid();
#endif
    static std::atomic<unsigned long> sequence(0);
    const auto now=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream id; id<<std::hex<<now<<'-'<<pid<<'-'<<++sequence;
    const std::string path=directory+"/"+id.str()+".json",temporary=path+".tmp";
    std::ostringstream json;
    json<<"{\"schema\":1,\"eventId\":\""<<id.str()<<"\",\"gameId\":\""<<game<<"\",\"score\":"<<score<<",\"recordedAtMs\":"<<now/1000;
    if(!name.empty()) json<<",\"playerName\":\""<<jsonPlayerName(name)<<'"';
    json<<",\"practice\":false}\n";
    const std::string data=json.str();
#ifdef _WIN32
    FILE *file=_wfopen(wide(temporary).c_str(),L"wb");
#else
    FILE *file=std::fopen(temporary.c_str(),"wb");
#endif
    if(!file) return false;
    const bool written=std::fwrite(data.data(),1,data.size(),file)==data.size();
    const bool closed=std::fclose(file)==0;
#ifdef _WIN32
    const bool renamed=written && closed && _wrename(wide(temporary).c_str(),wide(path).c_str())==0;
    if(!renamed) _wremove(wide(temporary).c_str());
#else
    const bool renamed=written && closed && std::rename(temporary.c_str(),path.c_str())==0;
    if(!renamed) std::remove(temporary.c_str());
#endif
    return renamed;
}
inline bool queue(const std::string &game,int64_t score,bool practice=false,
                  const std::string &playerName=std::string()) {
    return queueTo(outboxDirectory(),game,score,practice,playerName);
}
// Keep the native prompt's last confirmed name independently of the web
// profile. Each result carries its session name even when imported offline.
inline std::string loadPlayerName(const std::string &path) {
#ifdef _WIN32
    FILE *file=_wfopen(wide(path).c_str(),L"rb");
#else
    FILE *file=std::fopen(path.c_str(),"rb");
#endif
    if(!file) return std::string();
    char bytes[257]; const size_t length=std::fread(bytes,1,sizeof(bytes),file);
    std::fclose(file);std::string name;
    return length<=256 && normalizePlayerName(std::string(bytes,length),name)?name:std::string();
}
inline bool savePlayerName(const std::string &path,const std::string &value) {
    std::string name;if(!normalizePlayerName(value,name)) return false;
#ifdef _WIN32
    FILE *file=_wfopen(wide(path).c_str(),L"wb");
#else
    FILE *file=std::fopen(path.c_str(),"wb");
#endif
    if(!file) return false;
    const bool written=std::fwrite(name.data(),1,name.size(),file)==name.size();
    return std::fclose(file)==0 && written;
}
inline std::string playerNamePath() {
    const std::string directory=outboxDirectory();
    const size_t slash=directory.find_last_of("/\\");
    return (slash==std::string::npos?std::string("."):directory.substr(0,slash))+"/player-name.txt";
}
} }
#endif
