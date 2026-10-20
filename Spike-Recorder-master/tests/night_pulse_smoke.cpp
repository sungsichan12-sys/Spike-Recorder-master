#include "../src/games/GamePlugin.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>

namespace {
int observedScore = 0;
bool completed = false;
bool failed = false;
int triangles = 0;
int reportedRounds = 0;
int64_t reportedScore = -1;
int BYB_GAME_CALL inspectResult(void *,uint64_t,int64_t score,int practice) {
    if(practice) return 0;
    ++reportedRounds; reportedScore=score; return 1;
}

void BYB_GAME_CALL ignoreRect(void *, int, int, int, int, uint32_t) {}
void BYB_GAME_CALL ignoreLine(void *, int, int, int, int, int, uint32_t) {}
void BYB_GAME_CALL inspectTriangles(void *, const float *, const uint32_t *, int count) { triangles += count; }
void BYB_GAME_CALL ignoreImage(void *, const char *, int, int, int, int) {}
void BYB_GAME_CALL inspectText(void *, const char *value, int, int, uint32_t, int)
{
    int score = 0;
    if(std::sscanf(value, "점수 %d", &score) == 1)
        observedScore = score;
    if(std::strcmp(value, "연주 완료") == 0)
        completed = true;
    if(std::strcmp(value, "다시 맞춰볼까요") == 0)
        failed = true;
}

BYBGameHost host()
{
    BYBGameHost value = {};
    value.width = 1200;
    value.height = 800;
    value.delta_seconds = 0.01f;
    value.trigger_level = 100.0f;
    value.asset_root = "";
    value.signal_source = 1;
    value.draw_triangles = inspectTriangles;
    value.fill_rect = ignoreRect;
    value.draw_line = ignoreLine;
    value.draw_text = inspectText;
    value.draw_image = ignoreImage;
    value.report_result = inspectResult;
    return value;
}
}

int main(int argc, char **argv)
{
    if(argc != 2)
        return 2;
    HMODULE library = LoadLibraryA(argv[1]);
    if(!library)
    {
        std::printf("LoadLibrary failed: %lu\n", GetLastError());
        return 2;
    }
    BYBGetGamePluginFn entry = reinterpret_cast<BYBGetGamePluginFn>(
        GetProcAddress(library, BYB_GAME_PLUGIN_ENTRY));
    const BYBGamePlugin *game = entry ? entry() : NULL;
    if(!game || game->api_version != BYB_GAME_PLUGIN_API_VERSION ||
       std::strcmp(game->id, "night-pulse") != 0)
        return 3;

    BYBGameHost frame = host();
    void *state = game->create();
    game->key_event(state, ' ', 0);
    game->key_event(state, ' ', 1);
    game->update(state, &frame);
    float nextBeat = 2.0f;
    for(int step = 1; step <= 3500; ++step)
    {
        if(step * 0.01f + 0.005f >= nextBeat && nextBeat <= 33.5f)
        {
            const int beat = static_cast<int>((nextBeat - 2.0f) / .5f + .5f);
            const int bar = beat / 4, position = beat % 4;
            if(!((bar % 4 == 1 && position == 1) || (bar % 4 == 3 && position == 3))) {
                game->key_event(state, ' ', 0);
                game->key_event(state, ' ', 1);
            }
            nextBeat += 0.5f;
        }
        game->update(state, &frame);
    }
    game->render(state, &frame);
    game->destroy(state);
    if(!completed || observedScore != 5600 || triangles < 500 || reportedRounds!=1 || reportedScore!=5600)
    {
        std::printf("Clear path failed: completed=%d score=%d\n",
                    completed, observedScore);
        return 4;
    }
    const int clearScore = observedScore;

    state = game->create();
    game->key_event(state, ' ', 0);
    game->key_event(state, ' ', 1);
    game->update(state, &frame);
    for(int step = 0; step < 1200; ++step)
        game->update(state, &frame);
    game->render(state, &frame);
    if(!failed)
    {
        std::printf("Miss limit did not fail the run\n");
        return 5;
    }
    game->key_event(state, ' ', 0);
    game->key_event(state, ' ', 1);
    game->update(state, &frame);
    failed = false;
    game->render(state, &frame);
    game->destroy(state);
    FreeLibrary(library);
    if(failed)
        return 6;
    std::printf("Night Pulse smoke test passed (score %d, fail/retry OK).\n",
                clearScore);
    return 0;
}
