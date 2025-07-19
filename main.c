#include <oct/Octarine.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <oct/cJSON.h>
#include <string.h>
#include <stdio.h>

///////////////////////// ENUMS /////////////////////////
typedef enum {
    CHARACTER_TYPE_INVALID = 0, // for empty list positions
    CHARACTER_TYPE_JUMPER = 1,
    CHARACTER_TYPE_Y_SHOOTER = 2,
    CHARACTER_TYPE_X_SHOOTER = 3,
    CHARACTER_TYPE_XY_SHOOTER = 4,
    CHARACTER_TYPE_BOMBER = 5,
    CHARACTER_TYPE_LASER = 6,
    CHARACTER_TYPE_DASHER = 7,
} CharacterType;

// For transitioning game states
typedef enum {
    GAME_STATUS_PLAY_GAME, // play the game from menu
    GAME_STATUS_MENU, // go back to menu
    GAME_STATUS_QUIT, // alt + f4
} GameStatus;

typedef enum {
    COLLISION_EVENT_TYPE_NO_COLLISION = 0,
    COLLISION_EVENT_TYPE_WALL = 1,
    COLLISION_EVENT_TYPE_BOUNCY_WALL = 2,
    COLLISION_EVENT_TYPE_CHARACTER = 3,
    COLLISION_EVENT_TYPE_PROJECTILE = 4,
} CollisionEventType;

typedef enum {
    MENU_OPTION_PLAY, // go to play menu
    MENU_OPTION_START_GAME, // start the game
    MENU_OPTION_QUIT, // quit game
    MENU_OPTION_BACK, // back to top menu
    MENU_OPTION_SETTINGS, // go to settings
    MENU_OPTION_LEADERBOARD, // go to leaderboard menu
    MENU_OPTION_RESET_TUTORIAL, // sets the save file tutorial thing to false
    MENU_OPTION_TOGGLE_MUSIC, // toggles music on/off
    MENU_OPTION_TOGGLE_SOUND, // toggles sound on/off
    MENU_OPTION_TOGGLE_FULLSCREEN, // toggles fullscreen on/off
    MENU_OPTION_TOGGLE_PIXEL_PERFECT, // toggles pixel perfect scaling on/off
    MENU_OPTION_START_JUMPER, // selects the jumper to be the starting character
    MENU_OPTION_START_Y_SHOOTER, // selects the y shooter to be the starting character
    MENU_OPTION_MAP_1, // start in map1
    MENU_OPTION_MAP_2, // start in map2
    MENU_OPTION_MAP_3, // start in map3
} MenuOption;

typedef enum {
    MENU_INDEX_TOP = 0,
    MENU_INDEX_PLAY = 1,
    MENU_INDEX_SETTINGS = 2,
    MENU_INDEX_LEADERBOARDS = 3,
} MenuIndices;

typedef enum {
    STARTING_BODY_JUMPER = 0,
    STARTING_BODY_Y_SHOOTER = 1,
    STARTING_BODY_MAX = 2,
} StartingBody;

typedef enum {
    STARTING_MAP_1 = 0,
    STARTING_MAP_2 = 1,
    STARTING_MAP_3 = 2,
    STARTING_MAP_MAX = 3
} StartingMap;

///////////////////////// GLOBALS /////////////////////////
Oct_AssetBundle gBundle;
Oct_Allocator gAllocator;
Oct_Allocator gFrameAllocator; // arena for frame-time allocations
Oct_Texture gBackBuffer;
uint64_t gFrameCounter = 9999;
uint64_t gParticleIDs = 999999;
float gSoundVolume = 1;
float gMusicVolume = 1;
bool gPixelPerfect;
Oct_Sound gPlayingMusic;

///////////////////////// CONSTANTS /////////////////////////

// Default level layout
const int32_t LEVEL_WIDTH = 32;
const int32_t LEVEL_HEIGHT = 18;

// Backbuffer and room size
const float GAME_WIDTH = LEVEL_WIDTH * 16;
const float GAME_HEIGHT = LEVEL_HEIGHT * 16;

// how long the player has to live in these bodies
const float CHARACTER_TYPE_LIFESPANS[] = {
        0, // CHARACTER_TYPE_INVALID,
        40, // CHARACTER_TYPE_JUMPER,
        40, // CHARACTER_TYPE_Y_SHOOTER,
        30, // CHARACTER_TYPE_X_SHOOTER,
        30, // CHARACTER_TYPE_XY_SHOOTER,
        30, // CHARACTER_TYPE_BOMBER,
        15, // CHARACTER_TYPE_LASER,
        30, // CHARACTER_TYPE_DASHER,
};

// acceleration values for the ai, player uses this * a constant as well
const float ACCELERATION_VALUES[] = {
        0, // CHARACTER_TYPE_INVALID,
        0.12, // CHARACTER_TYPE_JUMPER,
        0.10, // CHARACTER_TYPE_X_SHOOTER,
        0.25, // CHARACTER_TYPE_Y_SHOOTER, this guy gotta haul ass
        0.08, // CHARACTER_TYPE_XY_SHOOTER,
        0.20, // CHARACTER_TYPE_BOMBER,
        0.08, // CHARACTER_TYPE_LASER,
        0.20, // CHARACTER_TYPE_DASHER,
};

// how long after taking an action an ai is allowed to take it again
const float ACTION_COOLDOWNS[] = {
        0, // CHARACTER_TYPE_INVALID,
        1.2, // CHARACTER_TYPE_JUMPER, -- only ai
        1.2, // CHARACTER_TYPE_X_SHOOTER,
        1.0, // CHARACTER_TYPE_Y_SHOOTER,
        0.5, // CHARACTER_TYPE_XY_SHOOTER,
        0.0, // CHARACTER_TYPE_BOMBER,
        2.0, // CHARACTER_TYPE_LASER,
        3.0, // CHARACTER_TYPE_DASHER,
};

// how long an ai has to telegraph an action
const float ACTION_TELEGRAPH_TIMES[] = {
        0, // CHARACTER_TYPE_INVALID,
        0.3, // CHARACTER_TYPE_JUMPER,
        0.6, // CHARACTER_TYPE_X_SHOOTER,
        0.4, // CHARACTER_TYPE_Y_SHOOTER,
        0.3, // CHARACTER_TYPE_XY_SHOOTER,
        1.0, // CHARACTER_TYPE_BOMBER,
        0.8, // CHARACTER_TYPE_LASER,
        0.0, // CHARACTER_TYPE_DASHER,
};

// chance on each potential action frame ai will do something
const float ACTION_CHANCE[] = {
        0, // CHARACTER_TYPE_INVALID,
        0.4, // CHARACTER_TYPE_JUMPER,
        0.3, // CHARACTER_TYPE_X_SHOOTER,
        0.5, // CHARACTER_TYPE_Y_SHOOTER,
        0.9, // CHARACTER_TYPE_XY_SHOOTER,
        0.4, // CHARACTER_TYPE_BOMBER,
        0.5, // CHARACTER_TYPE_LASER,
        1.0, // CHARACTER_TYPE_DASHER,
};

// every x frames the above chances are rolled
const int ACTION_CHANCE_FREQUENCY[] = {
        0, // CHARACTER_TYPE_INVALID,
        5, // CHARACTER_TYPE_JUMPER,
        6, // CHARACTER_TYPE_X_SHOOTER,
        7, // CHARACTER_TYPE_Y_SHOOTER,
        8, // CHARACTER_TYPE_XY_SHOOTER,
        4, // CHARACTER_TYPE_BOMBER,
        7, // CHARACTER_TYPE_LASER,
        1, // CHARACTER_TYPE_DASHER,
};

// extra score player gets for killing this enemy
const int ADDITIONAL_SCORE[] = {
        0, // CHARACTER_TYPE_INVALID,
        5, // CHARACTER_TYPE_JUMPER,
        6, // CHARACTER_TYPE_X_SHOOTER,
        7, // CHARACTER_TYPE_Y_SHOOTER,
        8, // CHARACTER_TYPE_XY_SHOOTER,
        15, // CHARACTER_TYPE_BOMBER,
        10, // CHARACTER_TYPE_LASER,
        20, // CHARACTER_TYPE_DASHER,
};

const int32_t GAME_PHASES = 8;
const int32_t SPAWN_FREQUENCIES[] = { // in frames
        75,
        60,
        45,
        40,
        38,
        35,
        35,
        30,
};
const int32_t TIME_BETWEEN_PHASES[] = {
        30 * 60, // j j y
        30 * 50, // j j y y x
        30 * 45, // xy y y x x
        30 * 45, // xy xy j
        30 * 40, // j j x x l d
        30 * 25, // d
        30 * 40, // l b y
        30 * 20, // doesnt matter, its infinite
};
const int32_t TIME_BETWEEN_PHASES2[] = {
        30 * 60, // j y y
        30 * 50, // j x y
        30 * 35, // xy y
        30 * 45, // d l xy
        30 * 40, // l l x
        30 * 25, // y b b
        30 * 40, // b d
        30 * 20, // doesnt matter, its infinite
};
const int32_t TIME_BETWEEN_PHASES3[] = {
        30 * 60, // xy y j
        30 * 40, // x xy j
        30 * 45, // j j l
        30 * 45, // d x x
        30 * 40, // l d d
        30 * 25, // j d l
        30 * 20, // b
        30 * 20, // doesnt matter, its infinite
};

const char * TOP_LEVEL_MENU[] = {
        "Play",
        "Settings",
        "Quit"
};
const int32_t TOP_MENU_SIZE = 3;

const char *OPTION_MENU[] = {
        "Reset tutorial",
        "Toggle music",
        "Toggle sound",
        "Toggle fullscreen",
        "Toggle pixel-perfect",
        "Back"
};
const int32_t OPTIONS_MENU_SIZE = 6;

const char *PLAY_MENU[] = {
        "Start Game",
        "Swap Body",
        "Swap Map",
        "Back",
};
const int32_t PLAY_MENU_SIZE = 4;

const float MAP_UNLOCK_SCORES[] = {
        0,
        10000,
        20000,
};

#define MAX_CHARACTERS 100
#define MAX_PROJECTILES 100
#define MAX_PARTICLES 1000
#define MAX_PHYSICS_OBJECTS (MAX_CHARACTERS + MAX_PROJECTILES) // particles noclip
const float GROUND_FRICTION = 0.07;
const float AIR_FRICTION = 0.04;
const float GRAVITY = 0.5;
const float BOUNCE_PRESERVED_BOUNCE_WALL = 0.45; // how much velocity is preserved when rebounding off walls
const float BOUNCE_PRESERVED = 0.15; // how much velocity is preserved when rebounding off walls
const float GAMEPAD_DEADZONE = 0.25;
const float PLAYER_JUMP_SPEED = 8;
const float JUMPER_DESCEND_SPEED = 4.5; // how fast the player can descend as jumper
const float PARTICLES_GROUND_IMPACT_SPEED = 6;
const float SPEED_LIMIT = 12;
const float PLAYER_STARTING_LIFESPAN = 60; // seconds;
const int32_t START_REQ_KILLS = 2;
const int32_t REQ_KILLS_ACCUMULATOR = 3; // every x transforms the kills required goes up by 1
const float ENEMY_FLING_SPEED = 5;
const int32_t PLAYER_I_FRAMES = 30;
const float PLAYER_SPEED_FACTOR = 3; // how much fast the player is than the ai in the same types
const float X_SHOOTER_BULLET_LIFETIME = 2;
const float X_SHOOTER_BULLET_SPEED = 12;
const float X_SHOOTER_RECOIL = 4;
const float Y_SHOOTER_BULLET_LIFETIME = 1.5;
const float Y_SHOOTER_BULLET_SPEED = 12;
const float XY_SHOOTER_BULLET_LIFETIME = 1.5;
const float XY_SHOOTER_BULLET_SPEED = 12;
const float XY_SHOOTER_RECOIL = 4;
const float DASHER_FLING_Y_DISTANCE = 7;
const float DASHER_FLING_X_DISTANCE = 10;
const float BOMBER_BLAST_RADIUS = 80;
const int32_t MOUTH_OPEN_DURATION = 8;
const float FADE_IN_OUT_TIME = 1 * 30;
const float TRANSFORM_INDICATE_TIME = 1.2;
const float GLOBAL_MUSIC_VOLUME = 0.23;
const char *SAVE_NAME = "save.json";

///////////////////////// STRUCTS /////////////////////////
typedef struct Save_t {
    float highscore[3];
    bool has_done_tutorial;
    bool fullscreen;
    bool pixel_perfect;
    float sound_volume;
    float music_volume;

    // probably later idk
    int32_t server_ipv4;
    int32_t port;
} Save;

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
    bool alive;
    PhysicsObject physx;
    float facing; // direction facing, -1 or 1
    float shown_facing; // for a cool visual effect
    int32_t mouth_open; // for laser animation

    // for telegraphing ai behaviour
    bool wants_to_action; // might be unnecessary idk
    float action_timer; // how long until action will be taken

    float direction; // relevant for ai
    bool player_controlled; // True if this is the current player false means AI
} Character;

typedef struct Particle_t {
    bool sprite_based; // if true the sprite and frame is used
    PhysicsObject physx;
    float lifetime; // in seconds
    float total_lifetime;
    uint64_t id;
    _Atomic bool alive;
    Oct_Sprite sprite;
    Oct_SpriteInstance instance;
    Oct_Texture texture;
} Particle;

