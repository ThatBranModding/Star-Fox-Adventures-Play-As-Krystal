#pragma once

// Trigger IDs come from paired character branches in player.c, rather than
// MusyX sample names inherited from the game's development builds. Unpaired
// reaction triggers use existing Krystal voices as intentional fallbacks.
struct VoiceReplacement {
    uint16_t fox;
    uint16_t krystal;
    const char* action;
    int16_t move = -1;
    int16_t alternateMove = -1;
    bool exitWindowOnly = false;
};
static constexpr VoiceReplacement kVoiceReplacements[] = {
    {0x01B, 0x2D5, "defence effort (exertion fallback)"},
    {0x01C, 0x2D5, "attack effort (exertion fallback)"},
    {0x01D, 0x398, "climb/pull up"},
    {0x024, 0x01F, "hurt reaction"},
    {0x025, 0x2CF, "landing effort"},
    {0x026, 0x2D0, "fall scream"},
    {0x027, 0x399, "landing/panting"},
    {0x029, 0x2CB, "grab ledge"},
    {0x02A, 0x327, "jump/landing variant (jump effort fallback)"},
    {0x02B, 0x2D3, "impact reaction"},
    {0x02E, 0x3CE, "roll"},
    {0x02F, 0x398, "water exit effort (climb fallback)"},
    {0x20E, 0x399, "fall recovery (landing/panting fallback)"},
    {0x214, 0x2D2, "death landing reaction"},
    {0x219, 0x2D5, "charge effort (exertion fallback)"},
    {0x2D4, 0x2D5, "exertion reaction"},
    {0x2D6, 0x2D7, "lifting effort"},
    {0x318, 0x01F, "special damage (hurt fallback)"},
    {0x367, 0x01F, "mushroom/poison damage reaction"},
    {0x379, 0x327, "jump effort"},
    {0x3C1, 0x320, "interaction/movement reaction"},
    {0x452, 0x399, "low health (panting fallback)"},
};

using PlayObjectSoundExFn = void (*)(void*, void*, uint32_t, uint16_t);
using PlaySimpleSoundFn = void (*)(void*, uint16_t);
using PlayChannelSoundFn = void (*)(void*, uint32_t, uint16_t);
using PlayPositionSoundFn = void (*)(void*, float, float, float, uint16_t);
using PlayLimitedSoundFn = uint32_t (*)(void*, uint16_t, int);
using IsPlayingSoundFn = int32_t (*)(void*, uint16_t);
using SetSoundVolumeFn = void (*)(void*, uint16_t, uint8_t, float);
using FindObjectSoundChannelFn = void* (*)(void*, uint32_t, uint16_t, int32_t);
using FindSoundTriggerFn = void* (*)(uint16_t);
using PlayAudioStreamFn = int (*)(int, void (*)());
static PlayAudioStreamFn g_playAudioStream{};
static void* g_playAudioStreamTarget{};
static PlayObjectSoundExFn g_playObjectSoundEx{};
static FindObjectSoundChannelFn g_findObjectSoundChannel{};
static FindSoundTriggerFn g_findSoundTrigger{};
static PlaySimpleSoundFn g_playSimpleSound{}, g_stopObjectSound{};
static PlayChannelSoundFn g_playChannelSound{};
static PlayPositionSoundFn g_playPositionSound{};
static PlayLimitedSoundFn g_playLimitedSound{};
static IsPlayingSoundFn g_isPlayingSound{};
static SetSoundVolumeFn g_setSoundVolume{};
static void *g_playSimpleSoundTarget{}, *g_playChannelSoundTarget{}, *g_playPositionSoundTarget{},
    *g_playLimitedSoundTarget{}, *g_stopObjectSoundTarget{}, *g_isPlayingSoundTarget{}, *g_setSoundVolumeTarget{};
