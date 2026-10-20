// Headless device inventory. Does not open ports, send commands, or change settings.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <algorithm>
#include <cwctype>
#include <iostream>
#include <string>
#include <vector>

static std::string utf8(const std::wstring &s) {
    if(s.empty()) return std::string();
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),s.size(),NULL,0,NULL,NULL);
    std::string result(n,'\0');
    WideCharToMultiByte(CP_UTF8,0,s.data(),s.size(),&result[0],n,NULL,NULL);
    return result;
}
static std::wstring property(HDEVINFO set,SP_DEVINFO_DATA &device,DWORD key) {
    DWORD type=0,bytes=0;
    SetupDiGetDeviceRegistryPropertyW(set,&device,key,&type,NULL,0,&bytes);
    if(!bytes) return L"";
    std::vector<wchar_t> value(bytes/sizeof(wchar_t)+2,0);
    if(!SetupDiGetDeviceRegistryPropertyW(set,&device,key,&type,
            reinterpret_cast<PBYTE>(value.data()),bytes,NULL)) return L"";
    // Hardware IDs are a MULTI_SZ; include each ID for reliable matching.
    if(type==REG_MULTI_SZ) for(size_t i=0;i+1<value.size();++i)
        if(value[i]==0 && value[i+1]!=0) value[i]=L' ';
    return value.data();
}
int main() {
    SetConsoleOutputCP(CP_UTF8);
    HDEVINFO set=SetupDiGetClassDevsW(NULL,NULL,NULL,DIGCF_PRESENT|DIGCF_ALLCLASSES);
    if(set==INVALID_HANDLE_VALUE) {
        std::cout<<"Enumeration failed, Windows error "<<GetLastError()<<"\n";
        return 2;
    }
    int total=0,byb=0,serial=0,bluetooth=0,problemDevices=0;
    SP_DEVINFO_DATA device={}; device.cbSize=sizeof(device);
    DWORD index=0;
    for(;SetupDiEnumDeviceInfo(set,index,&device);++index) {
        ++total;
        std::wstring ids=property(set,device,SPDRP_HARDWAREID);
        std::wstring name=property(set,device,SPDRP_FRIENDLYNAME);
        if(name.empty()) name=property(set,device,SPDRP_DEVICEDESC);
        std::wstring cls=property(set,device,SPDRP_CLASS);
        std::wstring upper=ids;
        std::transform(upper.begin(),upper.end(),upper.begin(),towupper);
        bool isByb=upper.find(L"VID_2E73")!=std::wstring::npos;
        bool isPort=cls==L"Ports";
        bool isBluetooth=upper.find(L"BTHENUM")!=std::wstring::npos ||
            upper.find(L"BTHMODEM")!=std::wstring::npos;
        if(isByb) ++byb;
        if(isPort) { ++serial; if(isBluetooth) ++bluetooth; }
        if(!isByb && !isPort) continue;
        ULONG status=0,problem=0;
        CONFIGRET result=CM_Get_DevNode_Status(&status,&problem,device.DevInst,0);
        std::cout<<(isByb?"BYB device: ":isBluetooth?"Bluetooth port: ":"Serial port: ")<<utf8(name)<<"\n";
        std::cout<<"  IDs: "<<utf8(ids)<<"\n";
        if(result==CR_SUCCESS) {
            std::cout<<"  Driver problem code: "<<problem<<"\n";
            if(problem) ++problemDevices;
        } else std::cout<<"  Driver status unavailable: "<<result<<"\n";
    }
    DWORD error=GetLastError();
    SetupDiDestroyDeviceInfoList(set);
    if(error!=ERROR_NO_MORE_ITEMS) {
        std::cout<<"Enumeration incomplete, Windows error "<<error<<"\n";
        return 2;
    }
    std::cout<<"Present devices: "<<total<<"\nBYB entries: "<<byb
        <<"\nSerial ports: "<<serial<<" (Bluetooth: "<<bluetooth<<")\n";
    if(!total) { std::cout<<"Device inventory unavailable; cannot confirm connection.\n"; return 2; }
    if(!byb && serial==bluetooth)
        std::cout<<"No BYB USB/HID device or wired serial input detected. No sensor samples were read.\n";
    else std::cout<<"Connection candidates found. Sample reception still needs a separate check.\n";
    return problemDevices?3:0;
}