typedef struct Projectile_t {
    PhysicsObject physx;
    float lifetime; // in seconds
    float max_lifetime;
    Oct_Texture tex;
    uint64_t id;
    bool player_bullet; // whether the player shot it
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

typedef struct ParticleJob_t {
    int32_t index;
    int32_t count;
    Particle *list;
} ParticleJob;

typedef struct CreateParticlesJob_t {
    int32_t count;
    float x;
    float y;
    float x_vel;
    float y_vel;
    float variation; // 0 is none
    Oct_Texture tex;
    Oct_Sprite spr;
    float lifetime; // seconds
} CreateParticlesJob;

typedef struct MenuState_t {
    int32_t cursor;
    bool quit;
    bool start_game;
    MenuIndices menu;
    StartingMap map;
    StartingBody character;
    float cursor_x;
    float cursor_y;
    float cursor_width;
    float cursor_height;
    int32_t starting_text_box_frame;
    const char *drop_text;
    float fade_in;
    float fade_out;
    float highscore[3];
} MenuState;

MenuState menu_state;

// LEAVE THIS AT THE BOTTOM
typedef struct GameState_t {
    // set when player gets a character
    float lifespan;
    float max_lifespan;
    Character *player;
    float total_time;
    int32_t req_kills; // for transforming
    int32_t current_kills;
    int32_t req_kills_accumulator;
    float displayed_kills; // for lerping a nice val
    int32_t player_iframes; // after getting hit
    bool player_died;
    float player_die_time;
    bool banner_dropped; // animation
    bool got_highscore;
    int32_t frame_count;
    int32_t game_phase;

    bool in_tutorial;
    Oct_Sound outta_time;
    Oct_SpriteInstance fire;
    float shown_clock_percent;
    float score; // literally just 1 per frame
    float fade_in;
    float fade_out;
    float player_transform_time;

    Oct_Tilemap level_map;
    Character characters[MAX_CHARACTERS];
    Projectile projectiles[MAX_PROJECTILES];
    Particle particles[MAX_PARTICLES];
} GameState;

GameState state;

#define NEAR_LEVEL_UP (state.req_kills - 1 == state.current_kills)

///////////////////////// HELPERS /////////////////////////
Projectile *create_projectile(bool player_shot, Oct_Texture tex, float lifetime, float x, float y, float x_speed, float y_speed);
Save parse_save();
void save_game(Save *save);

void create_particles_job(CreateParticlesJob *data) {
    CreateParticlesJob *job = data;
    for (int i = 0; i < job->count; i++) {
        // find a spot in the list for this particle
        int32_t spot = -1;
        for (int j = 0; j < MAX_PARTICLES; j++) {
            if (!state.particles[j].alive) {
                spot = j;
                break;
            }
        }

        if (spot >= 0) {
            Particle *p = &state.particles[spot];
            p->sprite_based = job->spr != OCT_NO_ASSET;
            p->physx = (PhysicsObject){
                    .x = job->x,
                    .y = job->y,
                    .x_vel = job->x_vel + oct_Random(-job->variation, job->variation),
                    .y_vel = job->y_vel + oct_Random(-job->variation, job->variation),
                    .noclip = true
            };
            p->lifetime = job->lifetime;
            p->total_lifetime = job->lifetime;
            p->texture = job->tex;
            if (p->sprite_based) p->sprite = job->spr;
            oct_InitSpriteInstance(&p->instance, job->spr, true);
            p->id = gParticleIDs++;
            p->alive = true;
        }
    }
}

static inline float sign(float x) {
    return x > 0 ? 1 : (x < 0 ? -1 : 0);
}

static inline bool aabb(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2) {
    return x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2;
}

// returns the sprite corresponding to a certain character type
Oct_Sprite character_type_sprite(Character *character) {
    if (character->player_controlled) {
        switch (character->type) {
            case CHARACTER_TYPE_JUMPER: return oct_GetAsset(gBundle, "sprites/playerjumper.json");
            case CHARACTER_TYPE_X_SHOOTER: return oct_GetAsset(gBundle, "sprites/playershooter.json");
            case CHARACTER_TYPE_Y_SHOOTER: return oct_GetAsset(gBundle, "sprites/playershooter.json");
            case CHARACTER_TYPE_XY_SHOOTER: return oct_GetAsset(gBundle, "sprites/playershooter.json");
            case CHARACTER_TYPE_BOMBER: return oct_GetAsset(gBundle, "sprites/playerbomber.json");
            case CHARACTER_TYPE_LASER: return oct_GetAsset(gBundle, "sprites/playerlaser.json");
            case CHARACTER_TYPE_DASHER: return oct_GetAsset(gBundle, "sprites/playerdasher.json");
            default: return OCT_NO_ASSET;
        }
    }

    switch (character->type) {
        case CHARACTER_TYPE_JUMPER: return oct_GetAsset(gBundle, "sprites/jumper.json");
        case CHARACTER_TYPE_X_SHOOTER: return oct_GetAsset(gBundle, "sprites/shooter.json");
        case CHARACTER_TYPE_Y_SHOOTER: return oct_GetAsset(gBundle, "sprites/shooter.json");
        case CHARACTER_TYPE_XY_SHOOTER: return oct_GetAsset(gBundle, "sprites/shooter.json");
        case CHARACTER_TYPE_BOMBER: return oct_GetAsset(gBundle, "sprites/bomber.json");
        case CHARACTER_TYPE_LASER: return oct_GetAsset(gBundle, "sprites/laser.json");
        case CHARACTER_TYPE_DASHER: return oct_GetAsset(gBundle, "sprites/dasher.json");
        default: return OCT_NO_ASSET;
    }
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
    // 21
    for (int i = 0; i < 4; i++) {
        // invisible walls for the player
        if ((this_c && !this_c->player_controlled && wall[i] == 21)) continue;

        if (wall[i]) {
            e.type = wall[i] >= 2 && wall[i] <= 8 ? COLLISION_EVENT_TYPE_BOUNCY_WALL : COLLISION_EVENT_TYPE_WALL;
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

// same as above but walls and projectiles dont count
CollisionEvent collision_at_no_walls(Character *this_c, Projectile *this_p, float x, float y, float width, float height) {
    CollisionEvent e = {.type = COLLISION_EVENT_TYPE_NO_COLLISION};

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

    return e;
}

// Returns true if a horizontal collision was processed
bool process_physics(Character *this_c, Projectile *this_p, PhysicsObject *physx, float x_acceleration, float y_acceleration) {
    bool collision = false;

    // Add acceleration to velocity
    physx->x_vel = oct_Clamp(-SPEED_LIMIT, SPEED_LIMIT, physx->x_vel + x_acceleration);
    physx->y_vel = oct_Clamp(-SPEED_LIMIT, SPEED_LIMIT, physx->y_vel + y_acceleration);

    CollisionEvent ground = collision_at(this_c, this_p, physx->x, physx->y + 1, physx->bb_width, physx->bb_height);
    const bool kinda_touching_ground = ground.type != 0;

    // Friction and gravity
    if (kinda_touching_ground) {
        if (ground.wallIndex != 20)
            physx->x_vel *= (1 - GROUND_FRICTION);
    } else {
        physx->x_vel *= (1 - AIR_FRICTION);
    }
    physx->y_vel += GRAVITY;

    if (physx->noclip) {
        physx->x += physx->x_vel;
        physx->y += physx->y_vel;
        return false;
    }

    // Bouncy dogshit collisions
    CollisionEvent ce = collision_at(this_c, this_p, physx->x + physx->x_vel, physx->y, physx->bb_width, physx->bb_height);
    bool counts_as_collision = ce.type;
    if ((this_p && ce.type == COLLISION_EVENT_TYPE_CHARACTER) || (this_c && ce.type == COLLISION_EVENT_TYPE_PROJECTILE)) {
        counts_as_collision = false;
    }

    if (counts_as_collision) {
        // Get close to the wall
        while (!collision_at(this_c, this_p, physx->x + (sign(physx->x_vel) * 0.1), physx->y, physx->bb_width, physx->bb_height).type)
            physx->x += (sign(physx->x_vel) * 0.1);

        // Sound effect when dashing into a wall
        if (physx->x_vel > PARTICLES_GROUND_IMPACT_SPEED) {
            oct_PlaySound(
                    oct_GetAsset(gBundle, "sounds/bumpwall.wav"),
                    (Oct_Vec2){0.2 * gSoundVolume, 0.2 * gSoundVolume},
                    false);
        }

        // Bounce off the wall slightly
        if (ce.type == COLLISION_EVENT_TYPE_BOUNCY_WALL) {
            physx->x_vel = physx->x_vel * (-BOUNCE_PRESERVED_BOUNCE_WALL);
        } else {
            if (ce.type == COLLISION_EVENT_TYPE_CHARACTER) {
                ce.character->physx.x_vel += physx->x_vel;
            }
            if (ce.type == COLLISION_EVENT_TYPE_PROJECTILE) {
                ce.projectile->physx.x_vel += physx->x_vel;
            }
            physx->x_vel = physx->x_vel * (-BOUNCE_PRESERVED);
        }
        collision = true;
    }
    physx->x += physx->x_vel;

    ce = collision_at(this_c, this_p, physx->x, physx->y + physx->y_vel, physx->bb_width, physx->bb_height);
    counts_as_collision = ce.type;
    if ((this_p && ce.type == COLLISION_EVENT_TYPE_CHARACTER) || (this_c && ce.type == COLLISION_EVENT_TYPE_PROJECTILE)) {
        counts_as_collision = false;
    }
    if (counts_as_collision) {
        // Get close to the wall
        while (!collision_at(this_c, this_p, physx->x, physx->y + (sign(physx->y_vel) * 0.1), physx->bb_width, physx->bb_height).type)
            physx->y += (sign(physx->y_vel) * 0.1);

        // Particles cuz you hit the ground hard
        if (physx->y_vel > PARTICLES_GROUND_IMPACT_SPEED) {
            create_particles_job(&(CreateParticlesJob){
                .variation = 1,
                .y_vel = -2,
                .x_vel = 0,
                .tex = oct_GetAsset(gBundle, "textures/garbageparticle.png"),
                .spr = OCT_NO_ASSET,
                .x = physx->x + (physx->bb_width / 2),
                .y = physx->y + physx->bb_height,
                .count = 10,
                .lifetime = 1
            });
            oct_PlaySound(
                    oct_GetAsset(gBundle, "sounds/bumpwall.wav"),
                    (Oct_Vec2){0.2 * gSoundVolume, 0.2 * gSoundVolume},
                    false);
        }

        // Bounce off the wall slightly
        if (ce.type == COLLISION_EVENT_TYPE_BOUNCY_WALL) {
            physx->y_vel = physx->y_vel * (-BOUNCE_PRESERVED_BOUNCE_WALL);
        } else {
            if (ce.type == COLLISION_EVENT_TYPE_CHARACTER) {
                ce.character->physx.y_vel += physx->y_vel;
            }
            if (ce.type == COLLISION_EVENT_TYPE_PROJECTILE) {
                ce.projectile->physx.y_vel += physx->y_vel;
            }
            physx->y_vel = physx->y_vel * (-BOUNCE_PRESERVED);
        }
    }
    physx->y += physx->y_vel;
    return collision;
}

void draw_character(Character *character) {
    // for iframes
    Oct_Colour c = {1, 1, 1, 1};
    if (character->player_controlled && state.player_iframes > 0 && (state.player_iframes % 2 == 0)) {
        c.a = 0;
    }

    // draw fire effect when time to level up or whatever its called
    if (character->player_controlled && NEAR_LEVEL_UP) {
        // 49, 95
        oct_DrawSpriteInt(
                OCT_INTERPOLATE_ALL, 666,
                oct_GetAsset(gBundle, "sprites/fire.json"), &state.fire,
                (Oct_Vec2){character->physx.x - 33 + (character->physx.bb_width / 2), character->physx.y - 80 + character->physx.bb_height});
    }

    // paper mario effect
    character->shown_facing += (character->facing - character->shown_facing) * 0.5;

    // Draw telegraphing effect
    if (character->wants_to_action && !character->player_controlled) {
        const float x = character->physx.x + (character->physx.bb_width / 2) - 8.5;
        oct_DrawTextureInt(
                OCT_INTERPOLATE_ALL, character->id + 4,
                oct_GetAsset(gBundle, "textures/angry.png"),
                (Oct_Vec2){x, character->physx.y - 17}
                );
    }

    if (character->type == CHARACTER_TYPE_JUMPER || character->type == CHARACTER_TYPE_BOMBER) {
        const float x = character->facing == 1 ? character->physx.x : (character->physx.x + character->physx.bb_width);
        oct_DrawSpriteIntColourExt(
                OCT_INTERPOLATE_ALL, character->id,
                character_type_sprite(character),
                &character->sprite,
                &c,
                (Oct_Vec2) {x, character->physx.y},
                (Oct_Vec2){character->shown_facing, 1},
                0, (Oct_Vec2){0, 0});
    } else if (character->type == CHARACTER_TYPE_LASER) {
        const float x = character->facing == 1 ? character->physx.x : (character->physx.x + character->physx.bb_width);
        character->mouth_open -= 1;
        const Oct_Sprite spr = character->mouth_open > 0 ? oct_GetAsset(gBundle, "sprites/playerlaseropen.json") : character_type_sprite(character);
        oct_DrawSpriteIntColourExt(
                OCT_INTERPOLATE_ALL, character->id,
                spr,
                &character->sprite,
                &c,
                (Oct_Vec2) {x, character->physx.y},
                (Oct_Vec2){character->shown_facing, 1},
                0, (Oct_Vec2){0, 0});
    } else if (character->type == CHARACTER_TYPE_X_SHOOTER) {
        const float x = character->facing == 1 ? character->physx.x : (character->physx.x + character->physx.bb_width);
        const float gun_x = character->facing == 1 ? (character->physx.x + character->physx.bb_width) : character->physx.x;
        oct_DrawSpriteIntColourExt(
                OCT_INTERPOLATE_ALL, character->id,
                character_type_sprite(character),
                &character->sprite,
                &c,
                (Oct_Vec2) {x, character->physx.y},
                (Oct_Vec2){character->shown_facing, 1},
                0, (Oct_Vec2){0, 0});
        oct_DrawTextureIntColourExt(
                OCT_INTERPOLATE_ALL, character->id + 3,
                oct_GetAsset(gBundle, "textures/gun.png"),
                &c,
                (Oct_Vec2) {gun_x, character->physx.y - 8},
                (Oct_Vec2){character->shown_facing, 1},
                0, (Oct_Vec2){0, 0});
    } else if (character->type == CHARACTER_TYPE_XY_SHOOTER) {
        const float x = character->facing == 1 ? character->physx.x : (character->physx.x + character->physx.bb_width);
        const float gun_x = character->facing == 1 ? (character->physx.x + character->physx.bb_width - 4) : character->physx.x + 4;
        oct_DrawSpriteIntColourExt(
                OCT_INTERPOLATE_ALL, character->id,
                character_type_sprite(character),
                &character->sprite,
                &c,
                (Oct_Vec2) {x, character->physx.y},
                (Oct_Vec2){character->shown_facing, 1},
                0, (Oct_Vec2){0, 0});
        oct_DrawTextureIntColourExt(
                OCT_INTERPOLATE_ALL, character->id + 3,
                oct_GetAsset(gBundle, "textures/xygun.png"),
                &c,
                (Oct_Vec2) {gun_x, character->physx.y - 16},
                (Oct_Vec2){character->shown_facing, 1},
                0, (Oct_Vec2){0, 0});
    } else if (character->type == CHARACTER_TYPE_Y_SHOOTER) {
        const float x = character->facing == 1 ? character->physx.x : (character->physx.x + character->physx.bb_width);
        const float gun_x = character->facing == 1 ? (x + (character->physx.bb_width / 2) - (19 / 2)) : (x + (character->physx.bb_width / 2) - (19 / 2) + 6);
        oct_DrawSpriteIntColourExt(
                OCT_INTERPOLATE_ALL, character->id,
                character_type_sprite(character),
                &character->sprite,
                &c,
                (Oct_Vec2) {x, character->physx.y},
                (Oct_Vec2){character->shown_facing, 1},
                0, (Oct_Vec2){0, 0});
        oct_DrawTextureIntColourExt(
                OCT_INTERPOLATE_ALL, character->id + 3,
                oct_GetAsset(gBundle, "textures/ygun.png"),
                &c,
                (Oct_Vec2) {gun_x, character->physx.y - 23},
                (Oct_Vec2){character->shown_facing, 1},
                0, (Oct_Vec2){0, 0});
    } else if (character->type == CHARACTER_TYPE_DASHER) {
        const float x = character->facing == 1 ? character->physx.x : (character->physx.x + character->physx.bb_width);
        const float gun_x = character->facing == 1 ? (character->physx.x + character->physx.bb_width) : character->physx.x;
        const float gun2_x = character->facing == -1 ? (character->physx.x + character->physx.bb_width) : character->physx.x;
        oct_DrawSpriteIntColourExt(
                OCT_INTERPOLATE_ALL, character->id,
                character_type_sprite(character),
                &character->sprite,
                &c,
                (Oct_Vec2) {x, character->physx.y},
                (Oct_Vec2){character->shown_facing, 1},
                0, (Oct_Vec2){0, 0});
        oct_DrawTextureIntColourExt(
                OCT_INTERPOLATE_ALL, character->id + 3,
                oct_GetAsset(gBundle, "textures/jacked.png"),
                &c,
                (Oct_Vec2) {gun_x, character->physx.y - 4},
                (Oct_Vec2){character->shown_facing, 1},
                0, (Oct_Vec2){0, 0});
        oct_DrawTextureIntColourExt(
                OCT_INTERPOLATE_ALL, character->id + 5,
                oct_GetAsset(gBundle, "textures/jacked.png"),
                &c,
                (Oct_Vec2) {gun2_x, character->physx.y - 4},
                (Oct_Vec2){-character->shown_facing, 1},
                0, (Oct_Vec2){0, 0});
    } // TODO: The rest of these
}
void kill_character(bool player_is_killer, Character *character, bool dramatic);

// bwah
void imma_firin_muh_lazor(Character *character) {
    oct_PlaySound(
            oct_GetAsset(gBundle, "sounds/laser.wav"),
            (Oct_Vec2){1 * gSoundVolume, 1 * gSoundVolume},
            0);
    create_particles_job(&(CreateParticlesJob) {
            .lifetime = 0.8,
            .count = 1,
            .variation = 1,
            .spr = OCT_NO_ASSET,
            .tex = oct_GetAsset(gBundle, "textures/lazer.png"),
            .x = character->physx.x + (character->physx.bb_width / 2) - (character->facing == -1 ? 512 : 0),
            .y = character->physx.y + (character->physx.bb_height / 2) - 4,
            .y_vel = -2,
            .x_vel = (character->facing == -1 ? 3 : 3)
    });
    for (int i = 0 ; i < 10; i++) {
        CollisionEvent kaboom = collision_at_no_walls(
                character, null,
                character->physx.x + (character->physx.bb_width / 2) - (character->facing == -1 ? 512 : 0),
                character->physx.y - (character->physx.bb_width / 2) - 8,
                512,
                16);
        if (kaboom.type == COLLISION_EVENT_TYPE_CHARACTER) {
            kill_character(character->player_controlled, kaboom.character, true);
        }
    }
    character->mouth_open = MOUTH_OPEN_DURATION;
}

// blow the fuck up
void blow_up(Character *character) {
    oct_PlaySound(
            oct_GetAsset(gBundle, "sounds/kaboom.wav"),
            (Oct_Vec2){1 * gSoundVolume, 1 * gSoundVolume},
            0);
    create_particles_job(&(CreateParticlesJob) {
            .lifetime = 0.8,
            .count = 1,
            .variation = 0,
            .spr = oct_GetAsset(gBundle, "sprites/kaboom.json"),
            .tex = OCT_NO_ASSET,
            .x = character->physx.x + (character->physx.bb_width / 2) - 64,
            .y = character->physx.y + (character->physx.bb_height / 2) - 64,
            .y_vel = -2,
            .x_vel = 3
    });
    for (int i = 0 ; i < 10; i++) {
        CollisionEvent kaboom = collision_at_no_walls(
                character, null,
                character->physx.x - (character->physx.bb_width / 2) - BOMBER_BLAST_RADIUS,
                character->physx.y - (character->physx.bb_width / 2) - BOMBER_BLAST_RADIUS,
                BOMBER_BLAST_RADIUS * 2,
                BOMBER_BLAST_RADIUS * 2);
        if (kaboom.type == COLLISION_EVENT_TYPE_CHARACTER) {
            kill_character(character->player_controlled, kaboom.character, true);
        }
    }
}

void shoot_x_bullet(Character *character);
void shoot_y_bullet(Character *character);
void shoot_xy_bullet(Character *character);
InputProfile process_player(Character *character) {
    InputProfile input = {0};

    if (state.player_died) return input;
    state.player_iframes -= 1;
    if (oct_KeyDown(OCT_KEY_LEFT) || oct_GamepadButtonDown(0, OCT_GAMEPAD_BUTTON_DPAD_LEFT) ||
        oct_GamepadLeftAxisX(0) < 0) {
        input.x_acc = -(ACCELERATION_VALUES[character->type] * PLAYER_SPEED_FACTOR);
    } else if (oct_KeyDown(OCT_KEY_RIGHT) || oct_GamepadButtonDown(0, OCT_GAMEPAD_BUTTON_DPAD_RIGHT) ||
               oct_GamepadLeftAxisX(0) > 0) {
        input.x_acc = (ACCELERATION_VALUES[character->type] * PLAYER_SPEED_FACTOR);
    }
    const bool kinda_touching_ground = collision_at(character, null, character->physx.x, character->physx.y + 2, character->physx.bb_width, character->physx.bb_height).type;

    // jumping (player can always jump)
    if (kinda_touching_ground && (oct_KeyPressed(OCT_KEY_UP) || oct_GamepadButtonPressed(0, OCT_GAMEPAD_BUTTON_A))) {
        input.y_acc = -PLAYER_JUMP_SPEED;
        oct_PlaySound(
                oct_GetAsset(gBundle, "sounds/jump.wav"),
                (Oct_Vec2){0.5 * gSoundVolume, 0.5 * gSoundVolume},
                false);
    }

    // action depending on type of entity
    if (oct_KeyPressed(OCT_KEY_SPACE) || oct_GamepadButtonPressed(0, OCT_GAMEPAD_BUTTON_X)) {
        if (character->type == CHARACTER_TYPE_JUMPER) {
            input.y_acc = JUMPER_DESCEND_SPEED;
        } else if (character->type == CHARACTER_TYPE_LASER) {
            imma_firin_muh_lazor(character);
        } else if (character->type == CHARACTER_TYPE_X_SHOOTER) {
            shoot_x_bullet(character);
        } else if (character->type == CHARACTER_TYPE_Y_SHOOTER) {
            shoot_y_bullet(character);
        } else if (character->type == CHARACTER_TYPE_XY_SHOOTER) {
            shoot_xy_bullet(character);
        } else if (character->type == CHARACTER_TYPE_DASHER && kinda_touching_ground) {
            oct_PlaySound(oct_GetAsset(gBundle, "sounds/punch.wav"),
                          (Oct_Vec2) {1 * gSoundVolume, 1 * gSoundVolume}, false);
            character->physx.y_vel -= DASHER_FLING_Y_DISTANCE;
            CollisionEvent bigass = collision_at_no_walls(character, null, character->physx.x - (character->physx.bb_width * 1.5), character->physx.y-20, character->physx.bb_width * 4, character->physx.bb_height + 16);
            if (bigass.type == COLLISION_EVENT_TYPE_CHARACTER) {
                bigass.character->physx.y_vel -= DASHER_FLING_Y_DISTANCE;
                kill_character(true, bigass.character, true);
            }
        } else if (character->type == CHARACTER_TYPE_BOMBER) {
            blow_up(character);
            kill_character(false, character, false);
        }
    }

    // dont fall off edge
    if (character->physx.y > GAME_HEIGHT) {
        character->physx.x = 15.5 * 16;
        character->physx.y = 11 * 16;
    }

    return input;
}

// transforms the player into this dude
void take_body(Character *character) {
    // reset kills and show a nice particle effect
    state.req_kills_accumulator++;
    if (state.req_kills_accumulator == REQ_KILLS_ACCUMULATOR) {
        state.req_kills_accumulator = 0;
        state.req_kills++;
    }
    state.player_transform_time = state.total_time;
    state.current_kills = 0;
    create_particles_job(&(CreateParticlesJob){
            .lifetime = 3,
            .count = 10,
            .variation = 1,
            .spr = OCT_NO_ASSET,
            .tex = oct_GetAsset(gBundle, "textures/thumbsup.png"),
            .x = GAME_WIDTH / 2,
            .y = 48,
            .y_vel = -2
    });
    state.player_iframes = PLAYER_I_FRAMES;

    // Take dudes body
    Character *player = state.player;
    state.player = character;
    character->player_controlled = true;
    character->alive = true;
    state.max_lifespan = CHARACTER_TYPE_LIFESPANS[character->type];
    state.lifespan = CHARACTER_TYPE_LIFESPANS[character->type];

    oct_PlaySound(
            oct_GetAsset(gBundle, "sounds/transform.wav"),
            (Oct_Vec2){1 * gSoundVolume, 1 * gSoundVolume},
            false);

    // kill old player
    player->player_controlled = false;
    kill_character(false, player, true);
}

// shoot horizontal bullet
void shoot_x_bullet(Character *character) {
    const float x = character->facing == 1 ? character->physx.x + character->physx.bb_width + 10 : character->physx.x -12;
    create_projectile(
            character->player_controlled,
            oct_GetAsset(gBundle, "textures/bullet.png"),
            X_SHOOTER_BULLET_LIFETIME,
            x,
            character->physx.y,
            X_SHOOTER_BULLET_SPEED * character->facing,
            0);
    oct_PlaySound(
            oct_GetAsset(gBundle, "sounds/gunshot.wav"),
            (Oct_Vec2){1 * gSoundVolume, 1 * gSoundVolume},
            false);
    character->physx.x_vel -= X_SHOOTER_RECOIL * character->facing;
}

// shoot vertical bullet
void shoot_y_bullet(Character *character) {
    const float x = character->physx.x + (character->physx.bb_width / 2);
    create_projectile(
            character->player_controlled,
            oct_GetAsset(gBundle, "textures/bullet.png"),
            Y_SHOOTER_BULLET_LIFETIME,
            x,
            character->physx.y - 10,
            0,
            -Y_SHOOTER_BULLET_SPEED);
    oct_PlaySound(
            oct_GetAsset(gBundle, "sounds/gunshot.wav"),
            (Oct_Vec2){1 * gSoundVolume, 1 * gSoundVolume},
            false);
}

// shoot diagonal bullet
void shoot_xy_bullet(Character *character) {
    const float x = character->facing == 1 ? character->physx.x + character->physx.bb_width + 10 : character->physx.x -12;
    create_projectile(
            character->player_controlled,
            oct_GetAsset(gBundle, "textures/bullet.png"),
            XY_SHOOTER_BULLET_LIFETIME,
            x,
            character->physx.y - 10,
            XY_SHOOTER_BULLET_SPEED * character->facing,
            -XY_SHOOTER_BULLET_SPEED);
    oct_PlaySound(
            oct_GetAsset(gBundle, "sounds/gunshot.wav"),
            (Oct_Vec2){1 * gSoundVolume, 1 * gSoundVolume},
            false);
    character->physx.x_vel -= XY_SHOOTER_RECOIL * character->facing;
}

// checks if the user got a highscore and records it if so
void check_highscore() {
    Save save = parse_save();
    if (save.highscore[menu_state.map] < state.score) {
        state.got_highscore = true;
        save.highscore[menu_state.map] = state.score;
        save_game(&save);
        // todo - possible global leaderboard
    }
}

void kill_character(bool player_is_the_killer, Character *character, bool dramatic) {
    if (!character->player_controlled) {
        character->alive = false;
        create_particles_job(&(CreateParticlesJob){
            .lifetime = 3,
            .count = 1,
            .variation = dramatic ? 3 : 1,
            .spr = character_type_sprite(character),
            .tex = OCT_NO_ASSET,
            .x = character->physx.x,
            .y = character->physx.y,
            .y_vel = dramatic ? -10 : -2
        });

        // small sound
        oct_PlaySound(
                oct_GetAsset(gBundle, "sounds/jumpenemy.wav"),
                (Oct_Vec2){1 * gSoundVolume, 1 * gSoundVolume},
                false);

        create_particles_job(&(CreateParticlesJob){
                .lifetime = 3,
                .count = dramatic ? 20 : 8,
                .variation = dramatic ? 3 : 1,
                .spr = OCT_NO_ASSET,
                .tex = oct_GetAsset(gBundle, "textures/blood.png"),
                .x = character->physx.x + (character->physx.bb_width / 2),
                .y = character->physx.y + (character->physx.bb_height / 2),
                .y_vel = -2
        });

        if (player_is_the_killer) {
            state.current_kills += 1;
            if (state.current_kills >= state.req_kills) {
                take_body(character);
            }
        }
        if (!state.player_died) {
            state.score += ADDITIONAL_SCORE[character->type];
        }
    } else if (state.player_iframes <= 0 && !state.player_died && !state.in_tutorial) {
        state.player_iframes = PLAYER_I_FRAMES;
        if (state.lifespan <= 0) {
            oct_PlaySound(
                    oct_GetAsset(gBundle, "sounds/die.wav"),
                    (Oct_Vec2) {1 * gSoundVolume, 1 * gSoundVolume},
                    false);

            create_particles_job(&(CreateParticlesJob) {
                    .lifetime = 0.8,
                    .count = 1,
                    .variation = 0,
                    .spr = oct_GetAsset(gBundle, "sprites/explosion.json"),
                    .tex = OCT_NO_ASSET,
                    .x = character->physx.x + (character->physx.bb_width / 2) - 20,
                    .y = character->physx.y + (character->physx.bb_height / 2) - 20,
                    .y_vel = -2
            });
            create_particles_job(&(CreateParticlesJob){
                    .lifetime = 3,
                    .count = 1,
                    .variation = dramatic ? 3 : 1,
                    .spr = character_type_sprite(character),
                    .tex = OCT_NO_ASSET,
                    .x = character->physx.x,
                    .y = character->physx.y,
                    .y_vel = -2
            });
            character->player_controlled = false;
            character->alive = false;
            state.player_died = true;
            state.player_die_time = state.total_time;
            check_highscore();
        } else {
            state.lifespan *= 0.75;

            oct_PlaySound(
                    oct_GetAsset(gBundle, "sounds/jumpenemy.wav"),
                    (Oct_Vec2) {1 * gSoundVolume, 1 * gSoundVolume},
                    false);

            create_particles_job(&(CreateParticlesJob){
                    .lifetime = 3,
                    .count = 8,
                    .variation = 1,
                    .spr = OCT_NO_ASSET,
                    .tex = oct_GetAsset(gBundle, "textures/blood.png"),
                    .x = character->physx.x + (character->physx.bb_width / 2),
                    .y = character->physx.y + (character->physx.bb_height / 2),
                    .y_vel = -2
            });
        }
    }
}

InputProfile pre_process_ai(Character *character) {
    InputProfile input = {0};

    // Get input from ai
    input.x_acc = ACCELERATION_VALUES[character->type] * character->direction;
    const bool kinda_touching_ground = collision_at(character, null, character->physx.x, character->physx.y + 2, character->physx.bb_width, character->physx.bb_height).type != 0;

    // Jumpers might jump every now and again
    if (character->type == CHARACTER_TYPE_JUMPER) {
        if (gFrameCounter % ACTION_CHANCE_FREQUENCY[character->type] == 0 &&
            oct_Random(0, 1) < ACTION_CHANCE[character->type] &&
            kinda_touching_ground &&
            character->action_timer <= ACTION_COOLDOWNS[character->type] &&
            character->physx.y + character->physx.bb_height > 3 * 16) {

            character->wants_to_action = true;
            character->action_timer = ACTION_TELEGRAPH_TIMES[character->type];
        }

        if (character->wants_to_action && character->action_timer <= 0) {
            input.y_acc = -PLAYER_JUMP_SPEED;
            character->wants_to_action = false;
        }
    } else if (character->type == CHARACTER_TYPE_X_SHOOTER || character->type == CHARACTER_TYPE_Y_SHOOTER || character->type == CHARACTER_TYPE_XY_SHOOTER) {
        if (gFrameCounter % ACTION_CHANCE_FREQUENCY[character->type] == 0 &&
            oct_Random(0, 1) < ACTION_CHANCE[character->type] &&
            kinda_touching_ground &&
            character->action_timer <= ACTION_COOLDOWNS[character->type] &&
            character->physx.y + character->physx.bb_height > 3 * 16) {

            character->wants_to_action = true;
            character->action_timer = ACTION_TELEGRAPH_TIMES[character->type];
        }

        if (character->wants_to_action && character->action_timer <= 0) {
            if (character->type == CHARACTER_TYPE_X_SHOOTER)
                shoot_x_bullet(character);
            else if (character->type == CHARACTER_TYPE_Y_SHOOTER)
                shoot_y_bullet(character);
            else
                shoot_xy_bullet(character);
            character->wants_to_action = false;
        }
    } else if (character->type == CHARACTER_TYPE_LASER) {
        if (gFrameCounter % ACTION_CHANCE_FREQUENCY[character->type] == 0 &&
            oct_Random(0, 1) < ACTION_CHANCE[character->type] &&
            kinda_touching_ground &&
            character->action_timer <= ACTION_COOLDOWNS[character->type] &&
            character->physx.y + character->physx.bb_height > 3 * 16) {

            character->wants_to_action = true;
            character->action_timer = ACTION_TELEGRAPH_TIMES[character->type];
        }

        if (character->wants_to_action && character->action_timer <= 0) {
            imma_firin_muh_lazor(character);
            character->wants_to_action = false;
        }
    } else if (character->type == CHARACTER_TYPE_BOMBER) {
        if (gFrameCounter % ACTION_CHANCE_FREQUENCY[character->type] == 0 &&
            oct_Random(0, 1) < ACTION_CHANCE[character->type] &&
            kinda_touching_ground &&
            character->action_timer <= ACTION_COOLDOWNS[character->type] &&
            character->physx.y + character->physx.bb_height > 3 * 16) {

            character->wants_to_action = true;
            character->action_timer = ACTION_TELEGRAPH_TIMES[character->type];
        }

        if (character->wants_to_action && character->action_timer <= 0) {
            blow_up(character);
            kill_character(false, character, true);
        }
    } else if (character->type == CHARACTER_TYPE_DASHER) {
        // dasher is always pissed
        character->wants_to_action = true;

        if (kinda_touching_ground) {
            CollisionEvent left = collision_at(character, null, character->physx.x + character->physx.bb_width, character->physx.y, character->physx.bb_width, character->physx.bb_height);
            CollisionEvent right = collision_at(character, null, character->physx.x - character->physx.bb_width, character->physx.y, character->physx.bb_width, character->physx.bb_height);
            if (left.type == COLLISION_EVENT_TYPE_CHARACTER) {
                left.character->physx.y_vel -= DASHER_FLING_Y_DISTANCE;
                character->physx.y_vel -= DASHER_FLING_Y_DISTANCE;
                character->physx.x_vel -= DASHER_FLING_X_DISTANCE;
                kill_character(false, left.character, true);
                oct_PlaySound(oct_GetAsset(gBundle, "sounds/punch.wav"), (Oct_Vec2){1 * gSoundVolume, 1 * gSoundVolume}, false);
            } else if (right.type == COLLISION_EVENT_TYPE_CHARACTER) {
                right.character->physx.y_vel -= DASHER_FLING_Y_DISTANCE;
                character->physx.y_vel -= DASHER_FLING_Y_DISTANCE;
                character->physx.x_vel += DASHER_FLING_X_DISTANCE;
                kill_character(false, right.character, true);
                oct_PlaySound(oct_GetAsset(gBundle, "sounds/punch.wav"), (Oct_Vec2){1 * gSoundVolume, 1 * gSoundVolume}, false);
            }
        }
    }

    character->action_timer -= 1.0/30.0;

    return input;
}

void process_character(Character *character) {
    // This is only to handle input
    InputProfile input = {0};
    if (character->player_controlled) {
        input = process_player(character);
    } else {
        input = pre_process_ai(character);
    }

    if (input.x_acc != 0) {
        character->facing = sign(input.x_acc);
    }
    const bool x_coll = process_physics(character, null, &character->physx, input.x_acc, input.y_acc);

    // Type specific stuff
    if (character->type == CHARACTER_TYPE_JUMPER) {
        // Jump on enemy head should kill them
        const CollisionEvent y_collision = collision_at(character, null, character->physx.x - 5, character->physx.y + 3, character->physx.bb_width + 5, character->physx.bb_height);
        if (y_collision.type == COLLISION_EVENT_TYPE_CHARACTER) {
            kill_character(character->player_controlled, y_collision.character, false);

            character->physx.y_vel -= PLAYER_JUMP_SPEED;
            character->physx.x_vel = oct_Random(-ENEMY_FLING_SPEED, ENEMY_FLING_SPEED);
        }
    }

    // ai controls post physics
    if (!character->player_controlled) {
        if (x_coll)
            character->direction *= -1;

        // If they fall out the map they die :skull: -- player will handle their own deaths
        if (character->physx.y > GAME_HEIGHT) {
            character->alive = false;
        }
    } else {
        // player specific stuff
    }

    draw_character(character);
}

void process_particle(Particle *particle) {
    process_physics(null, null, &particle->physx, 0, 0);
    const float percent = particle->lifetime / particle->total_lifetime;

    // draw
    if (particle->sprite_based) {
        oct_DrawSpriteIntColourExt(
                OCT_INTERPOLATE_ALL, particle->id,
                particle->sprite, &particle->instance,
                &(Oct_Colour){1, 1, 1, percent},
                (Oct_Vec2){particle->physx.x, particle->physx.y},
                (Oct_Vec2){percent, percent},
                0, (Oct_Vec2){0, 0});
    } else {
        oct_DrawTextureIntColourExt(
                OCT_INTERPOLATE_ALL, particle->id,
                particle->texture,
                &(Oct_Colour){1, 1, 1, percent},
                (Oct_Vec2){particle->physx.x, particle->physx.y},
                (Oct_Vec2){percent, percent},
                0, (Oct_Vec2){0, 0});
    }

    // kill
    particle->lifetime -= 1.0/30.0;
    if (particle->lifetime <= 0) {
        particle->alive = false;
    }
}

/*
void particle_job(void *data) {
    ParticleJob *particle_job = data;

    for (int i = particle_job->index; i < particle_job->count + particle_job->index; i++) {
        if (!particle_job->list[i].alive) return;
        process_particle(&particle_job->list[i]);
    }
}

void queue_particles_jobs(Oct_Allocator allocator) {
    const int particle_job_size = 25;
    for (int i = 0; i < MAX_PARTICLES / particle_job_size; i++) {
        ParticleJob job = {
                .list = state.particles,
                .index = i * particle_job_size,
                .count = particle_job_size
        };
        void *dest = oct_Malloc(allocator, sizeof(ParticleJob));
        memcpy(dest, &job, sizeof(ParticleJob));
        oct_QueueJob(particle_job, dest);
    }
}*/

/*
void create_particles(CreateParticlesJob *job) {
    oct_QueueJob(create_particles_job, job);
}*/

void process_projectile(Projectile *projectile) {
    process_physics(null, projectile, &projectile->physx, 0, 0);

    // hit opps
    CollisionEvent event = collision_at(
            null, projectile,
            projectile->physx.x,
            projectile->physx.y,
            projectile->physx.bb_width,
            projectile->physx.bb_height);
    if (event.type == COLLISION_EVENT_TYPE_CHARACTER) {
        kill_character(projectile->player_bullet, event.character, false);
        create_particles_job(&(CreateParticlesJob){
                .variation = 1,
                .y_vel = 0,
                .x_vel = 0,
                .tex = oct_GetAsset(gBundle, "textures/bullet.png"),
                .spr = OCT_NO_ASSET,
                .x = projectile->physx.x,
                .y = projectile->physx.y,
                .count = 1,
                .lifetime = 1
        });
        projectile->alive = false;
    }

    // lifetime
    projectile->lifetime -= 1.0 / 30.0;
    if (projectile->lifetime <= 0) {
        projectile->alive = false;
        create_particles_job(&(CreateParticlesJob){
                .variation = 1,
                .y_vel = 0,
                .x_vel = 0,
                .tex = oct_GetAsset(gBundle, "textures/bullet.png"),
                .spr = OCT_NO_ASSET,
                .x = projectile->physx.x,
                .y = projectile->physx.y,
                .count = 1,
                .lifetime = 1
        });

    }

    // draw
    oct_DrawTextureInt(
            OCT_INTERPOLATE_ALL, projectile->id,
            projectile->tex,
            (Oct_Vec2){projectile->physx.x, projectile->physx.y}
            );
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
            oct_InitSpriteInstance(&slot->sprite, character_type_sprite(slot), true);
            slot->physx.bb_width = 12;
            slot->physx.bb_height = 12;
            slot->facing = 1;
            slot->id = gParticleIDs;
            gParticleIDs += 10;

            break;
        }
    }
    return slot;
}

// Adds an ai (higher level version of add_character)
Character *add_ai(CharacterType type) {
    const bool spawn_left = oct_Random(0, 1) > 0.5;
    float x_spawn;
    if (spawn_left) {
        x_spawn = 1.5 * 16;
    } else {
        x_spawn = 29.5 * 16;
    }

    return add_character(&(Character){
            .type = type,
            .physx = {
                    .x = x_spawn,
                    .y = -16,
            },
            .direction = spawn_left ? 1 : -1
    });
}

Projectile *create_projectile(bool player_shot, Oct_Texture tex, float lifetime, float x, float y, float x_speed, float y_speed) {
    Projectile *slot = null;
    for (int i = 0; i < MAX_CHARACTERS; i++) {
        if (!state.projectiles[i].alive) {
            slot = &state.projectiles[i];
            slot->alive = true;

            slot->physx.bb_width = oct_TextureWidth(tex);
            slot->physx.bb_height = oct_TextureHeight(tex);
            slot->physx.x = x - (slot->physx.bb_width / 2);
            slot->physx.y = y - (slot->physx.bb_height / 2);
            slot->physx.x_vel = x_speed;
            slot->physx.y_vel = y_speed;
            slot->lifetime = lifetime;
            slot->max_lifetime = lifetime;
            slot->tex = tex;
            slot->player_bullet = player_shot;
            slot->id = gParticleIDs++;

            // we wont make projectiles in spots where they are already colliding
            const CollisionEvent event = collision_at(
                    null,
                    slot,
                    slot->physx.x,
                    slot->physx.y,
                    slot->physx.bb_width,
                    slot->physx.bb_height);
            if (event.type == COLLISION_EVENT_TYPE_WALL || event.type == COLLISION_EVENT_TYPE_BOUNCY_WALL || event.type == COLLISION_EVENT_TYPE_PROJECTILE) {
                slot->alive = false;
                slot = null;
            }

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
    state.req_kills = START_REQ_KILLS;
    oct_InitSpriteInstance(&state.fire, oct_GetAsset(gBundle, "sprites/fire.json"), true);
    state.player_transform_time = -5;
    state.outta_time = UINT64_MAX;

    // Open json with level
    const char *maps[] = {"map1.tmj", "map2.tmj", "map3.tmj"};
    uint32_t size;
    uint8_t *data = oct_ReadFile(maps[menu_state.map], gAllocator, &size);
    cJSON *json = cJSON_ParseWithLength((void *)data, size);
    if (!data || !json)
        oct_Raise(OCT_STATUS_FILE_DOES_NOT_EXIST, true, "no level file womp womp");

    // Find level data in the tiled json {"layers": [{"data": [0, 0, ...]}]}
    cJSON *level_data = cJSON_GetObjectItem(
            cJSON_GetArrayItem(
                    cJSON_GetObjectItem(json, "layers"),
                    0),
            "data");

    if (!level_data)
        oct_Raise(OCT_STATUS_FILE_DOES_NOT_EXIST, true, "map json wrong :skull:");

    for (int y = 0; y < LEVEL_HEIGHT; y++) {
        for (int x = 0; x < LEVEL_WIDTH; x++) {
            int32_t item = (int)cJSON_GetNumberValue(cJSON_GetArrayItem(level_data, (y * LEVEL_WIDTH) + x));
            oct_SetTilemap(state.level_map, x, y, item);
        }
    }
    cJSON_Delete(json);

    Save s = parse_save();
    state.in_tutorial = !s.has_done_tutorial;
    s.has_done_tutorial = true;
    save_game(&s);

    // Add the player
    state.player = add_character(&(Character){
        .type = menu_state.character == STARTING_BODY_JUMPER ? CHARACTER_TYPE_JUMPER : CHARACTER_TYPE_Y_SHOOTER,
        .player_controlled = true,
        .physx = {
                .x = 15.5 * 16,
                .y = 11 * 16,
        }
    });
    state.lifespan = PLAYER_STARTING_LIFESPAN;
    state.max_lifespan = PLAYER_STARTING_LIFESPAN;
    state.fade_in = FADE_IN_OUT_TIME;

    // play game music
    oct_StopSound(gPlayingMusic);
    if (oct_Random(0, 1) > 0.5) {
        gPlayingMusic = oct_PlaySound(
                oct_GetAsset(gBundle, "sounds/ost1.ogg"),
                (Oct_Vec2){GLOBAL_MUSIC_VOLUME * gMusicVolume, GLOBAL_MUSIC_VOLUME * gMusicVolume},
                true);
    } else {
        gPlayingMusic = oct_PlaySound(
                oct_GetAsset(gBundle, "sounds/ost2.ogg"),
                (Oct_Vec2){GLOBAL_MUSIC_VOLUME * gMusicVolume, GLOBAL_MUSIC_VOLUME * gMusicVolume},
                true);
    }
}

float str_count(const char *s, char c) {
    if (!s) return 0;
    float count = 0;
    while (*s) {
        if (*s == c) count += 1;
        s++;
    }
    return count;
}

// fuck you
float longest_len(const char *s, char c) {
    if (!s) return 0;
    float count = 0;
    float big = 0;
    while (*s) {
        count += 1;
        if (*s == c) {
            if (count > big) big = count;
            count = 0;
        }
        s++;
    }
    if (count > big) big = count;
    return big - 1;
}

void draw_text_box(float x, float y, const char *txt) {
    const float size_x = longest_len(txt, '\n') * 7;
    const float size_y = 11 + (11 * str_count(txt, '\n'));

    oct_DrawRectangleIntColour(
            OCT_INTERPOLATE_ALL, 89,
            &(Oct_Colour){0, 0, 0, 1},
            &(Oct_Rectangle){
                .size = {size_x + 2, size_y + 5},
                .position = {x - 1 - (size_x / 2), y - 1 - (size_y / 2)}
            },
            true, 1);
    oct_DrawTextInt(
            OCT_INTERPOLATE_ALL, 90,
            oct_GetAsset(gBundle, "fnt_monogram"),
            (Oct_Vec2){roundf(x - (size_x / 2)), roundf(y - (size_y / 2))},
            1,
            "%s", txt);
}

void handle_tutorial() {
    if (!state.in_tutorial) return;
    if (state.total_time >= 45) state.in_tutorial = false;

    /*
     * order is as follows:
     *  - welcome to game
     *  - press arrow keys to move and space to interact
     *  - you only have this much time here before you die
     *  - take over dead bodies by filling the kill gauge
     *  - stay alive
     * */

    if (state.total_time < 10) {
        draw_text_box(GAME_WIDTH / 2, GAME_HEIGHT / 2, "Welcome to the game!\nTake some time to learn the controls.");
    } else if (state.total_time < 20) {
        draw_text_box(GAME_WIDTH / 2, GAME_HEIGHT / 2, "Press arrow keys to move\nand space to use your action.\nYour action depends on the body\nyou inhabit.");
    } else if (state.total_time < 30) {
        draw_text_box(GAME_WIDTH / 2, 64, "You will die when this time runs out.\nTake over bodies to get more time.");
        oct_DrawTextureInt(
                OCT_INTERPOLATE_ALL, 8,
                oct_GetAsset(gBundle, "textures/pointer.png"),
                (Oct_Vec2){160 + (sin(oct_Time() * 2) * 10), 24});
    } else if (state.total_time < 40) {
        draw_text_box(GAME_WIDTH / 2, 100, "Take over bodies by filling up\nthis kill gauge.");
        oct_DrawTextureInt(
                OCT_INTERPOLATE_ALL, 8,
                oct_GetAsset(gBundle, "textures/pointer.png"),
                (Oct_Vec2){155 + (sin(oct_Time() * 2) * 10), 58});
    } else if (state.total_time < 45) {
        draw_text_box(GAME_WIDTH / 2, GAME_HEIGHT / 2, "Watch out for the bouncy\nwalls and have fun!");
    }
}

void draw_time_bar() {
    const float percent = oct_Clamp(0, 1, state.lifespan / state.max_lifespan);
    const float clock_x = 232;
    const float clock_y = 17 - 3;
    const float clock_hand_x = 255;
    const float clock_hand_y = 40 - 3;

    if (state.player_iframes > 0) {
        oct_DrawTextureColour(
                oct_GetAsset(gBundle, "textures/clock.png"),
                &(Oct_Colour){1, 0.5, 0.5, 1},
                (Oct_Vec2){clock_x, clock_y}
        );
    } else {
        oct_DrawTexture(
                oct_GetAsset(gBundle, "textures/clock.png"),
                (Oct_Vec2){clock_x, clock_y}
        );
    }

    state.shown_clock_percent += (percent - state.shown_clock_percent) * 0.3;
    oct_DrawTextureIntExt(
            OCT_INTERPOLATE_ALL, 420,
            oct_GetAsset(gBundle, "textures/clockhand.png"),
            (Oct_Vec2){clock_hand_x, clock_hand_y},
            (Oct_Vec2){1, 1},
            -state.shown_clock_percent * M_PI * 2,
            (Oct_Vec2){OCT_ORIGIN_MIDDLE, OCT_ORIGIN_MIDDLE});
}

void draw_score() {
    const Oct_FontAtlas kingdom = oct_GetAsset(gBundle, "fnt_kingdom");
    const float y = 2;
    // If user is 1 kill away from transforming, tell them
    if (state.req_kills -1 == state.current_kills && !state.player_died) {
        Oct_Vec2 text_size;
        const float scale = (sin(oct_Time() * 4) / 4) + 1;
        oct_GetTextSize(kingdom, text_size, scale, "Transform!");
        oct_DrawTextColour(kingdom, (Oct_Vec2) {(GAME_WIDTH / 2) - (text_size[0] / 2) + 1, y},
                           &(Oct_Colour) {0, 0, 0, 1}, scale, "Transform!");
        oct_DrawText(kingdom, (Oct_Vec2) {(GAME_WIDTH / 2) - (text_size[0] / 2), y}, scale, "Transform!");
    } else {
        if (state.player_died && state.got_highscore) {
            const float scale = (sin(oct_Time() * 4) / 4) + 1;
            Oct_Vec2 text_size;
            oct_GetTextSize(kingdom, text_size, scale, "Score: %i", (int)state.score);
            oct_DrawTextColour(kingdom, (Oct_Vec2) {(GAME_WIDTH / 2) - (text_size[0] / 2) + 1, y},
                               &(Oct_Colour) {0, 0, 0, 1}, scale, "Score: %i", (int)state.score);
            oct_DrawText(kingdom, (Oct_Vec2) {(GAME_WIDTH / 2) - (text_size[0] / 2), y}, scale, "Score: %i", (int)state.score);
        } else {
            Oct_Vec2 size;
            oct_GetTextSize(kingdom, size, 1, "Score: %i", (int)state.score);
            oct_DrawTextColour(kingdom, (Oct_Vec2) {(GAME_WIDTH / 2) - (size[0] / 2) + 1, y}, &(Oct_Colour) {0, 0, 0, 1},
                               1, "Score: %i", (int)state.score);
            oct_DrawText(kingdom, (Oct_Vec2) {(GAME_WIDTH / 2) - (size[0] / 2), y}, 1, "Score: %i", (int)state.score);
        }
    }
}

void draw_time_alert() {
    // 160,80
    const float x = (GAME_WIDTH / 2);
    const float y = (GAME_HEIGHT / 2);

    if (state.lifespan < 5 && !state.player_died) {
        if (state.outta_time == UINT64_MAX) {
            state.outta_time = oct_PlaySound(oct_GetAsset(gBundle, "sounds/outtatime.wav"), (Oct_Vec2){gSoundVolume, gSoundVolume}, false);
        }

        const float scale = (sin(oct_Time() * 2) + 1.8) * 0.3;
        const float rotation = cos(oct_Time() * 2.5) * 0.3;
        oct_DrawTextureIntExt(
                OCT_INTERPOLATE_ALL, 7,
                oct_GetAsset(gBundle, "textures/danger.png"),
                (Oct_Vec2){x, y},
                (Oct_Vec2){scale, scale},
                rotation, (Oct_Vec2){OCT_ORIGIN_MIDDLE, OCT_ORIGIN_MIDDLE});
    } else if (state.outta_time != UINT64_MAX) {
        oct_StopSound(state.outta_time);
        state.outta_time = UINT64_MAX;
    }
}

void draw_kill_bar() {
    const float percent_kills = oct_Clamp(0, 1, (state.displayed_kills / ((float)state.req_kills - 1)));
    state.displayed_kills += (state.current_kills - state.displayed_kills) * 0.5;
    const float x2 = 210;
    const float y2 = 56;
    oct_DrawTexture(
            oct_GetAsset(gBundle, "textures/killcountempty.png"),
            (Oct_Vec2){x2, y2}
    );
    Oct_DrawCommand cmd2 = {
            .type = OCT_DRAW_COMMAND_TYPE_TEXTURE,
            .interpolate = OCT_INTERPOLATE_ALL,
            .id = 42069,
            .colour = {1, 1, 1, 1},
            .Texture = {
                    .texture = oct_GetAsset(gBundle, "textures/killcount.png"),
                    .viewport = (Oct_Rectangle){
                            .position = {0, 0},
                            .size = {92 * percent_kills, 16},
                    },
                    .position = {x2, y2},
                    .scale = {1, 1},
                    .origin = {0, 0},
                    .rotation = 0,
            }
    };
    oct_Draw(&cmd2);
}

int32_t same_chance(int32_t n){
    return (int32_t)floorf(oct_Random(0, n));
}

void handle_enemy_spawns1() {
    state.frame_count++;
    if (state.game_phase < GAME_PHASES - 1 && state.frame_count >= TIME_BETWEEN_PHASES[state.game_phase] && !state.player_died) {
        state.game_phase += 1;
        state.frame_count = 0;
    }

    if (gFrameCounter % SPAWN_FREQUENCIES[state.game_phase] == 0) {
        if (state.game_phase == 0) {
            const int32_t opps[] = {
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_Y_SHOOTER
            };
            const int32_t opp = same_chance(3);
            add_ai(opps[opp]);
        } else if (state.game_phase == 1) {
            const int32_t opps[] = {
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_Y_SHOOTER,
                    CHARACTER_TYPE_Y_SHOOTER,
                    CHARACTER_TYPE_X_SHOOTER
            };
            const int32_t opp = same_chance(5);
            add_ai(opps[opp]);
        } else if (state.game_phase == 2) {
            const int32_t opps[] = {
                    CHARACTER_TYPE_XY_SHOOTER,
                    CHARACTER_TYPE_Y_SHOOTER,
                    CHARACTER_TYPE_Y_SHOOTER,
                    CHARACTER_TYPE_X_SHOOTER,
                    CHARACTER_TYPE_X_SHOOTER
            };
            const int32_t opp = same_chance(5);
            add_ai(opps[opp]);
        } else if (state.game_phase == 3) {
            const int32_t opps[] = {
                    CHARACTER_TYPE_XY_SHOOTER,
                    CHARACTER_TYPE_XY_SHOOTER,
                    CHARACTER_TYPE_JUMPER,
            };
            const int32_t opp = same_chance(3);
            add_ai(opps[opp]);
        } else if (state.game_phase == 4) {
            const int32_t opps[] = {
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_X_SHOOTER,
                    CHARACTER_TYPE_X_SHOOTER,
                    CHARACTER_TYPE_LASER,
                    CHARACTER_TYPE_DASHER
            };
            const int32_t opp = same_chance(6);
            add_ai(opps[opp]);
        } else if (state.game_phase == 5) {
            const int32_t opps[] = {
                    CHARACTER_TYPE_DASHER
            };
            const int32_t opp = same_chance(1);
            add_ai(opps[opp]);
        } else if (state.game_phase == 6) {
            const int32_t opps[] = {
                    CHARACTER_TYPE_LASER,
                    CHARACTER_TYPE_BOMBER,
                    CHARACTER_TYPE_Y_SHOOTER
            };
            const int32_t opp = same_chance(3);
            add_ai(opps[opp]);
        } else if (state.game_phase == 7) {
            const int32_t opps[] = {
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_X_SHOOTER,
                    CHARACTER_TYPE_Y_SHOOTER,
                    CHARACTER_TYPE_XY_SHOOTER,
                    CHARACTER_TYPE_LASER,
                    CHARACTER_TYPE_DASHER,
                    CHARACTER_TYPE_BOMBER,
                    CHARACTER_TYPE_BOMBER,
            };
            const int32_t opp = same_chance(8);
            add_ai(opps[opp]);
        }
    }
}
void handle_enemy_spawns2() {
    state.frame_count++;
    if (state.game_phase < GAME_PHASES - 1 && state.frame_count >= TIME_BETWEEN_PHASES2[state.game_phase] && !state.player_died) {
        state.game_phase += 1;
        state.frame_count = 0;
    }

    if (gFrameCounter % SPAWN_FREQUENCIES[state.game_phase] == 0) {
        if (state.game_phase == 0) {
            const int32_t opps[] = { // j y y
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_Y_SHOOTER,
                    CHARACTER_TYPE_Y_SHOOTER
            };
            const int32_t opp = same_chance(3);
            add_ai(opps[opp]);
        } else if (state.game_phase == 1) { // j x y
            const int32_t opps[] = {
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_Y_SHOOTER,
                    CHARACTER_TYPE_X_SHOOTER
            };
            const int32_t opp = same_chance(3);
            add_ai(opps[opp]);
        } else if (state.game_phase == 2) { // xy y
            const int32_t opps[] = {
                    CHARACTER_TYPE_XY_SHOOTER,
                    CHARACTER_TYPE_Y_SHOOTER,
            };
            const int32_t opp = same_chance(2);
            add_ai(opps[opp]);
        } else if (state.game_phase == 3) { // d l xy
            const int32_t opps[] = {
                    CHARACTER_TYPE_DASHER,
                    CHARACTER_TYPE_LASER,
                    CHARACTER_TYPE_XY_SHOOTER,
            };
            const int32_t opp = same_chance(3);
            add_ai(opps[opp]);
        } else if (state.game_phase == 4) { // l l x
            const int32_t opps[] = {
                    CHARACTER_TYPE_X_SHOOTER,
                    CHARACTER_TYPE_LASER,
                    CHARACTER_TYPE_LASER,
            };
            const int32_t opp = same_chance(3);
            add_ai(opps[opp]);
        } else if (state.game_phase == 5) { // y b b
            const int32_t opps[] = {
                    CHARACTER_TYPE_BOMBER,
                    CHARACTER_TYPE_BOMBER,
                    CHARACTER_TYPE_Y_SHOOTER,
            };
            const int32_t opp = same_chance(3);
            add_ai(opps[opp]);
        } else if (state.game_phase == 6) { // b d
            const int32_t opps[] = {
                    CHARACTER_TYPE_BOMBER,
                    CHARACTER_TYPE_DASHER,
            };
            const int32_t opp = same_chance(2);
            add_ai(opps[opp]);
        } else if (state.game_phase == 7) {
            const int32_t opps[] = {
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_X_SHOOTER,
                    CHARACTER_TYPE_Y_SHOOTER,
                    CHARACTER_TYPE_XY_SHOOTER,
                    CHARACTER_TYPE_LASER,
                    CHARACTER_TYPE_DASHER,
                    CHARACTER_TYPE_BOMBER,
                    CHARACTER_TYPE_BOMBER,
            };
            const int32_t opp = same_chance(8);
            add_ai(opps[opp]);
        }
    }
}
void handle_enemy_spawns3() {
    state.frame_count++;
    if (state.game_phase < GAME_PHASES - 1 && state.frame_count >= TIME_BETWEEN_PHASES3[state.game_phase] && !state.player_died) {
        state.game_phase += 1;
        state.frame_count = 0;
    }

    if (gFrameCounter % SPAWN_FREQUENCIES[state.game_phase] == 0) {
        if (state.game_phase == 0) {
            const int32_t opps[] = { // xy y j
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_XY_SHOOTER,
                    CHARACTER_TYPE_Y_SHOOTER
            };
            const int32_t opp = same_chance(3);
            add_ai(opps[opp]);
        } else if (state.game_phase == 1) { // x xy j
            const int32_t opps[] = {
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_X_SHOOTER,
                    CHARACTER_TYPE_XY_SHOOTER
            };
            const int32_t opp = same_chance(3);
            add_ai(opps[opp]);
        } else if (state.game_phase == 2) { // j j l
            const int32_t opps[] = {
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_LASER,
            };
            const int32_t opp = same_chance(3);
            add_ai(opps[opp]);
        } else if (state.game_phase == 3) { // d x x
            const int32_t opps[] = {
                    CHARACTER_TYPE_X_SHOOTER,
                    CHARACTER_TYPE_X_SHOOTER,
                    CHARACTER_TYPE_DASHER,
            };
            const int32_t opp = same_chance(3);
            add_ai(opps[opp]);
        } else if (state.game_phase == 4) { // l d d
            const int32_t opps[] = {
                    CHARACTER_TYPE_LASER,
                    CHARACTER_TYPE_DASHER,
                    CHARACTER_TYPE_DASHER,
            };
            const int32_t opp = same_chance(3);
            add_ai(opps[opp]);
        } else if (state.game_phase == 5) { // j d l
            const int32_t opps[] = {
                    CHARACTER_TYPE_DASHER,
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_LASER,
            };
            const int32_t opp = same_chance(3);
            add_ai(opps[opp]);
        } else if (state.game_phase == 6) { // b
            const int32_t opps[] = {
                    CHARACTER_TYPE_BOMBER,
            };
            const int32_t opp = same_chance(1);
            add_ai(opps[opp]);
        } else if (state.game_phase == 7) {
            const int32_t opps[] = {
                    CHARACTER_TYPE_JUMPER,
                    CHARACTER_TYPE_X_SHOOTER,
                    CHARACTER_TYPE_Y_SHOOTER,
                    CHARACTER_TYPE_XY_SHOOTER,
                    CHARACTER_TYPE_LASER,
                    CHARACTER_TYPE_DASHER,
                    CHARACTER_TYPE_BOMBER,
                    CHARACTER_TYPE_BOMBER,
            };
            const int32_t opp = same_chance(8);
            add_ai(opps[opp]);
        }
    }
}

void draw_player_death_screen() {
    if (state.player_died) {
        const float banner_drop_time = 2; // seconds
        const float drop_percent = 1 - oct_Clamp(0, 1, -pow(((state.total_time - state.player_die_time) / banner_drop_time), 2) + 1);
        const float target_x = (GAME_WIDTH / 2);
        const float target_y = (GAME_HEIGHT / 2);
        const float real_y = target_y * drop_percent;

        // a bunch of effects when the banner hits the bottom
        if (drop_percent >= 1 && !state.banner_dropped) {
            state.banner_dropped = true;
            oct_PlaySound(
                    oct_GetAsset(gBundle, "sounds/bumpwall.wav"),
                    (Oct_Vec2){1 * gSoundVolume, 1 * gSoundVolume},
                    false);
            create_particles_job(&(CreateParticlesJob){
                    .variation = 3,
                    .y_vel = -4,
                    .x_vel = 0,
                    .tex = oct_GetAsset(gBundle, "textures/garbageparticle.png"),
                    .spr = OCT_NO_ASSET,
                    .x = (GAME_WIDTH / 2) - 64,
                    .y = (GAME_HEIGHT / 2) + 24,
                    .count = 20,
                    .lifetime = 1
            });
            create_particles_job(&(CreateParticlesJob){
                    .variation = 3,
                    .y_vel = -4,
                    .x_vel = 0,
                    .tex = oct_GetAsset(gBundle, "textures/garbageparticle.png"),
                    .spr = OCT_NO_ASSET,
                    .x = (GAME_WIDTH / 2),
                    .y = (GAME_HEIGHT / 2) + 24,
                    .count = 20,
                    .lifetime = 1
            });
            create_particles_job(&(CreateParticlesJob){
                    .variation = 3,
                    .y_vel = -4,
                    .x_vel = 0,
                    .tex = oct_GetAsset(gBundle, "textures/garbageparticle.png"),
                    .spr = OCT_NO_ASSET,
                    .x = (GAME_WIDTH / 2) + 64,
                    .y = (GAME_HEIGHT / 2) + 24,
                    .count = 20,
                    .lifetime = 1
            });
        }

        oct_DrawTextureIntExt(
                OCT_INTERPOLATE_ALL, 21,
                state.got_highscore ? oct_GetAsset(gBundle, "textures/highscore.png") : oct_GetAsset(gBundle, "textures/itsover.png"),
                (Oct_Vec2){target_x, real_y},
                (Oct_Vec2){drop_percent, drop_percent},
                0, (Oct_Vec2){OCT_ORIGIN_MIDDLE, OCT_ORIGIN_MIDDLE});
    }
}

void draw_transform_indicator() {
    if (state.total_time - state.player_transform_time < TRANSFORM_INDICATE_TIME) {
        const float percent = (state.total_time - state.player_transform_time) / TRANSFORM_INDICATE_TIME;
        oct_DrawCircleIntColour(
                OCT_INTERPOLATE_ALL,
                55,
                &(Oct_Circle){
                    .position = {state.player->physx.x + 6, state.player->physx.y + 6},
                    .radius = percent * 60,
                },
                &(Oct_Colour){1, 1, 1, oct_Sirp(1, 0, percent)},
                false, 2);
    }
}

GameStatus game_update() {
    Oct_Texture texs[] = {
            oct_GetAsset(gBundle, "textures/bg1.png"),
            oct_GetAsset(gBundle, "textures/bg2.png"),
            oct_GetAsset(gBundle, "textures/bg3.png")
    };
    oct_DrawTexture(texs[menu_state.map], (Oct_Vec2){0, 0});

    oct_TilemapDraw(state.level_map);

    // DEBUG
    if (oct_KeyPressed(OCT_KEY_Q))
        add_ai(CHARACTER_TYPE_DASHER);
    if (oct_KeyPressed(OCT_KEY_E))
        add_ai(CHARACTER_TYPE_LASER);
    if (oct_KeyPressed(OCT_KEY_R))
        add_ai(CHARACTER_TYPE_BOMBER);

    // this is causing major fuckups that im not dealing with
    //queue_particles_jobs(gFrameAllocator);
    draw_kill_bar();
    draw_time_bar();
    draw_score();

    for (int i = 0; i < MAX_CHARACTERS; i++) {
        if (!state.characters[i].alive) continue;
        process_character(&state.characters[i]);
    }

    // TODO: Put this shit in a job cuz idgaf about race conditions
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!state.projectiles[i].alive) continue;
        process_projectile(&state.projectiles[i]);
    }

    draw_player_death_screen();
    draw_transform_indicator();
    draw_time_alert();
    handle_tutorial();

    // things that only happen if no tutorial
    state.total_time += 1.0 / 30.0;
    if (!state.in_tutorial) {
        // player :skull: if out of time
        state.lifespan -= 1.0 / 30.0;
        if (state.lifespan <= 0 && !state.player_died) {
            kill_character(false, state.player, false);
        }

        if (!state.player_died) {
            const float prev_score = state.score;
            state.score += 1 + state.game_phase;

            // show little animations for getting scores
            if (state.score >= 5000 && prev_score < 5000) {
                create_particles_job(&(CreateParticlesJob){
                        .variation = 3,
                        .y_vel = -2,
                        .x_vel = 3,
                        .tex = oct_GetAsset(gBundle, "textures/5kpoints.png"),
                        .spr = OCT_NO_ASSET,
                        .x = GAME_WIDTH / 2 - 120,
                        .y = 40,
                        .count = 3,
                        .lifetime = 4
                });
            } else if (state.score >= 10000 && prev_score < 10000) {
                    create_particles_job(&(CreateParticlesJob){
                        .variation = 3,
                        .y_vel = -2,
                        .x_vel = 3,
                        .tex = oct_GetAsset(gBundle, "textures/10kpoints.png"),
                        .spr = OCT_NO_ASSET,
                        .x = GAME_WIDTH / 2 - 120,
                        .y = 40,
                        .count = 3,
                        .lifetime = 4
                });
            } else if (state.score >= 20000 && prev_score < 20000) {
                    create_particles_job(&(CreateParticlesJob){
                        .variation = 3,
                        .y_vel = -2,
                        .x_vel = 3,
                        .tex = oct_GetAsset(gBundle, "textures/20kpoints.png"),
                        .spr = OCT_NO_ASSET,
                        .x = GAME_WIDTH / 2 - 120,
                        .y = 40,
                        .count = 3,
                        .lifetime = 4
                });
            } else if (state.score >= 40000 && prev_score < 40000) {
                    create_particles_job(&(CreateParticlesJob){
                        .variation = 3,
                        .y_vel = -2,
                        .x_vel = 3,
                        .tex = oct_GetAsset(gBundle, "textures/40kpoints.png"),
                        .spr = OCT_NO_ASSET,
                        .x = GAME_WIDTH / 2 - 120,
                        .y = 40,
                        .count = 3,
                        .lifetime = 4
                });
            } else if (state.score >= 100000 && prev_score < 100000) {
                    create_particles_job(&(CreateParticlesJob){
                        .variation = 3,
                        .y_vel = -2,
                        .x_vel = 3,
                        .tex = oct_GetAsset(gBundle, "textures/100kpoints.png"),
                        .spr = OCT_NO_ASSET,
                        .x = GAME_WIDTH / 2 - 120,
                        .y = 40,
                        .count = 3,
                        .lifetime = 4
                });
            } else if (state.score >= 200000 && prev_score < 200000) {
                    create_particles_job(&(CreateParticlesJob){
                        .variation = 3,
                        .y_vel = -2,
                        .x_vel = 3,
                        .tex = oct_GetAsset(gBundle, "textures/200kpoints.png"),
                        .spr = OCT_NO_ASSET,
                        .x = GAME_WIDTH / 2 - 120,
                        .y = 40,
                        .count = 3,
                        .lifetime = 4
                });
            }
        }

        if (menu_state.map == 0)
            handle_enemy_spawns1();
        else if (menu_state.map == 1)
            handle_enemy_spawns2();
        else if (menu_state.map == 2)
            handle_enemy_spawns3();
    }

    // particles on top for some fucking reason
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!state.particles[i].alive) continue;
        process_particle(&state.particles[i]);
    }

    // just particles
    oct_WaitJobs();


    // quit when player rip
    if (oct_KeyPressed(OCT_KEY_SPACE) && state.player_died && state.fade_out < 0 && state.total_time - state.player_die_time > 3) {
        state.fade_out = FADE_IN_OUT_TIME;
        oct_PlaySound(
                oct_GetAsset(gBundle, "sounds/stonelong.wav"),
                (Oct_Vec2){1 * gSoundVolume, 1 * gSoundVolume},
                false);
    }

    // debug
    if (oct_KeyDown(OCT_KEY_F)) {
        return GAME_STATUS_MENU;
    }

    state.fade_in -= 1;
    state.fade_out -= 1;
    if (state.fade_in > 0) {
        const float percent = oct_Sirp(1, 0, state.fade_in / FADE_IN_OUT_TIME);
        oct_DrawTextureInt(
                OCT_INTERPOLATE_ALL, 74,
                oct_GetAsset(gBundle, "textures/curtains.png"),
                (Oct_Vec2){GAME_WIDTH * percent, 0});
    }
    if (state.fade_out > 0) {
        const float percent = oct_Sirp(0, 1, state.fade_out / FADE_IN_OUT_TIME);
        oct_DrawTextureInt(
                OCT_INTERPOLATE_ALL, 74,
                oct_GetAsset(gBundle, "textures/curtains.png"),
                (Oct_Vec2){GAME_WIDTH * percent, 0});
        if (state.fade_out <= 1) {
            return GAME_STATUS_MENU;
        }
    }

    return GAME_STATUS_PLAY_GAME;
}

