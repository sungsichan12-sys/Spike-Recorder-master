#include "Paths.h"
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#include <string>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>

namespace BackyardBrains {

std::string ExePath() {
    char buffer[MAX_PATH];
    GetModuleFileName( NULL, buffer, MAX_PATH );
    std::string::size_type pos = std::string( buffer ).find_last_of( "\\/" );
    return std::string( buffer ).substr( 0, pos);
}

std::string getRecordingPath() {
	char path[MAX_PATH];

	SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, 0, path);
	std::string res = std::string(path) + "/BYB";
	mkdir(res.c_str());

	return res;
}

std::string getLoggingPath() {

    std::stringstream ss;
    ss << ExePath() << "\\byb.log";
    return ss.str();
}

std::string getConfigPath() {
    const char *appData = getenv("APPDATA");
    if (appData == NULL || appData[0] == '\0')
        return std::string();

    std::string appDirectory = std::string(appData) + "\\BackyardBrains";
    std::string configDirectory = appDirectory + "\\SpikeRecorder";
    // CreateDirectoryA safely leaves an existing directory unchanged.
    CreateDirectoryA(appDirectory.c_str(), NULL);
    CreateDirectoryA(configDirectory.c_str(), NULL);
    return configDirectory + "\\config.xml";
}


}
