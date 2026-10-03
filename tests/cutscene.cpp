#include "../mod.cpp"
#include <cstdlib>

static uint8_t character = 1, camera;
static bool acquired;
static int sequence;
static void* player;
static NativeModelHeader fox{}, krystal{};
static int16_t foxIds[]{12, 34, -1, 56};
static int releases, staffBank = -1;
static NativeModelHeader* requestedHeader;
static uint8_t definition[0x100]{};
static int32_t modelId = 1;
static uint8_t getCharacter() { return character; }
static uint32_t getBit(int id) { return id == 0x18B && acquired; }
static int getSequence() { return sequence; }
static void* getPlayer() { return player; }
static void select(void* obj, int bank) { static_cast<uint8_t*>(obj)[OBJ_BANK_INDEX_OFFSET] = bank; }
static void* loadModel(int id, int, int*) { return id == -1 ? &fox : &krystal; }
static void release(void*) { ++releases; }
static void* loadDefinition(int) { return definition; }
static void staff(void* obj, void*) { staffBank = static_cast<uint8_t*>(obj)[OBJ_BANK_INDEX_OFFSET]; }
static void* loadMove(NativeModelHeader* header, int16_t, int, uint8_t* output) {
    requestedHeader = header;
    // Reproduce the native cutscene-pack restriction. Also verify that the
    // joint layout and AMAP offset still come from the Krystal header.
    if (header->modelId != 1 && header->modelId != 3) return nullptr;
    if (header->jointCount != krystal.jointCount || header->animationDataFileOffset != 456) std::abort();
    return output;
}
static void resetMove(void* model, void* state) {
    auto* header = *reinterpret_cast<NativeModelHeader**>(model);
    *static_cast<bool*>(state) = header->modelId == 1 && header->cachedAnimIds[1] == 34;
}
static void require(bool ok) { if (!ok) std::abort(); }

int main() {
    alignas(8) uint8_t actor[0x110]{};
    uint8_t* save = actor;
    player = actor;
    *reinterpret_cast<int16_t*>(actor + 0xFC) = -1;
    fox.modelId = 1; fox.flags = 0x40; fox.cachedAnimIds = foxIds;
    fox.animationCount = 4; fox.animationDataFileOffset = 456; fox.animGroupBaseIndices[1] = 3;
    krystal.modelId = KRYSTAL_MODEL_ID; krystal.flags = 0x40; krystal.jointCount = 24; krystal.refCount = 1;
    g_storySaveData = &save; g_saveCharacter = getCharacter; g_mainGetBit = getBit;
    g_currentSequence = getSequence; g_sequenceCameraActive = &camera;
    g_objGetPlayerObject = getPlayer; g_setModel = select;
    g_objModelLoad = loadModel; g_modelRelease = release; g_loadObjectFile = loadDefinition;
    g_loadAnimation = loadMove; g_playerStaffInit = staff;
    g_resetAnimation = resetMove;
    *reinterpret_cast<int32_t**>(definition + 8) = &modelId; definition[0x91] = 1;

    // A cutscene actor loads before any gameplay player. Its mapping must
    // survive release of the temporary Fox donor.
    int size;
    require(objModelLoadHook(-KRYSTAL_MODEL_ID, 0, &size) == &krystal);
    require(releases == 1 && krystal.cachedAnimIds != foxIds && krystal.cachedAnimIds[1] == 34);
    foxIds[1] = 999; require(krystal.cachedAnimIds[1] == 34);
    require(krystal.animationCount == 4 && krystal.animGroupBaseIndices[1] == 3);
    for (int id : {0x485, 0x487, 0x488}) {
        modelId = 1; require(loadObjectFileHook(id) == definition && modelId == KRYSTAL_MODEL_ID);
    }
    modelId = 1; loadObjectFileHook(20); require(modelId == 1);
    modelId = 99; loadObjectFileHook(0x487); require(modelId == 99);
    modelId = 1; definition[0x91] = 2; loadObjectFileHook(0x487); require(modelId == 1);
    definition[0x91] = 1;
    uint8_t output[16]{};
    require(loadAnimationHook(&krystal, 34, 0, output) == output);
    require(requestedHeader != &krystal && krystal.modelId == KRYSTAL_MODEL_ID);
    NativeModelHeader ordinary = krystal;
    require(loadAnimationHook(&ordinary, 34, 0, output) == nullptr && requestedHeader == &ordinary);

    NativeModel km{}, fm{}; km.file = &krystal; fm.file = &fox;
    bool initialized = false;
    resetAnimationHook(&km, &initialized);
    require(initialized && km.file == &krystal && krystal.modelId == KRYSTAL_MODEL_ID);
    NativeModel* banks[]{&km, &fm};
    *reinterpret_cast<NativeModel***>(actor + OBJ_MODEL_BANKS_OFFSET) = banks;
    g_gameplayPlayer = actor;
    camera = 1; sequence = 1151; *reinterpret_cast<int16_t*>(actor + 0xFC) = 0;
    setModelHook(actor, 1); require(actor[OBJ_BANK_INDEX_OFFSET] == 0);
    acquired = true; playerStaffInitHook(actor, nullptr);
    require(staffBank == 1 && actor[OBJ_BANK_INDEX_OFFSET] == 0 && !krystalStaffActive(actor));
    camera = 0; sequence = 0; *reinterpret_cast<int16_t*>(actor + 0xFC) = -1;
    playerStaffInitHook(actor, nullptr);
    require(staffBank == 1 && actor[OBJ_BANK_INDEX_OFFSET] == 0 && krystalStaffActive(actor));
    setModelHook(actor, 2); require(actor[OBJ_BANK_INDEX_OFFSET] == 2);

    character = 0; modelId = KRYSTAL_MODEL_ID;
    loadObjectFileHook(0x487); require(modelId == 1);
    setModelHook(actor, 1); require(actor[OBJ_BANK_INDEX_OFFSET] == 1);
    require(loadAnimationHook(&krystal, 34, 0, output) == nullptr && requestedHeader == &krystal);
    initialized = true; resetAnimationHook(&km, &initialized); require(!initialized);
    character = 1;
    modelReleaseHook(&km); require(g_injectedKrystalHeaders.empty() && releases == 2);
    require(loadAnimationHook(&krystal, 34, 0, output) == nullptr);
    std::puts("PASS: cutscene-first load; owned animation IDs; actor redirection; native animation restriction; staff unlock after complete sequence; prologue/disguise preserved; released headers excluded");
}