void game_end() {

}

///////////////////////// MENU /////////////////////////
void draw_cursor(uint64_t id, float x, float y, const char *str) {
    Oct_Vec2 text_size;
    oct_GetTextSize(
            oct_GetAsset(gBundle, "fnt_kingdom"),
            text_size,
            1,
            "%s", str);

    const float target_x = x - 1;
    const float target_y = y - 1;
    const float target_width = text_size[0] + 2;
    const float target_height = text_size[1] + 4;
    const float factor = 0.3;
    menu_state.cursor_x += (target_x - menu_state.cursor_x) * factor;
    menu_state.cursor_y += (target_y - menu_state.cursor_y) * factor;
    menu_state.cursor_width += (target_width - menu_state.cursor_width) * factor;
    menu_state.cursor_height += (target_height - menu_state.cursor_height) * factor;

    oct_DrawRectangleIntColour(
            OCT_INTERPOLATE_ALL,
            25,
            &(Oct_Colour){0, 0, 0, 1},
            &(Oct_Rectangle){
                    .position = {menu_state.cursor_x, menu_state.cursor_y},
                    .size = {menu_state.cursor_width, menu_state.cursor_height}
            },
            true, 1);
}

void draw_text_fancy(uint64_t id, float x, float y, const char *str) {
    oct_DrawTextIntColour(
            OCT_INTERPOLATE_ALL, id,
            oct_GetAsset(gBundle, "fnt_kingdom"),
            (Oct_Vec2){x + 1, y + 1},
            &(Oct_Colour){0, 0, 0, 1},
            1,
            "%s", str);
    oct_DrawTextInt(
            OCT_INTERPOLATE_ALL, id + 1,
            oct_GetAsset(gBundle, "fnt_kingdom"),
            (Oct_Vec2){x, y},
            1,
            "%s", str);
}

