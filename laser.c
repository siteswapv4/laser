#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define EXPECT(condition, ...) \
    do { \
        if (!(condition)) { \
            SDL_Log(__VA_ARGS__); \
            goto error; \
        } \
    }while (0)

#define MAX_TARGETS 100
#define DEFAULT_LIFE 3

#define WINDOW_WIDTH 500
#define WINDOW_HEIGHT 500

static const SDL_Color LINE_DEFAULT_COLOR = {255, 255, 255, 255};
static const SDL_Color LINE_ATTACK_COLOR = {255, 50, 50, 255};

static const SDL_Color TARGET_FRIEND_COLOR = {50, 50, 255, 255};
static const SDL_Color TARGET_ENNEMY_COLOR = {255, 50, 50, 255};

static float DT = 1.0f / 60.0f;

typedef struct Target
{
    SDL_FRect rect;
    bool friend;
    
    SDL_FPoint velocity;
    SDL_FPoint acceleration;
}Target;

typedef enum AppPhase
{
    APP_PHASE_GAMEPLAY,
    APP_PHASE_RESULT
}AppPhase;

typedef struct AppState
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_AudioStream* stream;
    
    Uint8* music_data;
    Uint32 music_data_length;
        
    SDL_Point window_center;
    SDL_FPoint mouse_position;
    SDL_FPoint player_position;
    SDL_FPoint end_position;
    bool mouse_down;
    bool mouse_was_down;
    
    SDL_Texture* friend_texture;
    SDL_Texture* ennemy_texture;
    
    AppPhase phase;
    
    Target targets[MAX_TARGETS];
    int num_targets;
    
    int life;
    int score;
}AppState;

SDL_Texture* LoadPNG(SDL_Renderer* renderer, const char* path)
{
    SDL_Surface* surface = SDL_LoadPNG(path);
    if (!surface) return NULL;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    
    return texture;
}

void InitGameplay(AppState* app)
{
    app->life = DEFAULT_LIFE;
    app->score = 0;
    
    app->num_targets = 0;
    
    app->phase = APP_PHASE_GAMEPLAY;
}

SDL_AppResult SDL_AppInit(void** userdata, int argc, char* argv[])
{
    AppState* app = SDL_calloc(1, sizeof(AppState));
    *userdata = app;
    
    EXPECT(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO), "%s", SDL_GetError());
    EXPECT(SDL_CreateWindowAndRenderer("laser", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &app->window, &app->renderer), "%s", SDL_GetError());
    SDL_SetRenderLogicalPresentation(app->renderer, 500, 500, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");
    SDL_SetHint(SDL_HINT_RENDER_LINE_METHOD, "3");
    SDL_srand(0);
    
    SDL_AudioSpec spec;
    char* path;
    SDL_asprintf(&path, "%s/music.wav", SDL_GetBasePath());
    bool ret = SDL_LoadWAV(path, &spec, &app->music_data, &app->music_data_length);
    SDL_free(path);
    EXPECT(ret, "%s", SDL_GetError());
    
    app->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    EXPECT(app->stream, "%s", SDL_GetError());
    
    SDL_ResumeAudioStreamDevice(app->stream);
    
    SDL_asprintf(&path, "%s/friend.png", SDL_GetBasePath());
    app->friend_texture = LoadPNG(app->renderer, path);
    SDL_free(path);
    EXPECT(app->friend_texture, "%s", SDL_GetError());
    
    SDL_asprintf(&path, "%s/ennemy.png", SDL_GetBasePath());
    app->ennemy_texture = LoadPNG(app->renderer, path);
    SDL_free(path);
    EXPECT(app->ennemy_texture, "%s", SDL_GetError());
    
    app->window_center.x = WINDOW_WIDTH / 2.0f;
    app->window_center.y = WINDOW_HEIGHT / 2.0f;
    
    app->player_position.x = app->window_center.x;
    app->player_position.y = WINDOW_HEIGHT;
    
    InitGameplay(app);

    return SDL_APP_CONTINUE;

error:
    return SDL_APP_FAILURE;    
}

SDL_AppResult SDL_AppEvent(void* userdata, SDL_Event* event)
{
    AppState* app = userdata;
    
    SDL_ConvertEventToRenderCoordinates(app->renderer, event);
    
    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
    
    if (event->type == SDL_EVENT_MOUSE_MOTION)
    {
        app->mouse_position = (SDL_FPoint){event->motion.x, event->motion.y};
        if (app->phase == APP_PHASE_GAMEPLAY)
        {
            SDL_FPoint direction = {app->mouse_position.x - app->player_position.x, app->mouse_position.y - app->player_position.y};
            app->end_position = (SDL_FPoint){app->mouse_position.x + direction.x * 10000.0f, app->mouse_position.y + direction.y * 10000.0f};
        }
    }
    else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        if (event->button.button == SDL_BUTTON_LEFT) app->mouse_down = true;
    }
    else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        if (event->button.button == SDL_BUTTON_LEFT) app->mouse_down = false;
    }
    else if ((event->type == SDL_EVENT_KEY_DOWN) && !event->key.repeat)
    {
        if (event->key.scancode == SDL_SCANCODE_F11)
        {
            SDL_SetWindowFullscreen(app->window, !(SDL_GetWindowFlags(app->window) & SDL_WINDOW_FULLSCREEN));
        }
    }
    
    return SDL_APP_CONTINUE;
    
error:
    return SDL_APP_FAILURE;
}

