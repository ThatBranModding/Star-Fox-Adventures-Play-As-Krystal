#include "foxhollow_mod_api.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include "model_layout.h"

static FhMod* g_mod{};
static const FhModHost* g_host{};

using LoadCharacterFn = void* (*)(int16_t*, int, int, int, void*, int);
using ObjModelLoadFn = void* (*)(int, int, int*);
using LoadObjectFileFn = void* (*)(int);
using LoadAnimationFn = void* (*)(NativeModelHeader*, int16_t, int, uint8_t*);
using ModelReleaseFn = void (*)(void*);
using ResetAnimationFn = void (*)(void*, void*);
using ModelListGetHeaderFn = int (*)(void*, int, void*);
using GetTableFileEntryFn = int (*)(int, int, int*);
using LoadModelsBinFn = void (*)(int, int*, int*, int*, int*, int);
using LoadAndDecompressFn = void* (*)(int, void*, int, uint32_t, int*, int, uint32_t);
using GetCurrentDataFileFn = void* (*)(int);
using Tex1GetFrameFn = void (*)(int, int, int*, int*, int, int*, int);
using ZlbDecompressFn = int (*)(uint8_t*, int, uint8_t*, void*);
using PlayerInitFn = void (*)(void*);
using SetModelFn = void (*)(void*, int);
using TextureLoadFn = void* (*)(int, uint8_t);
using PlayerStaffInitFn = void (*)(void*, void*);
using ObjIsCurModelNotZeroFn = int (*)(void*);
using ObjGetPlayerObjectFn = void* (*)();
using ObjBuildWorldTransformMatrixFn = void (*)(void*, float*, int);

static LoadCharacterFn g_loadCharacter{};
static ObjModelLoadFn g_objModelLoad{};
static LoadObjectFileFn g_loadObjectFile{};
static LoadAnimationFn g_loadAnimation{};
static ModelReleaseFn g_modelRelease{};
static ResetAnimationFn g_resetAnimation{};
static ModelListGetHeaderFn g_modelListGetHeader{};
static GetTableFileEntryFn g_getTableFileEntry{};
static LoadModelsBinFn g_loadModelsBin{};
static LoadAndDecompressFn g_loadAndDecompress{};
static GetCurrentDataFileFn g_getCurrentDataFile{};
static int* g_texBankCount{};
static Tex1GetFrameFn g_tex1GetFrame{};
static ZlbDecompressFn g_zlbDecompress{};
static PlayerInitFn g_playerInit{};
static SetModelFn g_setModel{};
static TextureLoadFn g_textureLoad{};
static PlayerStaffInitFn g_playerStaffInit{};
static ObjIsCurModelNotZeroFn g_objIsCurModelNotZero{};
static ObjGetPlayerObjectFn g_objGetPlayerObject{};
static ObjBuildWorldTransformMatrixFn g_objBuildWorldTransformMatrix{};
static uint8_t (*g_saveCharacter)(){};
static uint8_t** g_storySaveData{};
static uint32_t (*g_mainGetBit)(int){};
static int (*g_currentSequence)(){};
static const uint8_t* g_sequenceCameraActive{};
static void* g_gameplayPlayer{};
static bool g_staffGameplayReady{};
static std::vector<void*> g_injectedKrystalHeaders;

static bool isFoxCampaign() {
    return g_storySaveData && *g_storySaveData && g_saveCharacter &&
        g_saveCharacter() == 1;
}

static bool staffPickupComplete() {
    return isFoxCampaign() && g_mainGetBit &&
        g_mainGetBit(0x18B);
}

static bool krystalGameplayActive(void* obj) {
    return obj && obj == g_gameplayPlayer && isFoxCampaign();
}

static bool playerSequenceActive(void* obj);

static bool krystalStaffActive(void* obj) {
    return krystalGameplayActive(obj) && g_staffGameplayReady && staffPickupComplete() && !playerSequenceActive(obj);
}

static bool playerSequenceActive(void* obj) {
    const int16_t sequence = obj ? *reinterpret_cast<const int16_t*>(static_cast<const uint8_t*>(obj) + 0xFC) : -1;
    return sequence != -1 || (g_sequenceCameraActive && *g_sequenceCameraActive) ||
        (g_currentSequence && g_currentSequence() != 0);
}

static void log(FhLogLevel level, const char* message);

static bool krystalInjectionAllowed() {
    return isFoxCampaign();
}

static void* g_loadCharacterTarget{};
static void* g_objModelLoadTarget{};
static void* g_loadObjectFileTarget{};
static void* g_loadAnimationTarget{};
static void* g_modelReleaseTarget{};
static void* g_resetAnimationTarget{};
static void* g_modelListGetHeaderTarget{};
static void* g_getTableFileEntryTarget{};
static void* g_loadModelsBinTarget{};
static void* g_loadAndDecompressTarget{};
static void* g_tex1GetFrameTarget{};
static void* g_playerInitTarget{};
static void* g_setModelTarget{};
static void* g_textureLoadTarget{};
static void* g_getCurrentDataFileTarget{};
static void* g_playerStaffInitTarget{};
static void* g_objIsCurModelNotZeroTarget{};
static void* g_objBuildWorldTransformMatrixTarget{};

static thread_local bool g_loadingKrystalModel{};
static thread_local bool g_loadingKrystalTexture{};
static std::vector<uint8_t> g_km0;
static std::vector<uint8_t> g_kt0;
static bool g_assetsReady{};
static uint16_t** g_texIdRemapGlobal{};
static bool g_texTablePatched{};
static bool g_loggedModel{};
static bool g_loggedTexture{};
static bool g_loggedAnim{};
static bool g_loggedCache{};
static bool g_loggedTextureRequests{};
static bool g_loggedSyntheticTexBank{};
static bool g_loggedFoxRestoreRedirect{};
static std::vector<int32_t> g_syntheticTex1Tab;

static constexpr int KRYSTAL_MODEL_ID = 0x4E8;
static constexpr int KRYSTAL_TEXTURE_ID = 0x724;
static constexpr int KRYSTAL_TEXTURE_COUNT = 7;
static constexpr int MLDF_MODELS_TAB_A = 0x2A;
static constexpr int MLDF_MODELS_BIN_A = 0x2B;
static constexpr int MLDF_TEX1_BIN_A = 0x20;
static constexpr int MLDF_TEX1_TAB_A = 0x21;
static int g_mappedTextureIds[KRYSTAL_TEXTURE_COUNT] = {-1,-1,-1,-1,-1,-1,-1};

