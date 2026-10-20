#ifndef BACKYARDBRAINS_GAME_PLUGIN_H
#define BACKYARDBRAINS_GAME_PLUGIN_H

#include <stdint.h>

/*
 * Small C ABI for games installed below game/<folder>/.
 * A module exports BYB_GetGamePlugin and implements the callbacks below.
 * Keep this header C-compatible so modules do not depend on the recorder's
 * C++ ABI or its internal widget classes.
 */

#ifdef _WIN32
#define BYB_GAME_EXPORT __declspec(dllexport)
#define BYB_GAME_CALL __cdecl
#else
#define BYB_GAME_EXPORT __attribute__((visibility("default")))
#define BYB_GAME_CALL
#endif

#define BYB_GAME_PLUGIN_API_VERSION 6
#define BYB_GAME_PLUGIN_ENTRY "BYB_GetGamePlugin"
/* Version 4 keeps the v3 struct layout and guarantees this larger batch limit.
   Old hosts must reject v4 modules instead of silently dropping dense meshes. */
#define BYB_GAME_MAX_TRIANGLES 65536

typedef struct BYBGameHost BYBGameHost;
typedef struct BYBGamePlugin BYBGamePlugin;

struct BYBGameHost
{
    int width;
    int height;
    float delta_seconds;
    float signal_level;
    float trigger_level;
    int signal_available;
    int signal_edge;
    void *draw_context;
    void (BYB_GAME_CALL *fill_rect)(void *context, int x, int y, int width,
                                    int height, uint32_t rgba);
    void (BYB_GAME_CALL *draw_line)(void *context, int x1, int y1, int x2,
                                    int y2, int thickness, uint32_t rgba);
    void (BYB_GAME_CALL *draw_text)(void *context, const char *text, int x,
                                    int y, uint32_t rgba, int alignment);
    /* Optional image support for polished games. Paths are relative to the
       active game's folder and are loaded/cached by the host. */
    const char *asset_root;
    void (BYB_GAME_CALL *draw_image)(void *context, const char *relative_path,
                                     int x, int y, int width, int height);
    /* Version 2 additions. Version 1 modules keep using the unchanged prefix.
       xy contains six floats per triangle; colors contains one RGBA per triangle.
       Coordinates are widget-local screen pixels, after 3D projection. */
    void (BYB_GAME_CALL *draw_triangles)(void *context, const float *xy,
                                        const uint32_t *colors, int triangle_count);
    /* 0: unavailable, 1: audio/microphone, 2: serial/HID, 3: recorded file. */
    int signal_source;
    /* Version 3: same xy layout, but three RGBA vertex colors per triangle.
       inverse_depth has three values per triangle (all zero for 2D layers).
       Versions 1 and 2 retain their unchanged struct prefix and flat callback. */
    void (BYB_GAME_CALL *draw_gradient_triangles)(void *context,const float *xy,
                         const uint32_t *vertex_colors,const float *inverse_depth,int triangle_count);
    /* Version 5: one completed round, monotonic run_id within a plugin session.
       Returns nonzero when durably queued (or deliberately excluded as practice).
       A zero return may be retried later; never report from the render callback. */
    int (BYB_GAME_CALL *report_result)(void *context,uint64_t run_id,int64_t score,int practice);
    /* Version 6: independently bound input channels for two-player modules.
       Older modules retain the unchanged v1-v5 prefix. Missing channels are
       unavailable, never mirrored from player one. Names live for the session. */
    int player_count;
    const char *player_names[2];
    float channel_signal_level[2];
    float channel_trigger_level[2];
    int channel_signal_available[2];
    void (BYB_GAME_CALL *draw_scaled_text)(void *context,const char *text,float x,
        float y,float scale,uint32_t rgba,int alignment);
    float input_sensitivity; /* Current host setting; 1 is normal. */
};

struct BYBGamePlugin
{
    uint32_t api_version;
    const char *id;
    const char *title;
    const char *description;
    void *(BYB_GAME_CALL *create)(void);
    void (BYB_GAME_CALL *destroy)(void *state);
    void (BYB_GAME_CALL *update)(void *state, const BYBGameHost *host);
    void (BYB_GAME_CALL *render)(void *state, const BYBGameHost *host);
    void (BYB_GAME_CALL *key_event)(void *state, int key, int pressed);
    void (BYB_GAME_CALL *mouse_event)(void *state, int x, int y, int button);
};

typedef const BYBGamePlugin *(BYB_GAME_CALL *BYBGetGamePluginFn)(void);

#endif