static void* g_findObjectSoundChannelTarget{};
static bool g_loggedVoiceReplacements[sizeof(kVoiceReplacements) / sizeof(kVoiceReplacements[0])]{};
static bool g_loggedMissingVoiceTriggers[sizeof(kVoiceReplacements) / sizeof(kVoiceReplacements[0])]{};
static bool g_loggedUnmappedPlayerTriggers[65536]{};
static unsigned int g_exitSoundTraceFrames = 0;

static uint16_t remapPlayerVoice(void* object, uint16_t trigger, bool report) {
    void* player = g_objGetPlayerObject ? g_objGetPlayerObject() : nullptr;
    if (!krystalGameplayActive(player)) return trigger;
    if (!player || (object && object != player)) return trigger;
    const auto* bytes = static_cast<const uint8_t*>(player);
    auto banks = *reinterpret_cast<NativeModel* const* const*>(bytes + OBJ_MODEL_BANKS_OFFSET);
    if (!banks || !banks[0] || !banks[0]->file || banks[0]->file->modelId != KRYSTAL_MODEL_ID) return trigger;
    if (*reinterpret_cast<const int8_t*>(bytes + OBJ_BANK_INDEX_OFFSET) != 0) return trigger;
    for (size_t i = 0; i < sizeof(kVoiceReplacements) / sizeof(kVoiceReplacements[0]); ++i) {
        const auto& replacement = kVoiceReplacements[i];
        if (trigger != replacement.fox) continue;
        if (replacement.exitWindowOnly && (object || !g_exitSoundTraceFrames)) continue;
        if (replacement.move >= 0 &&
            *reinterpret_cast<const int16_t*>(bytes + 0xE0) != replacement.move &&
            *reinterpret_cast<const int16_t*>(bytes + 0xE0) != replacement.alternateMove) continue;
        // If this build lacks the replacement trigger, retain the original.
        if (!g_findSoundTrigger || !g_findSoundTrigger(replacement.krystal)) {
            if (report && !g_loggedMissingVoiceTriggers[i]) {
                g_loggedMissingVoiceTriggers[i] = true;
                char message[192];
                std::snprintf(message, sizeof(message), "Krystal voice: unavailable trigger 0x%X for %s; retaining Fox 0x%X.",
                    replacement.krystal, replacement.action, trigger);
                log(FH_LOG_WARN, message);
            }
            return trigger;
        }
        if (report && !g_loggedVoiceReplacements[i]) {
            g_loggedVoiceReplacements[i] = true;
            char message[192];
            std::snprintf(message, sizeof(message), "Krystal voice: %s remapped Fox 0x%X -> Krystal 0x%X.",
                replacement.action, trigger, replacement.krystal);
            log(FH_LOG_INFO, message);
        }
        return replacement.krystal;
    }
    if (report && !g_loggedUnmappedPlayerTriggers[trigger]) {
        g_loggedUnmappedPlayerTriggers[trigger] = true;
        char message[160];
        const int move = *reinterpret_cast<const int16_t*>(bytes + 0xE0);
        std::snprintf(message, sizeof(message), "Krystal voice: unmapped player sound 0x%X during move 0x%X.",
            trigger, static_cast<unsigned int>(static_cast<uint16_t>(move)));
        log(FH_LOG_INFO, message);
    }
    return trigger;
}

struct ExitSoundTraceEntry {
    void* owner;
    uint32_t channel;
    uint16_t trigger;
    int16_t playerMove;
    int16_t ownerMove;
};
static ExitSoundTraceEntry g_exitSoundTrace[512]{};
static size_t g_exitSoundTraceCount = 0;

