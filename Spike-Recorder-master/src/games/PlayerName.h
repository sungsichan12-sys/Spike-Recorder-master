#ifndef BYB_PLAYER_NAME_H
#define BYB_PLAYER_NAME_H
#include <string>
#include <vector>
#include <stdint.h>

namespace BackyardBrains { namespace Rankings {
// Reject malformed UTF-8 instead of splitting a Korean character when editing.
inline bool nameCharacters(const std::string &text, std::vector<size_t> &offsets,
                           std::vector<uint32_t> *values=NULL) {
    offsets.clear();
    if(values) values->clear();
    for(size_t i=0;i<text.size();) {
        offsets.push_back(i);
        const unsigned char first=static_cast<unsigned char>(text[i++]);
        uint32_t value=first; int extra=0;
        if(first>=0xc2 && first<=0xdf) {value=first&31;extra=1;}
        else if(first>=0xe0 && first<=0xef) {value=first&15;extra=2;}
        else if(first>=0xf0 && first<=0xf4) {value=first&7;extra=3;}
        else if(first>=128) return false;
        if(i+extra>text.size()) return false;
        for(int j=0;j<extra;++j) {
            const unsigned char next=static_cast<unsigned char>(text[i++]);
            if((next&0xc0)!=0x80) return false;
            value=(value<<6)|(next&63);
        }
        if((extra==1 && value<128)||(extra==2 && value<2048)||
           (extra==3 && value<65536)||value>0x10ffff||
           (value>=0xd800 && value<=0xdfff)) return false;
        if(values) values->push_back(value);
    }
    offsets.push_back(text.size()); return true;
}
inline bool nameSpace(uint32_t c) {
    return c==32 || (c>=9 && c<=13) || c==0x85 || c==0xa0 || c==0x1680 ||
        (c>=0x2000 && c<=0x200a) || c==0x2028 || c==0x2029 || c==0x202f || c==0x205f || c==0x3000;
}
inline bool normalizePlayerName(const std::string &input,std::string &name) {
    std::vector<size_t> offsets; std::vector<uint32_t> values;
    if(!nameCharacters(input,offsets,&values)) return false;
    size_t begin=0,end=values.size();
    while(begin<end && nameSpace(values[begin])) ++begin;
    while(end>begin && nameSpace(values[end-1])) --end;
    if(end==begin || end-begin>32) return false;
    for(size_t i=begin;i<end;++i)
        if(values[i]<32 || (values[i]>=0x7f && values[i]<=0x9f)) return false;
    name=input.substr(offsets[begin],offsets[end]-offsets[begin]); return true;
}
inline std::string jsonPlayerName(const std::string &name) {
    std::string result;
    for(char c:name) { if(c=='"'||c=='\\') result+='\\'; result+=c; }
    return result;
}

// The confirmed session name survives editing/cancelling the next prompt.
// Backspace, delete and cursor movement use code point boundaries, not bytes.
struct PlayerNameEntry {
    bool open=false,selected=false,enterReady=true;
    std::string text,composition,sessionName,error;
    size_t cursor=0;
    void begin(const std::string &previous,bool keyboard=false) {
        text=previous;cursor=text.size();selected=!text.empty();
        composition.clear();error.clear();open=true;enterReady=!keyboard;
    }
    void cancel() {open=false;composition.clear();error.clear();}
    bool confirm() {
        if(!open || !enterReady || !composition.empty()) return false;
        std::string name;
        if(!normalizePlayerName(text,name)) {error="이름을 1~32자로 입력해 주세요.";return false;}
        sessionName=name;text=name;cursor=text.size();open=false;return true;
    }
    bool insert(const std::string &input) {
        composition.clear();
        std::string candidate=selected?input:text.substr(0,cursor)+input+text.substr(cursor);
        std::vector<size_t> offsets;std::vector<uint32_t> values;
        if(!nameCharacters(candidate,offsets,&values) || values.size()>32) {
            error="이름은 최대 32자까지 입력할 수 있습니다."; return false;
        }
        for(uint32_t c:values) if(c<32 || (c>=0x7f && c<=0x9f)) {
            error="줄바꿈이나 제어 문자는 사용할 수 없습니다.";return false;
        }
        cursor=selected?input.size():cursor+input.size();text=candidate;
        selected=false;composition.clear();error.clear();return true;
    }
    void move(int direction) {
        if(selected) {cursor=direction<0?0:text.size();selected=false;return;}
        std::vector<size_t> offsets;nameCharacters(text,offsets);
        for(size_t i=0;i<offsets.size();++i) if(offsets[i]==cursor) {
            if(direction<0 && i) cursor=offsets[i-1];
            if(direction>0 && i+1<offsets.size()) cursor=offsets[i+1];
            break;
        }
    }
    void erase(bool backward) {
        if(selected) {text.clear();cursor=0;selected=false;}
        else {
            const size_t old=cursor;move(backward?-1:1);
            const size_t begin=backward?cursor:old,end=backward?old:cursor;
            text.erase(begin,end-begin);cursor=begin;
        }
        error.clear();
    }
};
} }
#endif
