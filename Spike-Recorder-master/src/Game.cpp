#include "Game.h"
#include "MainView.h"
#include "Log.h"
#include "widgets/BitmapFontGL.h"
#include "widgets/Color.h"
#include "widgets/Painter.h"
#include "widgets/TextureGL.h"
#include <SDL.h>
#include <SDL_opengl.h>

#include <algorithm>
#include <string>

namespace BackyardBrains {

namespace {

const uint32_t kTypeDuration = 1680;
const uint32_t kHoldDuration = 420;
const uint32_t kFadeDuration = 600;
const uint32_t kPauseDuration = 220;
const uint32_t kSchoolFadeDuration = 850;
const uint32_t kSchoolHoldDuration = 1000;
const uint32_t kExitFadeDuration = 450;
const uint32_t kIntroDuration = kTypeDuration + kHoldDuration + kFadeDuration +
    kPauseDuration + kSchoolFadeDuration + kSchoolHoldDuration + kExitFadeDuration;

float clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

class StartupIntro : public Widgets::Widget
{
public:
    StartupIntro() : _elapsed(0), _lastTick(SDL_GetTicks())
    {
        setMouseTracking(true);
    }

    void advance()
    {
        const uint32_t now = SDL_GetTicks();
        const uint32_t step = std::min<uint32_t>(now - _lastTick, 100);
        _lastTick = now;
        _elapsed += step;
        if(_elapsed >= kIntroDuration)
        {
            close();
            setVisible(false);
        }
    }

    void keyPressEvent(Widgets::KeyboardEvent *event)
    {
        event->accept();
    }

    void keyReleaseEvent(Widgets::KeyboardEvent *event)
    {
        event->accept();
    }

    void mousePressEvent(Widgets::MouseEvent *event)
    {
        event->accept();
    }

    void paintEvent()
    {
        const int w = width();
        const int h = height();
        if(w <= 0 || h <= 0)
            return;

        glDisable(GL_TEXTURE_2D);
        Widgets::Painter::setColor(Widgets::Color(0, 0, 0, 255));
        Widgets::Painter::drawRect(Widgets::Rect(0, 0, w, h));
        glEnable(GL_TEXTURE_2D);

        const int centerX = w / 2;
        const int centerY = h / 2;

        const uint32_t typeEnd = kTypeDuration + kHoldDuration;
        const uint32_t fadeEnd = typeEnd + kFadeDuration;
        const uint32_t schoolStart = fadeEnd + kPauseDuration;
        float logoAlpha = 0.0f;
        if(_elapsed < typeEnd)
            logoAlpha = 1.0f;
        else if(_elapsed < fadeEnd)
            logoAlpha = 1.0f - clamp01(static_cast<float>(_elapsed - typeEnd) /
                                       static_cast<float>(kFadeDuration));

        if(logoAlpha > 0.0f)
        {
            const int charCount = std::min(6,
                static_cast<int>(_elapsed * 6 / kTypeDuration) + 1);
            std::string logo("LOTTOL", static_cast<size_t>(charCount));
            drawLargeText(logo.c_str(), centerX, centerY,
                          std::min(4.0f, std::max(1.5f,
                              static_cast<float>(w) / 220.0f)),
                          Widgets::Color(245, 245, 245,
                              static_cast<uint8_t>(255 * logoAlpha)));
        }

        float schoolAlpha = 0.0f;
        if(_elapsed >= schoolStart)
            schoolAlpha = clamp01(static_cast<float>(_elapsed - schoolStart) /
                                  static_cast<float>(kSchoolFadeDuration));
        const uint32_t schoolEnd = schoolStart + kSchoolFadeDuration +
                                   kSchoolHoldDuration;
        if(_elapsed > schoolEnd)
            schoolAlpha *= 1.0f - clamp01(static_cast<float>(_elapsed - schoolEnd) /
                                          static_cast<float>(kExitFadeDuration));
        if(schoolAlpha > 0.0f)
        {
            const float scale = std::min(2.3f, std::max(0.42f,
                                              static_cast<float>(w) / 360.0f));
            drawLargeText("Incheon Science High School", centerX, centerY,
                          scale, Widgets::Color(245, 245, 245,
                              static_cast<uint8_t>(255 * schoolAlpha)));
        }
    }

private:
    void drawLargeText(const char *text, int x, int y, float scale,
                       const Widgets::Color &color)
    {
        glEnable(GL_TEXTURE_2D);
        glPushMatrix();
        glTranslatef(static_cast<GLfloat>(x), static_cast<GLfloat>(y), 0.0f);
        glScalef(scale, scale, 1.0f);
        Widgets::Painter::setColor(color);
        Widgets::Application::font()->draw(text, 0, 0, Widgets::AlignCenter);
        glPopMatrix();
    }