static constexpr uint32_t kTextureData[KRYSTAL_TEXTURE_COUNT] = {
    0x87000000u, 0x810038F0u, 0x8100C8E0u, 0x01000000u,
    0x8100D230u, 0x8100D5C0u, 0x8100D710u,
};

// Foxhollow v1.0.5 native x86-64 layouts.
static constexpr size_t OBJ_MODEL_BANKS_OFFSET = 0xB8;
static constexpr size_t MODEL_FILE_OFFSET = 0x00;
static constexpr size_t HDR_MODEL_ID_OFFSET = 0x04;
static constexpr size_t HDR_ANIM_IDS_OFFSET = 0xB0;
static constexpr size_t HDR_ANIM_IDXS_OFFSET = 0xB8;
static constexpr size_t HDR_ANIM_IDXS_SIZE = 0x14;
// Native x86-64 ObjAnimComponent: verified from Foxhollow layout (modelBanks=0xB8).
static constexpr size_t OBJ_BANK_INDEX_OFFSET = 0xF5;
static constexpr float KRYSTAL_HEIGHT_SCALE = 0.865f;

struct NativeSphereDef {
    int16_t joint;
    uint8_t padding[2];
    float radius;
    float center[3];
    uint16_t links;
    int8_t sphereIndex;
    int8_t maskBit;
};
struct NativeSphere { float radius; float center[3]; };
struct NativeModel {
    NativeModelHeader* file;
    uint8_t padding[8];
    void* matrices[2];
    void* workspace;
    uint16_t flags;
    uint8_t padding2[2];
    void* vertices[2];
    void* normals;
    void* blendChannels;
    void* animA;
    void* animB;
    void* textures;
    void* renderCallback;
    void* postRenderCallback;
    void* vertexAnim;
    void* blendAnim;
    NativeSphere* buffers[2];
    NativeSphere* active;
};
static_assert(sizeof(NativeSphereDef) == 0x18);
static_assert(sizeof(NativeSphere) == 0x10);
static_assert(offsetof(NativeModel, flags) == 0x28);
static_assert(offsetof(NativeModel, buffers) == 0x88);
static_assert(offsetof(NativeModel, active) == 0x98);

using UpdateHitSpheresFn = void (*)(uint8_t*, uint8_t*, uint8_t*, uint8_t*, uint8_t*);
static UpdateHitSpheresFn g_updateHitSpheres{};
static void* g_updateHitSpheresTarget{};
static void log(FhLogLevel level, const char* message);

static bool getAttackDefinition(void* obj, NativeModel* expectedModel, size_t sphere, NativeSphereDef& out) {
    if (!krystalStaffActive(obj)) return false;
    if (!obj || !g_objGetPlayerObject || obj != g_objGetPlayerObject()) return false;
    const auto* bytes = static_cast<const uint8_t*>(obj);
    const int16_t move = *reinterpret_cast<const int16_t*>(bytes + 0xE0);
    const bool selected = (sphere == 16 && (move == 0x460 || move == 0x464)) ||
                          (sphere == 12 && (move == 0x464 || move == 0x468));
    if (*reinterpret_cast<const int8_t*>(bytes + OBJ_BANK_INDEX_OFFSET) != 0 || !selected) return false;
    auto banks = *reinterpret_cast<NativeModel* const* const*>(bytes + OBJ_MODEL_BANKS_OFFSET);
    if (!banks || !banks[0] || !banks[1] || banks[0] != expectedModel) return false;
    const auto* kh = banks[0]->file;
    const auto* fh = banks[1]->file;
    if (!kh || !fh || kh->modelId != KRYSTAL_MODEL_ID || fh->modelId != 1 ||
        !kh->hitVolumes || !fh->hitVolumes || kh->hitVolumeCount <= sphere ||
        kh->hitVolumeCount > 64 || fh->hitVolumeCount <= sphere) return false;
    const auto* kd = reinterpret_cast<const NativeSphereDef*>(kh->hitVolumes);
    const auto* fd = reinterpret_cast<const NativeSphereDef*>(fh->hitVolumes);
    const auto& donor = fd[sphere];
    if (donor.joint < 0 || donor.joint >= kh->jointCount + kh->extraJointCount ||
        donor.sphereIndex != sphere || donor.maskBit != sphere ||
        donor.links != kd[sphere].links ||
        kd[sphere].sphereIndex != donor.sphereIndex || kd[sphere].maskBit != donor.maskBit) return false;
    out = donor;
    return true;
}

static void updateHitSpheresHook(uint8_t* modelBytes, uint8_t* headerBytes, uint8_t* previousObj,
                                 uint8_t* boneMatrix, uint8_t* obj) {
    auto* model = reinterpret_cast<NativeModel*>(modelBytes);
    NativeSphereDef donor12{}, donor16{};
    const bool eligible = obj == previousObj && model && headerBytes == reinterpret_cast<uint8_t*>(model->file);
    const bool use12 = eligible && getAttackDefinition(obj, model, 12, donor12);
    const bool use16 = eligible && getAttackDefinition(obj, model, 16, donor16);
    if (use12 || use16) {
        // Pass a private header and definitions for this evaluation only. The
        // model's shared header, sphere count, and allocated buffers stay intact.
        NativeModelHeader header = *model->file;
        NativeSphereDef defs[64];
        std::memcpy(defs, header.hitVolumes, header.hitVolumeCount * sizeof(NativeSphereDef));
        if (use12) defs[12] = donor12;
        if (use16) defs[16] = donor16;
        header.hitVolumes = reinterpret_cast<uint8_t*>(defs);
        g_updateHitSpheres(modelBytes, reinterpret_cast<uint8_t*>(&header), previousObj, boneMatrix, obj);
        return;
    }
    g_updateHitSpheres(modelBytes, headerBytes, previousObj, boneMatrix, obj);
}

static void log(FhLogLevel level, const char* message) {
    if (g_host && g_host->log) g_host->log(g_mod, level, message);
}

static uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

static bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const auto n = f.tellg();
    if (n <= 0) return false;
    out.resize(static_cast<size_t>(n));
    f.seekg(0, std::ios::beg);
    return !!f.read(reinterpret_cast<char*>(out.data()), n);
}