Target NewTarget(AppState* app)
{
    Target target = {0};
    
    target.rect.w = 50.0f;
    target.rect.h = 50.0f;
    
    target.rect.x = SDL_rand(app->window_center.x / 2); 
    if (SDL_rand(2))
        target.rect.x = WINDOW_WIDTH - target.rect.w - target.rect.x;

    target.rect.y = WINDOW_HEIGHT - target.rect.h;
    
    target.velocity = (SDL_FPoint){SDL_rand(600) - 300, -(600 + SDL_rand(300))};
    target.acceleration = (SDL_FPoint){0.0f, 900.0f};
    
    target.friend = SDL_rand(2);
    
    return target;
}

void RemoveTarget(AppState* app, int i)
{
    app->num_targets--;
    SDL_memmove(&app->targets[i], &app->targets[i + 1], (app->num_targets - i) * sizeof(Target));
}

bool LineIntersectsRect(SDL_FPoint p0, SDL_FPoint p1, SDL_FRect r)
{
    return SDL_GetRectAndLineIntersectionFloat(&r, &p0.x, &p0.y, &p1.x, &p1.y);
}


void Logic(AppState* app)
{
    if (SDL_rand(SDL_clamp(60 - app->score, 5, 60)) == 0)
    {
        if (app->num_targets != MAX_TARGETS)
        {
            app->targets[app->num_targets] = NewTarget(app);
            app->num_targets++;
        }
    }
    
    for (int i = app->num_targets - 1; i >= 0; i--)
    {
        app->targets[i].velocity.x += app->targets[i].acceleration.x * DT;
        app->targets[i].velocity.y += app->targets[i].acceleration.y * DT;
        
        app->targets[i].rect.x += app->targets[i].velocity.x * DT;
        app->targets[i].rect.y += app->targets[i].velocity.y * DT;
        
        if (((app->targets[i].rect.y > WINDOW_HEIGHT) && (app->targets[i].velocity.y > 0)) ||
            (app->targets[i].rect.x > WINDOW_WIDTH) || (app->targets[i].rect.x + app->targets[i].rect.w < 0))
        {
            RemoveTarget(app, i);
        }
    }
    
    for (int i = app->num_targets - 1; i >= 0; i--)
    {
        if (LineIntersectsRect(app->player_position, app->end_position, app->targets[i].rect) && app->mouse_down)
        {
            if (app->targets[i].friend)
                app->life--;
            else
                app->score++;
            RemoveTarget(app, i);
        }
    }
    
    if (app->life <= 0)
    {
        SDL_ClearAudioStream(app->stream);
        app->phase = APP_PHASE_RESULT;
    }
}

void Render(AppState* app)
{
    for (int i = 0; i < app->num_targets; i++)
    {
        SDL_RenderTexture(app->renderer, app->targets[i].friend ? app->friend_texture : app->ennemy_texture, NULL, &app->targets[i].rect);
    }

    if (app->mouse_down)
        SDL_SetRenderDrawColor(app->renderer, LINE_ATTACK_COLOR.r, LINE_ATTACK_COLOR.g, LINE_ATTACK_COLOR.b, LINE_ATTACK_COLOR.a);
    else
        SDL_SetRenderDrawColor(app->renderer, LINE_DEFAULT_COLOR.r, LINE_DEFAULT_COLOR.g, LINE_DEFAULT_COLOR.b, LINE_DEFAULT_COLOR.a);

    SDL_RenderLine(app->renderer, app->player_position.x, app->player_position.y, app->end_position.x, app->end_position.y);
    
    SDL_SetRenderDrawColor(app->renderer, 255, 255, 255, 255);
    SDL_RenderDebugTextFormat(app->renderer, 0.0f, 0.0f, "LIFE : %d", app->life);
    char buf[255];
    SDL_snprintf(buf, 255, "SCORE : %d", app->score);
    SDL_RenderDebugText(app->renderer, WINDOW_WIDTH - (SDL_strlen(buf) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE), 0.0f, buf);
}

void Gameplay(AppState* app)
{
    if (SDL_GetAudioStreamQueued(app->stream) < (int)app->music_data_length) {
        SDL_PutAudioStreamData(app->stream, app->music_data, app->music_data_length);
    }
    Logic(app);
    Render(app);
}

void Result(AppState* app)
{   
    SDL_SetRenderDrawColor(app->renderer, 255, 255, 255, 255);
    char buf[255];
    SDL_snprintf(buf, 255, "Final Score : %d", app->score);
    SDL_RenderDebugText(app->renderer, app->window_center.x - SDL_strlen(buf) / 2 * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE, app->window_center.y - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE / 2, buf);
    
    if (app->mouse_down && !app->mouse_was_down)
        InitGameplay(app);
}

SDL_AppResult SDL_AppIterate(void* userdata)
{
    AppState* app = userdata;
    
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderClear(app->renderer);
    
    switch (app->phase)
    {
        case APP_PHASE_GAMEPLAY:
            Gameplay(app);
            break;
            
        case APP_PHASE_RESULT:
            Result(app);
            break;
    }
    
    SDL_RenderPresent(app->renderer);
    
    app->mouse_was_down = app->mouse_down;
    
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* userdata, SDL_AppResult result)
{
    AppState* app = userdata;
    
    if (!app) return;
    if (app->music_data) SDL_free(app->music_data);
    if (app->friend_texture) SDL_DestroyTexture(app->friend_texture);
    if (app->ennemy_texture) SDL_DestroyTexture(app->ennemy_texture);
    SDL_free(app);
}