    uint32_t _elapsed;
    uint32_t _lastTick;
};

} // namespace

Game::Game() : _anaman(_manager), _fileRec(_manager) {
	Log::msg("Loading Resources...");
	loadResources();
	Log::msg("Creating Window...");

	addWindow(new MainView(_manager, _anaman, _fileRec));
	addWindow(new StartupIntro());
	createWindow(800,600);
	setWindowTitle("BYB Spike Recorder");

	Log::msg("Starting GUI...");
    roundingDifference = 0;
}

Game::~Game() {
    // Dispose game plugins and windows while RecordingManager and BASS live.
    closeAllWindows();
}


void Game::loadResources() {
	Widgets::TextureGL::load("data/pause.bmp");
	Widgets::TextureGL::load("data/pausehigh.bmp");
	Widgets::TextureGL::load("data/play.bmp");
	Widgets::TextureGL::load("data/playhigh.bmp");

	Widgets::TextureGL::load("data/forward.bmp");
	Widgets::TextureGL::load("data/forwardhigh.bmp");
	Widgets::TextureGL::load("data/backward.bmp");
	Widgets::TextureGL::load("data/backwardhigh.bmp");

	Widgets::TextureGL::load("data/config.bmp");
	Widgets::TextureGL::load("data/confighigh.bmp");
	Widgets::TextureGL::load("data/thresh.bmp");
	Widgets::TextureGL::load("data/threshhigh.bmp");
	Widgets::TextureGL::load("data/ekg.bmp");
	Widgets::TextureGL::load("data/ekghigh.bmp");
	Widgets::TextureGL::load("data/rec.bmp");
	Widgets::TextureGL::load("data/rechigh.bmp");
	Widgets::TextureGL::load("data/file.bmp");
	Widgets::TextureGL::load("data/filehigh.bmp");
	Widgets::TextureGL::load("data/analysis.bmp");
	Widgets::TextureGL::load("data/analysishigh.bmp");
	Widgets::TextureGL::load("data/fft.bmp");
	Widgets::TextureGL::load("data/ffthigh.bmp");

	Widgets::TextureGL::load("data/save.bmp");
	Widgets::TextureGL::load("data/savehigh.bmp");
    
	Widgets::TextureGL::load("data/pin.bmp");
	Widgets::TextureGL::load("data/threshpin.bmp");
	Widgets::TextureGL::load("data/dropdown.bmp");
	Widgets::TextureGL::load("data/gainup.bmp");
	Widgets::TextureGL::load("data/gaindown.bmp");

	Widgets::TextureGL::load("data/ckboxon.bmp");
	Widgets::TextureGL::load("data/ckboxoff.bmp");

	Widgets::TextureGL::load("data/plotview.bmp");
	Widgets::TextureGL::load("data/plotviewhigh.bmp");
	Widgets::TextureGL::load("data/plotviewdown.bmp");
	Widgets::TextureGL::load("data/plotviewdownhigh.bmp");
	Widgets::TextureGL::load("data/disconnected.bmp");
	Widgets::TextureGL::load("data/connected-high.bmp");
    Widgets::TextureGL::load("data/disconnected-high.bmp");
    Widgets::TextureGL::load("data/connected.bmp");

    Widgets::TextureGL::load("data/bdisconnected.bmp");
    Widgets::TextureGL::load("data/bconnected-high.bmp");
    Widgets::TextureGL::load("data/bdisconnected-high.bmp");
    Widgets::TextureGL::load("data/bconnected.bmp");


    Widgets::TextureGL::load("data/update.bmp");
    Widgets::TextureGL::load("data/update-high.bmp");
    Widgets::TextureGL::load("data/bupdate.bmp");
    Widgets::TextureGL::load("data/bupdate-high.bmp");

    Widgets::TextureGL::load("data/usbcon.bmp");
    Widgets::TextureGL::load("data/usbconhigh.bmp");
    Widgets::TextureGL::load("data/usbdiscon.bmp");
    
    Widgets::TextureGL::load("data/neuronprocon.bmp");
    Widgets::TextureGL::load("data/neuronproconhigh.bmp");
    Widgets::TextureGL::load("data/neuronprodiscon.bmp");
    
    Widgets::TextureGL::load("data/emgprocon.bmp");
    Widgets::TextureGL::load("data/emgproconhigh.bmp");
    Widgets::TextureGL::load("data/emgprodiscon.bmp");

    Widgets::TextureGL::load("data/plantcon.bmp");
    Widgets::TextureGL::load("data/plantconhigh.bmp");
    Widgets::TextureGL::load("data/plantdiscon.bmp");

    Widgets::TextureGL::load("data/musclecon.bmp");
    Widgets::TextureGL::load("data/muscleconhigh.bmp");
    Widgets::TextureGL::load("data/musclediscon.bmp");

    
    Widgets::TextureGL::load("data/neuroncon.bmp");
    Widgets::TextureGL::load("data/neurondiscon.bmp");
    
    Widgets::TextureGL::load("data/heartcon.bmp");
    Widgets::TextureGL::load("data/heartconhigh.bmp");
    Widgets::TextureGL::load("data/heartdiscon.bmp");

    Widgets::TextureGL::load("data/conne1.bmp");
    Widgets::TextureGL::load("data/conne2.bmp");
    Widgets::TextureGL::load("data/conne3.bmp");
    Widgets::TextureGL::load("data/conne4.bmp");
    Widgets::TextureGL::load("data/conne5.bmp");
    Widgets::TextureGL::load("data/conne6.bmp");

    Widgets::TextureGL::load("data/dconne1.bmp");
    Widgets::TextureGL::load("data/dconne2.bmp");
    Widgets::TextureGL::load("data/dconne3.bmp");
    Widgets::TextureGL::load("data/dconne4.bmp");
    Widgets::TextureGL::load("data/dconne5.bmp");
    Widgets::TextureGL::load("data/dconne6.bmp");

    Widgets::TextureGL::load("data/connn1.bmp");
    Widgets::TextureGL::load("data/connn2.bmp");
    Widgets::TextureGL::load("data/connn3.bmp");
    Widgets::TextureGL::load("data/connn4.bmp");
    Widgets::TextureGL::load("data/connn5.bmp");
    Widgets::TextureGL::load("data/connn6.bmp");
    
    Widgets::TextureGL::load("data/dconnn1.bmp");
    Widgets::TextureGL::load("data/dconnn2.bmp");
    Widgets::TextureGL::load("data/dconnn3.bmp");
    Widgets::TextureGL::load("data/dconnn4.bmp");
    Widgets::TextureGL::load("data/dconnn5.bmp");
    Widgets::TextureGL::load("data/dconnn6.bmp");

    Widgets::TextureGL::load("data/connm1.bmp");
    Widgets::TextureGL::load("data/connm2.bmp");
    Widgets::TextureGL::load("data/connm3.bmp");
    Widgets::TextureGL::load("data/connm4.bmp");
    Widgets::TextureGL::load("data/connm5.bmp");
    Widgets::TextureGL::load("data/connm6.bmp");

    Widgets::TextureGL::load("data/dconnm1.bmp");
    Widgets::TextureGL::load("data/dconnm2.bmp");
    Widgets::TextureGL::load("data/dconnm3.bmp");
    Widgets::TextureGL::load("data/dconnm4.bmp");
    Widgets::TextureGL::load("data/dconnm5.bmp");
    Widgets::TextureGL::load("data/dconnm6.bmp");

    Widgets::TextureGL::load("data/connp1.bmp");
    Widgets::TextureGL::load("data/connp2.bmp");
    Widgets::TextureGL::load("data/connp3.bmp");
    Widgets::TextureGL::load("data/connp4.bmp");
    Widgets::TextureGL::load("data/connp5.bmp");
    Widgets::TextureGL::load("data/connp6.bmp");

    Widgets::TextureGL::load("data/dconnp1.bmp");
    Widgets::TextureGL::load("data/dconnp2.bmp");
    Widgets::TextureGL::load("data/dconnp3.bmp");
    Widgets::TextureGL::load("data/dconnp4.bmp");
    Widgets::TextureGL::load("data/dconnp5.bmp");
    Widgets::TextureGL::load("data/dconnp6.bmp");

    Widgets::TextureGL::load("data/heart.bmp");
	Widgets::TextureGL::load("data/speaker.bmp");
	Widgets::TextureGL::load("data/speakeroff.bmp");
	Widgets::TextureGL::load("data/bncconn.bmp");
	Widgets::TextureGL::load("data/bncconnhigh.bmp");
	Widgets::TextureGL::load("data/rtimer.bmp");
	Widgets::TextureGL::load("data/rtimerhigh.bmp");
    Widgets::TextureGL::load("data/devbrd.bmp");
    Widgets::TextureGL::load("data/devbrdhigh.bmp");
    Widgets::TextureGL::load("data/okbtn.bmp");
    Widgets::TextureGL::load("data/okbtnhigh.bmp");
    Widgets::TextureGL::load("data/trigger.bmp");
    Widgets::TextureGL::load("data/triggerhigh.bmp");
    Widgets::TextureGL::load("data/e1.bmp");
    Widgets::TextureGL::load("data/e2.bmp");
    Widgets::TextureGL::load("data/e3.bmp");
    Widgets::TextureGL::load("data/e4.bmp");
    Widgets::TextureGL::load("data/e5.bmp");
    Widgets::TextureGL::load("data/e6.bmp");
    Widgets::TextureGL::load("data/e7.bmp");
    Widgets::TextureGL::load("data/e8.bmp");
    Widgets::TextureGL::load("data/e9.bmp");
    Widgets::TextureGL::load("data/e10.bmp");

    Widgets::TextureGL::load("data/analysiscrossed.bmp");
    Widgets::TextureGL::load("data/fftcrossed.bmp");
    Widgets::TextureGL::load("data/threshcrossed.bmp");
    Widgets::TextureGL::load("data/configcrossed.bmp");
    Widgets::TextureGL::load("data/calibratebtn-high.bmp");
    Widgets::TextureGL::load("data/cancel-high.bmp");
    Widgets::TextureGL::load("data/calibratebtn-normal.bmp");
    Widgets::TextureGL::load("data/cancel-normal.bmp");
    Widgets::TextureGL::load("data/okbtn-normal.bmp");
    Widgets::TextureGL::load("data/okbtn-high.bmp");
    
    Widgets::TextureGL::load("data/p300normal.bmp");
    Widgets::TextureGL::load("data/p300high.bmp");
    Widgets::TextureGL::load("data/p300selected.bmp");
    Widgets::TextureGL::load("data/p300audio-normal.bmp");
    Widgets::TextureGL::load("data/p300audio-high.bmp");
    Widgets::TextureGL::load("data/p300audio-selected.bmp");
    
    Widgets::TextureGL::load("data/preset-neuron.bmp");
    Widgets::TextureGL::load("data/preset-plant.bmp");
    Widgets::TextureGL::load("data/preset-eeg.bmp");
    Widgets::TextureGL::load("data/preset-ecg.bmp");
    Widgets::TextureGL::load("data/preset-emg.bmp");
    Widgets::TextureGL::load("data/preset-neuron-high.bmp");
    Widgets::TextureGL::load("data/preset-plant-high.bmp");
    Widgets::TextureGL::load("data/preset-eeg-high.bmp");
    Widgets::TextureGL::load("data/preset-ecg-high.bmp");
    Widgets::TextureGL::load("data/preset-emg-high.bmp");

}

void Game::advance() {
	static uint64_t t = 0; // TODO make this cleaner
	uint64_t newt = SDL_GetTicks();
    double timediff = (newt-t);
   // std::cout<<"Time: "<<timediff<<"\n";
    double sampleNumber = (double)(_manager.sampleRate())*(timediff/1000.0) + roundingDifference;
    double trimmedNumber = (double)(int64_t)sampleNumber;
    roundingDifference = sampleNumber-trimmedNumber;
    _manager.advance((int32_t)sampleNumber); // fetch more samples than necessary to prevent lag
	_fileRec.advance();

	t = newt;
}



} // namespace BackyardBrains