static bool loadInjectedAssets() {
    if (g_assetsReady) return true;
    if (!g_host || !g_host->modDir) return false;
    const char* dir = g_host->modDir(g_mod);
    if (!dir) return false;
#if defined(_WIN32)
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    std::string base(dir);
    if (!base.empty() && base.back() != '/' && base.back() != '\\') base += sep;
    const std::string km = base + "assets" + sep + "km0";
    const std::string kt = base + "assets" + sep + "kt0";
    if (!readFile(km, g_km0) || !readFile(kt, g_kt0)) {
        log(FH_LOG_ERROR, "Play As Krystal 0.9.7: could not load assets/km0 and assets/kt0 from the installed mod.");
        return false;
    }
    if (g_km0.size() < 0x38 || be32(g_km0.data()) != 0xFACEFEEDu || g_kt0.size() < 0x100) {
        log(FH_LOG_ERROR, "Play As Krystal 0.9.7: assets failed format validation.");
        g_km0.clear(); g_kt0.clear();
        return false;
    }
    g_assetsReady = true;
    char buf[224];
    std::snprintf(buf, sizeof(buf), "Play As Krystal 0.9.7: loaded km0 (%zu bytes) and kt0 (%zu bytes) from the mod package.", g_km0.size(), g_kt0.size());
    log(FH_LOG_INFO, buf);
    return true;
}

static void initSyntheticKrystalTextureTable() {
    if (!g_syntheticTex1Tab.empty()) return;
    // Foxhollow's map-local TEX1 banks can be much smaller than Krystal's
    // retail IDs (0x724-0x72A). textureLoad() clamps an ID to slot 0 BEFORE it
    // reloads the bank table, which is why 0.8.7/0.9.2 rendered psychedelic
    // textures. Give only Krystal's six TEX1 requests a private table large
    // enough to preserve those literal IDs; never write past the map's table.
    g_syntheticTex1Tab.assign(0x72E, 0);
    for (int i = 0; i < KRYSTAL_TEXTURE_COUNT; ++i)
        g_syntheticTex1Tab[KRYSTAL_TEXTURE_ID + i] = static_cast<int32_t>(kTextureData[i]);
    g_syntheticTex1Tab[0x72D] = -1;
}

static void* getCurrentDataFileHook(int fileId) {
    if (g_loadingKrystalTexture && fileId == MLDF_TEX1_TAB_A) {
        initSyntheticKrystalTextureTable();
        return g_syntheticTex1Tab.data();
    }
    return g_getCurrentDataFile ? g_getCurrentDataFile(fileId) : nullptr;
}

static int getTableFileEntryHook(int fileId, int index, int* out) {
    if (g_loadingKrystalModel && fileId == MLDF_MODELS_TAB_A && index == KRYSTAL_MODEL_ID && out && loadInjectedAssets()) {
        // The real offset is irrelevant: loadModelsBinHook/loadAndDecompressHook source km0 directly.
        *out = 0;
        return 1;
    }
    return g_getTableFileEntry ? g_getTableFileEntry(fileId, index, out) : 0;
}

static void loadModelsBinHook(int offsetFlags, int* animCount, int* headerSize, int* amapFlag, int* dataLen, int wpad0) {
    if (g_loadingKrystalModel && loadInjectedAssets()) {
        if (amapFlag)  *amapFlag  = static_cast<int>(be32(g_km0.data() + 0x18));
        if (animCount) *animCount = static_cast<int>(be32(g_km0.data() + 0x1C));
        if (headerSize)*headerSize= static_cast<int>(be32(g_km0.data() + 0x20));
        if (dataLen)   *dataLen   = static_cast<int>(be32(g_km0.data() + 0x04));
        if (!g_loggedModel) {
            g_loggedModel = true;
            log(FH_LOG_INFO, "Play As Krystal 0.9.7: model 0x4E8 metadata is now sourced directly from km0.");
        }
        return;
    }
    if (g_loadModelsBin) g_loadModelsBin(offsetFlags, animCount, headerSize, amapFlag, dataLen, wpad0);
}

static bool isInjectedTextureEntry(int entryIndex) {
    return entryIndex >= KRYSTAL_TEXTURE_ID && entryIndex < KRYSTAL_TEXTURE_ID + KRYSTAL_TEXTURE_COUNT && entryIndex != 0x727;
}

static void tex1GetFrameHook(int texId, int unused, int* outA, int* outB, int count, int* frameTable, int queryMode) {
    uint32_t word = static_cast<uint32_t>(texId);
    bool ours = false;
    for (uint32_t v : kTextureData) if (word == v) { ours = true; break; }
    if (!g_loadingKrystalTexture || !ours || !loadInjectedAssets()) {
        if (g_tex1GetFrame) g_tex1GetFrame(texId, unused, outA, outB, count, frameTable, queryMode);
        return;
    }
    const size_t baseOff = size_t(word & 0x00FFFFFFu) * 2u;
    if (baseOff + 16 > g_kt0.size()) return;
    const uint8_t* base = g_kt0.data();
    if (queryMode == 1 && frameTable) {
        const size_t e = baseOff + size_t(frameTable[count]) + 4u;
        if (e + 12 > g_kt0.size()) return;
        if (outA) *outA = static_cast<int>(be32(base + e + 4));
        if (outB) *outB = static_cast<int>(be32(base + e + 8));
    } else if (queryMode == 2 && frameTable) {
        const size_t bytes = size_t(count + 1) * 4u;
        if (baseOff + bytes > g_kt0.size()) return;
        for (int i = 0; i <= count; ++i) frameTable[i] = static_cast<int>(be32(base + baseOff + size_t(i) * 4u));
    } else {
        const uint8_t* e = base + baseOff;
        if (outA) *outA = static_cast<int>(be32(e + 8));
        if (outB) *outB = (std::memcmp(e, "DIR", 3) == 0) ? -1 : static_cast<int>(be32(e + 12));
    }
    if (!g_loggedTexture) {
        g_loggedTexture = true;
        log(FH_LOG_INFO, "Play As Krystal 0.9.7: Krystal texture metadata is being read directly from the altered kt0.");
    }
}

