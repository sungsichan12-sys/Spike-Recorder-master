/*
    This class FirmwareUpdater is used to update MSP430 devices 
    like old HID Muscle and Neuron SpikerBox Pro.
    Class takes info about available firmware versions from server 
    https://backyardbrains.com/products/firmwares/sbpro/compatibility.xml
    and offers users to update firmware.
    Works only on Windows since actual communiation with boothloader uses DLL for Windows only

*/
#include "FirmwareUpdater.h"
#include <cstdio>
#include <windows.h>

//#define ADDRESS_TO_COMPAT_XML "http://unit.rs/stanislav/compatibility.xml"
#define ADDRESS_TO_COMPAT_XML "https://backyardbrains.com/products/firmwares/sbpro/compatibility.xml"
#define NEW_FIRMWARE_FILENAME "newfirmware.txt"
#include "curl/curl.h"
using namespace tinyxml2;

namespace BackyardBrains
{

        FirmwareUpdater::FirmwareUpdater()
        {
             downloadCompatibilityXML(); // Keep and use the last valid list when offline.
             LoadXMLFile();

        }

        bool FirmwareUpdater::downloadCompatibilityXML()
        {
            CURL *curl = curl_easy_init();
            if(!curl) {
                logError("Could not initialize firmware list download.");
                return false;
            }

            const char *temporaryFile = "compatibility.xml.tmp";
            FILE *fp = fopen(temporaryFile, "wb");
            if(!fp) {
                curl_easy_cleanup(curl);
                logError("Could not create temporary compatibility list.");
                return false;
            }

            curl_easy_setopt(curl, CURLOPT_URL, ADDRESS_TO_COMPAT_XML);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &FirmwareUpdater::write_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            CURLcode res = curl_easy_perform(curl);
            if(res != CURLE_OK)
                logError("Could not download firmware compatibility list.");
            if(fclose(fp) != 0) {
                logError("Could not finish writing compatibility.xml.");
                res = CURLE_WRITE_ERROR;
            }
            curl_easy_cleanup(curl);
            if(res != CURLE_OK) {
                std::remove(temporaryFile);
                return false;
            }

            tinyxml2::XMLDocument downloadedList;
            if(downloadedList.LoadFile(temporaryFile) != XML_SUCCESS ||
               downloadedList.RootElement() == NULL ||
               findChildWithName(downloadedList.RootElement(), "firmwares") == NULL ||
               findChildWithName(downloadedList.RootElement(), "software") == NULL) {
                std::remove(temporaryFile);
                logError("Downloaded firmware compatibility list is invalid.");
                return false;
            }

            // Replace the cache only after the complete download has been written.
            if(!MoveFileExA(temporaryFile, "compatibility.xml",
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                std::remove(temporaryFile);
                logError("Could not replace cached compatibility list.");
                return false;
            }
            return true;
        }

        int FirmwareUpdater::downloadFirmware(BYBFirmwareVO * firmwareInfo)
        {
            if(!firmwareInfo || firmwareInfo->URL.empty())
                return 1;

            CURL *curl = curl_easy_init();
            if(!curl)
                return 1;

            FILE *fp = fopen(NEW_FIRMWARE_FILENAME, "wb");
            if(!fp) {
                curl_easy_cleanup(curl);
                return 1;
            }

            curl_easy_setopt(curl, CURLOPT_URL, firmwareInfo->URL.c_str());
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &FirmwareUpdater::write_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            CURLcode res = curl_easy_perform(curl);
            if(fclose(fp) != 0)
                res = CURLE_WRITE_ERROR;
            curl_easy_cleanup(curl);
            return res == CURLE_OK ? 0 : 1;
        }


        size_t FirmwareUpdater::write_data(char *ptr, size_t size, size_t nmemb, void *stream)
        {
            return fwrite(ptr, size, nmemb, static_cast<FILE *>(stream))*size;
        }




        //
        //Parse XML file
        //
        void FirmwareUpdater::LoadXMLFile()
        {
            try{
                    tinyxml2::XMLDocument doc;
                    if(doc.LoadFile("compatibility.xml")!= XML_NO_ERROR)
                    {
                        logError("Can not load XML file");
                        return;
                    }
                    XMLElement *rootnode = doc.RootElement();


                    //Get firmwares node
                    XMLNode *firmwaresNode  = findChildWithName(rootnode, "firmwares");
                    if(firmwaresNode==NULL)
                    {
                        logError("No firmwares in XML. Wrong XML format.");
                        return;
                    }

                    //get list of firmwares
                    std::list<BYBFirmwareVO> allFirmwares;
                    XMLNode *firmwareNode = firmwaresNode ->FirstChild();
                    while(firmwareNode!=NULL)
                    {
                        if( checkNodeName(firmwareNode, "firmware"))
                        {

                            //Parse one Firmware node and put into value-object
                            BYBFirmwareVO * newFirmware = new BYBFirmwareVO();
                            //go through all properties of one firmware
                            XMLNode *tempNode;
                            if((tempNode = findChildWithName(firmwareNode, "id"))==NULL)
                            {
                                return;
                            }

                            XMLElement * idNode = tempNode->ToElement();

                            if(idNode)
                            {
                                newFirmware->id = atoi(idNode->GetText());
                            }

                            if((tempNode = findChildWithName(firmwareNode, "version"))==NULL)
                            {
                                return;
                            }

                            XMLElement * versionNode = tempNode->ToElement();
                            if(versionNode)
                            {
                                newFirmware->version = std::string( versionNode->GetText());
                            }


                            if((tempNode = findChildWithName(firmwareNode, "desc"))==NULL)
                            {
                                return;
                            }

                            XMLElement * descNode = tempNode->ToElement();
                            if(descNode)
                            {
                                newFirmware->description =std::string( descNode->GetText());
                            }


                            if((tempNode = findChildWithName(firmwareNode, "type"))==NULL)
                            {
                                return;
                            }

                            XMLElement * typeNode = tempNode->ToElement();
                            if(typeNode)
                            {
                                newFirmware->type = std::string(typeNode->GetText());
                            }


                            if((tempNode = findChildWithName(firmwareNode, "url"))==NULL)
                            {
                                return;
                            }

                            XMLElement * urlNode = tempNode->ToElement();
                            if(urlNode)
                            {
                                newFirmware->URL = std::string(urlNode->GetText());
                            }

                            if((tempNode = findChildWithName(firmwareNode, "hardware"))==NULL)
                            {
                                return;
                            }

                            XMLElement * hardwareNode = tempNode->ToElement();
                            if(urlNode)
                            {
                                newFirmware->hardware = std::string(hardwareNode->GetText());
                            }

                            newFirmware->filepath = std::string(NEW_FIRMWARE_FILENAME);

                            //add to list of firmwares
                            if(newFirmware->id!=0)
                            {
                                allFirmwares.push_back((*newFirmware));
                            }
                        }


                        firmwareNode = firmwareNode->NextSibling();
                    }

                    //check if we found some firmwares
                    if(allFirmwares.size()==0)
                    {
                        logError("No firmwares in XML");
                        return;
                    }

                //------- find our version of software
                    XMLNode *softwareNode  = findChildWithName(rootnode, "software");
                    if(softwareNode==NULL)
                    {
                        logError("No softwareNode in XML. Wrong XML format.");
                        return;
                    }

                    XMLNode *buildNode = softwareNode ->FirstChild();
                    while(buildNode!=NULL)
                    {
                        if( checkNodeName(buildNode, "build"))
                        {
                            XMLNode * tempVersionNode = findChildWithName(buildNode, "version");
                            if(tempVersionNode==NULL)
                            {
                                logError("No version of software tag in XML.");
                                return;
                            }
                            XMLElement * versionElement  = findChildWithName(buildNode, "version")->ToElement();

                            if(versionElement)
                            {
                                if(std::string(versionElement->GetText()).compare(CURRENT_VERSION_STRING)==0)
                                {
                                    //found our version
                                    XMLNode * tempFirmwaresNode  = findChildWithName(buildNode, "firmwares");
                                    if(tempFirmwaresNode==NULL)
                                    {
                                        logError("No firmwares for our version of software in XML.");
                                        return;
                                    }
                                    //find all the IDs of the firmwares that are compatibile with our version
                                    XMLNode *idNode = tempFirmwaresNode ->FirstChild();
                                    while(idNode!=NULL)
                                    {
                                        if( checkNodeName(idNode, "firmwareid"))
                                        {
                                            XMLElement * idElement = idNode->ToElement();
                                            if(idElement)
                                            {
                                                int idToAdd = atoi(idElement->GetText());


                                                for( listBYBFirmwareVO::iterator ti = allFirmwares.begin();
                                                    ti != allFirmwares.end();
                                                    ti ++)
                                                {
                                                    if(ti->id == idToAdd)
                                                    {
                                                        BYBFirmwareVO tempBYBFirmware;
                                                        tempBYBFirmware.description = ((BYBFirmwareVO)(*ti)).description;
                                                        tempBYBFirmware.URL =((BYBFirmwareVO)(*ti)).URL;
                                                        tempBYBFirmware.version = ((BYBFirmwareVO)(*ti)).version;
                                                        tempBYBFirmware.type = ((BYBFirmwareVO)(*ti)).type;
                                                        tempBYBFirmware.id = ((BYBFirmwareVO)(*ti)).id;
                                                        tempBYBFirmware.hardware = ((BYBFirmwareVO)(*ti)).hardware;
                                                        firmwares.push_back(tempBYBFirmware);
                                                        break;
                                                    }
                                                }

                                            }
                                        }
                                        idNode = idNode->NextSibling();
                                    }
                                    break;
                                }
                            }
                        }

                        buildNode = buildNode->NextSibling();
                    }
            }
            catch(...)
            {
                logError("Error while downloading or parsing compatibility XML");
            }
        }

//============================ Utility functions ========================================
        void FirmwareUpdater::logError(const char * errorMessage)
        {
            std::string str(errorMessage);
            errorString = str;
            std::cout<<"Error: "<<errorString<<"\n";
        }

//================================== XML Utility functions ============================

        XMLNode * FirmwareUpdater::findChildWithName(XMLNode * parentNode, const char * name)
        {
            XMLNode *childNode = parentNode ->FirstChild();
            while(childNode!=NULL)
            {
                if( checkNodeName(childNode, name))
                {
                    break;
                }

                childNode = childNode->NextSibling();
            }
            if(childNode==NULL)
            {
                return NULL;
            }
            return childNode;
        }

        bool FirmwareUpdater::checkNodeName(XMLNode * node, const char * name)
        {
            if(node == NULL)
            {
                return false;
            }
            if((std::string(node->Value()).compare(name))==0)
            {
                return true;
            }
            return false;
        }

}
