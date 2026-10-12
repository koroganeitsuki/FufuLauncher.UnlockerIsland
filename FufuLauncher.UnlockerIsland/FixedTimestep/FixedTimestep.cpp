/*
Copyright (c) FufuLauncher Dev Team. All rights reserved.
Licensed under the AGPL-3.0 License.
*/
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "FixedTimestep.h"
#include "../Config/Config.h"
#include "../Core/SharedState.h"
#include "../Patterns/Patterns.h"
#include "../MinHook/MinHook.h"

#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>

namespace FixedTimestep {
    namespace {
        bool g_Initialized = false;
        float g_FixedDeltaTime = 0.0f;
        
        // Target real time for the next frame start
        double g_NextFrameTargetTime = 0.0;
        bool g_FirstFrame = true;

        float __fastcall hk_TimeGetDeltaTime() {
            auto& cfg = Config::Get();
            if (cfg.enable_fixed_timestep && g_FixedDeltaTime > 0.0f) {
                // Get current real time
                auto now = std::chrono::high_resolution_clock::now();
                double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();
                
                if (g_FirstFrame) {
                    g_NextFrameTargetTime = currentTime;
                    g_FirstFrame = false;
                }
                
                // Check if we need to wait (CPU ran too fast)
                if (currentTime < g_NextFrameTargetTime) {
                    double waitTime = g_NextFrameTargetTime - currentTime;
                    
                    // Wait for the difference
                    if (waitTime > 0.002) { // > 2ms
                        std::this_thread::sleep_for(std::chrono::duration<double>(waitTime - 0.001));
                    }
                    
                    while (true) {
                        auto nowWait = std::chrono::high_resolution_clock::now();
                        double currentWait = std::chrono::duration<double>(nowWait.time_since_epoch()).count();
                        if (currentWait >= g_NextFrameTargetTime) break;
                        std::this_thread::yield();
                    }
                    
                    // Update current time after waiting
                    currentTime = g_NextFrameTargetTime;
                }
                // If currentTime >= target, it means we're behind (dropped frames)
                // We don't wait, let the game catch up
                
                // Calculate target time for the NEXT frame
                g_NextFrameTargetTime = currentTime + g_FixedDeltaTime;
                
                return g_FixedDeltaTime;
            }
            
            auto orig = o_TimeGetDeltaTime;
            if (orig) {
                return orig();
            }
            return 0.0166667f; // fallback ~60fps
        }
    }

    bool Init() {
        auto& cfg = Config::Get();
        if (!cfg.enable_fixed_timestep) {
            std::cout << "[SCAN] [FixedTimestep] Disabled by config." << std::endl;
            return true;
        }

        // Calculate fixed delta time
        int fps = cfg.fixed_timestep_fps;
        if (fps < 1) fps = 280;
        g_FixedDeltaTime = 1.0f / (float)fps;
        
        // Reset time tracking
        g_NextFrameTargetTime = 0.0;
        g_FirstFrame = true;

        // Resolve address
        void* addr = nullptr;
        {
            HMODULE hMod = GetModuleHandle(NULL);
            if (hMod) {
                uintptr_t base = (uintptr_t)hMod;
                uintptr_t offset = 0;
                std::stringstream ss;
                ss << std::hex << Offsets::TimeGetDeltaTimeOffset;
                ss >> offset;
                if (offset > 0) {
                    addr = (void*)(base + offset);
                }
            }
        }

        if (!addr) {
            std::cout << "[WARN] [FixedTimestep] Time.get_deltaTime offset not found. Feature disabled." << std::endl;
            std::cout << "       Please set TimeGetDeltaTimeOffset in Patterns.h for your game version." << std::endl;
            return false;
        }

        if (MH_CreateHook(addr, (void*)hk_TimeGetDeltaTime, (void**)&o_TimeGetDeltaTime) != MH_OK) {
            std::cout << "[ERR] [FixedTimestep] Failed to create hook on Time.get_deltaTime at " << addr << std::endl;
            return false;
        }

        if (MH_EnableHook(addr) != MH_OK) {
            std::cout << "[ERR] [FixedTimestep] Failed to enable hook on Time.get_deltaTime" << std::endl;
            return false;
        }

        g_Initialized = true;
        std::cout << "[SCAN] [FixedTimestep] Time.get_deltaTime at: " << addr << std::endl;
        std::cout << "   -> FixedTimestep ready. Target FPS: " << fps << ", DeltaTime: " << g_FixedDeltaTime << std::endl;
        std::cout << "   -> Adaptive sync: If ahead, wait. If behind, catch up." << std::endl;
        return true;
    }

    void Shutdown() {
        if (!g_Initialized) return;
        
        void* addr = nullptr;
        {
            HMODULE hMod = GetModuleHandle(NULL);
            if (hMod) {
                uintptr_t base = (uintptr_t)hMod;
                uintptr_t offset = 0;
                std::stringstream ss;
                ss << std::hex << Offsets::TimeGetDeltaTimeOffset;
                ss >> offset;
                if (offset > 0) {
                    addr = (void*)(base + offset);
                }
            }
        }

        if (addr) {
            MH_DisableHook(addr);
            MH_RemoveHook(addr);
        }
        o_TimeGetDeltaTime = nullptr;
        g_Initialized = false;
        std::cout << "[SCAN] [FixedTimestep] Hook removed." << std::endl;
    }
}
