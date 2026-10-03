#include "../mod.cpp"
#include <cstdlib>

static const char* directory;
static void* player;
static uint8_t character = 1, camera;
static uint8_t saveByte;
static uint8_t* save = &saveByte;
static bool suppress, skipped, available = true;
static int playbackCalls, forbiddenHooks;
static uint16_t played, lookedUp;
static void* playedOwner;
static void* playedPosition;
static uint32_t playedChannel;
static int seenLimit;
static uint8_t seenVolume;
static float seenScale, seenX, seenY, seenZ;
static uint8_t getCharacter() { return character; }
static int getSequence() { return 0; }
static uint32_t getBit(int) { return 0; }
static void* getPlayer() { return player; }
static void unused() {}
static void* trigger(uint16_t) { return available ? &saveByte : nullptr; }
static void* findChannel(void*, uint32_t, uint16_t id, int32_t) { lookedUp = id; return &saveByte; }
static void nativePlayback(void* object, void* pos, uint32_t channel, uint16_t id) {
    // Reproduce native inlining: there is no separate resolver call.
    if (suppress) return;
    if (pos) { auto* xyz = static_cast<float*>(pos); seenX = xyz[0]; seenY = xyz[1]; seenZ = xyz[2]; }
    played = id; playedOwner = object; playedPosition = pos; playedChannel = channel; ++playbackCalls;
}
static void cutsceneSkipPlayback(void* object, void* pos, uint32_t channel, uint16_t id) {
    if (!skipped) nativePlayback(object, pos, channel, id);
}
static void simple(void* object, uint16_t id) { cutsceneSkipPlayback(object, nullptr, 0, id); }
static void channel(void* object, uint32_t c, uint16_t id) { cutsceneSkipPlayback(object, nullptr, c, id); }
static void position(void* object, float x, float y, float z, uint16_t id) { float xyz[]{x,y,z}; cutsceneSkipPlayback(object, xyz, 0, id); }
static uint32_t limited(void* object, uint16_t id, int limit) { seenLimit = limit; cutsceneSkipPlayback(object, nullptr, 0, id); return 123; }
static void stop(void*, uint16_t id) { lookedUp = id; }
static int32_t isPlaying(void*, uint16_t id) { lookedUp = id; return 17; }
static void volume(void*, uint16_t id, uint8_t level, float scale) { lookedUp = id; seenVolume = level; seenScale = scale; }
static const char* modDirectory(FhMod*) { return directory; }
static void* symbol(FhMod*, const char* name) {
    if (!std::strcmp(name, "Sfx_PlayFromObjectEx")) return reinterpret_cast<void*>(cutsceneSkipPlayback);
    if (!std::strcmp(name, "Sfx_PlayFromObject")) return reinterpret_cast<void*>(simple);
    if (!std::strcmp(name, "Sfx_PlayFromObjectChannel")) return reinterpret_cast<void*>(channel);
    if (!std::strcmp(name, "Sfx_PlayAtPositionFromObject")) return reinterpret_cast<void*>(position);
    if (!std::strcmp(name, "Sfx_PlayFromObjectLimited")) return reinterpret_cast<void*>(limited);
    if (!std::strcmp(name, "Sfx_StopFromObject")) return reinterpret_cast<void*>(stop);
    if (!std::strcmp(name, "Sfx_IsPlayingFromObject")) return reinterpret_cast<void*>(isPlaying);
    if (!std::strcmp(name, "Sfx_SetObjectSfxVolume")) return reinterpret_cast<void*>(volume);
    if (!std::strcmp(name, "Sfx_FindObjectChannel")) return reinterpret_cast<void*>(findChannel);
    if (!std::strcmp(name, "Sfx_FindTrigger")) return reinterpret_cast<void*>(trigger);
    if (!std::strcmp(name, "Obj_GetPlayerObject")) return reinterpret_cast<void*>(getPlayer);
    if (!std::strcmp(name, "SaveGame_getCurChar")) return reinterpret_cast<void*>(getCharacter);
    if (!std::strcmp(name, "mainGetBit")) return reinterpret_cast<void*>(getBit);
    if (!std::strcmp(name, "getCurSeqNo")) return reinterpret_cast<void*>(getSequence);
    if (!std::strcmp(name, "gGameBitSaveData")) return &save;
    if (!std::strcmp(name, "gObjSeqCameraActive")) return &camera;
    return reinterpret_cast<void*>(unused);
}
static int install(FhMod*, void* target, void*, void** original) {
    // The outer playback patch belongs to another mod and refuses installation.
    if (target == reinterpret_cast<void*>(cutsceneSkipPlayback)) { ++forbiddenHooks; return FH_MOD_ERROR; }
    *original = target;
    return FH_MOD_OK;
}
static int remove(FhMod*, void*) { return FH_MOD_OK; }
static void require(bool condition) { if (!condition) std::abort(); }