static void* loadAndDecompressHook(int fileId, void* destBuf, int offsetFlags,
                                   uint32_t length, int* sizeOut, int entryIndex,
                                   uint32_t flagBits) {
    if (g_loadingKrystalModel && fileId == MLDF_MODELS_BIN_A && destBuf && loadInjectedAssets() && g_zlbDecompress) {
        const uint32_t aux = be32(g_km0.data() + 0x08);
        const uint32_t compressed = be32(g_km0.data() + 0x0C);
        uint32_t outLen = be32(g_km0.data() + 0x04);
        const size_t streamOff = size_t(aux) + 0x28u;
        if (streamOff + (compressed >= 0x10 ? compressed - 0x10u : 0u) <= g_km0.size()) {
            g_zlbDecompress(g_km0.data() + streamOff, static_cast<int>(compressed - 0x10u), reinterpret_cast<uint8_t*>(destBuf), &outLen);
            return destBuf;
        }
    }

    if (g_loadingKrystalTexture && fileId == MLDF_TEX1_BIN_A && isInjectedTextureEntry(entryIndex) && loadInjectedAssets() && g_zlbDecompress) {
        const size_t off = size_t(offsetFlags & 0x00FFFFFF);
        if (off + 16 <= g_kt0.size()) {
            const uint8_t* p = g_kt0.data() + off;
            if (std::memcmp(p, "DIR", 3) == 0) return const_cast<uint8_t*>(p + 0x20);
            if (std::memcmp(p, "ZLB", 3) == 0 && destBuf) {
                uint32_t outLen = be32(p + 8);
                const uint32_t compLen = be32(p + 12);
                if (off + 0x10u + compLen <= g_kt0.size()) {
                    g_zlbDecompress(const_cast<uint8_t*>(p + 0x10), static_cast<int>(compLen), reinterpret_cast<uint8_t*>(destBuf), &outLen);
                    return destBuf;
                }
            }
        }
    }

    return g_loadAndDecompress ? g_loadAndDecompress(fileId, destBuf, offsetFlags, length, sizeOut, entryIndex, flagBits) : nullptr;
}

static void* textureLoadHook(int texId, uint8_t flagIn) {
    const int n = texId < 0 ? -texId : texId;
    const int slot = n & 0x7fff;
    const bool krystalTex = texId < 0 && (n & 0x8000) &&
        slot >= KRYSTAL_TEXTURE_ID && slot < KRYSTAL_TEXTURE_ID + KRYSTAL_TEXTURE_COUNT && slot != 0x727;

    if (g_loadingKrystalModel && krystalTex) {
        static int seen[16]{};
        static int seenCount = 0;
        bool duplicate = false;
        for (int i = 0; i < seenCount; ++i) if (seen[i] == texId) duplicate = true;
        if (!duplicate && seenCount < 16) {
            seen[seenCount++] = texId;
            char buf[192];
            std::snprintf(buf, sizeof(buf),
                "Play As Krystal 0.9.7: Krystal TEX1 request raw=%d slot=0x%X; using private TEX1 bank.",
                texId, slot);
            log(FH_LOG_INFO, buf);
        }

        initSyntheticKrystalTextureTable();
        const bool prev = g_loadingKrystalTexture;
        g_loadingKrystalTexture = true;
        int oldCount = 0;
        if (g_texBankCount) {
            oldCount = g_texBankCount[1];
            // This value is consulted before loadTextureBank(). Keep the real
            // 0x724-0x72A ID from being clamped to slot zero.
            g_texBankCount[1] = 0x72D;
        }
        void* result = g_textureLoad ? g_textureLoad(texId, flagIn) : nullptr;
        if (g_texBankCount) g_texBankCount[1] = oldCount;
        g_loadingKrystalTexture = prev;
        if (!g_loggedSyntheticTexBank) {
            g_loggedSyntheticTexBank = true;
            log(FH_LOG_INFO, "Play As Krystal 0.9.7: private TEX1 bank active; map TEX1 table left untouched.");
        }
        return result;
    }
    return g_textureLoad ? g_textureLoad(texId, flagIn) : nullptr;
}

static int modelListGetHeaderHook(void* list, int index, void* outHeader) {
    if (g_loadingKrystalModel && index == KRYSTAL_MODEL_ID) {
        if (!g_loggedCache) {
            g_loggedCache = true;
            log(FH_LOG_INFO, "Play As Krystal 0.9.7: bypassing cached 0x4E8 so the patched km0 model is constructed.");
        }
        return 0;
    }
    const int result = g_modelListGetHeader ? g_modelListGetHeader(list, index, outHeader) : 0;
    if (result && index == KRYSTAL_MODEL_ID && outHeader) {
        const void* header = *reinterpret_cast<void**>(outHeader);
        for (void* injected : g_injectedKrystalHeaders) if (header == injected) return 0;
    }
    return result;
}

// Own the mapping: cutscene actors can outlive the player instance that donated
// it. Geometry, joint layout and animation buffers remain Krystal's.
static std::vector<int16_t> g_foxAnimationIds;
static int16_t g_foxAnimationGroups[8]{};
static int32_t g_foxAnimationOffset{};
static bool g_loggedCutsceneActor{}, g_loggedSequenceAnimation{};

static bool ensureFoxAnimationMapping() {
    if (!g_foxAnimationIds.empty()) return true;
    if (!g_objModelLoad || !g_modelRelease) return false;
    int size = 0;
    const bool previous = g_loadingKrystalModel;
    g_loadingKrystalModel = false;
    auto* fox = static_cast<NativeModelHeader*>(g_objModelLoad(-1, 0, &size));
    g_loadingKrystalModel = previous;
    if (!fox) return false;
    const bool valid = fox->modelId == 1 && fox->cachedAnimIds &&
        (fox->flags & 0x40) && fox->animationCount > 0 && fox->animationCount < 1020;
    if (valid) {
        g_foxAnimationIds.assign(fox->cachedAnimIds, fox->cachedAnimIds + fox->animationCount);
        std::memcpy(g_foxAnimationGroups, fox->animGroupBaseIndices, sizeof(g_foxAnimationGroups));
        g_foxAnimationOffset = fox->animationDataFileOffset;
    }
    // ObjModel_Load acquires a header reference. Release it through the native
    // model API using an empty instance, with no per-instance allocations.
    alignas(8) uint8_t reference[0x120]{};
    *reinterpret_cast<NativeModelHeader**>(reference) = fox;
    g_modelRelease(reference);
    return valid;
}

static void applyFoxAnimationMapping(NativeModelHeader* header) {
    if (!header || g_foxAnimationIds.empty() || !(header->flags & 0x40)) return;
    std::memcpy(header->animGroupBaseIndices, g_foxAnimationGroups, sizeof(g_foxAnimationGroups));
    header->animationDataFileOffset = g_foxAnimationOffset;
    header->cachedAnimIds = g_foxAnimationIds.data();
    // This header uses a cached ID table, not an allocated array of animation
    // pointers. The replacement table owns every entry in the Fox count.
    header->animationCount = static_cast<uint16_t>(g_foxAnimationIds.size());
}

