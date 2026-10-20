#ifndef BYB_USB_INPUT_CHOICE_H
#define BYB_USB_INPUT_CHOICE_H
#include "ArduinoSerial.h"
#include "HIDUsbManager.h"
#include <vector>
namespace BackyardBrains {
struct USBInputChoice {
    ArduinoSerial::SerialPort serial;
    HIDBoardType hid;
    std::string label;
    bool sameDevice(const USBInputChoice &other) const {
        return hid==other.hid && (hid!=HID_BOARD_TYPE_NONE || ArduinoSerial::samePortName(serial.portName,other.serial.portName));
    }
    bool sameOption(const USBInputChoice &other) const {
        return sameDevice(other) && label==other.label && serial.baudRate==other.serial.baudRate;
    }
};
inline const char *serialDeviceTitle(ArduinoSerial::SerialDevice type) {
    switch(type) {
    case ArduinoSerial::plant:return "Plant SpikerShield";
    case ArduinoSerial::muscle:return "Muscle SpikerShield";
    case ArduinoSerial::heart:return "Heart and Brain SpikerShield";
    case ArduinoSerial::heartOneChannel:return "Heart and Brain SpikerBox";
    case ArduinoSerial::heartPro:return "Heart and Brain SpikerBox Pro";
    case ArduinoSerial::neuronOneChannel:return "Neuron SpikerBox";
    case ArduinoSerial::muscleusb:return "Muscle SpikerBox";
    case ArduinoSerial::humansb:return "Human SpikerBox";
    case ArduinoSerial::hhibox:return "Human-Human Interface";
    case ArduinoSerial::sbpromusclecdc:return "Muscle SpikerBox Pro";
    case ArduinoSerial::sbproneuroncdc:case ArduinoSerial::sbproneuronmfi:return "Neuron SpikerBox Pro";
    case ArduinoSerial::unibox:return "Spike Station";
    case ArduinoSerial::extclaw:return "Claw";
    default:return "Serial device";
    }
}
inline std::vector<USBInputChoice> usbInputChoices(const std::list<ArduinoSerial::SerialPort> &serials,bool muscle,bool neuron) {
    std::vector<USBInputChoice> choices;
    for(int type=HID_BOARD_TYPE_MUSCLE;type<=HID_BOARD_TYPE_NEURON;++type) {
        if(!(type==HID_BOARD_TYPE_MUSCLE?muscle:neuron))continue;
        USBInputChoice c;c.hid=static_cast<HIDBoardType>(type);
        c.label=type==HID_BOARD_TYPE_MUSCLE?"Muscle SpikerBox Pro - USB HID":"Neuron SpikerBox Pro - USB HID";
        choices.push_back(c);
    }
    for(const auto &port:serials) {
        USBInputChoice c;c.hid=HID_BOARD_TYPE_NONE;c.serial=port;
        c.label=std::string(serialDeviceTitle(port.deviceType))+" ("+port.portName+")";choices.push_back(c);
    }
    return choices;
}
inline int usbInputIndex(const std::vector<USBInputChoice> &choices,const USBInputChoice &selected) {
    for(size_t i=0;i<choices.size();++i)if(choices[i].sameDevice(selected))return static_cast<int>(i);
    return -1;
}
}
#endif
