#include <oct/Octarine.h>

///////////////////////// ENUMS /////////////////////////
typedef enum {
    CHARACTER_TYPE_INVALID = 0,
    CHARACTER_TYPE_JUMPER = 1,
    CHARACTER_TYPE_X_SHOOTER = 2,
    CHARACTER_TYPE_Y_SHOOTER = 3,
    CHARACTER_TYPE_XY_SHOOTER = 4,
    CHARACTER_TYPE_SWORDSMITH = 5,
    CHARACTER_TYPE_LASER = 6,
} CharacterType;

///////////////////////// GLOBALS /////////////////////////
Oct_AssetBundle gBundle;
Oct_Allocator gAllocator;

///////////////////////// CONSTANTS /////////////////////////
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
} Character;

///////////////////////// HELPERS /////////////////////////
///////////////////////// GAME /////////////////////////
void *startup() {
    gBundle = oct_LoadAssetBundle("data");
    gAllocator = oct_CreateHeapAllocator();
    return null;
}

// Called each logical frame, whatever you return is passed to either the next update or shutdown
void *update(void *ptr) {
    return null;
}

// Called once when the engine is about to be deinitialized
void shutdown(void *ptr) {
    oct_FreeAllocator(gAllocator);
    oct_FreeAssetBundle(gBundle);
}


///////////////////////// MAIN /////////////////////////
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