static void* loadObjectFileHook(int id) {
    void* result = g_loadObjectFile ? g_loadObjectFile(id) : nullptr;
    if (!result || result == reinterpret_cast<void*>(-1) || (id != 0x485 && id != 0x487 && id != 0x488)) return result;
    auto* bytes = static_cast<uint8_t*>(result);
    auto* models = *reinterpret_cast<int32_t**>(bytes + 8);
    const int8_t count = *reinterpret_cast<int8_t*>(bytes + 0x91);
    if (count == 1 && models && (models[0] == 1 || models[0] == KRYSTAL_MODEL_ID)) {
        // AnimFox/AnimFoxLink keep their original scripts, events and category.
        // Restore cached definitions when the native prologue loads them.
        models[0] = krystalInjectionAllowed() ? KRYSTAL_MODEL_ID : 1;
        if (models[0] == KRYSTAL_MODEL_ID && !g_loggedCutsceneActor) {
            g_loggedCutsceneActor = true;
            log(FH_LOG_INFO, "Krystal: separate Fox cutscene actor redirected to Krystal; original script retained.");
        }
    }
    return result;
}

static void* loadAnimationHook(NativeModelHeader* header, int16_t id, int bank, uint8_t* output) {
    if (!g_loadAnimation) return nullptr;
    if (isFoxCampaign() && header) {
        for (void* injected : g_injectedKrystalHeaders) if (header == injected) {
            // Fox-only sequence resource packs reject other model IDs before
            // loading an animation. Only this call sees the Fox identity.
            // animLoadFromTable reads the header synchronously; no pointer escapes.
            NativeModelHeader animationHeader = *header;
            animationHeader.modelId = 1;
            void* result = g_loadAnimation(&animationHeader, id, bank, output);
            if (!g_loggedSequenceAnimation || !result) {
                g_loggedSequenceAnimation = true;
                log(result ? FH_LOG_INFO : FH_LOG_WARN, result ?
                    "Krystal: Fox-compatible animation request loaded for injected Krystal model." :
                    "Krystal: animation request unavailable in current resource pack.");
            }
            return result;
        }
    }
    return g_loadAnimation(header, id, bank, output);
}

static void modelReleaseHook(void* model) {
    if (!model || !g_modelRelease) return;
    auto* header = *reinterpret_cast<NativeModelHeader**>(model);
    if (header && header->refCount == 1) {
        for (auto it = g_injectedKrystalHeaders.begin(); it != g_injectedKrystalHeaders.end();) {
            if (*it == header) it = g_injectedKrystalHeaders.erase(it); else ++it;
        }
    }
    g_modelRelease(model);
}

static void resetAnimationHook(void* model, void* state) {
    if (!g_resetAnimation) return;
    auto* header = model ? *reinterpret_cast<NativeModelHeader**>(model) : nullptr;
    if (isFoxCampaign() && header) {
        for (void* injected : g_injectedKrystalHeaders) if (header == injected) {
            // modelAnimResetState has its own Fox-only initial-cache gate and
            // reads only the instance's file field. It retains neither pointer.
            NativeModelHeader animationHeader = *header;
            animationHeader.modelId = 1;
            NativeModel animationModel{};
            animationModel.file = &animationHeader;
            g_resetAnimation(&animationModel, state);
            return;
        }
    }
    g_resetAnimation(model, state);
}

static void* objModelLoadHook(int id, int loadFlags, int* outSize) {
    const bool isKrystal = (id == -KRYSTAL_MODEL_ID || id == KRYSTAL_MODEL_ID);
    if (isKrystal && krystalInjectionAllowed() && !ensureFoxAnimationMapping()) {
        log(FH_LOG_ERROR, "Krystal: Fox animation mapping unavailable; keeping vanilla model.");
        return g_objModelLoad ? g_objModelLoad(id, loadFlags, outSize) : nullptr;
    }
    const bool prev = g_loadingKrystalModel;
    if (isKrystal) g_loadingKrystalModel = krystalInjectionAllowed();
    void* result = g_objModelLoad ? g_objModelLoad(id, loadFlags, outSize) : nullptr;
    if (isKrystal && result) {
        for (auto it = g_injectedKrystalHeaders.begin(); it != g_injectedKrystalHeaders.end();) {
            if (*it == result) it = g_injectedKrystalHeaders.erase(it);
            else ++it;
        }
        if (g_loadingKrystalModel) {
            applyFoxAnimationMapping(static_cast<NativeModelHeader*>(result));
            g_injectedKrystalHeaders.push_back(result);
        }
    }
    g_loadingKrystalModel = prev;
    return result;
}

static bool playerHasInjectedKrystal(void* obj) {
    if (!obj) return false;
    auto** banks = *reinterpret_cast<NativeModel***>(static_cast<uint8_t*>(obj) + OBJ_MODEL_BANKS_OFFSET);
    if (!banks || !banks[0] || !banks[0]->file) return false;
    for (void* header : g_injectedKrystalHeaders) if (header == banks[0]->file) return true;
    return false;
}

static void setModelHook(void* obj, int bank) {
    if (bank == 1 && isFoxCampaign() && g_objGetPlayerObject &&
        obj == g_objGetPlayerObject() && playerHasInjectedKrystal(obj)) bank = 0;
    if (g_setModel) g_setModel(obj, bank);
}

static bool copyFoxAnimationMapToKrystal(void* obj) {
    if (!obj) return false;
    auto*** banksField = reinterpret_cast<void***>(reinterpret_cast<uint8_t*>(obj) + OBJ_MODEL_BANKS_OFFSET);
    void** banks = *banksField;
    if (!banks || !banks[0] || !banks[1]) {
        log(FH_LOG_ERROR, "Play As Krystal 0.9.7: player model banks 0/1 unavailable after initialization.");
        return false;
    }
    auto* krystalModel = reinterpret_cast<uint8_t*>(banks[0]);
    auto* foxModel = reinterpret_cast<uint8_t*>(banks[1]);
    auto* krystalHdr = *reinterpret_cast<uint8_t**>(krystalModel + MODEL_FILE_OFFSET);
    auto* foxHdr = *reinterpret_cast<uint8_t**>(foxModel + MODEL_FILE_OFFSET);
    if (!krystalHdr || !foxHdr) return false;
    const uint16_t kid = *reinterpret_cast<uint16_t*>(krystalHdr + HDR_MODEL_ID_OFFSET);
    if (kid != KRYSTAL_MODEL_ID) {
        log(FH_LOG_ERROR, "Play As Krystal 0.9.7: injected bank 0 was not model 0x4E8; animation transplant skipped.");
        return false;
    }
    applyFoxAnimationMapping(reinterpret_cast<NativeModelHeader*>(krystalHdr));
    if (!g_loggedAnim) {
        g_loggedAnim = true;
        log(FH_LOG_INFO, "Play As Krystal 0.9.7: copied Fox animation mapping to injected Krystal using native x64 offsets 0xB8/0xB0.");
    }
    return true;
}

