#include <oct/Octarine.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>

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
const int32_t MAX_PROJECTILES = 100;
const int32_t MAX_PARTICLES = 100;
const float GROUND_FRICTION = 0.05;
const float AIR_FRICTION = 0.01;
const float GRAVITY = 0.5;
const float BOUNCE_PRESERVED = 0.20; // how much velocity is preserved when rebounding off walls

///////////////////////// STRUCTS /////////////////////////
typedef struct PhysicsObject_t {
    float x;
    float y;
    float x_vel;
    float y_vel;

    // For collisions
    bool noclip;
    float bb_width;
    float bb_height;
} PhysicsObject;

typedef struct Character_t {
    CharacterType type; // Type of opp
    Oct_SpriteInstance sprite;
    bool player_controlled; // True if this is the current player false means AI
    PhysicsObject physx;
    float hp; // Only applies to ai, players get 1 tapped bozo
    float lifespan; // Only applies to player, as he will EXPLODE when timeout (enemies fling off-screen)
    bool alive;
} Character;

typedef struct Particle_t {
    bool sprite_based; // if true the sprite and frame is used
    PhysicsObject physx;
    float lifetime; // in seconds
    _Atomic bool alive;
    union {
        struct {
            Oct_Sprite sprite;
            int32_t frame;
        };
        Oct_Texture texture;
    };
} Particle;

typedef struct Projectile_t {
    PhysicsObject physx;
    float lifetime; // in seconds
    Oct_Texture tex;
    float damage; // only relevant for enemies
    _Atomic bool alive;
} Projectile;

typedef struct GameState_t {
    Oct_Tilemap level_map;
    Character characters[MAX_CHARACTERS];
    Projectile projectiles[MAX_PROJECTILES];
    Particle particles[MAX_PARTICLES];
} GameState;

GameState state;

///////////////////////// HELPERS /////////////////////////
static inline float sign(float x) {
    return x > 0 ? 1 : (x < 0 ? -1 : 0);
}

// checks for collisions against the tilemap
bool collision_at(float x, float y, float width, float height) {
    int32_t grid_x1 = floorf(x / oct_TilemapCellWidth(state.level_map));
    int32_t grid_y1 = floorf(y / oct_TilemapCellHeight(state.level_map));
    int32_t grid_x2 = floorf((x + width) / oct_TilemapCellWidth(state.level_map));
    int32_t grid_y2 = floorf((y + height) / oct_TilemapCellHeight(state.level_map));

    return oct_GetTilemap(state.level_map, grid_x1, grid_y1) != 0 ||
           oct_GetTilemap(state.level_map, grid_x2, grid_y1) != 0 ||
           oct_GetTilemap(state.level_map, grid_x1, grid_y2) != 0 ||
           oct_GetTilemap(state.level_map, grid_x2, grid_y2) != 0;
}

void process_physics(PhysicsObject *physx, float x_acceleration, float y_acceleration) {
    // Add acceleration to velocity
    physx->x_vel += x_acceleration;
    physx->y_vel += y_acceleration;

    const bool kinda_touching_ground = collision_at(physx->x, physx->y + 1, physx->bb_width, physx->bb_height);

    // Friction and gravity
    if (kinda_touching_ground) {
        physx->x_vel *= GROUND_FRICTION;
    } else {
        physx->x_vel *= AIR_FRICTION;
    }
    physx->y_vel + GRAVITY;

    // TODO: Collisions with other entities

    // Bouncy dogshit collisions
    if (collision_at(physx->x + physx->x_vel, physx->y, physx->bb_width, physx->bb_height)) {
        // Get close to the wall
        while (!collision_at(physx->x + (sign(physx->x_vel) * 0.1), physx->y, physx->bb_width, physx->bb_height))
            physx->x += (sign(physx->x_vel) * 0.1);

        // Bounce off the wall slightly
        physx->x_vel = physx->x_vel * (-BOUNCE_PRESERVED);
    }
    physx->x += physx->x_vel;
    if (collision_at(physx->x, physx->y + physx->y_vel, physx->bb_width, physx->bb_height)) {
        // Get close to the wall
        while (!collision_at(physx->x, physx->y + (sign(physx->y_vel) * 0.1), physx->bb_width, physx->bb_height))
            physx->x += (sign(physx->y_vel) * 0.1);

        // Bounce off the wall slightly
        physx->y_vel = physx->y_vel * (-BOUNCE_PRESERVED);
    }
    physx->y += physx->y_vel;
}

void draw_character(Character *character) {
    if (character->type == CHARACTER_TYPE_JUMPER) {
        oct_DrawSprite(
                oct_GetAsset(gBundle, "sprites/jumper.json"),
                &character->sprite,
                (Oct_Vec2) {character->physx.x, character->physx.y}
                );
    } // TODO: The rest of these
}

void process_player(Character *character) {

}

void process_character(Character *character) {
    // This is only to handle physics/acceleration
    if (character->player_controlled) {
        process_player(character);
        return;
    }
    // todo: this
}

void process_particle(Particle *particle) {
    // todo: this
}

void process_projectile(Projectile *projectile) {
    // todo: this
}

// copies a character into an available character slot and returns the character in the slot or
// null if there was no available slot
Character *add_character(Character *character) {
    Character *slot = null;
    for (int i = 0; i < MAX_CHARACTERS; i++) {
        if (!state.characters[i].alive) {
            slot = &state.characters[i];
            memcpy(slot, character, sizeof(struct Character_t));
            slot->alive = true;

            // Handle sprite instance
            if (slot->type == CHARACTER_TYPE_JUMPER) {
                oct_InitSpriteInstance(&slot->sprite, oct_GetAsset(gBundle, "sprites/jumper.json"), true);
            }  // TODO: The rest of these

            break;
        }
    }
    return slot;
}

///////////////////////// GAME /////////////////////////
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

    // Add the player
    add_character(&(Character){
        .type = CHARACTER_TYPE_JUMPER,
        .lifespan = 9999,
        .player_controlled = true,
        .physx = {
                .x = 20,
                .y = 20,
                .bb_width = 8,
                .bb_height = 8
        }
    });
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
