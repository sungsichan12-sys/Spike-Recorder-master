#ifndef BACKYARDBRAINS_SIGNALPOWERMETER_H
#define BACKYARDBRAINS_SIGNALPOWERMETER_H

#include "widgets/Widget.h"
#include "engine/RecordingManager.h"
#include "engine/SignalPowerAnalysis.h"
#include <stdint.h>
#include <string>
#include <deque>
#include <utility>

namespace BackyardBrains {

class SignalPowerMeter : public Widgets::Widget
{
public:
    SignalPowerMeter(RecordingManager &manager, Widgets::Widget *parent = NULL);
    void advance();
    void resizeEvent(Widgets::ResizeEvent *event);

private:
    void updateReading();
    void paintEvent();
    void measureRest();
    void measureReference();
    void resetReference();
    void resetInput();
    std::string inputSignature(int deviceIndex) const;

    RecordingManager &_manager;
    std::string _channelName;
    SignalPower::Reading _reading;
    SignalPower::RelativeActivity _activity;
    std::deque<std::pair<int64_t,double> > _history;
    Widgets::Widget *_restButton;
    Widgets::Widget *_referenceButton;
    Widgets::Widget *_resetButton;
    std::string _inputSignature;
    int64_t _lastPosition;
    int _deviceIndex;
    uint32_t _lastUpdate;
    uint32_t _lastNewSamples;
    bool _hasReading;
};

} // namespace BackyardBrains

#endif
