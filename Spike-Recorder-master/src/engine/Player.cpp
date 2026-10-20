#include "Player.h"
#include "BASSErrors.h"
#include "Log.h"
#include <algorithm>

namespace BackyardBrains {

Player::Player() {
}

Player::~Player() {
	stop();
}

void Player::start(int sampleRate) {
    stop();
    if(sampleRate <= 0) return;
	_output = BASS_StreamCreate(sampleRate, 1, 0, STREAMPROC_PUSH, 0);
    if(!_output) { Log::error("Bass Error: creating playback stream: %s", GetBassStrError()); return; }
	setVolume(0);
	//setPaused(false);
	BASS_ChannelPlay(_output, TRUE);
}

uint64_t Player::pos(uint16_t bytesPerSample, uint16_t numOfChannels)
{

    if(!_output || !bytesPerSample || !numOfChannels) return 0;
    const QWORD position = BASS_ChannelGetPosition(_output, BASS_POS_BYTE);
    return position == static_cast<QWORD>(-1) ? 0 : position/(uint64_t(bytesPerSample)*numOfChannels);
}

void Player::setPos(uint64_t npos, uint16_t bytesPerSample, uint16_t numOfChannels)
{
    if(_output && bytesPerSample && numOfChannels)
        BASS_ChannelSetPosition(_output, bytesPerSample*npos*numOfChannels, BASS_POS_BYTE);
}


int Player::stateOfBuffer()
{
    if(!_output) return 0;
    const DWORD bytes = BASS_ChannelGetData(_output,NULL,BASS_DATA_AVAILABLE);
    return bytes == static_cast<DWORD>(-1) ? 0 : static_cast<int>(bytes);
}

int Player::volume() const {
	float val = 0;
    if(!_output || !BASS_ChannelGetAttribute(_output, BASS_ATTRIB_VOL, &val)) return 0;
	return val*100.f+0.5f;
}

void Player::setVolume(int volume) {
	if(!_output) return;
	volume = std::max(0,std::min(100,volume));
	
    if(!BASS_ChannelSetAttribute(_output, BASS_ATTRIB_VOL, volume*0.01f))
        Log::error("Bass Error: volume %s", GetBassStrError());
}

bool Player::paused() const {
	return BASS_ChannelIsActive(_output) != BASS_ACTIVE_PLAYING;
}

void Player::setPaused(bool npaused) {
	if(!_output) return;
	if(npaused == paused())
		return;

	if(npaused) {
		if(!BASS_ChannelPause(_output))
			Log::error("Bass Error: pausing channel failed: %s", GetBassStrError());
	} else {
		if(!BASS_ChannelPlay(_output, FALSE))
			Log::error("Bass Error: resuming channel playback failed: %s", GetBassStrError());
	}
}

int Player::sampleRate() const {
	float freq = 0;
    if(!_output || !BASS_ChannelGetAttribute(_output, BASS_ATTRIB_FREQ, &freq)) return 0;
	return freq;
}

void Player::setSampleRate(int sampleRate) {
    if(!_output || sampleRate <= 0) return;
	if(BASS_ChannelSetAttribute(_output, BASS_ATTRIB_FREQ, sampleRate))
    {
        Log::msg("Player::setSampleRate - Sample rate set OK");
    }
    else
    {
        Log::error("Bass Error: setting sample rate: %s", GetBassStrError());
    }
}

void Player::push(void *data, uint32_t size) {
    if(!_output || !data || !size) return;
    int buffered = BASS_ChannelGetData(_output, NULL, BASS_DATA_AVAILABLE);
    Log::msg("In buffer: %d - New: %d", buffered, size);
    
	if(BASS_StreamPutData(_output, data, size) == (DWORD)-1)
		Log::error("Bass Error: putting stream data failed: %s", GetBassStrError());
}

void Player::stop() {
	if(_output) BASS_StreamFree(_output);
    _output = 0;
}

}