static void traceExitSound(void* object, uint32_t channel, uint16_t trigger) {
    void* player = g_objGetPlayerObject ? g_objGetPlayerObject() : nullptr;
    if (krystalGameplayActive(player)) {
        const auto* bytes = static_cast<const uint8_t*>(player);
        const int16_t move = *reinterpret_cast<const int16_t*>(bytes + 0xE0);
        if (move == 0x262 || move == 0x263) g_exitSoundTraceFrames = 600;
        if (!g_exitSoundTraceFrames || g_exitSoundTraceCount >= 512) return;
        const int16_t ownerMove = object ?
            *reinterpret_cast<const int16_t*>(static_cast<const uint8_t*>(object) + 0xE0) : -1;
        for (size_t i = 0; i < g_exitSoundTraceCount; ++i) {
            const auto& entry = g_exitSoundTrace[i];
            if (entry.owner == object && entry.channel == channel && entry.trigger == trigger &&
                entry.playerMove == move && entry.ownerMove == ownerMove) return;
        }
        g_exitSoundTrace[g_exitSoundTraceCount++] = {object, channel, trigger, move, ownerMove};
        char message[256];
        std::snprintf(message, sizeof(message),
            "Arwing sound trace v2: move=0x%X trigger=0x%X owner=%s ptr=%p ownerMove=0x%X channel=0x%X playerBank=%d.",
            static_cast<unsigned int>(static_cast<uint16_t>(move)), trigger,
            object == player ? "player" : object ? "other" : "null", object,
            static_cast<unsigned int>(static_cast<uint16_t>(ownerMove)), channel,
            static_cast<int>(*reinterpret_cast<const int8_t*>(bytes + OBJ_BANK_INDEX_OFFSET)));
        log(FH_LOG_INFO, message);
    }
}

static void playObjectSoundExHook(void* object, void* position, uint32_t channel, uint16_t trigger) {
    traceExitSound(object, channel, trigger);
    g_playObjectSoundEx(object, position, channel, remapPlayerVoice(object, trigger, true));
}

static void playSimpleSoundHook(void* object, uint16_t trigger) {
    traceExitSound(object, 0, trigger);
    g_playSimpleSound(object, remapPlayerVoice(object, trigger, true));
}
static void playChannelSoundHook(void* object, uint32_t channel, uint16_t trigger) {
    traceExitSound(object, channel, trigger);
    g_playChannelSound(object, channel, remapPlayerVoice(object, trigger, true));
}
static void playPositionSoundHook(void* object, float x, float y, float z, uint16_t trigger) {
    traceExitSound(object, 0, trigger);
    g_playPositionSound(object, x, y, z, remapPlayerVoice(object, trigger, true));
}
static uint32_t playLimitedSoundHook(void* object, uint16_t trigger, int limit) {
    traceExitSound(object, 0, trigger);
    return g_playLimitedSound(object, remapPlayerVoice(object, trigger, true), limit);
}
static void stopObjectSoundHook(void* object, uint16_t trigger) {
    g_stopObjectSound(object, remapPlayerVoice(object, trigger, false));
}
static int32_t isPlayingSoundHook(void* object, uint16_t trigger) {
    return g_isPlayingSound(object, remapPlayerVoice(object, trigger, false));
}
static void setSoundVolumeHook(void* object, uint16_t trigger, uint8_t volume, float scale) {
    g_setSoundVolume(object, remapPlayerVoice(object, trigger, false), volume, scale);
}

static void* findObjectSoundChannelHook(void* object, uint32_t channel, uint16_t trigger, int32_t mode) {
    // Stop, is-playing, volume and duplicate checks must use the same trigger
    // as playback, otherwise the game's falling-voice lifecycle breaks.
    return g_findObjectSoundChannel(object, channel, remapPlayerVoice(object, trigger, false), mode);
}

