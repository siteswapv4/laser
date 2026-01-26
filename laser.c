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
    
    SDL_Point window_center;
    SDL_FPoint mouse_position;
    SDL_FPoint player_position;
    SDL_FPoint end_position;
    bool mouse_down;
    bool mouse_was_down;
    
    AppPhase phase;
    
    Target targets[MAX_TARGETS];
    int num_targets;
    
    int life;
    int score;
}AppState;

void InitGameplay(AppState* app)
{
    SDL_GetWindowSize(app->window, &app->window_center.x, &app->window_center.y);
    app->window_center.x /= 2;
    app->window_center.y /= 2;
    
    app->player_position.x = app->window_center.x;
    app->player_position.y = app->window_center.y * 2.0f;
    
    app->life = DEFAULT_LIFE;
    app->score = 0;
    
    app->num_targets = 0;
    
    app->phase = APP_PHASE_GAMEPLAY;
}

SDL_AppResult SDL_AppInit(void** userdata, int argc, char* argv[])
{
    AppState* app = SDL_calloc(1, sizeof(AppState));
    *userdata = app;
    
    EXPECT(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD), "%s", SDL_GetError());
    EXPECT(SDL_CreateWindowAndRenderer("game", 500, 500, 0, &app->window, &app->renderer), "%s", SDL_GetError());
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");
    SDL_SetHint(SDL_HINT_RENDER_LINE_METHOD, "3");
    SDL_srand(0);
    
    InitGameplay(app);

    return SDL_APP_CONTINUE;

error:
    return SDL_APP_FAILURE;    
}

SDL_AppResult SDL_AppEvent(void* userdata, SDL_Event* event)
{
    AppState* app = userdata;
    
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
    
    return SDL_APP_CONTINUE;
    
error:
    return SDL_APP_FAILURE;
}

Target NewTarget(AppState* app)
{
    Target target = {0};
    
    target.rect.w = 50.0f;
    target.rect.h = 50.0f;
    
    target.rect.x = SDL_rand(app->window_center.x * 2 - target.rect.w);
    target.rect.y = app->window_center.y * 2 - target.rect.h;
    
    target.velocity = (SDL_FPoint){SDL_rand(200) - 100, -(600 + SDL_rand(300))};
    target.acceleration = (SDL_FPoint){0.0f, 1000.0f};
    
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
        
        if ((app->targets[i].rect.y > app->window_center.y * 2) && (app->targets[i].velocity.y > 0))
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
        app->phase = APP_PHASE_RESULT;
}

void Render(AppState* app)
{
    for (int i = 0; i < app->num_targets; i++)
    {
        if (app->targets[i].friend)
            SDL_SetRenderDrawColor(app->renderer, TARGET_FRIEND_COLOR.r, TARGET_FRIEND_COLOR.g, TARGET_FRIEND_COLOR.b, TARGET_FRIEND_COLOR.a);
        else
            SDL_SetRenderDrawColor(app->renderer, TARGET_ENNEMY_COLOR.r, TARGET_ENNEMY_COLOR.g, TARGET_ENNEMY_COLOR.b, TARGET_ENNEMY_COLOR.a);

        SDL_RenderRect(app->renderer, &app->targets[i].rect);
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
    SDL_RenderDebugText(app->renderer, app->window_center.y * 2 - (SDL_strlen(buf) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE), 0.0f, buf);
}

void Gameplay(AppState* app)
{
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
    if (userdata) SDL_free(userdata);
}
