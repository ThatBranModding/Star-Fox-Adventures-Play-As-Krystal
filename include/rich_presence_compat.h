#pragma once
#include <filesystem>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void notifyKrystalRichPresence(bool enabled) {
    if (!g_host || !g_host->modDir)
        return;
    const char* directory = g_host->modDir(g_mod);
    if (!directory)
        return;
#if defined(_WIN32)
    constexpr auto platform = "windows-amd64";
    constexpr auto library = "mod.dll";
#elif defined(__APPLE__)
#if defined(__aarch64__) || defined(__arm64__)
    constexpr auto platform = "macos-arm64";
#else
    constexpr auto platform = "macos-x86_64";
#endif
    constexpr auto library = "mod.so";
#else
#if defined(__aarch64__)
    constexpr auto platform = "linux-arm64";
#else
    constexpr auto platform = "linux-amd64";
#endif
    constexpr auto library = "mod.so";
#endif
    const auto path = std::filesystem::path(directory).parent_path() /
                      "com.thatbran.sfa-rich-presence" / "lib" / platform / library;
    using NotifyFn = void (*)(int);
#if defined(_WIN32)
    HMODULE companion = GetModuleHandleW(path.c_str());
    if (!companion)
        return;
    const auto notify =
        reinterpret_cast<NotifyFn>(GetProcAddress(companion, "sfa_rp_set_play_as_krystal_v1"));
#else
    void* companion = dlopen(path.c_str(), RTLD_NOW | RTLD_NOLOAD);
    if (!companion)
        return;
    const auto notify =
        reinterpret_cast<NotifyFn>(dlsym(companion, "sfa_rp_set_play_as_krystal_v1"));
#endif
    if (notify)
        notify(enabled ? 1 : 0);
#if !defined(_WIN32)
    dlclose(companion);
#endif
    if (!notify)
        return;
    static bool logged = false;
    if (enabled && !logged) {
        logged = true;
        log(FH_LOG_INFO,
            "Krystal rich presence: notified the loaded Discord Rich Presence mod to report Krystal.");
    }
}
