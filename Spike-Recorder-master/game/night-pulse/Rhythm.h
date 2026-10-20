#ifndef NIGHT_PULSE_RHYTHM_H
#define NIGHT_PULSE_RHYTHM_H
#include <algorithm>
#include <cmath>
#include <vector>
namespace NightPulse {
const float Beat=.5f, FirstNote=2.0f, EndTime=34.8f, Travel=2.0f, HitWindow=.20f;
const int MissLimit=12;
enum Judgement { None, Perfect, Great, Good, Miss, Early };
struct Note { float time; bool judged; Note(float t):time(t),judged(false){} };
struct Rhythm {
    std::vector<Note> notes;
    double elapsed;
    float feedbackTime,strikeTime,lastTap;
    int score,combo,bestCombo,perfect,great,good,misses,hand,hitNote,strokes;
    bool started,finished,failed;
    Judgement judgement;
    // Visual histories are independent: a new stroke must not cancel the other
    // hand's rebound or an earlier cymbal's vibration.
    double stickAt[2],instrumentAt[3];
    int stickNote[2];
    float stickPower[2],instrumentPower[3];
    Rhythm() { reset(); started=false; }
    void reset() {
        notes.clear();
        for(int beat=0;beat<64;++beat) {
            const int bar=beat/4,p=beat%4;
            if((bar%4==1 && p==1)||(bar%4==3 && p==3)) continue;
            notes.push_back(Note(FirstNote+beat*Beat));
        }
        elapsed=0; feedbackTime=strikeTime=0; lastTap=-1;
        score=combo=bestCombo=perfect=great=good=misses=hand=0;
        hitNote=-1;
        strokes=0;
        for(int i=0;i<2;++i) { stickAt[i]=-10; stickNote[i]=0; stickPower[i]=0; }
        for(int i=0;i<3;++i) { instrumentAt[i]=-10; instrumentPower[i]=0; }
        started=true; finished=failed=false; judgement=None;
    }
    void feedback(Judgement j) { judgement=j; feedbackTime=.42f; }
    void miss() {
        ++misses; combo=0; feedback(Miss);
        if(misses>=MissLimit) { failed=finished=true; }
    }
    void tap() {
        if(finished || elapsed-lastTap<.12) return;
        lastTap=static_cast<float>(elapsed);
        int nearest=-1; double error=HitWindow+1e-6;
        for(size_t i=0;i<notes.size();++i) if(!notes[i].judged) {
            const double d=std::fabs(elapsed-notes[i].time);
            if(d<error) { error=d; nearest=static_cast<int>(i); }
        }
        // A physical tap always moves the stick, even when its timing is wrong.
        // Judgement still controls the score and remaining chances separately.
        ++strokes;
        strikeTime=.26f;
        hitNote=nearest>=0?nearest:0;
        hand=(strokes-1)%2;
        const int instrument=hitNote%8==7?2:(hitNote%8==3?1:0);
        const int playingHand=instrument==2?1:(instrument==1?0:hand);
        const float power=nearest<0?.65f:(error<=.080001?1.f:(error<=.140001?.85f:.72f));
        stickAt[playingHand]=instrumentAt[instrument]=elapsed;
        stickNote[playingHand]=hitNote;
        stickPower[playingHand]=instrumentPower[instrument]=power;
        if(nearest<0) {
            if(elapsed<FirstNote-HitWindow) feedback(Early);
            else miss();
            return;
        }
        notes[nearest].judged=true; ++combo;
        hitNote=nearest;
        bestCombo=std::max(combo,bestCombo);
        if(error<=.080001) { ++perfect; score+=100; feedback(Perfect); }
        else if(error<=.140001) { ++great; score+=75; feedback(Great); }
        else { ++good; score+=50; feedback(Good); }
    }
    // Absolute song time: frame stalls never slow the chart relative to music.
    void update(double songTime,bool tapped) {
        if(!started || finished || !std::isfinite(songTime)) return;
        const double next=std::max(elapsed,songTime);
        const float dt=static_cast<float>(next-elapsed);
        elapsed=next;
        feedbackTime=std::max(0.0f,feedbackTime-dt);
        strikeTime=std::max(0.0f,strikeTime-dt);
        // Expired notes are resolved before looking for the nearest live note.
        for(size_t i=0;i<notes.size() && !finished;++i)
            if(!notes[i].judged && elapsed>notes[i].time+HitWindow+1e-6) {
                notes[i].judged=true; miss();
            }
        if(tapped && !finished) tap();
        if(!finished && elapsed>=EndTime) finished=true;
    }
    float accuracy() const {
        const int judged=perfect+great+good+misses;
        return judged?100.f*(perfect+great*.75f+good*.5f)/judged:0;
    }
};
}
#endif