// Foxhollow uses bankIndex==0 as a legacy "Krystal" capability gate in a
// handful of player systems. Keep Krystal visually in bank 0, but make those
// capability checks see the Fox-capable player.
static int objIsCurModelNotZeroHook(void* obj) {
    if (krystalStaffActive(obj) && g_objGetPlayerObject && obj == g_objGetPlayerObject()) return 1;
    return g_objIsCurModelNotZero ? g_objIsCurModelNotZero(obj) : 0;
}

// playerStaffInit contains its own direct bankIndex!=0 check and disables the
// staff every update for bank 0. Temporarily expose bank 1 only to that check;
// the rendered/active model remains Krystal before and after the call.
static void playerStaffInitHook(void* obj, void* state) {
    if (!g_playerStaffInit) return;
    if (krystalGameplayActive(obj)) g_staffGameplayReady = staffPickupComplete();
    // The pickup script controls the staff prop during the sequence. Prevent
    // the legacy bank-zero check from removing it again after acquisition.
    // Player capabilities remain locked by krystalStaffActive until completion.
    if (!krystalGameplayActive(obj) || !staffPickupComplete() ||
        (g_objGetPlayerObject && obj != g_objGetPlayerObject())) {
        g_playerStaffInit(obj, state);
        return;
    }
    auto* bank = reinterpret_cast<int8_t*>(reinterpret_cast<uint8_t*>(obj) + OBJ_BANK_INDEX_OFFSET);
    const int8_t saved = *bank;
    if (saved == 0) *bank = 1;
    g_playerStaffInit(obj, state);
    *bank = saved;
}

static void objBuildWorldTransformMatrixHook(void* obj, float* mtx, int flags) {
    if (!g_objBuildWorldTransformMatrix) return;
    g_objBuildWorldTransformMatrix(obj, mtx, flags);

    // Krystal is slightly taller than Fox, while all gameplay/interaction
    // coordinates still come from the Fox actor. Scale only the 3x3 basis of
    // the player's Krystal world matrix around the actor origin. Translation
    // remains untouched, so collision, floor position and gameplay coordinates
    // stay exactly where vanilla Fox expects them. Temporary model banks (e.g.
    // SharpClaw disguise) are deliberately not scaled.
    if (!krystalGameplayActive(obj) || !mtx || !g_objGetPlayerObject || obj != g_objGetPlayerObject()) return;
    const auto* bank = reinterpret_cast<const int8_t*>(reinterpret_cast<const uint8_t*>(obj) + OBJ_BANK_INDEX_OFFSET);
    if (*bank != 0) return;

    mtx[1] *= KRYSTAL_HEIGHT_SCALE;
    mtx[5] *= KRYSTAL_HEIGHT_SCALE;
    mtx[9] *= KRYSTAL_HEIGHT_SCALE;
}

static void playerInitHook(void* obj) {
    g_gameplayPlayer = nullptr;
    g_staffGameplayReady = false;
    if (g_playerInit) g_playerInit(obj);
    if (!krystalInjectionAllowed() || !playerHasInjectedKrystal(obj)) return;
    if (!copyFoxAnimationMapToKrystal(obj)) return;
    if (obj && g_setModel) {
        g_gameplayPlayer = obj;
        g_staffGameplayReady = staffPickupComplete();
        g_setModel(obj, 0);
        log(FH_LOG_INFO, "Krystal: Fox campaign actor initialized; Krystal visuals during gameplay and sequences.");
    }
}

static void* loadCharacterHook(int16_t* data, int flags, int arg2, int arg3, void* parent, int unused) {
    if (!g_loadCharacter) return nullptr;
    if (krystalInjectionAllowed() && data && *data == 0) {
        if (!loadInjectedAssets()) return g_loadCharacter(data, flags, arg2, arg3, parent, unused);
        initSyntheticKrystalTextureTable();
    }
    return g_loadCharacter(data, flags, arg2, arg3, parent, unused);
}

static bool installHook(const char* symbol, void* replacement, void** targetOut, void** originalOut) {
    void* target = g_host->symbolAddress(g_mod, symbol);
    if (!target) return false;
    void* original = nullptr;
    if (g_host->hookInstall(g_mod, target, replacement, &original) != FH_MOD_OK || !original) return false;
    *targetOut = target; *originalOut = original; return true;
}

#include "krystal_voice.h"
#include "rich_presence_compat.h"


