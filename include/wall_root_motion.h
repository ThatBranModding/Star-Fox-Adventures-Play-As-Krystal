

using WallStateFn = int (*)(void*, void*);
using CacheWallRootsFn = void (*)(void*);
using AdvanceWallProgressFn = int (*)(void*, float, float, void*);
using WallPathPointFn = void (*)(void*, int, float*, float*, float*, int);
static WallPathPointFn g_nativeWallPathPoint{};
static void* g_wallPathTarget{};
using SampleWallRootFn = void (*)(NativeModel*, int, int, float, float, float*, int16_t*);
static WallStateFn g_nativeWallState{};
static CacheWallRootsFn g_nativeCacheWallRoots{};
static SampleWallRootFn g_nativeSampleWallRoot{};
static WallStateFn g_nativeWallUp{}, g_nativeWallDown{};
static AdvanceWallProgressFn g_nativeWallAdvance{};
static void *g_wallUpTarget{}, *g_wallDownTarget{}, *g_wallAdvanceTarget{};
static void* g_wallTimingActor{};
static bool g_wallFinishing{};
static void *g_wallStateTarget{}, *g_wallCacheTarget{}, *g_wallSampleTarget{};
static void* g_wallRootActor{};
static bool g_cachingWallRoots{};

static bool eligibleWallActor(void* obj) {
    return krystalGameplayActive(obj) && g_objGetPlayerObject && obj == g_objGetPlayerObject() &&
           *reinterpret_cast<int8_t*>(static_cast<uint8_t*>(obj) + OBJ_BANK_INDEX_OFFSET) == 0 &&
           !playerSequenceActive(obj);
}

static int advanceWallProgressHook(void* obj, float rate, float delta, void* events) {
    if (obj == g_wallTimingActor && eligibleWallActor(obj) && delta > 0 && rate > 0) {
        const int move = *reinterpret_cast<int16_t*>(static_cast<uint8_t*>(obj) + 0xE0);
        if (g_wallFinishing && (move == 0x71 || move == 0x403)) {
            const float phase = *reinterpret_cast<float*>(static_cast<uint8_t*>(obj) + 0xD8);

            if (phase < 0.995f && phase + rate * delta >= 0.995f)
                rate = (0.995f - phase) / delta;
        }
    }
    return g_nativeWallAdvance(obj, rate, delta, events);
}

static void
wallPathPointHook(void* obj, int point, float* x, float* y, float* z, int inputPosition) {
    g_nativeWallPathPoint(obj, point, x, y, z, inputPosition);

    if (point != 11 || inputPosition || !y || obj != g_wallTimingActor || !eligibleWallActor(obj) ||
        !g_foxRootBindHeightValid || !playerHasInjectedKrystal(obj))
        return;
    auto** banks =
        *reinterpret_cast<NativeModel***>(static_cast<uint8_t*>(obj) + OBJ_MODEL_BANKS_OFFSET);
    if (!banks || !banks[0] || !banks[0]->file || !banks[0]->file->jointData)
        return;
    float root;
    std::memcpy(&root, banks[0]->file->jointData + 8, sizeof(float));
    const float baseY = *reinterpret_cast<float*>(static_cast<uint8_t*>(obj) + 0x1C);
    const float scale = *reinterpret_cast<float*>(static_cast<uint8_t*>(obj) + 0x08);
    if (std::isfinite(root) && std::isfinite(*y) && std::isfinite(baseY) && std::isfinite(scale)) {
        *y = baseY + (*y - baseY) / KRYSTAL_CUTSCENE_HEIGHT_SCALE +
             (g_foxRootBindHeight - root) * scale;
    }
}

static int finishWallMove(void* obj, void* state, WallStateFn original) {
    const bool eligible = eligibleWallActor(obj);
    const int result = original(obj, state);
    g_wallTimingActor = eligible && result >= 0 ? obj : nullptr;
    g_wallFinishing = true;
    return result;
}

static int finishWallUpHook(void* obj, void* state) {
    return finishWallMove(obj, state, g_nativeWallUp);
}

static int finishWallDownHook(void* obj, void* state) {
    return finishWallMove(obj, state, g_nativeWallDown);
}

static void sampleWallRootHook(NativeModel* model,
                               int channel,
                               int index,
                               float phase,
                               float scale,
                               float* position,
                               int16_t* rotation) {
    g_nativeSampleWallRoot(model, channel, index, phase, scale, position, rotation);
    if (!g_wallRootActor || !model || !model->file || !position || !g_foxRootBindHeightValid ||
        !isFoxCampaign() || !playerHasInjectedKrystal(g_wallRootActor))
        return;
    auto** banks = *reinterpret_cast<NativeModel***>(static_cast<uint8_t*>(g_wallRootActor) +
                                                     OBJ_MODEL_BANKS_OFFSET);
    if (!banks || model != banks[0] || !model->file->jointData)
        return;
    const int16_t move =
        *reinterpret_cast<const int16_t*>(static_cast<uint8_t*>(g_wallRootActor) + 0xE0);
    const bool endpoint = !g_cachingWallRoots && channel == 1 && index == 0 && phase == 1.0f;
    const bool threshold = g_cachingWallRoots && channel == 0 && index == 0 && phase == 0 &&
                           (move == 0x71 || move == 0x72 || move == 0x403 || move == 0x404);
    if (!endpoint && !threshold)
        return;
    float rootHeight;
    std::memcpy(&rootHeight, model->file->jointData + 8, sizeof(float));
    if (std::isfinite(rootHeight) && std::isfinite(scale)) {
        position[1] += (g_foxRootBindHeight - rootHeight) * scale;
    }
}

