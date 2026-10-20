#include "SignalPowerMeter.h"
#include "widgets/Application.h"
#include "widgets/BitmapFontGL.h"
#include "widgets/Color.h"
#include "widgets/Painter.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

namespace BackyardBrains {
namespace {
class MeterButton : public Widgets::Widget {
public:
    MeterButton(const char *text, Widgets::Widget *parent) : Widget(parent), label(text), enabled(true), pressed(false), hover(false) { setMouseTracking(true); }
    sigslot::signal0<> clicked;
    std::string label;
    bool enabled;
    void paintEvent() {
        using namespace Widgets;
        glDisable(GL_TEXTURE_2D);
        Painter::setColor(enabled && (hover || pressed) ? Colors::buttonhigh : Colors::button);
        Painter::drawRoundedRect(rect(),2);
        glEnable(GL_TEXTURE_2D);
        Painter::setColor(enabled ? Colors::white : Colors::gray);
        Application::font()->draw(label.c_str(),width()/2,height()/2,AlignCenter);
    }
    void enterEvent() { hover=true; }
    void leaveEvent() { hover=false; }
    void mousePressEvent(Widgets::MouseEvent *e) {
        if(e->button()==Widgets::LeftButton) { pressed=enabled; e->accept(); }
    }
    void mouseReleaseEvent(Widgets::MouseEvent *e) {
        if(e->button()!=Widgets::LeftButton) return;
        const bool activate=pressed && enabled && rect().contains(e->pos());
        pressed=false; e->accept(); if(activate) clicked.emit();
    }
private:
    bool pressed,hover;
};
std::string number(double value, bool scientific=false) {
    std::ostringstream out;
    if(scientific) out<<std::scientific<<std::setprecision(2)<<value;
    else out<<std::fixed<<std::setprecision(1)<<value;
    return out.str();
}
}

SignalPowerMeter::SignalPowerMeter(RecordingManager &manager, Widgets::Widget *parent)
    : Widgets::Widget(parent), _manager(manager), _lastPosition(-1), _deviceIndex(-1),
      _lastUpdate(0), _lastNewSamples(0), _hasReading(false)
{
    setSizeHint(Widgets::Size(248,400));
    MeterButton *rest=new MeterButton("휴식 측정",this);
    MeterButton *reference=new MeterButton("기준 측정",this);
    MeterButton *reset=new MeterButton("초기화",this);
    rest->clicked.connect(this,&SignalPowerMeter::measureRest);
    reference->clicked.connect(this,&SignalPowerMeter::measureReference);
    reset->clicked.connect(this,&SignalPowerMeter::resetReference);
    _restButton=rest; _referenceButton=reference; _resetButton=reset;
    reference->enabled=false;
    _manager.bufferReset.connect(this,&SignalPowerMeter::resetInput);
}

void SignalPowerMeter::resizeEvent(Widgets::ResizeEvent *event) {
    Widgets::Widget::resizeEvent(event);
    const int inner=std::max(1,width()-36), half=std::max(1,(inner-8)/2);
    _restButton->setGeometry(Widgets::Rect(18,228,half,30));
    _referenceButton->setGeometry(Widgets::Rect(18+half+8,228,half,30));
    _resetButton->setGeometry(Widgets::Rect(std::max(18,width()-72),12,54,26));
}

void SignalPowerMeter::advance() {
    const uint32_t now=SDL_GetTicks();
    if(now-_lastUpdate>=50) { _lastUpdate=now; updateReading(); }
}

std::string SignalPowerMeter::inputSignature(int index) const {
    std::ostringstream signature;
    signature<<index<<'|'<<_manager.getCurrentInputType()<<'|'<<_manager.sampleRate()
        <<'|'<<_manager.virtualDevices()[index].name
        <<'|'<<_manager.highPassFilterEnabled()<<':'<<_manager.highCornerFrequency()
        <<'|'<<_manager.lowPassFilterEnabled()<<':'<<_manager.lowCornerFrequency()
        <<'|'<<_manager.fiftyHzFilterEnabled()<<'|'<<_manager.sixtyHzFilterEnabled();
    if(_manager.serialMode()) signature<<'|'<<_manager.getCurrentPort().portName;
    return signature.str();
}

void SignalPowerMeter::resetInput() {
    _activity.reset(); _history.clear(); _reading=SignalPower::Reading();
    _hasReading=false; _lastPosition=-1; _deviceIndex=-1;
}
void SignalPowerMeter::measureRest() {
    if(_hasReading && !_manager.paused()) { _activity.start(SignalPower::RelativeActivity::Rest); _history.clear(); }
}
void SignalPowerMeter::measureReference() {
    if(_hasReading && !_manager.paused()) { _activity.start(SignalPower::RelativeActivity::Reference); _history.clear(); }
}
void SignalPowerMeter::resetReference() { _activity.reset(); _history.clear(); }

void SignalPowerMeter::updateReading() {
    const auto &devices=_manager.virtualDevices();
    int index=_manager.signalDeviceIndex();
    if(index<0 || index>=static_cast<int>(devices.size()) || !devices[index].bound) {
        index=-1;
        for(size_t i=0;i<devices.size();++i) if(devices[i].bound) { index=static_cast<int>(i); break; }
    }
    const int rate=_manager.sampleRate();
    const int64_t end=_manager.pos();
    if(index<0 || rate<=0) { resetInput(); _inputSignature.clear(); }
    else {
        const std::string signature=inputSignature(index);
        if(signature!=_inputSignature || end<_lastPosition) { resetInput(); _inputSignature=signature; }
        _channelName=devices[index].name;
        _deviceIndex=index;
        if(_manager.paused()) _activity.cancel();
        if(end>0 && end!=_lastPosition) {
            const int count=static_cast<int>(std::min<int64_t>(end,std::max(2,rate/4)));
            std::vector<int16_t> samples(count);
            _manager.getData(index,end-count,count,samples.data());
            _reading=SignalPower::measure(samples.data(),samples.size());
            const double seconds=_lastPosition>=0 ? (end-_lastPosition)/static_cast<double>(rate) : 0;
            if(!_manager.paused() && count>=std::max(2,rate/4)) _activity.ingest(_reading,seconds);
            _lastPosition=end; _lastNewSamples=SDL_GetTicks(); _hasReading=_reading.valid;
            if(seconds>.5) _history.clear();
            if(_activity.ready()) {
                _history.push_back(std::make_pair(end,_activity.percent(_reading)));
                while(!_history.empty() && end-_history.front().first>static_cast<int64_t>(rate)*10) _history.pop_front();
            }
        } else if(!_manager.paused() && SDL_GetTicks()-_lastNewSamples>500) {
            _hasReading=false; _activity.cancel();
        }
    }
    const bool active=_hasReading && !_manager.paused();
    static_cast<MeterButton *>(_restButton)->enabled=active && _activity.stage()==SignalPower::RelativeActivity::Idle;
    static_cast<MeterButton *>(_referenceButton)->enabled=active && _activity.restReady() && _activity.stage()==SignalPower::RelativeActivity::Idle;
}

void SignalPowerMeter::paintEvent() {
    using namespace Widgets;
    using SignalPower::RelativeActivity;
    const BitmapFontGL *font=Application::font();
    const int inset=18, inner=std::max(1,width()-36);
    glDisable(GL_TEXTURE_2D);
    Painter::setColor(Colors::background); Painter::drawRect(rect());
    Painter::setColor(Colors::border);
    Painter::drawRect(Rect(0,0,1,height()));
    Painter::drawRect(Rect(inset,274,inner,1));
    glEnable(GL_TEXTURE_2D);
    Painter::setColor(Colors::white); font->draw("파워 측정기",inset,18);
    const char *source=_manager.fileMode()?"녹음 파일":(_manager.serialMode()||_manager.hidMode())?"SpikerBox":"마이크 입력";
    const std::string sourceLabel=std::string(source)+(_deviceIndex>=0?" / Ch "+std::to_string(_deviceIndex+1):"")+(_manager.autoSignalChannel()?" AUTO":"");
    Painter::setColor(Colors::mutedText); font->draw(sourceLabel.c_str(),inset,46);

    const bool ready=_hasReading && _activity.ready();
    const double percent=ready?_activity.percent(_reading):0;
    const std::string value=ready?(percent>999?">999%":number(percent)+"%") : "--%";
    Painter::setColor(_reading.rangeLimit?Colors::warning:Colors::white);
    glPushMatrix(); glTranslatef(inset,78,0); glScalef(2.1f,2.1f,1);
    font->draw(value.c_str(),0,0); glPopMatrix();
    Painter::setColor(Colors::mutedText); font->draw("내 기준 대비 신호 세기",inset,118);
    glDisable(GL_TEXTURE_2D);
    Painter::setColor(Colors::button); Painter::drawRect(Rect(inset,146,inner,6));
    if(ready && percent>0) {
        Painter::setColor(percent>100?Colors::warning:Colors::lightblue);
        Painter::drawRect(Rect(inset,146,static_cast<int>(inner*std::min(1.0,percent/100)),6));
    }
    if(_activity.stage()!=RelativeActivity::Idle) {
        Painter::setColor(Colors::lightblue);
        Painter::drawRect(Rect(inset,146,static_cast<int>(inner*_activity.progress()),6));
    }
    glEnable(GL_TEXTURE_2D);
    Painter::setColor(Colors::mutedText);
    font->draw("0",inset,158); font->draw("100%",width()-inset,158,AlignRight);

    std::string status,detail;
    const bool mic=!_manager.fileMode() && !_manager.serialMode() && !_manager.hidMode();
    if(_manager.paused()) { status="일시정지"; detail="측정하려면 재생을 켜세요."; }
    else if(!_hasReading) { status="입력 신호를 기다리는 중"; detail="센서 또는 마이크를 확인하세요."; }
    else if(_activity.stage()!=RelativeActivity::Idle) {
        status=_activity.stage()==RelativeActivity::Rest?(mic?"조용한 상태를 유지하세요.":"힘을 빼고 유지하세요."):
            (mic?"기준 소리를 유지하세요.":"기준으로 삼을 힘을 유지하세요.");
        detail=number(_activity.remaining())+"초 남음";
    } else if(_activity.result()==RelativeActivity::RangeLimit || _reading.rangeLimit) {
        status="입력 범위 초과"; detail="입력 크기를 줄이고 재측정하세요.";
    } else if(_activity.result()==RelativeActivity::TooWeak) {
        status="기준과 휴식 신호가 비슷합니다."; detail="기준 동작을 다시 측정하세요.";
    } else if(_activity.result()==RelativeActivity::Interrupted) {
        status="측정이 중단되었습니다."; detail="신호를 확인하고 다시 누르세요.";
    } else if(!_activity.restReady()) {
        status=mic?"1. 조용히 휴식 측정 (2초)":"1. 힘을 빼고 휴식 측정 (2초)";
        detail=mic?"2. 소리를 내며 기준 측정 (3초)":"2. 힘을 주고 기준 측정 (3초)";
    } else if(!_activity.ready()) {
        status="휴식값을 저장했습니다."; detail=mic?"기준 소리를 내며 기준 측정":"힘을 주고 기준 측정 (3초)";
    } else { status="기준값을 저장했습니다."; detail=mic?"마이크에서는 소리 세기입니다.":"자세나 전극을 바꾸면 재측정"; }
    Painter::setColor(Colors::white); font->draw(status.c_str(),inset,184);
    Painter::setColor(Colors::mutedText); font->draw(detail.c_str(),inset,205);

    bool calibrated=_manager.isCalibrated();
    const double scale=calibrated?_manager.getCalibrationCoeficient():1.0;
    calibrated=calibrated && std::isfinite(scale) && scale>0;
    const double unit=calibrated?scale:1.0;
    const char *labels[]={calibrated?"RMS (mV)":"RMS (ADC)",calibrated?"파워 (mV^2)":"파워 (ADC^2)",calibrated?"피크 (mV)":"피크 (ADC)"};
    const std::string values[]={number(_reading.rms*unit),number(_reading.power*unit*unit,true),number(_reading.peak*unit)};
    for(int i=0;i<3;++i) {
        Painter::setColor(Colors::mutedText); font->draw(labels[i],inset,288+24*i);
        Painter::setColor(Colors::white); font->draw(_hasReading?values[i].c_str():"--",width()-inset,288+24*i,AlignRight);
    }
    Painter::setColor(Colors::mutedText); font->draw("250 ms · 상대값 (N, W 아님)",inset,366);
    if(height()>490) {
        const int top=414, bottom=height()-20;
        font->draw("최근 10초 / 0-150%",inset,390);
        glDisable(GL_TEXTURE_2D);
        Painter::setColor(Colors::grid);
        Painter::drawRect(Rect(inset,bottom-(bottom-top)*2/3,inner,1));
        Painter::drawRect(Rect(inset,bottom,inner,1));
        if(_history.size()>1) {
            Painter::setColor(Colors::lightblue); glLineWidth(1);
            glBegin(GL_LINE_STRIP);
            const double span=std::max(1,_manager.sampleRate())*10.0;
            for(size_t i=0;i<_history.size();++i) glVertex2f(inset+inner*static_cast<float>(1.0-(_lastPosition-_history[i].first)/span),
                bottom-(bottom-top)*static_cast<float>(std::min(150.0,_history[i].second)/150));
            glEnd();
        }
        glEnable(GL_TEXTURE_2D);
    }
}
} // namespace BackyardBrains
