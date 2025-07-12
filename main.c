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

typedef enum {
    COLLISION_EVENT_TYPE_NO_COLLISION = 0,
    COLLISION_EVENT_TYPE_WALL = 1,
    COLLISION_EVENT_TYPE_BOUNCY_WALL = 2,
    COLLISION_EVENT_TYPE_CHARACTER = 3,
    COLLISION_EVENT_TYPE_PROJECTILE = 4,
} CollisionEventType;

///////////////////////// GLOBALS /////////////////////////
Oct_AssetBundle gBundle;
Oct_Allocator gAllocator;
Oct_Texture gBackBuffer;

///////////////////////// CONSTANTS /////////////////////////

// Default level layout
const int32_t LEVEL_LAYOUT[] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
const int32_t LEVEL_WIDTH = 32;
const int32_t LEVEL_HEIGHT = 18;

// Backbuffer and room size
const float GAME_WIDTH = LEVEL_WIDTH * 16;
const float GAME_HEIGHT = LEVEL_HEIGHT * 16;

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

#define MAX_CHARACTERS 100
#define MAX_PROJECTILES 100
#define MAX_PARTICLES 100
#define MAX_PHYSICS_OBJECTS (MAX_CHARACTERS + MAX_PROJECTILES) // particles noclip
const float GROUND_FRICTION = 0.05;
const float AIR_FRICTION = 0.01;
const float GRAVITY = 0.5;
const float BOUNCE_PRESERVED_BOUNCE_WALL = 0.50; // how much velocity is preserved when rebounding off walls
const float BOUNCE_PRESERVED = 0.20; // how much velocity is preserved when rebounding off walls
const float GAMEPAD_DEADZONE = 0.25;
const float PLAYER_ACCELERATION = 0.5;
const float PLAYER_JUMP_SPEED = 7;
const float SPEED_LIMIT = 12;

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

/* TODO: This if comparing all entities turns out to suck
typedef struct SpacePartition_t {
    PhysicsObject *objects[MAX_PHYSICS_OBJECTS];
    int32_t len; // how many physics objects here
} SpacePartition;

typedef struct GlobalSpacePartition_t {
    SpacePartition *partitions;
    int32_t width;
    int32_t height;
    float partition_width;
    float partition_height;
} GlobalSpacePartition_t;*/

typedef struct Character_t {
    CharacterType type; // Type of opp
    uint64_t id;
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

// Represents unified player and ai input
typedef struct InputProfile_t {
    float x_acc;
    float y_acc;
    bool action;
} InputProfile;

// Types of things you can collide with
typedef struct CollisionEvent_t {
    CollisionEventType type;
    union {
        int32_t wallIndex;
        Character *character;
        Projectile *projectile;
    };
} CollisionEvent;

// LEAVE THIS AT THE BOTTOM
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

static inline bool aabb(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2) {
    return x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2;
}

// checks for collisions against the tilemap
// returns < 0 means this is a collision with a projectile
CollisionEvent collision_at(Character *this_c, Projectile *this_p, float x, float y, float width, float height) {
    CollisionEvent e = {.type = COLLISION_EVENT_TYPE_NO_COLLISION};
    int32_t grid_x1 = floorf(x / oct_TilemapCellWidth(state.level_map));
    int32_t grid_y1 = floorf(y / oct_TilemapCellHeight(state.level_map));
    int32_t grid_x2 = floorf((x + width) / oct_TilemapCellWidth(state.level_map));
    int32_t grid_y2 = floorf((y + height) / oct_TilemapCellHeight(state.level_map));

    int32_t wall[4] = {oct_GetTilemap(state.level_map, grid_x1, grid_y1),
                       oct_GetTilemap(state.level_map, grid_x2, grid_y1),
                       oct_GetTilemap(state.level_map, grid_x1, grid_y2),
                       oct_GetTilemap(state.level_map, grid_x2, grid_y2)};
    for (int i = 0; i < 4; i++) {
        if (wall[i]) {
            e.type = wall[i] == 2 ? COLLISION_EVENT_TYPE_BOUNCY_WALL : COLLISION_EVENT_TYPE_WALL;
            e.wallIndex = wall[i];
            return e;
        }
    }

    // If no wall collision we will check against all possible physics objects
    // TODO: THIS MIGHT BE A PERFORMANCE PROBLEM LMAO
    for (int i = 0; i < MAX_CHARACTERS; i++) {
        if (&state.characters[i] == this_c || !state.characters[i].alive) continue;
        if (aabb(x, y, width, height, state.characters[i].physx.x, state.characters[i].physx.y, state.characters[i].physx.bb_width, state.characters[i].physx.bb_height)) {
            e.type = COLLISION_EVENT_TYPE_CHARACTER;
            e.character = &state.characters[i];
            return e;
        }
    }
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (&state.projectiles[i] == this_p || !state.projectiles[i].alive) continue;
        if (aabb(x, y, width, height, state.projectiles[i].physx.x, state.projectiles[i].physx.y, state.projectiles[i].physx.bb_width, state.projectiles[i].physx.bb_height)) {
            e.type = COLLISION_EVENT_TYPE_PROJECTILE;
            e.projectile = &state.projectiles[i];
            return e;
        }
    }

    return e;
}