using StreamIdFn = int32_t (*)();
using StreamCounterFn = uint32_t (*)();
static StreamIdFn g_currentStreamId{};
static StreamCounterFn g_streamSamples{}, g_streamPlayState{};
static bool g_arwingAudioReady{}, g_arwingVoicePlayed{};
static uint32_t g_arwingLastSample{};
static constexpr uint32_t ARWING_STREAM_RATE = 48043;
struct ArwingLandingVoice {
    uint16_t stream;
    const char* recording;
    uint32_t bytes;
    uint32_t cueSample;
    const char* context;
    uint16_t trigger = 0x398;
    bool allowBeforePlayerReady = false;
};
static constexpr ArwingLandingVoice kArwingLandingVoices[] = {
    {1270, "KP/kp1270.adp", 1146880, 744666, "Krazoa Palace"},
    {175, "TTH/tLanding.adp", 1081344, 736661, "shared landing (ThornTail/DarkIce/Walled City/Dragon Rock)"},
    {1404, "TTH/tLanding.adp", 1081344, 736661, "CloudRunner Fortress repeat landing"},
    {1128, "TTH/landing.adp", 1703936, 1268377, "ThornTail Hollow first landing", 0x398, true},
    {380, "CRF/landing.adp", 1540096, 558878, "CloudRunner Fortress landing"},
    {176, "TTH/tTakeoff.adp", 1310720, 419541, "shared boarding (ThornTail/DarkIce/Walled City/Dragon Rock)"},
    {1129, "TTH/tTakeoff.adp", 1310720, 419541, "CloudRunner/Walled City boarding"},
};
static bool g_arwingOverlayReady[sizeof(kArwingLandingVoices) / sizeof(kArwingLandingVoices[0])]{};
static int32_t g_arwingLastRecording = -1;
static constexpr uint16_t KRYSTAL_LEDGE_JUMP_TRIGGER = 0x398;
struct LandingStreamEntry {
    uint16_t id;
    uint8_t fadeFlags, volumeFlags;
    uint16_t length;
    char name[15];
    uint8_t flag;
};
static_assert(sizeof(LandingStreamEntry) == 22);
static const LandingStreamEntry* const* g_landingStreamEntries{};
static const int32_t* g_landingStreamCount{};

static void updateArwingLandingVoice() {
    if (!g_arwingAudioReady) return;
    // GetCurrentId returns a one-based table slot, not the recording ID
    // supplied to AudioStream_Play. Resolve it through the native table.
    const int32_t slot = g_currentStreamId();
    const auto* entries = g_landingStreamEntries ? *g_landingStreamEntries : nullptr;
    const int32_t count = g_landingStreamCount ? *g_landingStreamCount : 0;
    if (!entries || slot <= 0 || count <= 0 || count > 8192 || slot > count) {
        g_arwingVoicePlayed = false;
        g_arwingLastSample = 0;
        g_arwingLastRecording = -1;
        return;
    }
    const uint16_t recording = entries[slot - 1].id;
    const ArwingLandingVoice* landing = nullptr;
    for (size_t i = 0; i < sizeof(kArwingLandingVoices) / sizeof(kArwingLandingVoices[0]); ++i) {
        if (kArwingLandingVoices[i].stream == recording && g_arwingOverlayReady[i]) {
            landing = &kArwingLandingVoices[i];
            break;
        }
    }
    if (!landing) {
        g_arwingVoicePlayed = false;
        g_arwingLastSample = 0;
        g_arwingLastRecording = -1;
        return;
    }
    const uint32_t sample = g_streamSamples();
    if (recording != g_arwingLastRecording || sample < g_arwingLastSample) g_arwingVoicePlayed = false;
    g_arwingLastRecording = recording;
    g_arwingLastSample = sample;
    void* player = g_objGetPlayerObject ? g_objGetPlayerObject() : nullptr;
    bool krystal = false;
    if (krystalGameplayActive(player)) {
        const auto* bytes = static_cast<const uint8_t*>(player);
        auto banks = *reinterpret_cast<NativeModel* const* const*>(bytes + OBJ_MODEL_BANKS_OFFSET);
        krystal = *reinterpret_cast<const int8_t*>(bytes + OBJ_BANK_INDEX_OFFSET) == 0 &&
            banks && banks[0] && banks[0]->file && banks[0]->file->modelId == KRYSTAL_MODEL_ID;
    }
    const bool window = sample >= landing->cueSample && sample < landing->cueSample + ARWING_STREAM_RATE;
    if ((!krystal && !landing->allowBeforePlayerReady) || !window || g_streamPlayState() != 1 || !g_findSoundTrigger ||
        !g_findSoundTrigger(landing->trigger)) {
        return;
    }
    if (!g_arwingVoicePlayed) {
        g_arwingVoicePlayed = true;
        g_playObjectSoundEx(nullptr, nullptr, 0, landing->trigger);
        char message[256];
        std::snprintf(message, sizeof(message), "Krystal cutscene voice: %s, stream %u, trigger 0x%X at %.3f seconds; background retained.",
            landing->context, recording, landing->trigger, static_cast<double>(landing->cueSample) / ARWING_STREAM_RATE);
        log(FH_LOG_INFO, message);
    }
}