void handle_top_menu() {
    draw_cursor(25, 40 + (5 * menu_state.cursor), 150 + (24 * menu_state.cursor), TOP_LEVEL_MENU[menu_state.cursor]);
    for (int i = 0; i < TOP_MENU_SIZE; i++) {
        draw_text_fancy(20 * i, 40 + (5 * i), 150 + (24 * i), TOP_LEVEL_MENU[i]);
    }
    if (oct_KeyPressed(OCT_KEY_UP)) {
        menu_state.cursor = menu_state.cursor == 0 ? TOP_MENU_SIZE - 1 : menu_state.cursor - 1;
        oct_PlaySound(
                    oct_GetAsset(gBundle, "sounds/cursor.wav"),
                    (Oct_Vec2){0.8 * gSoundVolume, 0.8 * gSoundVolume},
                    false);
    }
    if (oct_KeyPressed(OCT_KEY_DOWN)) {
        menu_state.cursor = (menu_state.cursor + 1) % TOP_MENU_SIZE;
        oct_PlaySound(
                oct_GetAsset(gBundle, "sounds/cursor.wav"),
                (Oct_Vec2){0.8 * gSoundVolume, 0.8 * gSoundVolume},
                false);
    }
    if (oct_KeyPressed(OCT_KEY_SPACE)) {
        oct_PlaySound(
                oct_GetAsset(gBundle, "sounds/select.wav"),
                (Oct_Vec2){0.8 * gSoundVolume, 0.8 * gSoundVolume},
                false);

        if (menu_state.cursor == 0)  { // play menu
            menu_state.menu = MENU_INDEX_PLAY;
            menu_state.cursor = 0;
        } else if (menu_state.cursor == 1)  { // options menu
            menu_state.menu = MENU_INDEX_SETTINGS;
            menu_state.cursor = 0;
        } else if (menu_state.cursor == 2)  { // quit
            menu_state.quit = true;
        }
    }
}

