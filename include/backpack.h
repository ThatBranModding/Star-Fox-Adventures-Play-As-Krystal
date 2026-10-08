#pragma once

using RenderObjectModelFn = void (*)(void*);

static bool g_showBackpack = true;
static void** g_backpackObject{};
static RenderObjectModelFn g_renderObjectModel{};
static void* g_renderObjectModelTarget{};

static void refreshBackpackSetting() {
    g_showBackpack = true;
    if (g_host &&
        g_host->structSize >= offsetof(FhModHost, configBool) + sizeof(g_host->configBool) &&
        g_host->configBool) {
        g_showBackpack = g_host->configBool(g_mod, "showBackpack", 1) != 0;
    }
}

static void renderObjectModelHook(void* obj) {
    if (!g_showBackpack && obj && g_backpackObject && obj == *g_backpackObject && isFoxCampaign() &&
        g_objGetPlayerObject) {
        void* player = g_objGetPlayerObject();
        if (player && playerHasInjectedKrystal(player) &&
            static_cast<uint8_t*>(player)[OBJ_BANK_INDEX_OFFSET] == 0) {
            auto* scale = reinterpret_cast<float*>(static_cast<uint8_t*>(obj) + 0x08);
            const float savedScale = *scale;
            *scale = 0.0f;
            if (g_renderObjectModel) {
                g_renderObjectModel(obj);
            }
            *scale = savedScale;
            return;
        }
    }
    if (g_renderObjectModel) {
        g_renderObjectModel(obj);
    }
}

extern "C" FH_MOD_EXPORT void fh_mod_config_changed(FhMod*) {
    refreshBackpackSetting();
}
