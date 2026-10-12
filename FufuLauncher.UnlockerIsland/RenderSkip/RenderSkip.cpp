
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "RenderSkip.h"
#include "../Config/Config.h"
#include "../Core/SharedState.h"
#include "../Patterns/Patterns.h"
#include "../MinHook/MinHook.h"
#include "../Camera/Camera.h"

#include <iostream>
#include <sstream>
#include <intrin.h>

namespace RenderSkip {
    namespace {
        bool g_Initialized = false;

        // rdtsc-based timing — immune to QPC hooks
        static unsigned long long g_RdtscPerMs = 0;
        static unsigned long long g_LastRenderRdtsc = 0;
        static unsigned long long g_TargetIntervalRdtsc = 0;

        // set_skipRendering function pointer (resolved from offset)
        typedef void (__fastcall* tSetSkipRendering)(void* camera, bool skip);
        static tSetSkipRendering g_fnSetSkipRendering = nullptr;

        // Per-frame state: first camera in a frame decides, rest reuse the decision
        static bool g_CurrentFrameSkip = false;
        static unsigned long long g_CurrentFrameRdtsc = 0;
        static int g_CamerasThisFrame = 0;

        // Debug logging
        static int g_FrameCount = 0;
        static int g_RenderCount = 0;
        static unsigned long long g_LastDebugTime = 0;
        static bool g_LoggedFirstCall = false;

        unsigned long long NowRdtsc() {
            return __rdtsc();
        }

        void CalibrateRdtsc() {
            unsigned long long rdtsc_start = __rdtsc();
            Sleep(10);
            unsigned long long rdtsc_end = __rdtsc();
            g_RdtscPerMs = (rdtsc_end - rdtsc_start) / 10;
            if (g_RdtscPerMs == 0) g_RdtscPerMs = 1000000;
        }

        void SafeSetSkip(void* camera, bool skip) {
            if (!g_fnSetSkipRendering || !camera) return;
            __try {
                g_fnSetSkipRendering(camera, skip);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }

    // Hook for Camera.FireOnPreRender(Camera cam)
    // Called by Unity for EACH camera right before it renders.
    // __fastcall: rcx = cam (the Camera about to render)
    void __fastcall hk_FireOnPreRender(void* cam) {
        if (!g_LoggedFirstCall) {
            g_LoggedFirstCall = true;
            std::cout << "[RenderSkip] FireOnPreRender hook triggered! cam=" << cam << std::endl;
        }

        if (g_Initialized && Config::Get().enable_render_skip && cam) {
            // Detect new frame: if rdtsc changed significantly, this is a new frame
            unsigned long long now = NowRdtsc();
            if (g_CamerasThisFrame == 0 || (now - g_CurrentFrameRdtsc) > g_RdtscPerMs) {
                // New frame — make the render/skip decision
                g_CurrentFrameRdtsc = now;
                g_CamerasThisFrame = 0;

                unsigned long long elapsed = now - g_LastRenderRdtsc;
                if (elapsed >= g_TargetIntervalRdtsc) {
                    g_CurrentFrameSkip = false;  // Render this frame
                    g_LastRenderRdtsc = now;
                    g_RenderCount++;
                } else {
                    g_CurrentFrameSkip = true;  // Skip this frame
                }
                g_FrameCount++;
            }
            g_CamerasThisFrame++;

            // GetMainCamera() returns the main scene camera (heavy 3D rendering).
            // On skip frames, only skip the main scene camera — keep UI/clear cameras rendering.
            void* mainCam = Camera::GetMainCamera();
            if (g_CurrentFrameSkip && cam == mainCam) {
                // Skip frame: skip only the main scene camera
                SafeSetSkip(cam, true);
            } else {
                // Render frame, or this is not the main scene camera — always render
                SafeSetSkip(cam, false);
            }
        }

        // Always call original
        if (o_FireOnPreRender) o_FireOnPreRender(cam);

        // Debug log every ~1 second
        unsigned long long now = NowRdtsc();
        if (now - g_LastDebugTime >= g_RdtscPerMs * 1000) {
            std::cout << "[RenderSkip] frames=" << g_FrameCount
                      << " renders=" << g_RenderCount
                      << " (target " << Config::Get().target_render_fps << " fps)" << std::endl;
            g_FrameCount = 0;
            g_RenderCount = 0;
            g_LastDebugTime = now;
        }
    }

    bool Init() {
        auto& cfg = Config::Get();
        if (!cfg.enable_render_skip) {
            std::cout << "[SCAN] [RenderSkip] Disabled by config." << std::endl;
            return true;
        }

        // Resolve FireOnPreRender offset for hooking
        void* preRenderAddr = nullptr;
        // Resolve set_skipRendering offset
        void* skipRenderingAddr = nullptr;
        {
            HMODULE hMod = GetModuleHandle(NULL);
            if (hMod) {
                uintptr_t base = (uintptr_t)hMod;

                uintptr_t preOffset = 0;
                std::stringstream ssPre;
                ssPre << std::hex << Offsets::FireOnPreRenderOffset;
                ssPre >> preOffset;
                if (preOffset > 0) {
                    preRenderAddr = (void*)(base + preOffset);
                }

                uintptr_t skipOffset = 0;
                std::stringstream ssSkip;
                ssSkip << std::hex << Offsets::CameraSetSkipRenderingOffset;
                ssSkip >> skipOffset;
                if (skipOffset > 0) {
                    skipRenderingAddr = (void*)(base + skipOffset);
                }
            }
        }

        if (!preRenderAddr) {
            std::cout << "[WARN] [RenderSkip] FireOnPreRender offset not found. Feature disabled." << std::endl;
            return false;
        }

        if (MH_CreateHook(preRenderAddr, (void*)hk_FireOnPreRender, (void**)&o_FireOnPreRender) != MH_OK) {
            std::cout << "[ERR] [RenderSkip] MH_CreateHook FireOnPreRender failed." << std::endl;
            return false;
        }
        if (MH_EnableHook(preRenderAddr) != MH_OK) {
            std::cout << "[ERR] [RenderSkip] MH_EnableHook FireOnPreRender failed." << std::endl;
            return false;
        }

        if (skipRenderingAddr) {
            g_fnSetSkipRendering = (tSetSkipRendering)skipRenderingAddr;
            std::cout << "[SCAN] [RenderSkip] set_skipRendering at: " << skipRenderingAddr << std::endl;
        } else {
            std::cout << "[WARN] [RenderSkip] set_skipRendering offset not found." << std::endl;
        }

        CalibrateRdtsc();
        if (cfg.target_render_fps > 0) {
            g_TargetIntervalRdtsc = (g_RdtscPerMs * 1000) / cfg.target_render_fps;
        }
        g_LastRenderRdtsc = NowRdtsc();
        g_LastDebugTime = g_LastRenderRdtsc;

        g_Initialized = true;
        std::cout << "[SCAN] [RenderSkip] Hooked FireOnPreRender at: " << preRenderAddr << std::endl;
        std::cout << "   -> Target render FPS: " << cfg.target_render_fps
                  << " (interval: " << g_TargetIntervalRdtsc << " rdtsc ticks)" << std::endl;
        return true;
    }

    void Tick() {
        // No longer needed — timing decision is made in hk_FireOnPreRender
    }

    bool ShouldSkip() {
        return false;
    }

    bool IsReady() {
        return g_Initialized;
    }

    void ResetState() {
        g_LastRenderRdtsc = NowRdtsc();
    }
}