void show_confirmation(const char *text) {
    menu_state.starting_text_box_frame = gFrameCounter;
    menu_state.drop_text = text;
}

void handle_settings() {
    draw_cursor(25, 40 + (5 * menu_state.cursor), 120 + (24 * menu_state.cursor), OPTION_MENU[menu_state.cursor]);
    for (int i = 0; i < OPTIONS_MENU_SIZE; i++) {
        draw_text_fancy(20 * i, 40 + (5 * i), 120 + (24 * i), OPTION_MENU[i]);
    }
    if (oct_KeyPressed(OCT_KEY_UP)) {
        menu_state.cursor = menu_state.cursor == 0 ? OPTIONS_MENU_SIZE - 1 : menu_state.cursor - 1;
        oct_PlaySound(
                oct_GetAsset(gBundle, "sounds/cursor.wav"),
                (Oct_Vec2){0.8 * gSoundVolume, 0.8 * gSoundVolume},
                false);
    }
    if (oct_KeyPressed(OCT_KEY_DOWN)) {
        menu_state.cursor = (menu_state.cursor + 1) % OPTIONS_MENU_SIZE;
        oct_PlaySound(
                oct_GetAsset(gBundle, "sounds/cursor.wav"),
                (Oct_Vec2){0.8 * gSoundVolume, 0.8 * gSoundVolume},
                false);
    }
    if (oct_KeyPressed(OCT_KEY_SPACE)) {
        oct_PlaySound(
                oct_GetAsset(gBundle, "sounds/select.wav"),
                (Oct_Vec2){0.8 * gSoundVolume, 0.8 * gSoundVolume},
                false);

        if (menu_state.cursor == 0)  { // Reset tutorial
            Save s = parse_save();
            s.has_done_tutorial = false;
            save_game(&s);
            show_confirmation("bozo ");
        } else if (menu_state.cursor == 1)  { // Toggle music
            Save s = parse_save();
            s.music_volume = s.music_volume != 0 ? 0 : 1;
            save_game(&s);
            gMusicVolume = s.music_volume;
            oct_UpdateSound(gPlayingMusic, (Oct_Vec2){GLOBAL_MUSIC_VOLUME * gMusicVolume, GLOBAL_MUSIC_VOLUME * gMusicVolume}, true, false);
            if (s.music_volume == 0)
                show_confirmation("music = 0 ");
            else
                show_confirmation("music = 1 ");
        } else if (menu_state.cursor == 2)  { // Toggle sound
            Save s = parse_save();
            s.sound_volume = s.sound_volume != 0 ? 0 : 1;
            gSoundVolume = s.sound_volume;
            save_game(&s);
            if (s.sound_volume == 0)
                show_confirmation("sound = 0 ");
            else
                show_confirmation("sound = 1 ");
        } else if (menu_state.cursor == 3)  { // Toggle fullscreen
            Save s = parse_save();
            s.fullscreen = !s.fullscreen;
            save_game(&s);
            show_confirmation("okay ");
            oct_SetFullscreen(s.fullscreen);
        } else if (menu_state.cursor == 4)  { // Toggle pixel-perfect
            Save s = parse_save();
            s.pixel_perfect = !s.pixel_perfect;
            gPixelPerfect = s.pixel_perfect;
            save_game(&s);
            show_confirmation("okay ");
        } else if (menu_state.cursor == 5)  { // Back
            menu_state.menu = MENU_INDEX_TOP;
            menu_state.cursor = 0;
        }
    }
}