int main(int argc, char** argv) {
    require(argc == 2); directory = argv[1];
    FhModHost host{}; host.abiVersion = FH_MOD_ABI_VERSION;
    host.modDir = modDirectory; host.symbolAddress = symbol; host.hookInstall = install; host.hookRemove = remove;
    require(fh_mod_initialize(reinterpret_cast<FhMod*>(1), &host) == FH_MOD_OK);
    require(forbiddenHooks == 0 && g_arwingAudioReady);
    require(g_playObjectSoundEx == cutsceneSkipPlayback);
    alignas(8) uint8_t actor[0x110]{}, npc[0x110]{};
    NativeModelHeader header{}; header.modelId = KRYSTAL_MODEL_ID;
    NativeModel model{}; model.file = &header;
    NativeModel* banks[]{&model, &model};
    *reinterpret_cast<NativeModel***>(actor + OBJ_MODEL_BANKS_OFFSET) = banks;
    *reinterpret_cast<int16_t*>(actor + 0xFC) = -1;
    player = actor; g_gameplayPlayer = player;
    for (const auto& pair : kVoiceReplacements) {
        playSimpleSoundHook(actor, pair.fox);
        require(played == pair.krystal && playedOwner == actor && playedPosition == nullptr && playedChannel == 0);
        playChannelSoundHook(actor, 7, pair.fox); require(played == pair.krystal && playedChannel == 7);
        playPositionSoundHook(actor, 1, 2, 3, pair.fox); require(played == pair.krystal && seenX == 1 && seenY == 2 && seenZ == 3);
        require(playLimitedSoundHook(actor, pair.fox, 8) == 123 && seenLimit == 8 && played == pair.krystal);
        stopObjectSoundHook(actor, pair.fox); require(lookedUp == pair.krystal);
        require(isPlayingSoundHook(actor, pair.fox) == 17 && lookedUp == pair.krystal);
        setSoundVolumeHook(actor, pair.fox, 70, 0.5f); require(lookedUp == pair.krystal && seenVolume == 70 && seenScale == 0.5f);
        for (int mode : {0, 1, 2, 3}) {
            findObjectSoundChannelHook(actor, 0, pair.fox, mode); require(lookedUp == pair.krystal);
        }
    }
    int before = playbackCalls;
    skipped = true; playSimpleSoundHook(actor, 0x024); require(playbackCalls == before);
    skipped = false; suppress = true; playSimpleSoundHook(actor, 0x024); require(playbackCalls == before);
    suppress = false; available = false; playSimpleSoundHook(actor, 0x024); require(played == 0x024);
    available = true; playSimpleSoundHook(npc, 0x024); require(played == 0x024);
    playSimpleSoundHook(actor, 0xC0); require(played == 0xC0);
    character = 0; playSimpleSoundHook(actor, 0x024); require(played == 0x024);
    character = 1; actor[OBJ_BANK_INDEX_OFFSET] = 2;
    playSimpleSoundHook(actor, 0x024); require(played == 0x024);
    actor[OBJ_BANK_INDEX_OFFSET] = 0; camera = 1;
    playSimpleSoundHook(actor, 0x024); require(played == 0x01F && !krystalStaffActive(actor));
    fh_mod_shutdown(nullptr);
    std::puts("PASS: full initialization with occupied playback hook and inlined resolver; all 22 mappings through four playback entries and stop/check/volume lifecycle; Arwing enabled; skip/native suppression preserved; NPC/prologue/disguise and unknown/missing sounds unchanged");
}