static bool initializeArwingLandingVoice() {
    g_currentStreamId = reinterpret_cast<StreamIdFn>(g_host->symbolAddress(g_mod, "AudioStream_GetCurrentId"));
    g_streamSamples = reinterpret_cast<StreamCounterFn>(g_host->symbolAddress(g_mod, "AIGetStreamSampleCount"));
    g_streamPlayState = reinterpret_cast<StreamCounterFn>(g_host->symbolAddress(g_mod, "AIGetStreamPlayState"));
    g_landingStreamEntries = reinterpret_cast<const LandingStreamEntry* const*>(g_host->symbolAddress(g_mod, "gStreamsData"));
    g_landingStreamCount = static_cast<const int32_t*>(g_host->symbolAddress(g_mod, "gStreamsCount"));
    if (!g_currentStreamId || !g_streamSamples || !g_streamPlayState ||
        !g_landingStreamEntries || !g_landingStreamCount || !g_host->modDir) return false;
    const char* directory = g_host->modDir(g_mod);
    if (!directory) return false;
    bool anyReady = false;
    for (size_t i = 0; i < sizeof(kArwingLandingVoices) / sizeof(kArwingLandingVoices[0]); ++i) {
        const auto& landing = kArwingLandingVoices[i];
        std::ifstream audio(std::string(directory) + "/overlay/streams/" + landing.recording, std::ios::binary | std::ios::ate);
        g_arwingOverlayReady[i] = audio && audio.tellg() == landing.bytes;
        anyReady |= g_arwingOverlayReady[i];
        char message[256];
        std::snprintf(message, sizeof(message), "Krystal landing overlay: %s stream %u %s (cue %.3f seconds).",
            landing.recording, landing.stream, g_arwingOverlayReady[i] ? "ready" : "unavailable",
            static_cast<double>(landing.cueSample) / ARWING_STREAM_RATE);
        log(g_arwingOverlayReady[i] ? FH_LOG_INFO : FH_LOG_WARN, message);
    }
    if (!anyReady) return false;
    g_arwingAudioReady = true;
    return true;
}

static int playAudioStreamHook(int id, void (*preparedCallback)()) {
    // Sequence speech can be streamed rather than played as an object SFX.
    // Observe this separate route without changing playback or callbacks.
    static unsigned int streamTraceCount = 0;
    if (streamTraceCount < 256) {
        ++streamTraceCount;
        void* player = g_objGetPlayerObject ? g_objGetPlayerObject() : nullptr;
        const int move = player ? *reinterpret_cast<const int16_t*>(
            static_cast<const uint8_t*>(player) + 0xE0) : -1;
        char message[192];
        std::snprintf(message, sizeof(message),
            "Arwing stream trace: stream=%d (0x%X) playerMove=0x%X exitWindow=%s.",
            id, static_cast<unsigned int>(id),
            static_cast<unsigned int>(static_cast<uint16_t>(move)), g_exitSoundTraceFrames ? "yes" : "no");
        log(FH_LOG_INFO, message);
    }
    return g_playAudioStream(id, preparedCallback);
}
