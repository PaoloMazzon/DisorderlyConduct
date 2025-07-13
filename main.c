#include <oct/Octarine.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <oct/cJSON.h>

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
Oct_Allocator gFrameAllocator; // arena for frame-time allocations
Oct_Texture gBackBuffer;
uint64_t gFrameCounter;
uint64_t gParticleIDs = 999999;

///////////////////////// CONSTANTS /////////////////////////

// Default level layout
const int32_t LEVEL_WIDTH = 32;
const int32_t LEVEL_HEIGHT = 18;

// Backbuffer and room size
const float GAME_WIDTH = LEVEL_WIDTH * 16;
const float GAME_HEIGHT = LEVEL_HEIGHT * 16;

// These should line up with character types
const float CHARACTER_TYPE_LIFESPANS[] = {
        0, // CHARACTER_TYPE_INVALID,
        15, // CHARACTER_TYPE_JUMPER,
        10, // CHARACTER_TYPE_X_SHOOTER,
        20, // CHARACTER_TYPE_Y_SHOOTER,
        10, // CHARACTER_TYPE_XY_SHOOTER,
        8, // CHARACTER_TYPE_SWORDSMITH,
        3, // CHARACTER_TYPE_LASER,
};

#define MAX_CHARACTERS 100
#define MAX_PROJECTILES 100
#define MAX_PARTICLES 1000
#define MAX_PHYSICS_OBJECTS (MAX_CHARACTERS + MAX_PROJECTILES) // particles noclip
const float GROUND_FRICTION = 0.05;
const float AIR_FRICTION = 0.01;
const float GRAVITY = 0.5;
const float BOUNCE_PRESERVED_BOUNCE_WALL = 0.50; // how much velocity is preserved when rebounding off walls
const float BOUNCE_PRESERVED = 0.20; // how much velocity is preserved when rebounding off walls
const float GAMEPAD_DEADZONE = 0.25;
const float PLAYER_ACCELERATION = 0.5;
const float AI_ACCELERATION = 0.15;
const float PLAYER_JUMP_SPEED = 7;
const float PARTICLES_GROUND_IMPACT_SPEED = 6;
const float SPEED_LIMIT = 12;
const float PLAYER_STARTING_LIFESPAN = 30; // seconds;
const int32_t START_REQ_KILLS = 3;
const float ENEMY_FLING_SPEED = 5;
const int32_t PLAYER_I_FRAMES = 30;

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
    bool alive;
    PhysicsObject physx;

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
    union {
        struct {
            Oct_Sprite sprite;
            Oct_SpriteInstance instance;
        };
        Oct_Texture texture;
    };
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

// LEAVE THIS AT THE BOTTOM
typedef struct GameState_t {
    // set when player gets a character
    float lifespan;
    float max_lifespan;
    Character *player;
    float total_time;
    int req_kills; // for transforming
    int current_kills;
    float displayed_kills; // for lerping a nice val
    int32_t player_iframes; // after getting hit

    Oct_SpriteInstance fire;

    Oct_Tilemap level_map;
    Character characters[MAX_CHARACTERS];
    Projectile projectiles[MAX_PROJECTILES];
    Particle particles[MAX_PARTICLES];
} GameState;

GameState state;

#define NEAR_LEVEL_UP (state.req_kills - 1 == state.current_kills)

///////////////////////// HELPERS /////////////////////////
Projectile *create_projectile(bool player_shot, Oct_Texture tex, float lifetime, float x, float y, float x_speed, float y_speed);

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
            case CHARACTER_TYPE_X_SHOOTER: return OCT_NO_ASSET;//oct_GetAsset(gBundle, "sprites/");
            case CHARACTER_TYPE_Y_SHOOTER: return OCT_NO_ASSET;//oct_GetAsset(gBundle, "sprites/");
            case CHARACTER_TYPE_XY_SHOOTER: return OCT_NO_ASSET;//oct_GetAsset(gBundle, "sprites/");
            case CHARACTER_TYPE_SWORDSMITH: return OCT_NO_ASSET;//oct_GetAsset(gBundle, "sprites/");
            case CHARACTER_TYPE_LASER: return OCT_NO_ASSET;//oct_GetAsset(gBundle, "sprites/");
            default: return OCT_NO_ASSET;
        }
    }

    switch (character->type) {
        case CHARACTER_TYPE_JUMPER: return oct_GetAsset(gBundle, "sprites/jumper.json");
        case CHARACTER_TYPE_X_SHOOTER: return OCT_NO_ASSET;//oct_GetAsset(gBundle, "sprites/");
        case CHARACTER_TYPE_Y_SHOOTER: return OCT_NO_ASSET;//oct_GetAsset(gBundle, "sprites/");
        case CHARACTER_TYPE_XY_SHOOTER: return OCT_NO_ASSET;//oct_GetAsset(gBundle, "sprites/");
        case CHARACTER_TYPE_SWORDSMITH: return OCT_NO_ASSET;//oct_GetAsset(gBundle, "sprites/");
        case CHARACTER_TYPE_LASER: return OCT_NO_ASSET;//oct_GetAsset(gBundle, "sprites/");
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

// Returns true if a horizontal collision was processed
bool process_physics(Character *this_c, Projectile *this_p, PhysicsObject *physx, float x_acceleration, float y_acceleration) {
    bool collision = false;

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
                    (Oct_Vec2){0.2, 0.2},
                    false);
        }

        // Bounce off the wall slightly
        if (ce.type == COLLISION_EVENT_TYPE_BOUNCY_WALL)
            physx->x_vel = physx->x_vel * (-BOUNCE_PRESERVED_BOUNCE_WALL);
        else
            physx->x_vel = physx->x_vel * (-BOUNCE_PRESERVED);
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
                    (Oct_Vec2){0.2, 0.2},
                    false);
        }

        // Bounce off the wall slightly
        if (ce.type == COLLISION_EVENT_TYPE_BOUNCY_WALL)
            physx->y_vel = physx->y_vel * (-BOUNCE_PRESERVED_BOUNCE_WALL);
        else
            physx->y_vel = physx->y_vel * (-BOUNCE_PRESERVED);
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

    if (character->type == CHARACTER_TYPE_JUMPER) {
        oct_DrawSpriteIntColour(
                OCT_INTERPOLATE_ALL, character->id,
                character_type_sprite(character),
                &character->sprite,
                &c,
                (Oct_Vec2) {character->physx.x, character->physx.y});
    } // TODO: The rest of these
}

