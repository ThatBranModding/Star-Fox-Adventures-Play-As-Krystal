#include "../mod.cpp"
#include <cstdlib>
#include <iterator>
static void* testPlayer;
static void* getPlayer() { return testPlayer; }
static NativeSphereDef observed;
static NativeSphereDef observed12;
static uint8_t* observedHeader;
static unsigned updateCalls;
static void originalUpdate(uint8_t*, uint8_t* hdr, uint8_t*, uint8_t*, uint8_t*) {
    ++updateCalls;
    observedHeader = hdr;
    auto* h = reinterpret_cast<NativeModelHeader*>(hdr);
    observed = reinterpret_cast<NativeSphereDef*>(h->hitVolumes)[16];
    observed12 = reinterpret_cast<NativeSphereDef*>(h->hitVolumes)[12];
}
static void require(bool condition) { if (!condition) std::abort(); }
static uint8_t campaign() { return 1; }
static uint32_t completeBit(int) { return 1; }
int main() {
    alignas(8) uint8_t player[0x100]{};
    testPlayer = player; static uint8_t dummySave; static uint8_t* readySave = &dummySave; g_storySaveData = &readySave; g_saveCharacter = campaign; g_mainGetBit = completeBit; g_gameplayPlayer = player; g_staffGameplayReady = true;
    NativeSphereDef kd{}; kd.maskBit = 12;
    NativeSphereDef fd{}; fd.maskBit = 16;
    *reinterpret_cast<int16_t*>(player + 0xFC) = -1;
    NativeModelHeader kh{}, fh{};
    kh.modelId = 0x4E8; kh.hitVolumeCount = 1; kh.hitVolumes = reinterpret_cast<uint8_t*>(&kd);
    fh.modelId = 1; fh.hitVolumeCount = 1; fh.hitVolumes = reinterpret_cast<uint8_t*>(&fd);
    NativeSphere sphere{2, {3, 4, 5}};
    NativeModel km{}, fm{};
    km.file = &kh; fm.file = &fh;
    km.active = fm.active = &sphere;
    km.buffers[1] = fm.buffers[1] = &sphere;
    NativeModel* banks[]{&km, &fm};
    *reinterpret_cast<NativeModel***>(player + OBJ_MODEL_BANKS_OFFSET) = banks;
    *reinterpret_cast<int16_t*>(player + 0xE0) = 0x123;
    *reinterpret_cast<float*>(player + 0xD8) = 0.5f;
    g_objGetPlayerObject = getPlayer;
    NativeSphereDef kdefs[20]{}, fdefs[20]{};
    kdefs[16].joint = 23; kdefs[16].radius = 2.856f;
    fdefs[16].joint = 27; fdefs[16].radius = 3.8066f;
    kdefs[16].sphereIndex = fdefs[16].sphereIndex = 16;
    kdefs[16].maskBit = fdefs[16].maskBit = 16;
    kdefs[12].joint = 27; kdefs[12].radius = 2.9485f;
    fdefs[12].joint = 23; fdefs[12].radius = 3.7352f;
    kdefs[12].sphereIndex = fdefs[12].sphereIndex = 12;
    kdefs[12].maskBit = fdefs[12].maskBit = 12;
    kh.hitVolumeCount = fh.hitVolumeCount = 20;
    kh.jointCount = fh.jointCount = 36;
    kh.hitVolumes = reinterpret_cast<uint8_t*>(kdefs);
    fh.hitVolumes = reinterpret_cast<uint8_t*>(fdefs);
    *reinterpret_cast<int16_t*>(player + 0xE0) = 0x460;
    g_updateHitSpheres = originalUpdate;
    auto* kmBytes = reinterpret_cast<uint8_t*>(&km);
    auto* khBytes = reinterpret_cast<uint8_t*>(&kh);
    updateHitSpheresHook(kmBytes, khBytes, player, nullptr, player);
    require(observed.joint == 27 && observed.radius == 3.8066f && observedHeader != khBytes);
    require(kh.hitVolumes == reinterpret_cast<uint8_t*>(kdefs) && kdefs[16].joint == 23);
    require(kh.hitVolumeCount == 20 && km.active == &sphere);
    require(observed12.joint == 27 && observed12.radius == 2.9485f);
    *reinterpret_cast<int16_t*>(player + 0xE0) = 0x464;
    updateHitSpheresHook(kmBytes, khBytes, player, nullptr, player);
    require(observed.joint == 27 && observed.radius == 3.8066f);
    require(observed12.joint == 23 && observed12.radius == 3.7352f);
    *reinterpret_cast<int16_t*>(player + 0xE0) = 0x468;
    updateHitSpheresHook(kmBytes, khBytes, player, nullptr, player);
    require(observed.joint == 23 && observed.radius == 2.856f);
    require(observed12.joint == 23 && observed12.radius == 3.7352f);
    *reinterpret_cast<int16_t*>(player + 0xE0) = 0x472;
    updateHitSpheresHook(kmBytes, khBytes, player, nullptr, player);
    require(observed.joint == 23 && observedHeader == khBytes);
    *reinterpret_cast<int16_t*>(player + 0xE0) = 0x460;
    player[OBJ_BANK_INDEX_OFFSET] = 1;
    updateHitSpheresHook(kmBytes, khBytes, player, nullptr, player);
    require(observed.joint == 23 && observedHeader == khBytes);
    player[OBJ_BANK_INDEX_OFFSET] = 0;
    fdefs[16].joint = 100;
    updateHitSpheresHook(kmBytes, khBytes, player, nullptr, player);
    require(observed.joint == 23 && observedHeader == khBytes && updateCalls == 6);
    require(kdefs[12].joint == 27 && kdefs[16].joint == 23);
    std::puts("PASS: three move-specific sphere transplants, shared data preserved, other moves/banks and invalid donor unaffected");
}


