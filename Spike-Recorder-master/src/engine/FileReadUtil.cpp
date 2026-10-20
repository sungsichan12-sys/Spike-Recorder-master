#include "FileReadUtil.h"
#include "Log.h"
#include "BASSErrors.h"
#include <bass.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
namespace BackyardBrains {

namespace {
struct WavLength { std::string filename; int64_t frames; };
std::map<HSTREAM, WavLength> wavLengths;

int64_t pcm24Frames(const char *filename, int channels) {
    std::ifstream file(filename, std::ios::binary);
    char header[12];
    if(!file.read(header,12) || std::memcmp(header,"RIFF",4) || std::memcmp(header+8,"WAVE",4)) return -1;
    file.seekg(0,std::ios::end);
    const std::streamoff end = file.tellg();
    file.seekg(12);
    while(file && file.tellg() >= 0 && static_cast<std::streamoff>(file.tellg()) + 8 <= end) {
        unsigned char chunk[8];
        if(!file.read(reinterpret_cast<char *>(chunk),8)) return -1;
        const uint32_t size = uint32_t(chunk[4]) | uint32_t(chunk[5])<<8 | uint32_t(chunk[6])<<16 | uint32_t(chunk[7])<<24;
        const std::streamoff pos = file.tellg();
        if(pos + static_cast<std::streamoff>(size) > end) return -1;
        if(!std::memcmp(chunk,"data",4)) return size / (channels * 3);
        file.seekg(pos + static_cast<std::streamoff>(size) + (size & 1));
    }
    return -1;
}
}



bool openAnyFile(const char *file, HSTREAM &handle, int &nchan, int &samplerate, int &bytespersample)
{
    std::string filenameAndPath(file);
    std::string lowerCasePath;

      // Allocate the destination space
    lowerCasePath.resize(filenameAndPath.size());

      // Convert the source string to lower case
      // storing the result in destination string
      std::transform(filenameAndPath.begin(),
                     filenameAndPath.end(),
                     lowerCasePath.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if(lowerCasePath.size() >= 4 && lowerCasePath.compare(lowerCasePath.size() - 4, 4, ".wav") == 0)
    {
        return OpenWAVFile(file, handle, nchan, samplerate, bytespersample);
    }

    handle = 0;
    nchan = 0;
    samplerate = 0;
    bytespersample = 0;
    Log::error("Unsupported audio file format: '%s'", file);
    return false;
}


bool OpenWAVFile(const char *file, HSTREAM &handle, int &nchan, int &samplerate, int &bytespersample) {
	handle = 0;
	nchan = samplerate = bytespersample = 0;
	handle = BASS_StreamCreateFile(false, file, 0, 0, BASS_STREAM_DECODE);
	if(handle == 0) {
		Log::error("Bass Error: Failed to load file '%s': %s", file, GetBassStrError());
		return false;
	}

	BASS_CHANNELINFO info;
	if(!BASS_ChannelGetInfo(handle, &info)) {
		Log::error("Bass Error: Failed to inspect file '%s': %s", file, GetBassStrError());
		BASS_StreamFree(handle);
		handle = 0;
		return false;
	}

	bytespersample = (info.origres & 0xffff)/8;
	if(bytespersample == 0) {
		Log::error("Bass Error: Failed to load file '%s': strange bytes per sample", file);
        bytespersample = 2;
		//return false;
	}
    
    if(bytespersample >= 3)//probably float sample
    {
       
        HSTREAM floatHandle = BASS_StreamCreateFile(false, file, 0, 0, BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE);
        if(floatHandle == 0) {
            Log::error("Bass Error: Failed to load float file '%s': %s", file, GetBassStrError());
            BASS_StreamFree(handle);
            handle = 0;
            return false;
        }

		BASS_StreamFree(handle);
		handle = floatHandle;
		if(!BASS_ChannelGetInfo(handle, &info)) {
			Log::error("Bass Error: Failed to inspect float file '%s': %s", file, GetBassStrError());
			BASS_StreamFree(handle);
			handle = 0;
			return false;
		}
		bytespersample = 4; // bass converts everything it doesn’t support.
    }

	nchan = info.chans;
	samplerate = info.freq;
	// The bundled decoder can report a 24-bit PCM length one frame short
	// while still returning that final frame. Use the validated RIFF data size
	// for this small discrepancy, retaining the decoder for all sample reads.
	for(auto it = wavLengths.begin(); it != wavLengths.end();) {
		BASS_CHANNELINFO active = {};
		if(!BASS_ChannelGetInfo(it->first,&active)) it = wavLengths.erase(it);
		else ++it;
	}
	wavLengths.erase(handle);
	if((info.origres & 0xffff) == 24 && nchan > 0) {
		const int64_t frames = pcm24Frames(file,nchan);
		const QWORD decodedBytes = BASS_ChannelGetLength(handle,BASS_POS_BYTE);
		if(decodedBytes != static_cast<QWORD>(-1)) {
			const int64_t decodedFrames = decodedBytes / bytespersample / nchan;
			if(frames >= decodedFrames && frames - decodedFrames <= 1 && info.filename)
				wavLengths[handle] = WavLength{info.filename,frames};
		}
	}

	return true;
}

const char * readEventsAndSpikesForAnyFile(HSTREAM handle)
{
    if(handle)
    {
        return readEventsAndSpikesForWav(handle);
    }
    return 0;
}



const char * readEventsAndSpikesForWav(HSTREAM handle)
{
    return BASS_ChannelGetTags(handle, BASS_TAG_RIFF_INFO);
}

bool readAnyFile(std::vector<std::vector<int16_t> > &channels, int len, HSTREAM handle, int nchan, int bytespersample)
{
    if(handle)
    {
        return ReadWAVFile(channels, len, handle, nchan, bytespersample);
    }
    return 0;
}



bool ReadWAVFile(std::vector<std::vector<int16_t> > &channels, int len, HSTREAM handle, int nchan, int bytespersample) {
	if(len <= 0 || handle == 0 || nchan <= 0 || bytespersample <= 0)
		return false;
	channels.resize(nchan);
	uint8_t *buffer = new uint8_t[len];
	DWORD samplesRead = BASS_ChannelGetData(handle, buffer, len);
   // long positionInfile = BASS_ChannelGetPosition(handle, BASS_POS_BYTE)/bytespersample;
   // std::cout<<"Current position: "<<positionInfile<<"\n";
	if(samplesRead == (DWORD)-1) {
		Log::error("Bass Error: getting channel data failed: %s", GetBassStrError());
		delete[] buffer;
		return false;
	}

	samplesRead /= bytespersample;

	for(int chan = 0; chan < nchan; chan++)
		channels[chan].resize(samplesRead/nchan);

	// de-interleave the channels
	for(DWORD i = 0; i < samplesRead/nchan; i++) {
		for(int chan = 0; chan < nchan; chan++) {
			if(bytespersample == 1) { // unsigned 8 bit format
				channels[chan][i] = (static_cast<int>(buffer[i*nchan + chan])-128)*256;
			} else { // else assume signedness and take the 2 most significant bytes
                if(bytespersample==4)
                {//if it is 4 byte we assume that it is float and multiply it by 32000 to convert from [-1,1] to [-32000,32000]
                    float tempBuffer;
                   memcpy(&tempBuffer, buffer+(i*nchan + chan)*bytespersample, 4);
                    const float scaled = std::isfinite(tempBuffer) ? tempBuffer * 32766.0f : 0.0f;
                    channels[chan][i] = static_cast<int16_t>(std::max(-32768.0f, std::min(32767.0f, scaled)));
                }
                else
                {
                    memcpy(&channels[chan][i], buffer+(i*nchan + chan)*bytespersample, 2);
                }
    
			}
		}
	}
	delete[] buffer;
	return true;
}
	

int64_t anyFilesLength(HSTREAM handle, int bytespersample, int channels)
{
    if(handle && bytespersample > 0 && channels > 0)
    {
        auto found = wavLengths.find(handle);
        if(found != wavLengths.end()) {
            BASS_CHANNELINFO info = {};
            if(BASS_ChannelGetInfo(handle,&info) && info.filename && found->second.filename == info.filename)
                return found->second.frames;
            wavLengths.erase(found);
        }
        const QWORD length = BASS_ChannelGetLength(handle, BASS_POS_BYTE);
        return length == static_cast<QWORD>(-1) ? 0 : length/bytespersample/channels;
    }
    return 0;
}


}//namespace BYB
