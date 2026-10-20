// Hardware check: no window, no audio/sensor recording, statistics only.
#include "engine/RecordingManager.h"
#include "engine/USBInputChoice.h"
#include <windows.h>
#include <iostream>
#include <stdexcept>
#include <cmath>
#ifdef main
#undef main
#endif
using namespace BackyardBrains;
namespace BackyardBrains {
std::string getRecordingPath(){return "../../tests/core-artifacts";}
std::string getLoggingPath(){return "../../tests/core-artifacts/usb-engine.log";}
std::string getConfigPath(){return "../../tests/core-artifacts/usb-config.xml";}
}
static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
static void catalogChecks() {
    ArduinoSerial::SerialPort a,b;a.portName="COM1:";a.deviceType=ArduinoSerial::humansb;
    b.portName="COM10:";b.deviceType=ArduinoSerial::sbproneuroncdc;
    auto first=usbInputChoices({a,b},true,false);
    require(first.size()==3 && first[0].hid==HID_BOARD_TYPE_MUSCLE,"HID-only USB choice missing");
    require(first[0].label.find("USB HID")!=std::string::npos,"HID transport not explained");
    const auto selected=first[2];
    auto reordered=usbInputChoices({b,a},true,true);
    const int index=usbInputIndex(reordered,selected);
    require(index==2 && reordered[index].serial.portName=="COM10:","Port reorder changed selected device");
    auto removed=usbInputChoices({a},true,true);
    require(usbInputIndex(removed,selected)==-1,"Unplugged COM10 silently became COM1");
    auto hidOnly=usbInputChoices({},true,false);
    require(hidOnly.size()==1 && hidOnly[0].hid==HID_BOARD_TYPE_MUSCLE,"HID requires a COM port");
    require(usbInputChoices({},false,false).empty(),"Phantom connection offered");
    require(usbInputIndex(reordered,first[1])==3,"COM1 confused with COM10");
    std::cout<<"PASS USB catalog: HID without COM, stable identity, reorder, unplug, duplicate-prefix ports\n";
}
static int64_t capture(RecordingManager &manager,int milliseconds,double &peak) {
    const int64_t startPosition=manager.pos();const DWORD start=GetTickCount();DWORD last=start;
    while(GetTickCount()-start<static_cast<DWORD>(milliseconds)) {
        Sleep(20);const DWORD now=GetTickCount();
        manager.advance(static_cast<uint32_t>((now-last)*manager.sampleRate()/1000));last=now;
        require(manager.hidMode() && !manager.serialMode(),"HID disconnected or incorrectly switched to serial");
        const int count=std::min<int64_t>(manager.sampleRate()/20,manager.pos());
        if(count>0 && manager.selectedVDevice()>=0) {
            std::vector<int16_t> samples(count);
            manager.getData(manager.selectedVDevice(),manager.pos()-count,count,samples.data());
            for(int16_t sample:samples)peak=std::max(peak,double(std::abs(int(sample))));
        }
    }
    return manager.pos()-startPosition;
}
int main() {
    try {
        catalogChecks();RecordingManager manager;
        manager.scanForHIDDevices();
        const auto board=manager.isHIDBoardTypeAvailable(HID_BOARD_TYPE_MUSCLE)?HID_BOARD_TYPE_MUSCLE:
            manager.isHIDBoardTypeAvailable(HID_BOARD_TYPE_NEURON)?HID_BOARD_TYPE_NEURON:HID_BOARD_TYPE_NONE;
        std::cout<<"USB available_hid="<<board<<" serial_ports="<<manager.serailPorts().size()<<'\n';
        require(board!=HID_BOARD_TYPE_NONE,"No supported HID sensor is currently connected");
        // Reproduce the old serial-disconnect request that used to tear down
        // the HID stream on the next advance().
        manager.disconnectFromSerial();
        require(manager.initHIDUSB(board),manager.hidError.c_str());
        require(manager.virtualDevices().size()>=2 && manager.virtualDevices()[0].bound && manager.virtualDevices()[1].bound,"Second EMG input is hidden by default");
        manager.setPaused(false);double peak=0;
        const int64_t frames=capture(manager,2200,peak);
        require(frames>manager.sampleRate()/2,"USB opened but no live sample frames arrived");
        std::cout<<"PASS real USB HID: board="<<board<<" rate="<<manager.sampleRate()<<" channels="<<manager.numberOfHIDChannels()
                 <<" frames="<<frames<<" peak="<<peak<<" serial_mode="<<manager.serialMode()<<'\n';
        require(!manager.initSerial("COM99999",222222),"Nonexistent serial port opened");
        const int64_t afterFailedSerial=capture(manager,500,peak);
        require(afterFailedSerial>0,"Failed serial attempt destroyed active HID input");
        std::cout<<"PASS failed serial selection preserves HID stream\n";
        manager.disconnectFromHID();manager.scanForHIDDevices();
        require(!manager.hidMode(),"HID did not disconnect");
        require(manager.initHIDUSB(board),manager.hidError.c_str());
        const int64_t reopened=capture(manager,1500,peak);
        require(reopened>manager.sampleRate()/2,"Reopened HID did not resume samples");
        manager.disconnectFromHID();
        std::cout<<"PASS USB disconnect / reopen: frames="<<reopened<<" microphone_mode="<<(!manager.serialMode()&&!manager.hidMode())<<'\n';
        std::cout<<"RESULT USB hardware and connection regression checks passed\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