extern "C" FH_MOD_EXPORT int fh_mod_initialize(FhMod* mod, const FhModHost* host) {
    if (!mod || !host || host->abiVersion != FH_MOD_ABI_VERSION || !host->symbolAddress || !host->hookInstall) return FH_MOD_ERROR;
    g_mod = mod; g_host = host;
    g_getCurrentDataFile = reinterpret_cast<GetCurrentDataFileFn>(host->symbolAddress(mod, "getCurrentDataFile"));
    g_texBankCount = reinterpret_cast<int*>(host->symbolAddress(mod, "gRcpTexBankCount"));
    g_zlbDecompress = reinterpret_cast<ZlbDecompressFn>(host->symbolAddress(mod, "zlbDecompress"));
    g_setModel = reinterpret_cast<SetModelFn>(host->symbolAddress(mod, "Obj_SetActiveModelIndex"));
    g_objGetPlayerObject = reinterpret_cast<ObjGetPlayerObjectFn>(host->symbolAddress(mod, "Obj_GetPlayerObject"));
    g_findSoundTrigger = reinterpret_cast<FindSoundTriggerFn>(host->symbolAddress(mod, "Sfx_FindTrigger"));
    g_playObjectSoundEx = reinterpret_cast<PlayObjectSoundExFn>(host->symbolAddress(mod, "Sfx_PlayFromObjectEx"));
    g_saveCharacter = reinterpret_cast<uint8_t (*)()>(host->symbolAddress(mod, "SaveGame_getCurChar"));
    g_storySaveData = reinterpret_cast<uint8_t**>(host->symbolAddress(mod, "gGameBitSaveData"));
    g_mainGetBit = reinterpret_cast<uint32_t (*)(int)>(host->symbolAddress(mod, "mainGetBit"));
    g_currentSequence = reinterpret_cast<int (*)()>(host->symbolAddress(mod, "getCurSeqNo"));
    g_sequenceCameraActive = reinterpret_cast<const uint8_t*>(host->symbolAddress(mod, "gObjSeqCameraActive"));
    if (!g_storySaveData || !g_saveCharacter || !g_mainGetBit || !g_currentSequence || !g_sequenceCameraActive) {
        log(FH_LOG_ERROR, "Krystal: story progression symbols unavailable; refusing to modify the prologue.");
        return FH_MOD_ERROR;
    }
    if (!g_getCurrentDataFile || !g_zlbDecompress || !g_setModel || !g_texBankCount || !g_objGetPlayerObject || !g_findSoundTrigger || !g_playObjectSoundEx) {
        log(FH_LOG_ERROR, "Play As Krystal 0.9.7: required Foxhollow symbols are unavailable."); return FH_MOD_ERROR;
    }
    if (!loadInjectedAssets()) return FH_MOD_ERROR;

    void* original = nullptr;
#define HOOK(sym, fn, target, orig, type) do { original=nullptr; if(!installHook(sym,reinterpret_cast<void*>(fn),&target,&original)){log(FH_LOG_ERROR,"Play As Krystal 0.9.7: failed to install required hook: " sym);return FH_MOD_ERROR;} orig=reinterpret_cast<type>(original); } while(0)
    HOOK("getCurrentDataFile", getCurrentDataFileHook, g_getCurrentDataFileTarget, g_getCurrentDataFile, GetCurrentDataFileFn);
    HOOK("getTableFileEntry", getTableFileEntryHook, g_getTableFileEntryTarget, g_getTableFileEntry, GetTableFileEntryFn);
    HOOK("loadModelsBin", loadModelsBinHook, g_loadModelsBinTarget, g_loadModelsBin, LoadModelsBinFn);
    HOOK("loadAndDecompressDataFile", loadAndDecompressHook, g_loadAndDecompressTarget, g_loadAndDecompress, LoadAndDecompressFn);
    HOOK("tex1GetFrame", tex1GetFrameHook, g_tex1GetFrameTarget, g_tex1GetFrame, Tex1GetFrameFn);
    HOOK("ModelList_getHeader", modelListGetHeaderHook, g_modelListGetHeaderTarget, g_modelListGetHeader, ModelListGetHeaderFn);
    HOOK("ObjModel_Release", modelReleaseHook, g_modelReleaseTarget, g_modelRelease, ModelReleaseFn);
    HOOK("loadAnimation", loadAnimationHook, g_loadAnimationTarget, g_loadAnimation, LoadAnimationFn);
    HOOK("modelAnimResetState", resetAnimationHook, g_resetAnimationTarget, g_resetAnimation, ResetAnimationFn);
    HOOK("loadObjectFile", loadObjectFileHook, g_loadObjectFileTarget, g_loadObjectFile, LoadObjectFileFn);
    HOOK("ObjModel_Load", objModelLoadHook, g_objModelLoadTarget, g_objModelLoad, ObjModelLoadFn);
    HOOK("loadCharacter", loadCharacterHook, g_loadCharacterTarget, g_loadCharacter, LoadCharacterFn);
    HOOK("textureLoad", textureLoadHook, g_textureLoadTarget, g_textureLoad, TextureLoadFn);
    HOOK("playerStaffInit", playerStaffInitHook, g_playerStaffInitTarget, g_playerStaffInit, PlayerStaffInitFn);
    HOOK("objIsCurModelNotZero", objIsCurModelNotZeroHook, g_objIsCurModelNotZeroTarget, g_objIsCurModelNotZero, ObjIsCurModelNotZeroFn);
    HOOK("objLoadPlayerFromSave", playerInitHook, g_playerInitTarget, g_playerInit, PlayerInitFn);
    HOOK("Obj_SetActiveModelIndex", setModelHook, g_setModelTarget, g_setModel, SetModelFn);
    HOOK("Obj_BuildWorldTransformMatrix", objBuildWorldTransformMatrixHook, g_objBuildWorldTransformMatrixTarget, g_objBuildWorldTransformMatrix, ObjBuildWorldTransformMatrixFn);
    HOOK("objUpdateHitSpheres", updateHitSpheresHook, g_updateHitSpheresTarget, g_updateHitSpheres, UpdateHitSpheresFn);
    HOOK("Sfx_PlayFromObject", playSimpleSoundHook, g_playSimpleSoundTarget, g_playSimpleSound, PlaySimpleSoundFn);
    HOOK("Sfx_PlayFromObjectChannel", playChannelSoundHook, g_playChannelSoundTarget, g_playChannelSound, PlayChannelSoundFn);
    HOOK("Sfx_PlayAtPositionFromObject", playPositionSoundHook, g_playPositionSoundTarget, g_playPositionSound, PlayPositionSoundFn);
    HOOK("Sfx_PlayFromObjectLimited", playLimitedSoundHook, g_playLimitedSoundTarget, g_playLimitedSound, PlayLimitedSoundFn);
    HOOK("Sfx_StopFromObject", stopObjectSoundHook, g_stopObjectSoundTarget, g_stopObjectSound, PlaySimpleSoundFn);
    HOOK("Sfx_IsPlayingFromObject", isPlayingSoundHook, g_isPlayingSoundTarget, g_isPlayingSound, IsPlayingSoundFn);
    HOOK("Sfx_SetObjectSfxVolume", setSoundVolumeHook, g_setSoundVolumeTarget, g_setSoundVolume, SetSoundVolumeFn);
    HOOK("Sfx_FindObjectChannel", findObjectSoundChannelHook, g_findObjectSoundChannelTarget, g_findObjectSoundChannel, FindObjectSoundChannelFn);
#undef HOOK
    original = nullptr;
    if (installHook("AudioStream_Play", reinterpret_cast<void*>(playAudioStreamHook), &g_playAudioStreamTarget, &original)) {
        g_playAudioStream = reinterpret_cast<PlayAudioStreamFn>(original);
        log(FH_LOG_INFO, "Arwing stream trace enabled.");
        const bool arwingReady = initializeArwingLandingVoice();
        log(arwingReady ? FH_LOG_INFO : FH_LOG_WARN,
            arwingReady ? "Krystal Arwing stream replacement enabled: per-recording landing cues, ledge-jump voice 0x398; background retained by audio overlays." :
                "Krystal Arwing stream replacement unavailable: audio clock/table symbols or edited audio overlay missing.");
    } else {
        log(FH_LOG_WARN, "Arwing stream trace unavailable: AudioStream_Play hook could not be installed.");
    }

    log(FH_LOG_INFO, "Play As Krystal  0.9.7 loaded: Fox gameplay actor + Krystal visuals + Fox capability gates (staff/PDA/map/full staff ability suite + 86.5% Krystal height-only scale).");
    char voiceSummary[192];
    std::snprintf(voiceSummary, sizeof(voiceSummary),
        "Krystal voice replacements loaded: %zu gameplay vocal mappings (paired clips and fallbacks); existing attack corrections active.",
        sizeof(kVoiceReplacements) / sizeof(kVoiceReplacements[0]));
    log(FH_LOG_INFO, voiceSummary);
    log(FH_LOG_INFO, "Krystal build: release-prep-v21; staff diagnostics removed; v18 cutscene/model/staff behavior retained; native object-sound entry points replace voices before Cutscene Skip playback; stop/check/volume IDs matched; Arwing cues retained.");
    return FH_MOD_OK;
}