void process_physics(Character *this_c, Projectile *this_p, PhysicsObject *physx, float x_acceleration, float y_acceleration) {
    // Add acceleration to velocity
    physx->x_vel = oct_Clamp(-SPEED_LIMIT, SPEED_LIMIT, physx->x_vel + x_acceleration);
    physx->y_vel = oct_Clamp(-SPEED_LIMIT, SPEED_LIMIT, physx->y_vel + y_acceleration);

    const bool kinda_touching_ground = collision_at(this_c, this_p, physx->x, physx->y + 1, physx->bb_width, physx->bb_height).type != 0;

    // Friction and gravity
    if (kinda_touching_ground) {
        physx->x_vel *= (1 - GROUND_FRICTION);
    } else {
        physx->x_vel *= (1 - AIR_FRICTION);
    }
    physx->y_vel += GRAVITY;

    if (physx->noclip) return;

    // Bouncy dogshit collisions
    CollisionEvent ce = collision_at(this_c, this_p, physx->x + physx->x_vel, physx->y, physx->bb_width, physx->bb_height);
    if (ce.type) {
        // Get close to the wall
        while (!collision_at(this_c, this_p, physx->x + (sign(physx->x_vel) * 0.1), physx->y, physx->bb_width, physx->bb_height).type)
            physx->x += (sign(physx->x_vel) * 0.1);

        // Bounce off the wall slightly
        if (ce.type == COLLISION_EVENT_TYPE_BOUNCY_WALL)
            physx->x_vel = physx->x_vel * (-BOUNCE_PRESERVED_BOUNCE_WALL);
        else
            physx->x_vel = physx->x_vel * (-BOUNCE_PRESERVED);
    }
    physx->x += physx->x_vel;

    ce = collision_at(this_c, this_p, physx->x, physx->y + physx->y_vel, physx->bb_width, physx->bb_height);
    if (ce.type) {
        // Get close to the wall
        while (!collision_at(this_c, this_p, physx->x, physx->y + (sign(physx->y_vel) * 0.1), physx->bb_width, physx->bb_height).type)
            physx->y += (sign(physx->y_vel) * 0.1);

        // Bounce off the wall slightly
        if (ce.type == COLLISION_EVENT_TYPE_BOUNCY_WALL)
            physx->y_vel = physx->y_vel * (-BOUNCE_PRESERVED_BOUNCE_WALL);
        else
            physx->y_vel = physx->y_vel * (-BOUNCE_PRESERVED);
    }
    physx->y += physx->y_vel;
}

void draw_character(Character *character) {
    if (character->type == CHARACTER_TYPE_JUMPER) {
        oct_DrawSpriteInt(
                OCT_INTERPOLATE_ALL, character->id,
                oct_GetAsset(gBundle, "sprites/jumper.json"),
                &character->sprite,
                (Oct_Vec2) {character->physx.x, character->physx.y}
                );
    } // TODO: The rest of these
}

InputProfile process_player(Character *character) {
    InputProfile input = {0};
    if (oct_KeyDown(OCT_KEY_LEFT) || oct_GamepadButtonDown(0, OCT_GAMEPAD_BUTTON_DPAD_LEFT) ||
        oct_GamepadLeftAxisX(0) < 0) {
        input.x_acc = -PLAYER_ACCELERATION;
    } else if (oct_KeyDown(OCT_KEY_RIGHT) || oct_GamepadButtonDown(0, OCT_GAMEPAD_BUTTON_DPAD_RIGHT) ||
               oct_GamepadLeftAxisX(0) > 0) {
        input.x_acc = PLAYER_ACCELERATION;
    }
    const bool kinda_touching_ground = collision_at(character, null, character->physx.x, character->physx.y + 1, character->physx.bb_width, character->physx.bb_height).type;

    if (kinda_touching_ground && (oct_KeyPressed(OCT_KEY_SPACE) || oct_KeyPressed(OCT_KEY_UP) || oct_GamepadButtonPressed(0, OCT_GAMEPAD_BUTTON_A))) {
        input.y_acc = -PLAYER_JUMP_SPEED;
    }

    return input;
}

void process_character(Character *character) {
    // This is only to handle input
    InputProfile input = {0};
    if (character->player_controlled) {
        input = process_player(character);
    } else {
        // Get input from ai
    }

    process_physics(character, null, &character->physx, input.x_acc, input.y_acc);
    draw_character(character);
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

            // Handle sprite instance & bounding box
            if (slot->type == CHARACTER_TYPE_JUMPER) {
                oct_InitSpriteInstance(&slot->sprite, oct_GetAsset(gBundle, "sprites/jumper.json"), true);
                slot->physx.bb_width = 12;
                slot->physx.bb_height = 12;
                slot->id = 10000 + i;
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
                .x = 200,
                .y = 20,
        }
    });
}

GameStatus game_update() {

    oct_TilemapDraw(state.level_map);

    for (int i = 0; i < MAX_CHARACTERS; i++) {
        if (!state.characters[i].alive) continue;
        process_character(&state.characters[i]);
    }

    // TODO: Put this shit in a job cuz idgaf about race conditions
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!state.projectiles[i].alive) continue;
        process_projectile(&state.projectiles[i]);
    }
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!state.particles[i].alive) continue;
        process_particle(&state.particles[i]);
    }

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

    // oct shit
    oct_GamepadSetAxisDeadzone(GAMEPAD_DEADZONE);
    oct_SetFullscreen(true);

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
