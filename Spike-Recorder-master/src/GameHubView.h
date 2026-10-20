#ifndef BACKYARDBRAINS_GAMEHUBVIEW_H
#define BACKYARDBRAINS_GAMEHUBVIEW_H

#include "engine/RecordingManager.h"
#include "games/GamePlugin.h"
#include "games/PlayerName.h"
#include "games/InputCalibration.h"
#include "widgets/Widget.h"
#include "widgets/PushButton.h"

#include <stdint.h>
#include <map>
#include <string>
#include <vector>
#include <set>

namespace BackyardBrains {

class GameHubButton : public Widgets::PushButton
{
public:
    explicit GameHubButton(Widgets::Widget *parent = NULL);
private:
    void paintEvent();
};

class GameHubView : public Widgets::Widget
{
public:
    GameHubView(RecordingManager &manager, Widgets::Widget *parent = NULL);
    ~GameHubView();

    void open();
    void toggle();
    void advance();
    void keyPressEvent(Widgets::KeyboardEvent *event);
    void keyReleaseEvent(Widgets::KeyboardEvent *event);
    void mousePressEvent(Widgets::MouseEvent *event);
    void resizeEvent(Widgets::ResizeEvent *event);
    void textInputEvent(const SDL_Event &event);
    void textEditingEvent(const SDL_Event &event);

private:
    friend struct GameCalibrationFixture;
    friend void BYB_GAME_CALL drawPluginImage(void *, const char *, int, int,
                                               int, int);
    friend int BYB_GAME_CALL reportPluginResult(void *, uint64_t, int64_t, int);

    struct GameEntry
    {
        std::string id;
        std::string title;
        std::string description;
        std::string folder;
        std::string module;
        int players;
    };

    void scanGames();
    void startSelectedGame(bool keyboard=false);
    void confirmPlayerName();
    void cancelPlayerName();
    void launchSelectedGame();
    void paintPlayerName();
    void beginCalibration();
    void cancelCalibration();
    void skipCalibration();
    void calibrationAction();
    void finishCalibration(bool keyboardOnly);
    void updateCalibration(uint32_t now);
    void changeCalibrationChannel();
    void paintCalibrationChoice();
    void paintCalibration();
    std::string calibrationSignature(int device) const;
    bool calibrationReleased() const;
    void updateNameInputRect();
    void stopPlugin();
    void select(int index);
    void updateSignal(uint32_t now);
    void updatePlayerSignals();
    void populatePlayerInputs(BYBGameHost &host);
    void paintEvent();

    RecordingManager &_manager;
    std::vector<GameEntry> _games;
    int _selectedGame;
    int _activeGame;
    void *_pluginHandle;
    const BYBGamePlugin *_plugin;
    void *_pluginState;
    uint64_t _lastReportedPluginRun;
    std::string _activeAssetRoot;
    std::map<std::string, unsigned int> _pluginTextures;
    float _signalLevel;
    float _signalBaseline;
    float _triggerLevel;
    float _sensitivity;
    uint32_t _lastFrame;
    uint32_t _lastSensorUpdate;
    uint32_t _calibrationStartedAt;
    uint32_t _lastSignalEdgeAt;
    int64_t _signalPosition;
    uint32_t _signalUpdatedAt;
    int _signalRate;
    int _signalSource;
    bool _sensorAvailable;
    bool _signalAboveThreshold;
    bool _signalEdge;
    bool _baselineReady;
    int _signalDeviceIndex;
    bool _menuOpen;
    std::string _statusMessage;
    Rankings::PlayerNameEntry _nameEntry;
    std::string _lastPlayerName;
    std::string _lastSecondPlayerName;
    std::string _gamePlayerNames[2];
    int _namePlayer;
    float _channelLevel[2];
    float _channelTrigger[2];
    int _channelAvailable[2];
    GameInput::Calibration _inputSetup;
    int _gameDevices[2] = {-1,-1};
    std::string _gameSignatures[2];
    float _setupLevels[2] = {0,0};
    bool _setupAvailable[2] = {false,false};
    bool _profilesReady = false;
    bool _keyboardOnly = false;
    bool _calibrationChoice = false;
    bool _channelLocked = false;
    bool _restoreAutoChannel = true;
    bool _setupEnterReady = true;
    uint32_t _setupLastSample = 0;
    int64_t _setupPosition = -1;
    std::vector<float> _setupHistory;
    std::set<int> _setupKeys;
    bool _sessionUsedFile = false;
};

} // namespace BackyardBrains

#endif
