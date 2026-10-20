#include "games/PlayerName.h"
#include <cstdlib>
#include <iostream>
using namespace BackyardBrains::Rankings;
static int checks=0;
void check(bool good,const char *description) {
    ++checks;if(!good) {std::cerr<<"FAIL: "<<description<<'\n';std::exit(1);}
}
int main() {
    PlayerNameEntry entry;
    entry.begin("");check(!entry.confirm() && entry.open,"blank cannot launch");
    check(entry.insert("   ") && !entry.confirm(),"spaces cannot launch");
    entry.selected=true;check(entry.insert("가나다"),"Korean input");
    entry.move(-1);entry.erase(true);check(entry.text=="가다","backspace removes one complete Korean code point");
    entry.insert("나");check(entry.text=="가나다","insert at Korean cursor");
    entry.erase(false);check(entry.text=="가나","delete removes next code point");
    check(entry.confirm() && !entry.open && entry.sessionName=="가나","confirmed name binds session");
    entry.begin("가나");check(entry.selected,"previous name is preselected");
    entry.insert("다른 이름");entry.cancel();
    check(!entry.open && entry.sessionName=="가나","cancel leaves previous session untouched");
    entry.begin("가나",true);check(!entry.confirm(),"opening Enter/repeat cannot auto-confirm");
    entry.enterReady=true;entry.composition="민";
    check(!entry.confirm() && entry.open,"Enter during IME composition cannot start game");
    entry.insert("민수");check(entry.text=="민수" && entry.composition.empty(),"IME commit replaces selection");
    check(entry.confirm() && entry.sessionName=="민수","new game uses new name");
    entry.begin("");std::string korean32;for(int i=0;i<32;++i) korean32+="가";
    check(entry.insert(korean32),"limit counts Korean code points, not UTF-8 bytes");
    entry.composition="나";check(!entry.insert("나") && entry.composition.empty(),"reject 33rd character without keeping stale composition");
    check(entry.confirm(),"32 Korean characters accepted");
    std::string name;
    check(normalizePlayerName("　 한별 　",name) && name=="한별","Unicode edge spaces trimmed");
    check(!normalizePlayerName("a\nb",name),"control characters rejected");
    check(!normalizePlayerName(std::string("\xc0\x80",2),name),"overlong UTF-8 rejected");
    check(!normalizePlayerName(std::string("\xed\xa0\x80",3),name),"UTF-8 surrogate rejected");
    check(!normalizePlayerName(std::string("\xf4\x90\x80\x80",4),name),"out-of-range Unicode rejected");
    entry.begin("");entry.insert("가😀나");entry.move(-1);entry.erase(true);
    check(entry.text=="가나","four-byte characters are deleted intact");
    entry.selected=true;check(!entry.insert("bad\nname") && entry.text=="가나","invalid paste preserves name");
    entry.erase(true);check(entry.text.empty() && entry.cursor==0,"delete selection");
    entry.insert("g name");check(entry.confirm() && entry.sessionName=="g name","G and space are normal name characters");
    check(jsonPlayerName("한별 \"1\"\\A")=="한별 \\\"1\\\"\\\\A","JSON quotes and slash escaped");
    std::cout<<checks<<" player name input checks passed.\n";
}