extern "C" FH_MOD_EXPORT void fh_mod_update(FhMod*) {
    if (g_exitSoundTraceFrames) --g_exitSoundTraceFrames;
    static unsigned int presenceRetry = 0;
    if (presenceRetry++ % 120 == 0) notifyKrystalRichPresence(krystalGameplayActive(g_gameplayPlayer));
    updateArwingLandingVoice();
    // Vanilla temporary player-model states (notably the SharpClaw disguise)
    // restore the player to Fox by selecting model bank 1 when they finish.
    // Our gameplay actor is intentionally still Fox, but its normal visible
    // bank is Krystal (0). Leave every non-Fox temporary bank untouched and
    // only redirect an attempted return to the ordinary Fox bank.
    if (!g_objGetPlayerObject || !g_setModel) return;
    void* obj = g_objGetPlayerObject();
    if (!obj) { g_gameplayPlayer = nullptr; g_staffGameplayReady = false; return; }

    if (!isFoxCampaign()) { g_gameplayPlayer = nullptr; g_staffGameplayReady = false; return; }
    if (obj != g_gameplayPlayer) {
        if (!krystalInjectionAllowed() || !playerHasInjectedKrystal(obj)) return;
        if (!copyFoxAnimationMapToKrystal(obj)) return;
        g_gameplayPlayer = obj;
        g_staffGameplayReady = staffPickupComplete();
        notifyKrystalRichPresence(true);
    }

    g_staffGameplayReady = staffPickupComplete();

    auto* bank = reinterpret_cast<int8_t*>(reinterpret_cast<uint8_t*>(obj) + OBJ_BANK_INDEX_OFFSET);
    if (*bank == 1) {
        g_setModel(obj, 0);
        if (!g_loggedFoxRestoreRedirect) {
            g_loggedFoxRestoreRedirect = true;
            log(FH_LOG_INFO, "Play As Krystal 0.9.7: redirected vanilla Fox model restoration (bank 1) back to Krystal bank 0.");
        }
    }
}
extern "C" FH_MOD_EXPORT void fh_mod_shutdown(FhMod*) {
    notifyKrystalRichPresence(false);
    g_arwingAudioReady = false;
    if (g_host && g_host->hookRemove) {
        if (g_modelReleaseTarget) g_host->hookRemove(g_mod, g_modelReleaseTarget);
        if (g_resetAnimationTarget) g_host->hookRemove(g_mod, g_resetAnimationTarget);
        if (g_loadAnimationTarget) g_host->hookRemove(g_mod, g_loadAnimationTarget);
        if (g_loadObjectFileTarget) g_host->hookRemove(g_mod, g_loadObjectFileTarget);
        if (g_setModelTarget) g_host->hookRemove(g_mod, g_setModelTarget);
        if (g_playAudioStreamTarget) g_host->hookRemove(g_mod, g_playAudioStreamTarget);
        if (g_findObjectSoundChannelTarget) g_host->hookRemove(g_mod, g_findObjectSoundChannelTarget);
        if (g_playSimpleSoundTarget) g_host->hookRemove(g_mod, g_playSimpleSoundTarget);
        if (g_playChannelSoundTarget) g_host->hookRemove(g_mod, g_playChannelSoundTarget);
        if (g_playPositionSoundTarget) g_host->hookRemove(g_mod, g_playPositionSoundTarget);
        if (g_playLimitedSoundTarget) g_host->hookRemove(g_mod, g_playLimitedSoundTarget);
        if (g_stopObjectSoundTarget) g_host->hookRemove(g_mod, g_stopObjectSoundTarget);
        if (g_isPlayingSoundTarget) g_host->hookRemove(g_mod, g_isPlayingSoundTarget);
        if (g_setSoundVolumeTarget) g_host->hookRemove(g_mod, g_setSoundVolumeTarget);
        if (g_updateHitSpheresTarget) g_host->hookRemove(g_mod, g_updateHitSpheresTarget);
        if (g_objBuildWorldTransformMatrixTarget) g_host->hookRemove(g_mod, g_objBuildWorldTransformMatrixTarget);
        if (g_playerInitTarget) g_host->hookRemove(g_mod, g_playerInitTarget);
        if (g_objIsCurModelNotZeroTarget) g_host->hookRemove(g_mod, g_objIsCurModelNotZeroTarget);
        if (g_playerStaffInitTarget) g_host->hookRemove(g_mod, g_playerStaffInitTarget);
        if (g_textureLoadTarget) g_host->hookRemove(g_mod, g_textureLoadTarget);
        if (g_loadCharacterTarget) g_host->hookRemove(g_mod, g_loadCharacterTarget);
        if (g_objModelLoadTarget) g_host->hookRemove(g_mod, g_objModelLoadTarget);
        if (g_modelListGetHeaderTarget) g_host->hookRemove(g_mod, g_modelListGetHeaderTarget);
        if (g_tex1GetFrameTarget) g_host->hookRemove(g_mod, g_tex1GetFrameTarget);
        if (g_loadAndDecompressTarget) g_host->hookRemove(g_mod, g_loadAndDecompressTarget);
        if (g_loadModelsBinTarget) g_host->hookRemove(g_mod, g_loadModelsBinTarget);
        if (g_getTableFileEntryTarget) g_host->hookRemove(g_mod, g_getTableFileEntryTarget);
        if (g_getCurrentDataFileTarget) g_host->hookRemove(g_mod, g_getCurrentDataFileTarget);
    }
    g_host=nullptr; g_mod=nullptr;
}