InputProfile process_player(Character *character) {
    InputProfile input = {0};
    state.player_iframes -= 1;
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
        oct_PlaySound(
                oct_GetAsset(gBundle, "sounds/jump.wav"),
                (Oct_Vec2){0.5, 0.5},
                false);
    }

    return input;
}

void kill_character(bool player_is_killer, Character *character, bool dramatic);

// transforms the player into this dude
void take_body(Character *character) {
    // reset kills and show a nice particle effect
    state.req_kills++;
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

    // Take dudes body
    Character *player = state.player;
    state.player = character;
    character->player_controlled = true;
    character->alive = true;
    state.max_lifespan = CHARACTER_TYPE_LIFESPANS[character->type];
    state.lifespan = CHARACTER_TYPE_LIFESPANS[character->type];

    oct_PlaySound(
            oct_GetAsset(gBundle, "sounds/transform.wav"),
            (Oct_Vec2){1, 1},
            false);

    // kill old player
    player->player_controlled = false;
    kill_character(false, player, true);
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
            .y_vel = -2
        });

        // small sound
        oct_PlaySound(
                oct_GetAsset(gBundle, "sounds/jumpenemy.wav"),
                (Oct_Vec2){1, 1},
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
    } else if (state.player_iframes <= 0) {
        state.player_iframes = PLAYER_I_FRAMES;
        if (state.lifespan <= 0) {
            // TODO: death ssound effect

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
        } else {
            state.lifespan *= 0.75;

            oct_PlaySound(
                    oct_GetAsset(gBundle, "sounds/jumpenemy.wav"),
                    (Oct_Vec2) {1, 1},
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

void process_character(Character *character) {
    // This is only to handle input
    InputProfile input = {0};
    if (character->player_controlled) {
        input = process_player(character);
    } else {
        // Get input from ai
        input.x_acc = AI_ACCELERATION * character->direction;
        const bool kinda_touching_ground = collision_at(character, null, character->physx.x, character->physx.y + 1, character->physx.bb_width, character->physx.bb_height).type != 0;

        // Jumpers might jump every now and again
        if (character->type == CHARACTER_TYPE_JUMPER) {
            if (gFrameCounter % 5 == 0 && oct_Random(0, 1) > 0.5 && kinda_touching_ground) {
                input.y_acc = -PLAYER_JUMP_SPEED;
            }
        }
    }

    const bool x_coll = process_physics(character, null, &character->physx, input.x_acc, input.y_acc);

    // Type specific stuff
    if (character->type == CHARACTER_TYPE_JUMPER) {
        // Jump on enemy head should kill them
        const CollisionEvent y_collision = collision_at(character, null, character->physx.x, character->physx.y + 2, character->physx.bb_width, character->physx.bb_height);
        if (y_collision.type == COLLISION_EVENT_TYPE_CHARACTER) {
            kill_character(character->player_controlled, y_collision.character, false);

            // fly away if not the player
            if (!character->player_controlled) {
                character->physx.y_vel -= PLAYER_JUMP_SPEED;
                character->physx.x_vel = oct_Random(-ENEMY_FLING_SPEED, ENEMY_FLING_SPEED);
            }
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
        projectile->alive = false;
    }

    // lifetime
    projectile->lifetime -= 1.0 / 30.0;
    if (projectile->lifetime <= 0) {
        projectile->alive = false;
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
            if (slot->type == CHARACTER_TYPE_JUMPER) {
                oct_InitSpriteInstance(&slot->sprite, character_type_sprite(slot), true);
                slot->physx.bb_width = 12;
                slot->physx.bb_height = 12;
                slot->id = 10000 + i * 2;
            }  // TODO: The rest of these

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
            if (event.type != COLLISION_EVENT_TYPE_NO_COLLISION) {
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

    // Open json with level
    uint32_t size;
    uint8_t *data = oct_ReadFile("data/map1.tmj", gAllocator, &size);
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

    // Add the player
    state.player = add_character(&(Character){
        .type = CHARACTER_TYPE_JUMPER,
        .player_controlled = true,
        .physx = {
                .x = 15.5 * 16,
                .y = 11 * 16,
        }
    });
    state.lifespan = PLAYER_STARTING_LIFESPAN;
    state.max_lifespan = PLAYER_STARTING_LIFESPAN;

    // Add test dude
    add_ai(CHARACTER_TYPE_JUMPER);
}

GameStatus game_update() {

    oct_TilemapDraw(state.level_map);

    // DEBUG
    if (oct_KeyPressed(OCT_KEY_L))
        add_ai(CHARACTER_TYPE_JUMPER);

    // this is causing major fuckups that im not dealing with
    //queue_particles_jobs(gFrameAllocator);

    for (int i = 0; i < MAX_CHARACTERS; i++) {
        if (!state.characters[i].alive) continue;
        process_character(&state.characters[i]);
    }

    // TODO: Put this shit in a job cuz idgaf about race conditions
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!state.projectiles[i].alive) continue;
        process_projectile(&state.projectiles[i]);
    }

    // draw player time left
    const float percent = oct_Clamp(0, 1, state.lifespan / state.max_lifespan);
    const float x = (GAME_WIDTH / 2) - (112 / 2);
    const float y = 16;
    oct_DrawTexture(
            oct_GetAsset(gBundle, "textures/timebarempty.png"),
            (Oct_Vec2){x, y}
    );
    Oct_DrawCommand cmd = {
            .type = OCT_DRAW_COMMAND_TYPE_TEXTURE,
            .interpolate = OCT_INTERPOLATE_ALL,
            .id = 420,
            .colour = {1, 1, 1, 1},
            .Texture = {
                    .texture = oct_GetAsset(gBundle, "textures/timebar.png"),
                    .viewport = (Oct_Rectangle){
                        .position = {0, 0},
                        .size = {112 * percent, 32},
                    },
                    .position = {x, y},
                    .scale = {1, 1},
                    .origin = {0, 0},
                    .rotation = 0,
            }
    };
    oct_Draw(&cmd);
    state.lifespan -= 1.0 / 30.0;

    const float percent_kills = oct_Clamp(0, 1, (state.displayed_kills / ((float)state.req_kills - 1)));
    state.displayed_kills += (state.current_kills - state.displayed_kills) * 0.5;
    const float x2 = (GAME_WIDTH / 2) - (92 / 2);
    const float y2 = 48;
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
                            .size = {92 * percent_kills, 10},
                    },
                    .position = {x2, y2},
                    .scale = {1, 1},
                    .origin = {0, 0},
                    .rotation = 0,
            }
    };
    oct_Draw(&cmd2);

    const Oct_FontAtlas kingdom = oct_GetAsset(gBundle, "fnt_kingdom");
    // If user is 1 kill away from transforming, tell them
    if (state.req_kills -1 == state.current_kills) {
        Oct_Vec2 text_size;
        const float scale = (sin(oct_Time() * 4) / 4) + 1;
        oct_GetTextSize(kingdom, text_size, scale, "Transform!");
        oct_DrawTextColour(kingdom, (Oct_Vec2) {(GAME_WIDTH / 2) - (text_size[0] / 2) + 1, 32 - (text_size[1] / 2) + 1},
                           &(Oct_Colour) {0, 0, 0, 1}, scale, "Transform!");
        oct_DrawText(kingdom, (Oct_Vec2) {(GAME_WIDTH / 2) - (text_size[0] / 2), 32 - (text_size[1] / 2)}, scale, "Transform!");
    } else {
        // Draw total time
        Oct_Vec2 size;
        oct_GetTextSize(kingdom, size, 1, "%.1fs", state.total_time);
        oct_DrawTextColour(kingdom, (Oct_Vec2) {(GAME_WIDTH / 2) - (size[0] / 2) + 1, 22}, &(Oct_Colour) {0, 0, 0, 1},
                           1, "%.1fs", state.total_time);
        oct_DrawText(kingdom, (Oct_Vec2) {(GAME_WIDTH / 2) - (size[0] / 2), 21}, 1, "%.1fs", state.total_time);
    }

    state.total_time += 1.0 / 30.0;

    // Spawn more enemies
    if (gFrameCounter % 30 == 0) {
        add_ai(CHARACTER_TYPE_JUMPER);
    }

    // particles on top for some fucking reason
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!state.particles[i].alive) continue;
        process_particle(&state.particles[i]);
    }

    // just particles
    oct_WaitJobs();

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
    gFrameAllocator = oct_CreateArenaAllocator(4096);

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
            .windowTitle = "Jam Game",
            .windowWidth = 1280,
            .windowHeight = 720,
            .debug = true,
    };
    oct_Init(&initInfo);
    return 0;
}
