#include "games/ScoreOutbox.h"
#include <iostream>
int main(int argc,char **argv) {
    if(argc!=2) return 2;
    using namespace BackyardBrains::Rankings;
    if(queueTo(argv[1],"../invalid",1) || queueTo(argv[1],"dino",-1)) return 3;
    if(!queueTo(argv[1],"dino",999,true)) return 4;
    const std::string name="한별 \"1\"\\A";
    const std::string profilePath=std::string(argv[1])+"/../player-name.txt";
    if(!savePlayerName(profilePath,name) || loadPlayerName(profilePath)!=name) return 6;
    if(queueTo(argv[1],"dino",50,false,"   ")) return 7;
    if(!queueTo(argv[1],"dino",1234,false,name) || !queueTo(argv[1],"night-pulse",5600,false,"민수")) return 5;
    std::cout<<"Two durable results queued; practice and invalid inputs excluded.\n";
}
