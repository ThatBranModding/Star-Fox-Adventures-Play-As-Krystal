#pragma once

struct PortraitTexture {
    void* next;
    uint8_t reserved06[6];
    uint16_t width, height, references, frames;
    uint8_t reserved16[2];
    uint16_t frameStep;
    uint8_t format, wrapS, wrapT, minFilter, magFilter, reserved1f, minLod, maxLod;
    uint8_t reserved22[2];
    uint32_t gxObject[16];
    void* tmem;
    uint32_t dataSize;
    uint8_t preloaded, cached, reserved76, eviction;
    uint32_t loadedSize;
    int32_t imageOffset;
    uint8_t reserved80[12];
};
static_assert(offsetof(PortraitTexture, width) == 0x0e);
static_assert(offsetof(PortraitTexture, gxObject) == 0x24);
static_assert(sizeof(PortraitTexture) == 0x90);

struct PortraitMeter {
    int32_t initial, capacity, fillWidth, value, segmentWidth, yOffset;
    uint8_t alpha, reserved19[11];
    float field24;
    uint8_t reserved28[8];
    uint16_t backgroundId;
    uint8_t reserved32[6];
    PortraitTexture* background;
    void* end;
    void* filled;
    void* empty;
    int32_t type;
    uint8_t flags, reserved5d[3];
};
static_assert(offsetof(PortraitMeter, background) == 0x38);
static_assert(offsetof(PortraitMeter, type) == 0x58);

using PortraitDrawFn = void (*)(void*, float, float, int, int);
static PortraitDrawFn g_portraitDraw{};
static void (*g_portraitMeterDraw)(){};
static PortraitMeter** g_portraitMeter{};
static void* g_portraitDrawTarget{}, *g_portraitMeterTarget{};
static PortraitTexture* g_portraitOriginal{};
static std::vector<uint64_t> g_portraitTexture;

static bool portraitMeterEligible(const PortraitMeter* meter) {
    return meter && meter->type == 1 && meter->backgroundId == 0x603 && meter->background &&
        !g_portraitTexture.empty() && krystalGameplayActive(g_gameplayPlayer) &&
        playerHasInjectedKrystal(g_gameplayPlayer) && !playerSequenceActive(g_gameplayPlayer) &&
        *reinterpret_cast<int8_t*>(static_cast<uint8_t*>(g_gameplayPlayer) + OBJ_BANK_INDEX_OFFSET) == 0;
}

static void portraitDrawHook(void* texture, float x, float y, int alpha, int scale) {
    if (texture && texture == g_portraitOriginal) {
        const auto* original = static_cast<const PortraitTexture*>(texture);
        if (original->width == 70 && original->height == 68) {
            g_portraitDraw(g_portraitTexture.data(), x, y, alpha, scale);
            return;
        }
    }
    g_portraitDraw(texture, x, y, alpha, scale);
}

static void portraitMeterHook() {
    const auto* meter = g_portraitMeter ? *g_portraitMeter : nullptr;
    auto* saved = g_portraitOriginal;
    g_portraitOriginal = portraitMeterEligible(meter) ? meter->background : nullptr;
    g_portraitMeterDraw();
    g_portraitOriginal = saved;
}

static void shutdownPortraitSprite() {
    if (g_host && g_host->hookRemove) {
        if (g_portraitDrawTarget) g_host->hookRemove(g_mod, g_portraitDrawTarget);
        if (g_portraitMeterTarget) g_host->hookRemove(g_mod, g_portraitMeterTarget);
    }
    g_portraitDrawTarget = g_portraitMeterTarget = g_portraitOriginal = nullptr;
    g_portraitTexture.clear();
}

static void initializePortraitSprite() {
    const char* directory = g_host->modDir ? g_host->modDir(g_mod) : nullptr;
    std::vector<uint8_t> pixels;
    auto initialize = reinterpret_cast<void (*)(void*)>(g_host->symbolAddress(g_mod, "textureInitGXTexObj"));
    g_portraitMeter = reinterpret_cast<PortraitMeter**>(g_host->symbolAddress(g_mod, "airMeter"));
    if (!directory || !initialize || !g_portraitMeter ||
        !readFile(std::string(directory) + "/assets/gas-meter-portrait.rgba", pixels) || pixels.size() != 70 * 68 * 4) {
        log(FH_LOG_WARN, "Krystal gas portrait unavailable; original meter retained.");
        return;
    }
    g_portraitTexture.assign((sizeof(PortraitTexture) + pixels.size()) / sizeof(uint64_t), 0);
    auto* texture = reinterpret_cast<PortraitTexture*>(g_portraitTexture.data());
    texture->width = 70;
    texture->height = 68;
    texture->references = 1;
    texture->format = 0x46;
    texture->minFilter = texture->magFilter = 1;
    texture->loadedSize = uint32_t(sizeof(PortraitTexture) + pixels.size());
    std::memcpy(reinterpret_cast<uint8_t*>(texture) + sizeof(PortraitTexture), pixels.data(), pixels.size());
    initialize(texture);
    void* original{};
    bool ready = installHook("hudDrawAirMeter", reinterpret_cast<void*>(portraitMeterHook), &g_portraitMeterTarget, &original);
    if (ready) g_portraitMeterDraw = reinterpret_cast<void (*)()>(original);
    if (ready) {
        ready = installHook("drawTexture", reinterpret_cast<void*>(portraitDrawHook), &g_portraitDrawTarget, &original);
        if (ready) g_portraitDraw = reinterpret_cast<PortraitDrawFn>(original);
    }
    if (!ready) {
        shutdownPortraitSprite();
        log(FH_LOG_WARN, "Krystal gas portrait hooks unavailable; original meter retained.");
    }
}