bool highscore_reaches_x(float x) {
    return menu_state.highscore[0] >= x || menu_state.highscore[1] >= x || menu_state.highscore[2] >= x;
}

void handle_leaderboards() {
    menu_state.menu = MENU_INDEX_TOP;
    menu_state.cursor = 0;
    // todo: this
}

void handle_play() {
    draw_cursor(25, 40 + (5 * menu_state.cursor), 150 + (24 * menu_state.cursor), PLAY_MENU[menu_state.cursor]);
    for (int i = 0; i < PLAY_MENU_SIZE; i++) {
        draw_text_fancy(20 * i, 40 + (5 * i), 150 + (24 * i), PLAY_MENU[i]);
    }
    if (oct_KeyPressed(OCT_KEY_UP)) {
        menu_state.cursor = menu_state.cursor == 0 ? PLAY_MENU_SIZE - 1 : menu_state.cursor - 1;
        oct_PlaySound(
                oct_GetAsset(gBundle, "sounds/cursor.wav"),
                (Oct_Vec2){0.8 * gSoundVolume, 0.8 * gSoundVolume},
                false);
    }
    if (oct_KeyPressed(OCT_KEY_DOWN)) {
        menu_state.cursor = (menu_state.cursor + 1) % PLAY_MENU_SIZE;
        oct_PlaySound(
                oct_GetAsset(gBundle, "sounds/cursor.wav"),
                (Oct_Vec2){0.8 * gSoundVolume, 0.8 * gSoundVolume},
                false);
    }
    if (oct_KeyPressed(OCT_KEY_SPACE)) {
        oct_PlaySound(
                oct_GetAsset(gBundle, "sounds/select.wav"),
                (Oct_Vec2){0.8 * gSoundVolume, 0.8 * gSoundVolume},
                false);

        if (menu_state.cursor == 0 && menu_state.fade_out < 0)  { // play
            if (highscore_reaches_x(MAP_UNLOCK_SCORES[menu_state.map])) {
                menu_state.fade_out = FADE_IN_OUT_TIME;
                oct_PlaySound(
                        oct_GetAsset(gBundle, "sounds/stonelong.wav"),
                        (Oct_Vec2){1 * gSoundVolume, 1 * gSoundVolume},
                        false);
            } else {
                show_confirmation("no lol");
            }
        } else if (menu_state.cursor == 1)  { // swap body
            menu_state.character = (menu_state.character + 1) % STARTING_BODY_MAX;
        } else if (menu_state.cursor == 2)  { // swap map
            menu_state.map = (menu_state.map + 1) % STARTING_MAP_MAX;
        } else if (menu_state.cursor == 3)  { // back
            menu_state.menu = MENU_INDEX_TOP;
            menu_state.cursor = 0;
        }
    }

    // draw selected map & character
    const float character_x = 144 + 30;
    const float character_y = GAME_HEIGHT / 2 + 20;

    if (menu_state.character == STARTING_BODY_Y_SHOOTER) {
        const float gun_x = character_x + 6 - (19 / 2);
        oct_DrawSpriteFrame(
                oct_GetAsset(gBundle, "sprites/playershooter.json"), 0,
                (Oct_Vec2) {character_x, character_y});
        oct_DrawTextureExt(
                oct_GetAsset(gBundle, "textures/ygun.png"),
                (Oct_Vec2) {gun_x, character_y - 23},
                (Oct_Vec2) {1, 1},
                0, (Oct_Vec2) {0, 0});
    } else {
        oct_DrawSpriteFrame(
                oct_GetAsset(gBundle, "sprites/playerjumper.json"), 0,
                (Oct_Vec2) {character_x, character_y});
    }

    const Oct_Texture maps[] = {
            oct_GetAsset(gBundle, "textures/map1.png"),
            oct_GetAsset(gBundle, "textures/map2.png"),
            oct_GetAsset(gBundle, "textures/map3.png"),
    };
    oct_DrawText(
            oct_GetAsset(gBundle, "fnt_monogram"),
            (Oct_Vec2){150, 110},
            1,
            "   Body              Map");
    oct_DrawTextureExt(
            maps[menu_state.map],
            (Oct_Vec2){GAME_WIDTH / 2 + 30, GAME_HEIGHT / 2 + 20},
            (Oct_Vec2){1, 1},
            0, (Oct_Vec2){OCT_ORIGIN_MIDDLE, OCT_ORIGIN_MIDDLE});

    if (!highscore_reaches_x(MAP_UNLOCK_SCORES[menu_state.map])) {
        oct_DrawTextureExt(
                oct_GetAsset(gBundle, "textures/locked.png"),
                (Oct_Vec2){GAME_WIDTH / 2 + 30, GAME_HEIGHT / 2 + 20},
                (Oct_Vec2){1, 1},
                0, (Oct_Vec2){OCT_ORIGIN_MIDDLE, OCT_ORIGIN_MIDDLE});
        oct_DrawText(
                oct_GetAsset(gBundle, "fnt_monogram"),
                (Oct_Vec2){240 - 9, 110 + 96},
                1,
                "Reach %i points", (int)MAP_UNLOCK_SCORES[menu_state.map]);
    } else {
        // draw highscore instead 294
        char buf[100];
        snprintf(buf, 99, "Highscore: %i", (int)menu_state.highscore[menu_state.map]);
        float size = strlen(buf) * 7;
        oct_DrawText(
                oct_GetAsset(gBundle, "fnt_monogram"),
                (Oct_Vec2){roundf(294 - (size / 2)), 110 + 96}, 1,
                "Highscore: %i", (int)menu_state.highscore[menu_state.map]);
    }
}

