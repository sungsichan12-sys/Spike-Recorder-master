#include "GameHubView.h"

#include "GameSignal.h"
#include "games/ScoreOutbox.h"
#include "widgets/Application.h"
#include "widgets/BitmapFontGL.h"
#include "widgets/Color.h"
#include "widgets/Painter.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_loadso.h>
#include <SDL_opengl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

namespace BackyardBrains {

namespace {
const uint32_t kSensorSampleWindowMs = 50;
const uint32_t kCalibrationMs = 700;
const uint32_t kSignalCooldownMs = 180;
const int kMenuCardTop = 112;
const int kMenuCardHeight = 72;
const int kMenuCardGap = 0;

float clampFloat(float value, float low, float high)
{
    return std::max(low, std::min(value, high));
}

std::string trim(const std::string &value)
{
    const std::string::size_type first = value.find_first_not_of(" \t\r\n");
    if(first == std::string::npos)
        return std::string();
    const std::string::size_type last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string joinPath(const std::string &left, const std::string &right)
{
    if(left.empty())
        return right;
    const char last = left[left.size() - 1];
    if(last == '/' || last == '\\')
        return left + right;
#ifdef _WIN32
    return left + "\\" + right;
#else
    return left + "/" + right;
#endif
}

bool isDirectory(const std::string &path)
{
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

bool isFile(const std::string &path)
{
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    struct stat info;
    return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
#endif
}

float measureSignalPeak(RecordingManager &manager, int deviceIndex,
                       int64_t endPosition, int sampleCount)
{
    if(deviceIndex < 0 || endPosition <= 0 || sampleCount <= 0)
        return 0.0f;

    std::vector<int16_t> samples(static_cast<size_t>(sampleCount));
    manager.getData(deviceIndex, endPosition - sampleCount,
                    sampleCount, samples.data());

    double mean = 0.0;
    for(size_t i = 0; i < samples.size(); ++i)
        mean += samples[i];
    mean /= static_cast<double>(samples.size());

    float peak = 0.0f;
    for(size_t i = 0; i < samples.size(); ++i)
        peak = std::max(peak,
            static_cast<float>(std::fabs(static_cast<double>(samples[i]) - mean)));
    return peak;
}

void listDirectories(const std::string &root, std::vector<std::string> &result)
{
    if(!isDirectory(root))
        return;
#ifdef _WIN32
    WIN32_FIND_DATAA item;
    const std::string pattern = joinPath(root, "*");
    HANDLE search = FindFirstFileA(pattern.c_str(), &item);
    if(search == INVALID_HANDLE_VALUE)
        return;
    do
    {
        if((item.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
           item.cFileName[0] != '.' && item.cFileName[0] != '\0')
            result.push_back(joinPath(root, item.cFileName));
    } while(FindNextFileA(search, &item));
    FindClose(search);
#else
    DIR *directory = opendir(root.c_str());
    if(!directory)
        return;
    struct dirent *item = NULL;
    while((item = readdir(directory)) != NULL)
    {
        if(item->d_name[0] == '.')
            continue;
        const std::string path = joinPath(root, item->d_name);
        if(isDirectory(path))
            result.push_back(path);
    }
    closedir(directory);
#endif
}

std::map<std::string, std::string> readManifest(const std::string &path)
{
    std::map<std::string, std::string> values;
    std::ifstream input(path.c_str());
    std::string line;
    while(std::getline(input, line))
    {
        line = trim(line);
        if(line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[')
            continue;
        const std::string::size_type split = line.find('=');
        if(split == std::string::npos)
            continue;
        values[trim(line.substr(0, split))] = trim(line.substr(split + 1));
    }
    return values;
}

bool safeModulePath(const std::string &path)
{
    if(path.empty() || path[0] == '/' || path[0] == '\\' ||
       path.find(':') != std::string::npos)
        return false;
    std::string segment;
    for(size_t i = 0; i <= path.size(); ++i)
    {
        const char c = i == path.size() ? '/' : path[i];
        if(c == '/' || c == '\\')
        {
            if(segment == "..")
                return false;
            segment.clear();
        }
        else
            segment += c;
    }
    return true;
}

Widgets::Color unpackColor(uint32_t rgba)
{
    return Widgets::Color(static_cast<uint8_t>((rgba >> 24) & 0xff),
                          static_cast<uint8_t>((rgba >> 16) & 0xff),
                          static_cast<uint8_t>((rgba >> 8) & 0xff),
                          static_cast<uint8_t>(rgba & 0xff));
}

void BYB_GAME_CALL drawPluginRect(void *, int x, int y, int w, int h, uint32_t rgba)
{
    glDisable(GL_TEXTURE_2D);
    Widgets::Painter::setColor(unpackColor(rgba));
    Widgets::Painter::drawRect(Widgets::Rect(x, y, w, h));
    glEnable(GL_TEXTURE_2D);
}

void BYB_GAME_CALL drawPluginLine(void *, int x1, int y1, int x2, int y2,
                                  int thickness, uint32_t rgba)
{
    glDisable(GL_TEXTURE_2D);
    Widgets::Painter::setColor(unpackColor(rgba));
    glLineWidth(static_cast<GLfloat>(std::max(1, thickness)));
    glBegin(GL_LINES);
    glVertex2i(x1, y1);
    glVertex2i(x2, y2);
    glEnd();
    glLineWidth(1.0f);
    glEnable(GL_TEXTURE_2D);
}

void BYB_GAME_CALL drawPluginText(void *, const char *text, int x, int y,
                                  uint32_t rgba, int alignment)
{
    if(!text)
        return;
    const Widgets::Alignment align = alignment == 1 ? Widgets::AlignCenter :
        (alignment == 2 ? Widgets::AlignRight : Widgets::AlignLeft);
    glEnable(GL_TEXTURE_2D);
    Widgets::Painter::setColor(unpackColor(rgba));
    Widgets::Application::font()->draw(text, x, y, align);
}

void BYB_GAME_CALL drawPluginTriangles(void *, const float *xy,
                                      const uint32_t *colors, int count)
{
    if(!xy || !colors || count <= 0 || count > BYB_GAME_MAX_TRIANGLES) return;
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glBegin(GL_TRIANGLES);
    for(int i = 0; i < count; ++i) {
        Widgets::Painter::setColor(unpackColor(colors[i]));
        for(int v = 0; v < 3; ++v)
            glVertex2f(xy[i * 6 + v * 2], xy[i * 6 + v * 2 + 1]);
    }
    glEnd();
    glPopAttrib();
}

void BYB_GAME_CALL drawPluginScaledText(void *,const char *text,float x,float y,
                                       float scale,uint32_t rgba,int alignment)
{
    if(!text || scale<=0 || scale>8) return;
    glEnable(GL_TEXTURE_2D);
    Widgets::Painter::setColor(unpackColor(rgba));
    const Widgets::Alignment align=alignment==1?Widgets::AlignCenter:
        (alignment==2?Widgets::AlignRight:Widgets::AlignLeft);
    glPushMatrix();glTranslatef(x,y,0);glScalef(scale,scale,1);
    Widgets::Application::font()->draw(text,0,0,align);glPopMatrix();
}
void BYB_GAME_CALL drawPluginGradientTriangles(void *,const float *xy,const uint32_t *colors,const float *depth,int count)
{
    if(!xy || !colors || count<=0 || count>BYB_GAME_MAX_TRIANGLES) return;
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LIGHTING_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D); glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    GLint depthBits=0; glGetIntegerv(GL_DEPTH_BITS,&depthBits);
    glDepthMask(GL_TRUE); glDepthFunc(GL_LEQUAL);
    bool useDepth=false;
    glBegin(GL_TRIANGLES);
    for(int i=0;i<count*3;++i) {
        if(i%3==0) {
            const bool next=depth && depthBits>0 && depth[i]>0;
            if(next!=useDepth) {
                glEnd();
                if(next) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
                useDepth=next; glBegin(GL_TRIANGLES);
            }
        }
        Widgets::Painter::setColor(unpackColor(colors[i]));
        glVertex3f(xy[i*2],xy[i*2+1],useDepth?(depth[i]*.6f-1)*99:0);
    }
    glEnd(); glPopAttrib();
}
}

void BYB_GAME_CALL drawPluginImage(void *context, const char *relativePath,
                                   int x, int y, int w, int h)
{
    GameHubView *view = static_cast<GameHubView *>(context);
    if(!view || !relativePath || !*relativePath || w <= 0 || h <= 0)
        return;

    const std::string path = joinPath(view->_activeAssetRoot, relativePath);
    std::map<std::string, unsigned int>::iterator found =
        view->_pluginTextures.find(path);
    if(found == view->_pluginTextures.end())
    {
        SDL_Surface *loaded = IMG_Load(path.c_str());
        if(!loaded)
        {
            std::fprintf(stderr, "Game image could not be loaded: %s (%s)\n",
                         path.c_str(), IMG_GetError());
            view->_pluginTextures[path] = 0;
            return;
        }
        SDL_Surface *rgba = SDL_ConvertSurfaceFormat(
            loaded, SDL_PIXELFORMAT_ABGR8888, 0);
        SDL_FreeSurface(loaded);
        if(!rgba)
        {
            std::fprintf(stderr, "Game image conversion failed: %s (%s)\n",
                         path.c_str(), SDL_GetError());
            view->_pluginTextures[path] = 0;
            return;
        }

        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
        SDL_FreeSurface(rgba);
        view->_pluginTextures[path] = static_cast<unsigned int>(texture);
        found = view->_pluginTextures.find(path);
    }

    if(found == view->_pluginTextures.end() || found->second == 0)
        return;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(found->second));
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2i(x, y);
    glTexCoord2f(0.0f, 1.0f); glVertex2i(x, y + h);
    glTexCoord2f(1.0f, 1.0f); glVertex2i(x + w, y + h);
    glTexCoord2f(1.0f, 0.0f); glVertex2i(x + w, y);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
}

int BYB_GAME_CALL reportPluginResult(void *context,uint64_t runId,int64_t score,int practice)
{
    GameHubView *view=static_cast<GameHubView *>(context);
    if(!view || !runId || view->_activeGame<0 || view->_activeGame>=static_cast<int>(view->_games.size())) return 0;
    if(runId<=view->_lastReportedPluginRun) return 1;
    const bool excluded=practice || view->_manager.fileMode() || view->_sessionUsedFile;
    if(!Rankings::queue(view->_games[view->_activeGame].id,score,excluded,view->_nameEntry.sessionName)) return 0;
    view->_lastReportedPluginRun=runId;
    return 1;
}

GameHubButton::GameHubButton(Widgets::Widget *parent)
    : Widgets::PushButton(parent)
{
    setSizeHint(Widgets::Size(94, 48));
}

void GameHubButton::paintEvent()
{
    glDisable(GL_TEXTURE_2D);
    Widgets::Painter::setColor(Widgets::Colors::border);
    Widgets::Painter::drawRect(Widgets::Rect(0, 12, 1, height() - 24));
    glEnable(GL_TEXTURE_2D);
    Widgets::Painter::setColor(Widgets::Colors::white);
    Widgets::Application::font()->draw("Games [G]", (width() - 5) / 2,
                                       height() / 2, Widgets::AlignCenter);
}

GameHubView::GameHubView(RecordingManager &manager, Widgets::Widget *parent)
    : Widgets::Widget(parent), _manager(manager),
      _selectedGame(0), _activeGame(-1), _pluginHandle(NULL), _plugin(NULL),
      _pluginState(NULL), _lastReportedPluginRun(0), _signalLevel(0.0f), _signalBaseline(0.0f),
      _triggerLevel(100.0f), _sensitivity(1.0f), _lastFrame(0),
      _lastSensorUpdate(0), _calibrationStartedAt(0), _lastSignalEdgeAt(0),
      _signalPosition(-1), _signalUpdatedAt(0), _signalRate(0), _signalSource(0),
      _sensorAvailable(false), _signalAboveThreshold(false), _signalEdge(false),
      _baselineReady(false), _signalDeviceIndex(-1), _menuOpen(false),
      _namePlayer(0)
{
    for(int i=0;i<2;++i) {_channelLevel[i]=0;_channelTrigger[i]=100;_channelAvailable[i]=0;}
    IMG_Init(IMG_INIT_PNG);
    enableFocus();
    _lastPlayerName=Rankings::loadPlayerName(Rankings::playerNamePath());
    setMouseTracking(true);
    setVisible(false);
}

GameHubView::~GameHubView()
{
    cancelCalibration();
    stopPlugin();
    IMG_Quit();
}

void GameHubView::open()
{
    cancelPlayerName();
    cancelCalibration();
    stopPlugin();
    _activeGame = -1;
    _menuOpen = true;
    _statusMessage.clear();
    scanGames();
    _baselineReady = false;
    _signalDeviceIndex = -1;
    _signalAboveThreshold = false;
    _calibrationStartedAt = SDL_GetTicks();
    _lastSensorUpdate = _calibrationStartedAt - kSensorSampleWindowMs;
    _lastFrame = _calibrationStartedAt;
    setVisible(true);
}

void GameHubView::toggle()
{
    if(_nameEntry.open) {cancelPlayerName();return;}
    if(_inputSetup.open()) {cancelCalibration();stopPlugin();_menuOpen=true;_activeGame=-1;return;}
    if(!isVisible())
    {
        open();
        return;
    }

    if(!_menuOpen)
    {
        cancelCalibration();
        stopPlugin();
        _activeGame = -1;
        _menuOpen = true;
        _statusMessage.clear();
    }
    else
    {
        cancelCalibration();
        stopPlugin();
        _menuOpen = false;
        setVisible(false);
    }
}

void GameHubView::scanGames()
{
    _games.clear();
    std::vector<std::string> roots;
    char *basePath = SDL_GetBasePath();
    if(basePath)
    {
        const std::string base(basePath);
        const std::string projectGames = joinPath(base, "../../game");
        const std::string installedGames = joinPath(base, "game");
        // In a source checkout the project game directory is canonical. The
        // executable-side directory is only used for standalone deployments;
        // scanning both would resurrect modules removed from the source tree
        // from an older staged copy.
        roots.push_back(isDirectory(projectGames) ? projectGames : installedGames);
        SDL_free(basePath);
    }

    std::vector<std::string> directories;
    for(size_t rootIndex = 0; rootIndex < roots.size(); ++rootIndex)
        listDirectories(roots[rootIndex], directories);

    for(size_t i = 0; i < directories.size(); ++i)
    {
        const std::map<std::string, std::string> values =
            readManifest(joinPath(directories[i], "game.ini"));
        const std::string id = values.find("id") != values.end() ? values.find("id")->second : "";
        const std::string title = values.find("title") != values.end() ? values.find("title")->second : "";
        const std::string description = values.find("description") != values.end()
            ? values.find("description")->second : "";
        const std::string type = values.find("type") != values.end() ? values.find("type")->second : "plugin";
        const std::string module = values.find("module") != values.end() ? values.find("module")->second : "";
        if(id.empty() || title.empty() || module.empty())
            continue;

        bool duplicate = false;
        for(size_t existing = 0; existing < _games.size(); ++existing)
            if(_games[existing].id == id)
                duplicate = true;
        if(duplicate)
            continue;

        GameEntry entry;
        entry.id = id;
        entry.title = title;
        entry.description = description;
        entry.folder = directories[i];
        entry.module = module;
        entry.players = values.find("players")!=values.end() && values.find("players")->second=="2" ? 2 : 1;
        if(type != "plugin")
            continue;
        if(!safeModulePath(entry.module))
            continue;
        _games.push_back(entry);
    }

    std::sort(_games.begin(), _games.end(),
        [](const GameEntry &a, const GameEntry &b) { return a.title < b.title; });
    if(_games.empty())
        _statusMessage = "No games found. Add a game folder and game.ini under game/.";
    else if(_selectedGame >= static_cast<int>(_games.size()))
        _selectedGame = static_cast<int>(_games.size()) - 1;
    if(_selectedGame < 0)
        _selectedGame = 0;
}

void GameHubView::startSelectedGame(bool keyboard)
{
    if(_selectedGame<0 || _selectedGame>=static_cast<int>(_games.size())) return;
    cancelCalibration();
    _namePlayer=0;
    _gamePlayerNames[0].clear();_gamePlayerNames[1].clear();
    _nameEntry.begin(_lastPlayerName,keyboard);
    _statusMessage.clear();
    Widgets::Application::getInstance()->setFocusWidget(this,true);
    updateNameInputRect();
}

void GameHubView::cancelPlayerName()
{
    if(!_nameEntry.open) return;
    const bool composing=!_nameEntry.composition.empty();
    _nameEntry.cancel();
    if(composing) {SDL_StopTextInput();SDL_StartTextInput();}
    Widgets::Application::getInstance()->setFocusWidget(NULL);
}

void GameHubView::confirmPlayerName()
{
    if(!_nameEntry.confirm()) return;
    _gamePlayerNames[_namePlayer]=_nameEntry.sessionName;
    if(_namePlayer==0) {
        _lastPlayerName=_nameEntry.sessionName;
        Rankings::savePlayerName(Rankings::playerNamePath(),_lastPlayerName);
        if(_games[static_cast<size_t>(_selectedGame)].players==2) {
            _namePlayer=1;
            _nameEntry.begin(_lastSecondPlayerName,true);
            updateNameInputRect();
            return;
        }
    } else _lastSecondPlayerName=_nameEntry.sessionName;
    _nameEntry.sessionName=_gamePlayerNames[0];
    Widgets::Application::getInstance()->setFocusWidget(NULL);
    beginCalibration();
    _calibrationChoice=true;
}

void GameHubView::textInputEvent(const SDL_Event &event)
{
    if(_nameEntry.open) _nameEntry.insert(event.text.text);
}

void GameHubView::textEditingEvent(const SDL_Event &event)
{
    if(_nameEntry.open) _nameEntry.composition=event.edit.text;
}

void GameHubView::updateNameInputRect()
{
    if(!_nameEntry.open) return;
    const int panelWidth=std::max(160,std::min(560,width()-40));
    const Widgets::Point global=mapToGlobal(Widgets::Point((width()-panelWidth)/2+28,height()/2-20));
    SDL_Rect input={global.x,global.y,panelWidth-56,44};
    SDL_SetTextInputRect(&input);
}

void GameHubView::launchSelectedGame()
{
    if(_selectedGame < 0 || _selectedGame >= static_cast<int>(_games.size()))
        return;

    stopPlugin();
    _statusMessage.clear();
    const GameEntry &entry = _games[static_cast<size_t>(_selectedGame)];
    if(!isFile(joinPath(entry.folder, entry.module)))
    {
        _statusMessage = "Game module is missing: " + entry.module;
        return;
    }

    _pluginHandle = SDL_LoadObject(joinPath(entry.folder, entry.module).c_str());
    if(!_pluginHandle)
    {
        _statusMessage = std::string("Could not load game module: ") + SDL_GetError();
        return;
    }

    BYBGetGamePluginFn getPlugin = reinterpret_cast<BYBGetGamePluginFn>(
        SDL_LoadFunction(_pluginHandle, BYB_GAME_PLUGIN_ENTRY));
    if(!getPlugin)
    {
        _statusMessage = "Module does not export BYB_GetGamePlugin.";
        stopPlugin();
        return;
    }

    _plugin = getPlugin();
    if(!_plugin || _plugin->api_version < 1 ||
       _plugin->api_version > BYB_GAME_PLUGIN_API_VERSION ||
       !_plugin->id || entry.id != _plugin->id || !_plugin->create ||
       !_plugin->destroy || !_plugin->update || !_plugin->render)
    {
        _statusMessage = "Game module API or manifest id is invalid.";
        stopPlugin();
        return;
    }

    _pluginState = _plugin->create();
    if(!_pluginState)
    {
        _statusMessage = "Game module could not be started.";
        stopPlugin();
        return;
    }
    _activeGame = _selectedGame;
    _sessionUsedFile=_manager.fileMode();
    _activeAssetRoot = entry.folder;
    _menuOpen = false;
    _lastFrame = SDL_GetTicks();
}

void GameHubView::stopPlugin()
{
    _lastReportedPluginRun=0;
    if(_plugin && _pluginState && _plugin->destroy)
        _plugin->destroy(_pluginState);
    _pluginState = NULL;
    _plugin = NULL;
    if(_pluginHandle)
        SDL_UnloadObject(_pluginHandle);
    _pluginHandle = NULL;
    _activeAssetRoot.clear();
    for(std::map<std::string, unsigned int>::const_iterator it =
            _pluginTextures.begin(); it != _pluginTextures.end(); ++it)
    {
        if(it->second != 0)
        {
            const GLuint texture = static_cast<GLuint>(it->second);
            glDeleteTextures(1, &texture);
        }
    }
    _pluginTextures.clear();
}

void GameHubView::select(int index)
{
    if(_games.empty())
    {
        _selectedGame = 0;
        return;
    }
    const int count = static_cast<int>(_games.size());
    _selectedGame = (index % count + count) % count;
}

void GameHubView::updateSignal(uint32_t now)
{
    _signalEdge = false;
    if(_keyboardOnly) {
        _sensorAvailable=false;_signalLevel=0;
        for(int i=0;i<2;++i){_channelAvailable[i]=0;_channelLevel[i]=0;}
        return;
    }
    if(_inputSetup.open())return;
    if(_profilesReady) {
        const int64_t position=_manager.pos();
        bool valid=!_manager.paused() && position>0;
        for(int p=0;p<_inputSetup.players;++p)
            valid=valid && !_gameSignatures[p].empty() && _gameSignatures[p]==calibrationSignature(_gameDevices[p]);
        if(!valid || position<_signalPosition || (position==_signalPosition && now-_signalUpdatedAt>=500)) {
            _sessionUsedFile=_sessionUsedFile || _manager.fileMode();
            beginCalibration();_inputSetup.error="입력이 바뀌거나 끊겨 게임을 일시정지했습니다. 다시 설정하세요.";return;
        }
        if(position==_signalPosition)return;
        _signalPosition=position;_signalUpdatedAt=now;
        const int count=static_cast<int>(std::min<int64_t>(position,std::max(1,_manager.sampleRate()/20)));
        for(int p=0;p<_inputSetup.players;++p) {
            const float raw=measureSignalPeak(_manager,_gameDevices[p],position,count);
            const auto value=_inputSetup.profiles[p].sample(raw,now,true,_sensitivity);
            _channelLevel[p]=value.level;_channelTrigger[p]=_inputSetup.profiles[p].threshold(_sensitivity);_channelAvailable[p]=1;
            if(p==0){_signalLevel=value.level;_triggerLevel=_channelTrigger[p];_signalEdge=value.edge;}
        }
        _sensorAvailable=true;return;
    }
    const std::vector<RecordingManager::VirtualDevice> &devices =
        _manager.virtualDevices();
    int deviceIndex = _manager.signalDeviceIndex();
    if(deviceIndex < 0 || deviceIndex >= static_cast<int>(devices.size()) ||
       !devices[static_cast<size_t>(deviceIndex)].bound)
    {
        deviceIndex = -1;
        for(size_t i = 0; i < devices.size(); ++i)
        {
            if(devices[i].bound)
            {
                deviceIndex = static_cast<int>(i);
                break;
            }
        }
    }

    const int sampleRate = _manager.sampleRate();
    const int64_t endPosition = _manager.pos();
    const int source = _manager.fileMode() ? 3 : ((_manager.serialMode() || _manager.hidMode()) ? 2 : 1);
    if(sampleRate != _signalRate || source != _signalSource || endPosition < _signalPosition) {
        _baselineReady = false;
        _signalAboveThreshold = false;
        _signalPosition = -1;
    }
    _signalRate = sampleRate;
    _signalSource = source;
    const bool fresh = endPosition != _signalPosition;
    if(deviceIndex < 0 || sampleRate <= 0 || endPosition <= 0 || _manager.paused() ||
       (!fresh && now - _signalUpdatedAt >= 500))
    {
        if(_sensorAvailable)
            _baselineReady = false;
        _sensorAvailable = false;
        _signalLevel = 0.0f;
        _signalAboveThreshold = false;
        _signalDeviceIndex = -1;
        _triggerLevel = 100.0f / _sensitivity;
        return;
    }
    if(!fresh) return;
    _signalPosition = endPosition;
    _signalUpdatedAt = now;

    int sampleCount = std::max(1, sampleRate * static_cast<int>(kSensorSampleWindowMs) / 1000);
    sampleCount = std::min(sampleCount, 5000);
    if(endPosition < sampleCount)
        sampleCount = static_cast<int>(endPosition);
    if(sampleCount <= 0)
        return;

    float peak = measureSignalPeak(_manager, deviceIndex, endPosition, sampleCount);

    if(deviceIndex != _signalDeviceIndex)
    {
        _signalDeviceIndex = deviceIndex;
        _baselineReady = false;
        _signalAboveThreshold = false;
    }
    _sensorAvailable = true;
    _signalLevel = peak;
    const int64_t configured = std::max<int64_t>(20,
        std::min<int64_t>(32768, std::llabs(static_cast<int64_t>(
            devices[static_cast<size_t>(deviceIndex)].threshold))));
    if(!_baselineReady)
    {
        _signalBaseline = _signalLevel;
        _baselineReady = true;
        _calibrationStartedAt = now;
    }
    else if(now - _calibrationStartedAt < kCalibrationMs)
        _signalBaseline = _signalBaseline * 0.75f + _signalLevel * 0.25f;

    _triggerLevel = GameSignal::triggerLevel(static_cast<int>(configured),
                                             _signalBaseline, _sensitivity);
    if(now - _calibrationStartedAt < kCalibrationMs)
    {
        _signalAboveThreshold = false;
        return;
    }

    const bool above = _signalLevel >= _triggerLevel;
    if(!above)
        _signalBaseline = _signalBaseline * 0.96f + _signalLevel * 0.04f;
    if(above && !_signalAboveThreshold &&
       now - _lastSignalEdgeAt >= kSignalCooldownMs)
    {
        _signalEdge = true;
        _lastSignalEdgeAt = now;
    }
    if(above) _signalAboveThreshold = true;
    else if(_signalLevel < _triggerLevel * .75f) _signalAboveThreshold = false;
}

void GameHubView::updatePlayerSignals()
{
    if(_profilesReady || _keyboardOnly || _inputSetup.open())return;
    for(int i=0;i<2;++i) {_channelLevel[i]=0;_channelTrigger[i]=100;_channelAvailable[i]=0;}
    if(_activeGame<0 || _activeGame>=static_cast<int>(_games.size()) ||
       _games[static_cast<size_t>(_activeGame)].players<2 || !_sensorAvailable)
        return;
    const int rate=_manager.sampleRate();
    const int64_t position=_manager.pos();
    if(rate<=0 || position<=0 || _manager.paused()) return;
    const int count=static_cast<int>(std::min<int64_t>(position,std::min(5000,std::max(1,rate/20))));
    const std::vector<RecordingManager::VirtualDevice> &devices=_manager.virtualDevices();
    int player=0;
    for(size_t device=0;device<devices.size() && player<2;++device) {
        if(!devices[device].bound) continue;
        _channelLevel[player]=measureSignalPeak(_manager,static_cast<int>(device),position,count);
        const int configured=static_cast<int>(std::max<int64_t>(20,std::min<int64_t>(32768,
            std::llabs(static_cast<int64_t>(devices[device].threshold)))));
        _channelTrigger[player]=GameSignal::triggerLevel(configured,0,_sensitivity);
        _channelAvailable[player]=1;
        ++player;
    }
}

void GameHubView::populatePlayerInputs(BYBGameHost &host)
{
    host.draw_scaled_text=drawPluginScaledText;
    host.input_sensitivity=_sensitivity;
    host.player_count=_activeGame>=0 && _activeGame<static_cast<int>(_games.size()) ?
        _games[static_cast<size_t>(_activeGame)].players:1;
    for(int i=0;i<2;++i) {
        host.player_names[i]=_gamePlayerNames[i].empty()?NULL:_gamePlayerNames[i].c_str();
        host.channel_signal_level[i]=_channelLevel[i];
        host.channel_trigger_level[i]=_channelTrigger[i];
        host.channel_signal_available[i]=_channelAvailable[i];
    }
}

void GameHubView::advance()
{
    if(!isVisible())
        return;
    const uint32_t now = SDL_GetTicks();
    float delta = static_cast<float>(now - _lastFrame) / 1000.0f;
    _lastFrame = now;
    delta = std::max(0.0f, delta);
    if(now - _lastSensorUpdate >= kSensorSampleWindowMs)
    {
        _lastSensorUpdate = now;
        updateSignal(now);
        updatePlayerSignals();
    }
    else
        _signalEdge = false;

    if(_inputSetup.open()) {updateCalibration(now);return;}

    if(_plugin && _pluginState && !_menuOpen)
    {
        BYBGameHost host = {};
        host.width = width();
        host.height = height();
        host.delta_seconds = delta;
        host.signal_level = _signalLevel;
        host.trigger_level = _triggerLevel;
        host.signal_available = _sensorAvailable ? 1 : 0;
        host.signal_edge = _signalEdge ? 1 : 0;
        host.draw_context = this;
        host.fill_rect = drawPluginRect;
        host.draw_line = drawPluginLine;
        host.draw_text = drawPluginText;
        host.asset_root = _activeAssetRoot.c_str();
        host.draw_image = drawPluginImage;
        host.draw_triangles = drawPluginTriangles;
        host.draw_gradient_triangles = drawPluginGradientTriangles;
        host.report_result = reportPluginResult;
        host.signal_source = !_sensorAvailable ? 0 : (_manager.fileMode() ? 3 :
            ((_manager.serialMode() || _manager.hidMode()) ? 2 : 1));
        populatePlayerInputs(host);
        _plugin->update(_pluginState, &host);
    }
}

void GameHubView::keyPressEvent(Widgets::KeyboardEvent *event)
{
    const int key = static_cast<int>(event->key());
    if(!_inputSetup.open() && _setupKeys.count(key)){event->accept();return;}
    if(_calibrationChoice) {
        _setupKeys.insert(key);
        if(key==SDLK_ESCAPE || key==SDLK_g)toggle();
        else if(key==SDLK_RETURN || key==SDLK_KP_ENTER || key==SDLK_c) {
            _calibrationChoice=false;
            _setupEnterReady=false;
        } else if(key==SDLK_s)skipCalibration();
        else if(key==SDLK_k)finishCalibration(true);
        event->accept();return;
    }
    if(_inputSetup.open()) {
        _setupKeys.insert(key);
        if(key==SDLK_ESCAPE || key==SDLK_g)toggle();
        else if((key==SDLK_RETURN || key==SDLK_KP_ENTER) && _setupEnterReady) {_setupEnterReady=false;calibrationAction();}
        else if(key==SDLK_r) {_inputSetup.retry();_setupHistory.clear();}
        else if(key==SDLK_k)finishCalibration(true);
        else if(key==SDLK_TAB)changeCalibrationChannel();
        else if(key==SDLK_UP || key==SDLK_RIGHTBRACKET)_inputSetup.adjust(.05f);
        else if(key==SDLK_DOWN || key==SDLK_LEFTBRACKET)_inputSetup.adjust(-.05f);
        event->accept();return;
    }
    if(_nameEntry.open) {
        // Consume shortcuts here: typing G or a space must never leave the
        // prompt or trigger a jump. SDL_TEXTINPUT supplies committed IME text.
        if(key==SDLK_ESCAPE) cancelPlayerName();
        else if(key==SDLK_RETURN || key==SDLK_KP_ENTER) {
            confirmPlayerName();
            if(_inputSetup.open()){_setupEnterReady=false;_setupKeys.insert(key);}
        }
        else if(_nameEntry.composition.empty()) {
            if((event->mod() & KMOD_CTRL) && key==SDLK_a) _nameEntry.selected=true;
            else if((event->mod() & KMOD_CTRL) && key==SDLK_c && _nameEntry.selected)
                SDL_SetClipboardText(_nameEntry.text.c_str());
            else if((event->mod() & KMOD_CTRL) && key==SDLK_v) {
                char *value=SDL_GetClipboardText();
                if(value) {_nameEntry.insert(value);SDL_free(value);}
            }
            else if(key==SDLK_BACKSPACE) _nameEntry.erase(true);
            else if(key==SDLK_DELETE) _nameEntry.erase(false);
            else if(key==SDLK_LEFT) _nameEntry.move(-1);
            else if(key==SDLK_RIGHT) _nameEntry.move(1);
            else if(key==SDLK_HOME) {_nameEntry.cursor=0;_nameEntry.selected=false;}
            else if(key==SDLK_END) {_nameEntry.cursor=_nameEntry.text.size();_nameEntry.selected=false;}
        }
        event->accept();return;
    }
    if(key == SDLK_g)
    {
        toggle();
        event->accept();
        return;
    }

    if(!_menuOpen && _plugin && _pluginState)
    {
        if(key == SDLK_ESCAPE || key == SDLK_BACKSPACE)
        {
            cancelCalibration();
            stopPlugin();
            _activeGame = -1;
            _menuOpen = true;
        }
        else if(key == SDLK_LEFTBRACKET)
            _sensitivity = std::min(4.0f, _sensitivity * 1.25f);
        else if(key == SDLK_RIGHTBRACKET)
            _sensitivity = std::max(.25f, _sensitivity * .8f);
        else if(_plugin->key_event)
            _plugin->key_event(_pluginState, key, 1);
        event->accept();
        return;
    }

    if(_menuOpen)
    {
        if(key == SDLK_ESCAPE || key == SDLK_BACKSPACE)
        {
            toggle();
            event->accept();
        }
        else if(key == SDLK_UP)
        {
            select(_selectedGame - 1);
            event->accept();
        }
        else if(key == SDLK_DOWN)
        {
            select(_selectedGame + 1);
            event->accept();
        }
        else if(key == SDLK_RETURN || key == SDLK_KP_ENTER)
        {
            startSelectedGame(true);
            event->accept();
        }
        else if(key == SDLK_LEFTBRACKET)
        {
            _sensitivity = std::min(4.0f, _sensitivity * 1.25f);
            event->accept();
        }
        else if(key == SDLK_RIGHTBRACKET)
        {
            _sensitivity = std::max(0.25f, _sensitivity * 0.8f);
            event->accept();
        }
    }
}

void GameHubView::keyReleaseEvent(Widgets::KeyboardEvent *event)
{
    const int releasedKey=static_cast<int>(event->key());
    const bool consumed=_setupKeys.erase(releasedKey)>0;
    if(releasedKey==SDLK_RETURN || releasedKey==SDLK_KP_ENTER)_setupEnterReady=true;
    if(consumed && !_inputSetup.open()){event->accept();return;}
    if(_inputSetup.open()) {
        const int key=static_cast<int>(event->key());
        if(key==SDLK_RETURN || key==SDLK_KP_ENTER)_setupEnterReady=true;
        if(_plugin && _pluginState && _plugin->key_event)_plugin->key_event(_pluginState,key,0);
        event->accept();return;
    }
    if(_nameEntry.open) {
        const int key=static_cast<int>(event->key());
        if(key==SDLK_RETURN || key==SDLK_KP_ENTER) _nameEntry.enterReady=true;
        event->accept();return;
    }
    if(isVisible() && !_menuOpen && _plugin && _pluginState && _plugin->key_event)
    {
        _plugin->key_event(_pluginState, static_cast<int>(event->key()), 0);
        event->accept();
    }
}

void GameHubView::mousePressEvent(Widgets::MouseEvent *event)
{
    if(_calibrationChoice) {
        if(event->button()==Widgets::LeftButton) {
            const int panelWidth=std::max(220,std::min(560,width()-40));
            const int x=(width()-panelWidth)/2,y=height()/2-170;
            const int px=event->pos().x,py=event->pos().y;
            if(py>=y+248 && py<y+288) {
                if(px>=x+panelWidth-188 && px<x+panelWidth-24) {
                    _calibrationChoice=false;_setupEnterReady=true;
                } else if(px>=x+24 && px<x+156)toggle();
            } else if(py>=y+196 && py<y+236)skipCalibration();
            else if(py>=y+148 && py<y+188)finishCalibration(true);
        }
        event->accept();return;
    }
    if(_inputSetup.open()) {
        if(event->button()==Widgets::LeftButton) {
            const int pw=std::max(220,std::min(640,width()-32)),ph=std::max(360,std::min(520,height()-24));
            const int x=(width()-pw)/2,y=(height()-ph)/2,px=event->pos().x,py=event->pos().y;
            if(px>=x+pw-88 && py>=y+10 && py<y+46)toggle();
            else if(py>=y+ph-64 && py<y+ph-20) {
                if(px>=x+pw-184 && px<x+pw-24)calibrationAction();
                else if(px>=x+24 && px<x+156) {_inputSetup.retry();_setupHistory.clear();}
            } else if(px>=x+24 && px<x+pw-24 && py>=y+ph-102 && py<y+ph-70)finishCalibration(true);
            else if(px>=x+pw-160 && px<x+pw-24 && py>=y+90 && py<y+126)changeCalibrationChannel();
            else if(_inputSetup.stage==GameInput::Calibration::Review && py>=y+ph-180 && py<y+ph-142) {
                if(px>=x+pw-108)_inputSetup.adjust(.05f);
                else if(px>=x+pw-156)_inputSetup.adjust(-.05f);
            }
        }
        event->accept();return;
    }
    if(event->button() != Widgets::LeftButton) {
        if(!_nameEntry.open && !_menuOpen && _plugin && _pluginState && _plugin->mouse_event) {
            _plugin->mouse_event(_pluginState,event->pos().x,event->pos().y,static_cast<int>(event->button()));
            event->accept();
        }
        return;
    }

    if(_nameEntry.open) {
        const int panelWidth=std::max(160,std::min(560,width()-40));
        const int x=(width()-panelWidth)/2,y=height()/2-160;
        const int px=event->pos().x,py=event->pos().y;
        if(py>=y+248 && py<y+288) {
            if(px>=x+panelWidth-160 && px<x+panelWidth-28) {
                _nameEntry.enterReady=true;confirmPlayerName();
            } else if(px>=x+28 && px<x+140) cancelPlayerName();
        } else if(px>=x+28 && px<x+panelWidth-28 && py>=y+140 && py<y+184)
            _nameEntry.selected=true;
        event->accept();return;
    }

    if(!_menuOpen && event->pos().x >= width() - 154 && event->pos().y <= 62)
    {
        cancelCalibration();
        stopPlugin();
        _activeGame = -1;
        _menuOpen = true;
        event->accept();
        return;
    }

    if(_menuOpen)
    {
        if(event->pos().x >= width() - 142 && event->pos().y <= 58)
            toggle();
        else if(!_games.empty() && event->pos().y >= kMenuCardTop)
        {
            const int offset = event->pos().y - kMenuCardTop;
            const int localIndex = offset / (kMenuCardHeight + kMenuCardGap);
            const int visibleCount = std::max(1,
                (height() - kMenuCardTop - 76) / (kMenuCardHeight + kMenuCardGap));
            const int firstVisible = std::max(0,
                std::min(_selectedGame, static_cast<int>(_games.size()) - visibleCount));
            const int clicked = firstVisible + localIndex;
            if(event->pos().x >= 46 && event->pos().x < width() - 46 &&
               offset % (kMenuCardHeight + kMenuCardGap) < kMenuCardHeight &&
               localIndex >= 0 && localIndex < visibleCount &&
               clicked < static_cast<int>(_games.size()))
            {
                _selectedGame = clicked;
                startSelectedGame();
            }
        }
    }
    else if(_plugin && _pluginState && _plugin->mouse_event)
        _plugin->mouse_event(_pluginState, event->pos().x, event->pos().y,
                             static_cast<int>(event->button()));

    event->accept();
}

void GameHubView::resizeEvent(Widgets::ResizeEvent *event)
{
    Widgets::Widget::resizeEvent(event);
    updateNameInputRect();
}

std::string GameHubView::calibrationSignature(int device) const
{
    const auto &devices=_manager.virtualDevices();
    if(device<0 || device>=static_cast<int>(devices.size()) || !devices[device].bound)return "";
    std::ostringstream signature;
    signature<<device<<'|'<<devices[device].name<<'|'<<_manager.getCurrentInputType()<<'|'<<_manager.sampleRate()
        <<'|'<<_manager.fileMode()<<'|'<<_manager.hidMode()<<'|'<<_manager.serialMode()
        <<'|'<<_manager.highPassFilterEnabled()<<':'<<_manager.highCornerFrequency()
        <<'|'<<_manager.lowPassFilterEnabled()<<':'<<_manager.lowCornerFrequency()
        <<'|'<<_manager.fiftyHzFilterEnabled()<<'|'<<_manager.sixtyHzFilterEnabled();
    if(_manager.serialMode())signature<<'|'<<_manager.getCurrentPort().portName;
    if(_manager.hidMode())signature<<'|'<<_manager.currentlyConnectedHIDBoardType();
    return signature.str();
}

void GameHubView::beginCalibration()
{
    _calibrationChoice=false;
    if(!_channelLocked){_restoreAutoChannel=_manager.autoSignalChannel();_channelLocked=true;}
    const int primary=_manager.signalDeviceIndex();
    _manager.setAutoSignalChannel(false);
    const int players=_selectedGame>=0 && _selectedGame<static_cast<int>(_games.size())?_games[_selectedGame].players:1;
    _inputSetup.begin(players);_profilesReady=false;_keyboardOnly=false;
    _sensitivity=1;_signalEdge=false;_signalLevel=0;_sensorAvailable=false;
    _gameDevices[0]=primary;_gameDevices[1]=-1;
    if(players==2){
        int player=0;const auto &devices=_manager.virtualDevices();
        for(size_t i=0;i<devices.size() && player<2;++i)if(devices[i].bound)_gameDevices[player++]=static_cast<int>(i);
        if(player<2)_gameDevices[1]=-1;
    }
    for(int i=0;i<2;++i){
        _gameSignatures[i]=calibrationSignature(_gameDevices[i]);
        _setupLevels[i]=0;_setupAvailable[i]=false;_channelLevel[i]=0;_channelAvailable[i]=0;
    }
    _setupPosition=-1;_setupLastSample=SDL_GetTicks()-50;_setupEnterReady=true;_setupHistory.clear();
    Widgets::Application::getInstance()->setFocusWidget(this,true);
}

void GameHubView::cancelCalibration()
{
    if(_channelLocked){_manager.setAutoSignalChannel(_restoreAutoChannel);_channelLocked=false;}
    _inputSetup=GameInput::Calibration();_profilesReady=false;_keyboardOnly=false;_calibrationChoice=false;_signalEdge=false;
    _setupHistory.clear();
    Widgets::Application::getInstance()->setFocusWidget(NULL);
}

void GameHubView::skipCalibration()
{
    _calibrationChoice=false;
    _inputSetup=GameInput::Calibration();
    if(_channelLocked){_manager.setAutoSignalChannel(_restoreAutoChannel);_channelLocked=false;}
    _profilesReady=false;_keyboardOnly=false;_signalEdge=false;_baselineReady=false;
    _signalPosition=-1;_signalAboveThreshold=false;_signalLevel=0;_sensorAvailable=false;
    for(int i=0;i<2;++i){_channelLevel[i]=0;_channelTrigger[i]=100;_channelAvailable[i]=0;}
    Widgets::Application::getInstance()->setFocusWidget(NULL);
    if(_plugin && _pluginState && !_menuOpen)_lastFrame=SDL_GetTicks();
    else launchSelectedGame();
}

bool GameHubView::calibrationReleased() const
{
    if(!_inputSetup.quiet)return false;
    const uint32_t now=SDL_GetTicks();
    for(int p=0;p<_inputSetup.player;++p)
        if(!_setupAvailable[p] || !_inputSetup.profiles[p].released || now-_inputSetup.profiles[p].released<500)return false;
    return true;
}

void GameHubView::calibrationAction()
{
    if(!_inputSetup.open())return;
    if(_inputSetup.stage==GameInput::Calibration::Review) {
        if(!calibrationReleased())return;
        if(_inputSetup.confirm())finishCalibration(false);
        else {_setupHistory.clear();_setupPosition=-1;}
    } else if(!_inputSetup.measuring() && _setupAvailable[_inputSetup.player]) {
        _inputSetup.start(SDL_GetTicks());_setupHistory.clear();
    }
}

void GameHubView::finishCalibration(bool keyboardOnly)
{
    _calibrationChoice=false;
    _keyboardOnly=keyboardOnly;_profilesReady=!keyboardOnly;
    _inputSetup.stage=GameInput::Calibration::Closed;
    _signalPosition=_manager.pos();_signalUpdatedAt=SDL_GetTicks();_signalEdge=false;_signalLevel=0;
    for(int i=0;i<2;++i){_channelLevel[i]=0;_channelAvailable[i]=0;}
    Widgets::Application::getInstance()->setFocusWidget(NULL);
    if(_plugin && _pluginState && !_menuOpen)_lastFrame=SDL_GetTicks();
    else launchSelectedGame();
}

void GameHubView::updateCalibration(uint32_t now)
{
    if(!_inputSetup.open() || now-_setupLastSample<50)return;
    _setupLastSample=now;
    if(_inputSetup.stage==GameInput::Calibration::RestReady) {
        int bound=0;for(const auto &device:_manager.virtualDevices())if(device.bound)++bound;
        if(bound>=_inputSetup.players && (_gameDevices[0]<0 || (_inputSetup.players==2 && _gameDevices[1]<0))) {beginCalibration();return;}
    }
    const int64_t position=_manager.pos();
    const bool fresh=position>0 && position!=_setupPosition;
    if(!fresh) {
        if(_manager.paused() || now-_signalUpdatedAt>=500){
            for(int p=0;p<2;++p)_setupAvailable[p]=false;
            _inputSetup.feed(0,now,false);
        }
        return;
    }
    if(position<_setupPosition){beginCalibration();return;}
    _setupPosition=position;_signalUpdatedAt=now;
    const int count=static_cast<int>(std::min<int64_t>(position,std::max(1,_manager.sampleRate()/20)));
    for(int p=0;p<_inputSetup.players;++p) {
        const std::string signature=calibrationSignature(_gameDevices[p]);
        if(signature!=_gameSignatures[p]){beginCalibration();return;}
        _setupAvailable[p]=!signature.empty() && !_manager.paused();
        _setupLevels[p]=_setupAvailable[p]?measureSignalPeak(_manager,_gameDevices[p],position,count):0;
        if(p<_inputSetup.player)_inputSetup.profiles[p].sample(_setupLevels[p],now,_setupAvailable[p]);
    }
    const int player=_inputSetup.player;
    _inputSetup.feed(_setupLevels[player],now,_setupAvailable[player]);
    _setupHistory.push_back(_setupLevels[player]);
    if(_setupHistory.size()>120)_setupHistory.erase(_setupHistory.begin());
}

void GameHubView::changeCalibrationChannel()
{
    if(_inputSetup.players!=1 || _inputSetup.measuring() || _inputSetup.stage==GameInput::Calibration::Review)return;
    const auto &devices=_manager.virtualDevices();
    if(devices.empty())return;
    for(size_t step=1;step<=devices.size();++step) {
        const int device=(_gameDevices[0]+static_cast<int>(step))%static_cast<int>(devices.size());
        if(device>=0 && devices[device].bound) {
            _gameDevices[0]=device;_manager.setSelectedVDevice(device);
            _gameSignatures[0]=calibrationSignature(device);_inputSetup.retry();
            _setupHistory.clear();_setupPosition=-1;return;
        }
    }
}

void GameHubView::paintCalibration()
{
    using namespace Widgets;
    const auto *font=Application::font();
    const int pw=std::max(220,std::min(640,width()-32)),ph=std::max(360,std::min(520,height()-24));
    const int x=(width()-pw)/2,y=(height()-ph)/2;
    const int player=_inputSetup.player;
    const auto &profile=_inputSetup.profiles[player];
    const auto stage=_inputSetup.stage;
    const bool review=stage==GameInput::Calibration::Review;
    const bool microphone=!_manager.serialMode() && !_manager.hidMode() && !_manager.fileMode();
    const bool canAct=review?calibrationReleased():(!_inputSetup.measuring() && _setupAvailable[player]);
    glDisable(GL_TEXTURE_2D);Painter::setColor(Colors::background);Painter::drawRect(rect());
    Painter::setColor(Colors::border);Painter::drawRoundedRect(Rect(x,y,pw,ph),8);
    Painter::setColor(Colors::widgetbgdark);Painter::drawRoundedRect(Rect(x+1,y+1,pw-2,ph-2),7);
    glEnable(GL_TEXTURE_2D);Painter::setColor(Colors::white);
    std::string title="게임 입력 기준선";
    if(_inputSetup.players==2)title+=" / "+std::to_string(player+1)+"P "+_gamePlayerNames[player];
    font->draw(title.c_str(),x+24,y+24);
    Painter::setColor(Colors::mutedText);font->draw("취소 Esc",x+pw-24,y+24,AlignRight);
    const char *heading=stage==GameInput::Calibration::RestReady?"1. 먼저 힘을 빼고 편하게 쉬세요":
        stage==GameInput::Calibration::Rest?(microphone?"주변 소음 측정 중 · 조용히 있어 주세요":"휴식 신호 측정 중 · 움직이지 마세요"):
        stage==GameInput::Calibration::EffortReady?(microphone?"2. 게임에서 낼 소리를 준비하세요":"2. 게임에서 사용할 만큼 힘을 주세요"):
        stage==GameInput::Calibration::Effort?(microphone?"소리 측정 중 · 같은 크기로 소리를 내세요":"힘주기 측정 중 · 같은 힘을 유지하세요"):
        review?"3. 힘을 빼고 기준선을 확인하세요":"입력 상태를 확인해 주세요";
    Painter::setColor(Colors::white);font->draw(heading,x+24,y+66);
    Painter::setColor(Colors::mutedText);
    std::string source=_manager.fileMode()?"파일 입력":(_manager.hidMode()||_manager.serialMode())?"센서":"마이크";
    source+=" / Ch "+std::to_string(_gameDevices[player]+1);
    std::string detail=_setupAvailable[player]?source:"신호가 없습니다. 연결·일시정지 상태를 확인하세요.";
    font->draw(detail.c_str(),x+24,y+100);
    if(_inputSetup.players==1 && !_inputSetup.measuring() && !review)font->draw("채널 변경 Tab",x+pw-24,y+100,AlignRight);
    const int left=x+24,right=x+pw-24,top=y+142,bottom=y+ph-194;
    float maximum=std::max(100.f,profile.valid?profile.effort*1.25f:profile.rest*2);
    for(float level:_setupHistory)maximum=std::max(maximum,level*1.15f);
    auto screenY=[&](float value){return bottom-int(std::max(0.f,std::min(1.f,value/maximum))*(bottom-top));};
    glDisable(GL_TEXTURE_2D);Painter::setColor(Colors::plot);Painter::drawRect(Rect(left,top,right-left,bottom-top));
    if(profile.rest>0){Painter::setColor(Color(45,48,52));const int floor=screenY(profile.rest);Painter::drawRect(Rect(left,floor,right-left,bottom-floor));}
    Painter::setColor(Colors::lightblue);glBegin(GL_LINE_STRIP);
    for(size_t i=0;i<_setupHistory.size();++i)glVertex2f(left+float(i)*(right-left)/119,screenY(_setupHistory[i]));
    glEnd();
    if(profile.valid){Painter::setColor(Colors::warning);Painter::drawRect(Rect(left,screenY(profile.rest+profile.threshold()),right-left,2));}
    if(_inputSetup.measuring()) {
        Painter::setColor(Colors::lightblue);Painter::drawRect(Rect(left,bottom+8,int((right-left)*_inputSetup.progress(SDL_GetTicks())),3));
    }
    glEnable(GL_TEXTURE_2D);Painter::setColor(Colors::mutedText);
    std::ostringstream value;value<<"현재 "<<int(_setupLevels[player]);
    if(profile.valid)value<<"   기준선 "<<int(profile.rest+profile.threshold())<<"   ("<<int(profile.fraction*100+.5f)<<"%)";
    else if(_inputSetup.measuring())value<<"   남은 시간 "<<std::max(0,3-int((SDL_GetTicks()-_inputSetup.started)/1000))<<"초";
    font->draw(value.str().c_str(),left,y+ph-166);
    if(review){
        font->draw("↑ 더 둔감 / ↓ 더 민감",left,y+ph-140);
        glDisable(GL_TEXTURE_2D);Painter::setColor(Colors::button);
        Painter::drawRoundedRect(Rect(x+pw-156,y+ph-180,40,32),4);Painter::drawRoundedRect(Rect(x+pw-108,y+ph-180,40,32),4);
        Painter::setColor(Colors::white);
        Painter::drawRect(Rect(x+pw-142,y+ph-165,12,2));
        Painter::drawRect(Rect(x+pw-94,y+ph-165,12,2));
        Painter::drawRect(Rect(x+pw-89,y+ph-170,2,12));
        glEnable(GL_TEXTURE_2D);
    }
    Painter::setColor(_inputSetup.error.empty()?Colors::mutedText:Colors::warning);
    std::string hint=_inputSetup.error;
    if(hint.empty())hint=review?(canAct?"기준선 아래에서는 동작하지 않습니다. 준비되면 시작하세요.":"모두 힘을 빼세요. 신호가 내려오면 시작할 수 있습니다."):
        stage==GameInput::Calibration::EffortReady?(microphone?"측정 버튼을 누르고 3초 동안 소리를 내세요.":"측정 버튼을 누르고 3초 동안 힘을 유지하세요."):"측정 중에는 게임이 시작되거나 조작되지 않습니다.";
    font->drawMultiline(hint.c_str(),left,y+ph-126,right-left,AlignLeft);
    Painter::setColor(Colors::mutedText);font->draw("K  키보드로 시작 · 센서 / 마이크 입력 끔",left,y+ph-84);
    glDisable(GL_TEXTURE_2D);Painter::setColor(Colors::button);Painter::drawRoundedRect(Rect(left,y+ph-60,132,40),5);
    Painter::setColor(canAct?Colors::lightblue:Colors::button);Painter::drawRoundedRect(Rect(x+pw-184,y+ph-60,160,40),5);
    glEnable(GL_TEXTURE_2D);Painter::setColor(Colors::white);font->draw("다시 측정 R",left+66,y+ph-40,AlignCenter);
    Painter::setColor(canAct?Colors::background:Colors::mutedText);
    const char *action=review?(_inputSetup.player+1<_inputSetup.players?"다음 플레이어":"게임 시작"):_inputSetup.measuring()?"측정 중...":"측정 시작 Enter";
    font->draw(action,x+pw-104,y+ph-40,AlignCenter);
}

void GameHubView::paintCalibrationChoice()
{
    using namespace Widgets;
    const BitmapFontGL *font=Application::font();
    const int panelWidth=std::max(220,std::min(560,width()-40));
    const int x=(width()-panelWidth)/2,y=height()/2-170;
    glDisable(GL_TEXTURE_2D);Painter::setColor(Colors::background);Painter::drawRect(rect());
    Painter::setColor(Colors::border);Painter::drawRoundedRect(Rect(x,y,panelWidth,340),9);
    Painter::setColor(Colors::widgetbgdark);Painter::drawRoundedRect(Rect(x+1,y+1,panelWidth-2,338),8);
    glEnable(GL_TEXTURE_2D);Painter::setColor(Colors::mutedText);
    font->draw(_games[_selectedGame].title.c_str(),x+24,y+24);
    Painter::setColor(Colors::white);font->draw("게임 입력 설정",x+24,y+62);
    Painter::setColor(Colors::mutedText);
    font->draw("센서 반응을 안정적으로 맞추려면 기준선을 보정하세요.",x+24,y+98);
    glDisable(GL_TEXTURE_2D);
    Painter::setColor(Colors::lightblue);Painter::drawRoundedRect(Rect(x+24,y+132,panelWidth-48,48),5);
    Painter::setColor(Colors::button);Painter::drawRoundedRect(Rect(x+24,y+192,panelWidth-48,40),5);
    Painter::setColor(Colors::button);Painter::drawRoundedRect(Rect(x+24,y+240,panelWidth-48,40),5);
    Painter::setColor(Colors::button);Painter::drawRoundedRect(Rect(x+24,y+292,112,36),5);
    Painter::setColor(Colors::lightblue);Painter::drawRoundedRect(Rect(x+panelWidth-188,y+292,164,36),5);
    glEnable(GL_TEXTURE_2D);Painter::setColor(Colors::background);
    font->draw("기준선 보정 시작   C / Enter",x+panelWidth/2,y+147,AlignCenter);
    Painter::setColor(Colors::white);
    font->draw("기본 감도로 센서 사용   S",x+panelWidth/2,y+203,AlignCenter);
    font->draw("키보드 전용   K",x+panelWidth/2,y+251,AlignCenter);
    font->draw("취소  Esc",x+80,y+301,AlignCenter);
    Painter::setColor(Colors::background);font->draw("시작",x+panelWidth-106,y+301,AlignCenter);
}

void GameHubView::paintPlayerName()
{
    const Widgets::BitmapFontGL *font=Widgets::Application::font();
    const int panelWidth=std::max(160,std::min(560,width()-40));
    const int x=(width()-panelWidth)/2,y=height()/2-160;
    glDisable(GL_TEXTURE_2D);
    Widgets::Painter::setColor(Widgets::Colors::background);
    Widgets::Painter::drawRect(rect());
    Widgets::Painter::setColor(Widgets::Colors::border);
    Widgets::Painter::drawRoundedRect(Widgets::Rect(x,y,panelWidth,320),10);
    Widgets::Painter::setColor(Widgets::Colors::widgetbgdark);
    Widgets::Painter::drawRoundedRect(Widgets::Rect(x+1,y+1,panelWidth-2,318),9);
    glEnable(GL_TEXTURE_2D);
    Widgets::Painter::setColor(Widgets::Colors::mutedText);
    font->draw(_games[_selectedGame].title.c_str(),x+28,y+28);
    Widgets::Painter::setColor(Widgets::Colors::white);
    glPushMatrix();glTranslatef(x+28,y+60,0);glScalef(1.35f,1.35f,1);
    font->draw(_games[_selectedGame].players==2?(_namePlayer==0?"1P 플레이어 이름":"2P 플레이어 이름"):"플레이어 이름",0,0);glPopMatrix();
    Widgets::Painter::setColor(Widgets::Colors::mutedText);
    font->draw(_games[_selectedGame].players==2?"대결에서 사용할 두 사람의 이름을 입력하세요.":"이 이름으로 게임 점수가 기록됩니다.",x+28,y+100);

    glDisable(GL_TEXTURE_2D);
    Widgets::Painter::setColor(Widgets::Colors::lightblue);
    Widgets::Painter::drawRoundedRect(Widgets::Rect(x+28,y+140,panelWidth-56,44),5);
    Widgets::Painter::setColor(Widgets::Colors::background);
    Widgets::Painter::drawRoundedRect(Widgets::Rect(x+29,y+141,panelWidth-58,42),4);
    std::string prefix=_nameEntry.text.substr(0,_nameEntry.cursor);
    std::string suffix=_nameEntry.text.substr(_nameEntry.cursor);
    if(_nameEntry.selected && !_nameEntry.composition.empty()) {prefix.clear();suffix.clear();}
    std::string visible=prefix+_nameEntry.composition+suffix;
    // Keep the caret visible, including for a long uncommitted IME phrase.
    while(!prefix.empty() && font->textWidth(visible.c_str())>panelWidth-86) {
        std::vector<size_t> offsets;Rankings::nameCharacters(prefix,offsets);
        prefix.erase(0,offsets.size()>1?offsets[1]:prefix.size());
        visible=prefix+_nameEntry.composition+suffix;
    }
    while(!visible.empty() && font->textWidth(visible.c_str())>panelWidth-86) {
        std::vector<size_t> offsets;Rankings::nameCharacters(visible,offsets);
        visible.resize(offsets.size()>1?offsets[offsets.size()-2]:0);
    }
    const int textX=x+40,textY=y+154;
    if(_nameEntry.selected && !_nameEntry.text.empty() && _nameEntry.composition.empty()) {
        Widgets::Painter::setColor(Widgets::Colors::selectedSurface);
        Widgets::Painter::drawRect(Widgets::Rect(textX,textY-2,font->textWidth(visible.c_str())+2,20));
    }
    const int caret=std::min(panelWidth-82,font->textWidth((prefix+_nameEntry.composition).c_str()));
    if((SDL_GetTicks()/500)%2==0) {
        Widgets::Painter::setColor(Widgets::Colors::lightblue);
        Widgets::Painter::drawRect(Widgets::Rect(textX+caret,textY-1,1,20));
    }
    if(!_nameEntry.composition.empty()) {
        Widgets::Painter::setColor(Widgets::Colors::lightblue);
        const int begin=std::min(caret,font->textWidth(prefix.c_str()));
        Widgets::Painter::drawRect(Widgets::Rect(textX+begin,textY+18,std::max(1,caret-begin),1));
    }
    Widgets::Painter::setColor(Widgets::Colors::button);
    Widgets::Painter::drawRoundedRect(Widgets::Rect(x+28,y+248,112,40),5);
    Widgets::Painter::setColor(Widgets::Colors::lightblue);
    Widgets::Painter::drawRoundedRect(Widgets::Rect(x+panelWidth-160,y+248,132,40),5);
    glEnable(GL_TEXTURE_2D);
    Widgets::Painter::setColor(Widgets::Colors::white);
    if(visible.empty()) {
        Widgets::Painter::setColor(Widgets::Colors::mutedText);
        font->draw("이름을 입력하세요",textX,textY);
    } else font->draw(visible.c_str(),textX,textY);
    Widgets::Painter::setColor(_nameEntry.error.empty()?Widgets::Colors::mutedText:Widgets::Colors::warning);
    font->draw(_nameEntry.error.empty()?"한글·영문 1~32자 / Enter로 시작":_nameEntry.error.c_str(),x+28,y+202);
    Widgets::Painter::setColor(Widgets::Colors::white);
    font->draw("취소  Esc",x+84,y+260,Widgets::AlignHCenter);
    Widgets::Painter::setColor(Widgets::Colors::background);
    font->draw("게임 시작",x+panelWidth-94,y+260,Widgets::AlignHCenter);
}

void GameHubView::paintEvent()
{
    if(!isVisible())
        return;

    const Widgets::BitmapFontGL *font = Widgets::Application::font();
    if(_nameEntry.open) {paintPlayerName();return;}
    if(_calibrationChoice) {paintCalibrationChoice();return;}
    if(_inputSetup.open()) {paintCalibration();return;}
    if(!_menuOpen && _plugin && _pluginState)
    {
        BYBGameHost host = {};
        host.width = width();
        host.height = height();
        host.delta_seconds = 0.0f;
        host.signal_level = _signalLevel;
        host.trigger_level = _triggerLevel;
        host.signal_available = _sensorAvailable ? 1 : 0;
        host.signal_edge = 0;
        host.draw_context = this;
        host.fill_rect = drawPluginRect;
        host.draw_line = drawPluginLine;
        host.draw_text = drawPluginText;
        host.asset_root = _activeAssetRoot.c_str();
        host.draw_image = drawPluginImage;
        host.draw_triangles = drawPluginTriangles;
        host.draw_gradient_triangles = drawPluginGradientTriangles;
        host.report_result = reportPluginResult;
        host.signal_source = !_sensorAvailable ? 0 : (_manager.fileMode() ? 3 :
            ((_manager.serialMode() || _manager.hidMode()) ? 2 : 1));
        populatePlayerInputs(host);
        glPushAttrib(GL_DEPTH_BUFFER_BIT);
        glDepthMask(GL_TRUE); glClearDepth(1); glClear(GL_DEPTH_BUFFER_BIT);
        glPopAttrib();
        _plugin->render(_pluginState, &host);
        glDisable(GL_TEXTURE_2D);
        Widgets::Painter::setColor(Widgets::Colors::button);
        Widgets::Painter::drawRect(Widgets::Rect(std::max(0, width() - 154), 12,
                                                  142, 34));
        glEnable(GL_TEXTURE_2D);
        Widgets::Painter::setColor(Widgets::Colors::white);
        font->draw("Games [G]", width() - 83, 22, Widgets::AlignCenter);
        return;
    }
    if(!_menuOpen)
        return;

    glDisable(GL_TEXTURE_2D);
    Widgets::Painter::setColor(Widgets::Colors::background);
    Widgets::Painter::drawRect(rect());
    Widgets::Painter::setColor(Widgets::Colors::border);
    Widgets::Painter::drawRect(Widgets::Rect(46, kMenuCardTop - 1, std::max(1, width() - 92), 1));
    glEnable(GL_TEXTURE_2D);
    Widgets::Painter::setColor(Widgets::Colors::white);
    glPushMatrix(); glTranslatef(46, 38, 0); glScalef(1.4f, 1.4f, 1.f);
    font->draw("Games", 0, 0);
    glPopMatrix();
    Widgets::Painter::setColor(Widgets::Colors::mutedText);
    font->draw("Click a game to start.  Up / Down + Enter to select.", 46, 76);

    glDisable(GL_TEXTURE_2D);
    Widgets::Painter::setColor(Widgets::Colors::button);
    Widgets::Painter::drawRect(Widgets::Rect(std::max(0, width() - 138), 34, 104, 30));
    glEnable(GL_TEXTURE_2D);
    Widgets::Painter::setColor(Widgets::Colors::white);
    font->draw("Back [G]", width() - 86, 42, Widgets::AlignCenter);

    const int visibleCount = std::max(1,
        (height() - kMenuCardTop - 76) / (kMenuCardHeight + kMenuCardGap));
    const int firstVisible = std::max(0,
        std::min(_selectedGame, static_cast<int>(_games.size()) - visibleCount));
    const int lastVisible = std::min(static_cast<int>(_games.size()), firstVisible + visibleCount);
    for(int i = firstVisible; i < lastVisible; ++i)
    {
        const int row = i - firstVisible;
        const int y = kMenuCardTop + row * (kMenuCardHeight + kMenuCardGap);
        glDisable(GL_TEXTURE_2D);
        if(i == _selectedGame)
        {
            Widgets::Painter::setColor(Widgets::Colors::selectedSurface);
            Widgets::Painter::drawRect(Widgets::Rect(46, y, std::max(1, width() - 92), kMenuCardHeight));
        }
        Widgets::Painter::setColor(Widgets::Colors::border);
        Widgets::Painter::drawRect(Widgets::Rect(46, y + kMenuCardHeight - 1, std::max(1, width() - 92), 1));
        glEnable(GL_TEXTURE_2D);
        std::ostringstream number;
        number << (i + 1 < 10 ? "0" : "") << i + 1;
        Widgets::Painter::setColor(Widgets::Colors::mutedText);
        font->draw(number.str().c_str(), 62, y + 18);
        Widgets::Painter::setColor(Widgets::Colors::white);
        font->draw(_games[static_cast<size_t>(i)].title.c_str(), 100, y + 14);
        Widgets::Painter::setColor(Widgets::Colors::mutedText);
        std::string description = _games[static_cast<size_t>(i)].description;
        const size_t maxChars = static_cast<size_t>(std::max(12, (width() - 280) / 8));
        if(description.size() > maxChars) description = description.substr(0, maxChars - 3) + "...";
        font->draw(description.c_str(), 100, y + 38);
        Widgets::Painter::setColor(Widgets::Colors::mutedText);
        font->draw("Play >",
                   width() - 70, y + 25, Widgets::AlignRight);
    }

    if(_games.empty())
    {
        Widgets::Painter::setColor(Widgets::Colors::white);
        font->draw("No games installed", width() / 2, height() / 2 - 12,
                   Widgets::AlignCenter);
        Widgets::Painter::setColor(Widgets::Colors::mutedText);
        font->draw("Your games will appear here", width() / 2,
                   height() / 2 + 14, Widgets::AlignCenter);
    }

    Widgets::Painter::setColor(Widgets::Colors::mutedText);
    font->draw("Input   /   [ ] sensitivity", 46, height() - 84);
    std::ostringstream signalText;
    if(_sensorAvailable)
        signalText << static_cast<int>(_signalLevel) << " / "
                   << static_cast<int>(_triggerLevel) << " ADC";
    else
        signalText << "Waiting for audio or sensor input";
    font->draw(signalText.str().c_str(), 46, height() - 52);
    glDisable(GL_TEXTURE_2D);
    const int meterX = std::max(300, width() / 2);
    const int meterWidth = std::max(1, width() - meterX - 54);
    Widgets::Painter::setColor(Widgets::Colors::widgetbgdark);
    Widgets::Painter::drawRect(Widgets::Rect(meterX, height() - 60, meterWidth, 14));
    Widgets::Painter::setColor(_signalAboveThreshold
        ? Widgets::Colors::warning : Widgets::Colors::lightblue);
    const float meterScale = std::max(120.0f, _triggerLevel * 3.0f);
    Widgets::Painter::drawRect(Widgets::Rect(meterX, height() - 60,
        static_cast<int>(meterWidth * clampFloat(_signalLevel / meterScale, 0.0f, 1.0f)), 14));
    glEnable(GL_TEXTURE_2D);

    if(!_statusMessage.empty())
    {
        Widgets::Painter::setColor(Widgets::Colors::warning);
        font->draw(_statusMessage.c_str(), 46, height() - 27);
    }
}

} // namespace BackyardBrains