static int wallStateRootHook(void* obj, void* state) {
    void* saved = g_wallRootActor;
    const bool savedCache = g_cachingWallRoots;
    const bool eligible = eligibleWallActor(obj);
    g_wallRootActor = eligible ? obj : nullptr;
    g_cachingWallRoots = false;
    const int result = g_nativeWallState(obj, state);
    g_wallTimingActor = eligible && result == 0 ? obj : nullptr;
    g_wallFinishing = false;
    g_wallRootActor = saved;
    g_cachingWallRoots = savedCache;
    return result;
}

static void cacheWallRootsHook(void* obj) {
    void* saved = g_wallRootActor;
    const bool savedCache = g_cachingWallRoots;

    g_wallRootActor =
        obj && isFoxCampaign() && playerHasInjectedKrystal(obj) &&
                *reinterpret_cast<int8_t*>(static_cast<uint8_t*>(obj) + OBJ_BANK_INDEX_OFFSET) == 0
            ? obj
            : nullptr;
    g_cachingWallRoots = true;
    g_nativeCacheWallRoots(obj);
    g_wallRootActor = saved;
    g_cachingWallRoots = savedCache;
}

static void shutdownWallRootMotion() {
    if (g_host && g_host->hookRemove) {
        for (void* target : {g_wallPathTarget,
                             g_wallAdvanceTarget,
                             g_wallDownTarget,
                             g_wallUpTarget,
                             g_wallCacheTarget,
                             g_wallStateTarget,
                             g_wallSampleTarget})
            if (target)
                g_host->hookRemove(g_mod, target);
    }
    g_wallCacheTarget = g_wallStateTarget = g_wallSampleTarget = g_wallRootActor = nullptr;
    g_nativeWallState = nullptr;
    g_nativeCacheWallRoots = nullptr;
    g_nativeSampleWallRoot = nullptr;
    g_cachingWallRoots = false;
    g_wallUpTarget = g_wallDownTarget = g_wallAdvanceTarget = g_wallTimingActor = nullptr;
    g_nativeWallUp = g_nativeWallDown = nullptr;
    g_nativeWallAdvance = nullptr;
    g_wallFinishing = false;
    g_wallPathTarget = nullptr;
    g_nativeWallPathPoint = nullptr;
}

static void initializeWallRootMotion() {
    void* original = nullptr;
    bool ready = installHook("ObjModel_SampleJointTransform",
                             reinterpret_cast<void*>(sampleWallRootHook),
                             &g_wallSampleTarget,
                             &original);
    if (ready)
        g_nativeSampleWallRoot = reinterpret_cast<SampleWallRootFn>(original);
    if (ready) {
        ready = installHook("playerStateClimbWall",
                            reinterpret_cast<void*>(wallStateRootHook),
                            &g_wallStateTarget,
                            &original);
        if (ready)
            g_nativeWallState = reinterpret_cast<WallStateFn>(original);
    }
    if (ready) {
        ready = installHook("playerCacheMoveRootHeights",
                            reinterpret_cast<void*>(cacheWallRootsHook),
                            &g_wallCacheTarget,
                            &original);
        if (ready)
            g_nativeCacheWallRoots = reinterpret_cast<CacheWallRootsFn>(original);
    }
    if (ready) {
        ready = installHook("playerStateClimbUpFromWall",
                            reinterpret_cast<void*>(finishWallUpHook),
                            &g_wallUpTarget,
                            &original);
        if (ready)
            g_nativeWallUp = reinterpret_cast<WallStateFn>(original);
    }
    if (ready) {
        ready = installHook("playerStateClimbDownFromWall",
                            reinterpret_cast<void*>(finishWallDownHook),
                            &g_wallDownTarget,
                            &original);
        if (ready)
            g_nativeWallDown = reinterpret_cast<WallStateFn>(original);
    }
    if (ready) {
        ready = installHook("ObjAnim_AdvanceCurrentMove",
                            reinterpret_cast<void*>(advanceWallProgressHook),
                            &g_wallAdvanceTarget,
                            &original);
        if (ready)
            g_nativeWallAdvance = reinterpret_cast<AdvanceWallProgressFn>(original);
    }
    if (ready) {
        ready = installHook("ObjPath_GetPointWorldPosition",
                            reinterpret_cast<void*>(wallPathPointHook),
                            &g_wallPathTarget,
                            &original);
        if (ready)
            g_nativeWallPathPoint = reinterpret_cast<WallPathPointFn>(original);
    }
    if (!ready) {
        shutdownWallRootMotion();
        log(FH_LOG_WARN,
            "Krystal wall root-height correction unavailable: required hook missing or owned; native climbing retained.");
    } else
        log(FH_LOG_INFO, "Krystal climbing correction enabled.");
}