void menu_begin() {
    static bool fuck = false;
    memset(&menu_state, 0, sizeof(struct MenuState_t));
    menu_state.fade_in = FADE_IN_OUT_TIME;
    Save s = parse_save();
    gMusicVolume = s.music_volume;
    gSoundVolume = s.sound_volume;
    menu_state.highscore[0] = s.highscore[0];
    menu_state.highscore[1] = s.highscore[1];
    menu_state.highscore[2] = s.highscore[2];
    if (fuck) {
        oct_StopSound(gPlayingMusic);
    }
    fuck = true;
    gPlayingMusic = oct_PlaySound(
            oct_GetAsset(gBundle, "sounds/title.ogg"),
            (Oct_Vec2){GLOBAL_MUSIC_VOLUME * gMusicVolume, GLOBAL_MUSIC_VOLUME * gMusicVolume},
            true);
    oct_PlaySound(
            oct_GetAsset(gBundle, "sounds/stoneshort.wav"),
            (Oct_Vec2){1 * gSoundVolume, 1 * gSoundVolume},
            false);
}

GameStatus menu_update() {
    // moving bg
    const float x = gFrameCounter % (int)GAME_WIDTH;
    oct_DrawTexture(oct_GetAsset(gBundle, "textures/menubg.png"), (Oct_Vec2){x - GAME_WIDTH, 0});
    oct_DrawTexture(oct_GetAsset(gBundle, "textures/menubg.png"), (Oct_Vec2){x, 0});

    if (menu_state.menu == MENU_INDEX_TOP)
        handle_top_menu();
    else if (menu_state.menu == MENU_INDEX_SETTINGS)
        handle_settings();
    else if (menu_state.menu == MENU_INDEX_LEADERBOARDS)
        handle_leaderboards();
    else if (menu_state.menu == MENU_INDEX_PLAY)
        handle_play();

    // show text box thing
    const float duration = 30 * 4;
    if (gFrameCounter - menu_state.starting_text_box_frame < duration) {
        const float frames_left = gFrameCounter - menu_state.starting_text_box_frame;
        const float x = GAME_WIDTH / 2;
        const float bottom_y = GAME_HEIGHT / 2;
        if (frames_left < duration / 3) {
            const float percent = oct_Sirp(-0.1, 1, frames_left / (duration / 3));
            draw_text_box(x, bottom_y * percent, menu_state.drop_text);
        } else if (frames_left < (duration / 3) * 2) {
            draw_text_box(x, bottom_y, menu_state.drop_text);
        } else {
            const float percent = oct_Sirp(1, -0.1, (frames_left - ((duration / 3) * 2)) / (duration / 3));
            draw_text_box(x, bottom_y * percent, menu_state.drop_text);
        }
    }

    oct_DrawTextureInt(
            OCT_INTERPOLATE_ALL, 87,
            oct_GetAsset(gBundle, "textures/copyright.png"),
            (Oct_Vec2){408, 17 + (sin(oct_Time()) * 4)});

    // 212 draw little dude
    static Oct_SpriteInstance instance;
    static bool started = false;
    const Oct_Sprite spr = oct_GetAsset(gBundle, "sprites/laser.json");
    if (!started) {
        oct_InitSpriteInstance(&instance, spr, true);
        started = true;
    }
    oct_DrawSpriteExt(spr, &instance, (Oct_Vec2){400, 212}, (Oct_Vec2){-1, 1}, 0, (Oct_Vec2){0, 0});

    oct_DrawTextureExt(
            oct_GetAsset(gBundle, "textures/title.png"),
            (Oct_Vec2){GAME_WIDTH / 2 - 40, 40},
            (Oct_Vec2){1, 1},
            0, (Oct_Vec2){OCT_ORIGIN_MIDDLE, OCT_ORIGIN_MIDDLE});

    oct_DrawTexture(
            oct_GetAsset(gBundle, "textures/controls.png"),
            (Oct_Vec2){445, 235});

    menu_state.fade_in -= 1;
    menu_state.fade_out -= 1;
    if (menu_state.fade_in > 0) {
        const float percent = oct_Sirp(1, 0, menu_state.fade_in / FADE_IN_OUT_TIME);
        oct_DrawTextureInt(
                OCT_INTERPOLATE_ALL, 74,
                oct_GetAsset(gBundle, "textures/curtains.png"),
                (Oct_Vec2){GAME_WIDTH * percent, 0});
    }
    if (menu_state.fade_out > 0) {
        const float percent = oct_Sirp(0, 1, menu_state.fade_out / FADE_IN_OUT_TIME);
        oct_DrawTextureInt(
                OCT_INTERPOLATE_ALL, 74,
                oct_GetAsset(gBundle, "textures/curtains.png"),
                (Oct_Vec2){GAME_WIDTH * percent, 0});
        if (menu_state.fade_out == 1) {
            return GAME_STATUS_PLAY_GAME;
        }
    }

    if (menu_state.start_game) return GAME_STATUS_PLAY_GAME;
    if (menu_state.quit) return GAME_STATUS_QUIT;
    return GAME_STATUS_MENU;
}

