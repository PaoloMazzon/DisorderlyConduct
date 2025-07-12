#include <oct/Octarine.h>
#include <stdlib.h>
#include <memory.h>

///////////////////////// ENUMS /////////////////////////
typedef enum {
    CHARACTER_TYPE_INVALID = 0, // for empty list positions
    CHARACTER_TYPE_JUMPER = 1,
    CHARACTER_TYPE_X_SHOOTER = 2,
    CHARACTER_TYPE_Y_SHOOTER = 3,
    CHARACTER_TYPE_XY_SHOOTER = 4,
    CHARACTER_TYPE_SWORDSMITH = 5,
    CHARACTER_TYPE_LASER = 6,
} CharacterType;

// For transitioning game states
typedef enum {
    GAME_STATUS_PLAY_GAME, // play the game from menu
    GAME_STATUS_END_GAME, // go back to menu
    GAME_STATUS_QUIT, // alt + f4
} GameStatus;

///////////////////////// GLOBALS /////////////////////////
Oct_AssetBundle gBundle;
Oct_Allocator gAllocator;
Oct_Texture gBackBuffer;

///////////////////////// CONSTANTS /////////////////////////
// Backbuffer and room size
const float GAME_WIDTH = 320;
const float GAME_HEIGHT = 176;

// Default level layout
const int32_t LEVEL_LAYOUT[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
const int32_t LEVEL_WIDTH = 20;
const int32_t LEVEL_HEIGHT = 11;

// These should line up with character types
const float CHARACTER_TYPE_LIFESPANS[] = {
        0, // CHARACTER_TYPE_INVALID,
        5, // CHARACTER_TYPE_JUMPER,
        5, // CHARACTER_TYPE_X_SHOOTER,
        5, // CHARACTER_TYPE_Y_SHOOTER,
        6, // CHARACTER_TYPE_XY_SHOOTER,
        4, // CHARACTER_TYPE_SWORDSMITH,
        3, // CHARACTER_TYPE_LASER,
};

// HP ranges for each character, also should match up with character types
const Oct_Vec2 CHARACTER_HP_RANGES[] = {
        {0, 0}, // CHARACTER_TYPE_INVALID,
        {90, 100}, // CHARACTER_TYPE_JUMPER,
        {90, 100}, // CHARACTER_TYPE_X_SHOOTER,
        {90, 100}, // CHARACTER_TYPE_Y_SHOOTER,
        {90, 100}, // CHARACTER_TYPE_XY_SHOOTER,
        {90, 100}, // CHARACTER_TYPE_SWORDSMITH,
        {90, 100}, // CHARACTER_TYPE_LASER,
};

const int32_t MAX_CHARACTERS = 100;

///////////////////////// STRUCTS /////////////////////////
typedef struct Character_t {
    // Type of opp
    CharacterType type;

    // True if this is the current player false means AI
    bool player_controlled;

    // Physics
    float x;
    float y;
    float x_vel;
    float y_vel;

    // Only applies to ai, players get 1 tapped bozo
    float hp;

    // Only applies to player, as he will EXPLODE when timeout (enemies fling off-screen)
    float lifespan;
} Character;

typedef struct Particle_t {
    bool sprite_based; // if true the sprite and frame is used
    union {
        struct {
            Oct_Sprite sprite;
            int32_t frame;
        };
        Oct_Texture texture;
    };

    // Physics
    float x;
    float y;
    float x_vel;
    float y_vel;
} Particle;

///////////////////////// HELPERS /////////////////////////
void process_character(Character *character) {
    // todo: this
}

///////////////////////// GAME /////////////////////////
typedef struct GameState_t {
    Oct_Tilemap level_map;

} GameState;
GameState state;

void game_begin() {
    memset(&state, 0, sizeof(struct GameState_t));
    state.level_map = oct_CreateTilemap(
            oct_GetAsset(gBundle, "textures/tileset.png"),
            LEVEL_WIDTH, LEVEL_HEIGHT,
            (Oct_Vec2){16, 16});

    // Copy tilemap
    for (int y = 0; y < LEVEL_HEIGHT; y++) {
        for (int x = 0; x < LEVEL_WIDTH; x++) {
            oct_SetTilemap(state.level_map, x, y, LEVEL_LAYOUT[(y * LEVEL_WIDTH) + x]);
        }
    }
}

GameStatus game_update() {

    oct_TilemapDraw(state.level_map);

    return GAME_STATUS_PLAY_GAME;
}

void game_end() {

}

///////////////////////// MENU /////////////////////////
void menu_begin() {

}

GameStatus menu_update() {
    return GAME_STATUS_PLAY_GAME;
}

void menu_end() {

}

///////////////////////// MAIN /////////////////////////

void *startup() {
    gBundle = oct_LoadAssetBundle("data");
    gAllocator = oct_CreateHeapAllocator();

    // Backbuffer
    gBackBuffer = oct_CreateSurface((Oct_Vec2){GAME_WIDTH, GAME_HEIGHT});

    menu_begin();

    return null;
}

// Called each logical frame, whatever you return is passed to either the next update or shutdown
void *update(void *ptr) {
    static bool in_menu = true;

    // Use backbuffer
    oct_SetDrawTarget(gBackBuffer);
    oct_DrawClear(&(Oct_Colour){195.0 / 255.0, 209.0 / 255.0, 234.0 / 255.0, 1});

    if (in_menu) {
        const GameStatus status = menu_update();
        if (status == GAME_STATUS_PLAY_GAME) {
            in_menu = false;
            menu_end();
            game_begin();
        } else if (status == GAME_STATUS_QUIT) {
            abort();
        }
    } else {
        const GameStatus status = game_update();
        if (status == GAME_STATUS_END_GAME) {
            in_menu = true;
            game_end();
            menu_begin();
        } else if (status == GAME_STATUS_QUIT) {
            abort();
        }
    }

    oct_SetDrawTarget(OCT_NO_ASSET);

    // Draw backbuffer
    const float window_width = oct_WindowWidth();
    const float window_height = oct_WindowHeight();
    const float scale_x = window_width / GAME_WIDTH;
    const float scale_y = window_height / GAME_HEIGHT;
    oct_DrawTextureExt(
            gBackBuffer,
            (Oct_Vec2){0, 0},
            (Oct_Vec2){scale_x, scale_y},
            0,
            (Oct_Vec2){0, 0});

    return null;
}

// Called once when the engine is about to be deinitialized
void shutdown(void *ptr) {
    oct_FreeAllocator(gAllocator);
    oct_FreeAssetBundle(gBundle);
}

int main(int argc, const char **argv) {
    Oct_InitInfo initInfo = {
            .sType = OCT_STRUCTURE_TYPE_INIT_INFO,
            .startup = startup,
            .update = update,
            .shutdown = shutdown,
            .argc = argc,
            .argv = argv,

            // Change these to what you want
            .windowTitle = "Jam Game",
            .windowWidth = 1280,
            .windowHeight = 720,
            .debug = true,
    };
    oct_Init(&initInfo);
    return 0;
}