void menu_end() {

}

///////////////////////// MAIN /////////////////////////

// parses save file, returning reasonable defaults if it doesnt exist 
Save parse_save() {
    uint32_t size;
    uint8_t *data = oct_ReadFile(SAVE_NAME, gAllocator, &size);

    if (data) {
        cJSON *json = cJSON_ParseWithLength((void *) data, size);
        if (json) {
            // todo parse ip and port in future for remote highscore db
            float score[3] = {
                    cJSON_GetNumberValue(cJSON_GetObjectItem(json, "highscore1")),
                    cJSON_GetNumberValue(cJSON_GetObjectItem(json, "highscore2")),
                    cJSON_GetNumberValue(cJSON_GetObjectItem(json, "highscore3")),
            };
            if (!cJSON_IsNumber(cJSON_GetObjectItem(json, "highscore1")))
                score[0] = 0;
            if (!cJSON_IsNumber(cJSON_GetObjectItem(json, "highscore2")))
                score[1] = 0;
            if (!cJSON_IsNumber(cJSON_GetObjectItem(json, "highscore3")))
                score[2] = 0;
            float sound_volume = cJSON_GetNumberValue(cJSON_GetObjectItem(json, "sound_volume"));
            if (!cJSON_IsNumber(cJSON_GetObjectItem(json, "sound_volume")))
                sound_volume = 1;
            float music_volume = cJSON_GetNumberValue(cJSON_GetObjectItem(json, "music_volume"));
            if (!cJSON_IsNumber(cJSON_GetObjectItem(json, "music_volume")))
                music_volume = 1;
            float has_done_tutorial = cJSON_IsTrue(cJSON_GetObjectItem(json, "done_tutorial"));
            float fullscreen = cJSON_IsTrue(cJSON_GetObjectItem(json, "fullscreen"));
            float pixel_perfect = cJSON_IsTrue(cJSON_GetObjectItem(json, "pixel_perfect"));
            Save save = {
                    .highscore = {score[0], score[1], score[2]},
                    .sound_volume = sound_volume,
                    .music_volume = music_volume,
                    .has_done_tutorial = has_done_tutorial,
                    .fullscreen = fullscreen,
                    .pixel_perfect = pixel_perfect
            };
            cJSON_Delete(json);
            oct_Free(gAllocator, data);
            return save;

        } else {
            oct_Free(gAllocator, data);
            oct_Raise(OCT_STATUS_ERROR, false, "save file is cooked");
        }
    }
    return (Save){
        .pixel_perfect = false,
        .fullscreen = false,
        .music_volume = 1,
        .sound_volume = 1,
    };
}

void save_game(Save *save) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "highscore1", save->highscore[0]);
    cJSON_AddNumberToObject(json, "highscore2", save->highscore[1]);
    cJSON_AddNumberToObject(json, "highscore3", save->highscore[2]);
    cJSON_AddBoolToObject(json, "done_tutorial", save->has_done_tutorial);
    cJSON_AddBoolToObject(json, "fullscreen", save->fullscreen);
    cJSON_AddBoolToObject(json, "pixel_perfect", save->pixel_perfect);
    cJSON_AddNumberToObject(json, "sound_volume", save->sound_volume);
    cJSON_AddNumberToObject(json, "music_volume", save->music_volume);
    // todo add ip shit eventually
    char * s = cJSON_Print(json);
    oct_WriteFile(SAVE_NAME, s, strlen(s));
    cJSON_Delete(json);
}

void *startup() {
    if (oct_FileExists("data.bin")) {
        gBundle = oct_LoadAssetBundle("data.bin");
    } else {
        gBundle = oct_LoadAssetBundle("data");
    }
    gAllocator = oct_CreateHeapAllocator();
    gFrameAllocator = oct_CreateArenaAllocator(4096);

    // Backbuffer
    gBackBuffer = oct_CreateSurface((Oct_Vec2){GAME_WIDTH, GAME_HEIGHT});

    menu_begin();

    // oct shit
    oct_GamepadSetAxisDeadzone(GAMEPAD_DEADZONE);

    Save s = parse_save();
    gPixelPerfect = s.pixel_perfect;
    oct_SetFullscreen(s.fullscreen);
    gSoundVolume = s.sound_volume;
    gMusicVolume = s.music_volume;

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
        if (status == GAME_STATUS_MENU) {
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
    if (!gPixelPerfect) {
        const float scale_x = window_width / GAME_WIDTH;
        const float scale_y = window_height / GAME_HEIGHT;
        oct_DrawTextureExt(
                gBackBuffer,
                (Oct_Vec2) {0, 0},
                (Oct_Vec2) {scale_x, scale_y},
                0,
                (Oct_Vec2) {0, 0});
    } else {
        const float scale_x = floorf(window_width / GAME_WIDTH);
        const float scale_y = floorf(window_height / GAME_HEIGHT);
        const float width = GAME_WIDTH * scale_x;
        const float height = GAME_HEIGHT * scale_y;
        const float x = (window_width - width) / 2;
        const float y = (window_height - height) / 2;
        oct_DrawTextureExt(
                gBackBuffer,
                (Oct_Vec2) {x, y},
                (Oct_Vec2) {scale_x, scale_y},
                0,
                (Oct_Vec2) {0, 0});
    }

    gFrameCounter++;
    oct_ResetAllocator(gFrameAllocator);
    return null;
}

// Called once when the engine is about to be deinitialized
void shutdown(void *ptr) {
    oct_FreeAllocator(gAllocator);
    oct_FreeAllocator(gFrameAllocator);
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
            .windowTitle = "Disorderly Conduct",
            .windowWidth = GAME_WIDTH * 3,
            .windowHeight = GAME_HEIGHT * 3,
            .debug = false,
    };
    oct_Init(&initInfo);
    return 0;
}
