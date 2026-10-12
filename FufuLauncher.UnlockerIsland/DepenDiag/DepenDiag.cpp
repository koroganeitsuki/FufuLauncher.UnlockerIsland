/*
Copyright (c) FufuLauncher Dev Team. All rights reserved.
Licensed under the AGPL-3.0 License.
*/
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "DepenDiag.h"
#include "../Config/Config.h"
#include "../Core/SharedState.h"
#include "../Patterns/Patterns.h"
#include "../MinHook/MinHook.h"

#include <iostream>
#include <sstream>
#include <intrin.h>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace DepenDiag {
    namespace {
        bool g_Initialized = false;
        unsigned long long g_RdtscPerMs = 0;

        unsigned long long NowRdtsc() { return __rdtsc(); }

        void CalibrateRdtsc() {
            unsigned long long s = __rdtsc();
            Sleep(10);
            unsigned long long e = __rdtsc();
            g_RdtscPerMs = (e - s) / 10;
            if (g_RdtscPerMs == 0) g_RdtscPerMs = 1000000;
        }

        // ----------------------------------------------------------------------
        // SkillConfig instance registry
        //   tag 0 = Stick  copy A (get_maxDepenetrationVelocityReplacement)
        //   tag 1 = Stick  copy B
        //   tag 2 = Attr   copy A (get_otherMaxDepenetrationVelocityReplacement)
        //   tag 3 = Attr   copy B
        // ----------------------------------------------------------------------
        static const char* kTagName[4] = { "StickA", "StickB", "AttrA", "AttrB" };

        struct TrackedCfg {
            int tag = -1;
            float last_value = 0.0f;
            bool has_value = false;
        };
        static std::unordered_map<void*, TrackedCfg> g_TrackedCfgs;
        static std::mutex g_CfgMutex;

        void TrackCfg(void* cfg, int tag) {
            if (!cfg) return;
            std::lock_guard<std::mutex> lk(g_CfgMutex);
            auto it = g_TrackedCfgs.find(cfg);
            if (it == g_TrackedCfgs.end()) {
                TrackedCfg t;
                t.tag = tag;
                g_TrackedCfgs[cfg] = t;
                std::cout << "[Depen] +track " << kTagName[tag] << " cfg=" << cfg
                          << " (total " << g_TrackedCfgs.size() << ")" << std::endl;
            } else if (it->second.tag != tag) {
                // Same pointer re-tagged (shouldn't happen, but keep first tag)
            }
        }

        // ----------------------------------------------------------------------
        // Rigidbody get/set call dedupe (5s cooldown per rb+value)
        // ----------------------------------------------------------------------
        struct SeenKey {
            void* rb;
            float value;
            bool is_set;
            bool operator==(const SeenKey& o) const {
                return rb == o.rb && is_set == o.is_set
                    && (is_set ? (value == o.value) : true);
            }
        };
        struct SeenKeyHash {
            size_t operator()(const SeenKey& k) const {
                size_t h = std::hash<void*>{}(k.rb);
                h ^= std::hash<int>{}(k.is_set ? 1 : 0) << 1;
                if (k.is_set) h ^= std::hash<float>{}(k.value) << 2;
                return h;
            }
        };
        static std::unordered_map<SeenKey, unsigned long long, SeenKeyHash> g_LastSeen;
        static std::mutex g_LastSeenMutex;
        constexpr unsigned long long kCooldownMs = 5000ULL;

        bool ShouldLog(const SeenKey& key) {
            unsigned long long now = NowRdtsc();
            std::lock_guard<std::mutex> lk(g_LastSeenMutex);
            auto it = g_LastSeen.find(key);
            if (it == g_LastSeen.end()) { g_LastSeen[key] = now; return true; }
            if (g_RdtscPerMs == 0) return false;
            unsigned long long elapsed_ms = (now - it->second) / g_RdtscPerMs;
            if (elapsed_ms >= kCooldownMs) { it->second = now; return true; }
            return false;
        }

        // ----------------------------------------------------------------------
        // Poll scheduling (main thread)
        // ----------------------------------------------------------------------
        constexpr unsigned long long kPollIntervalMs = 3000ULL;
        constexpr int kHeartbeatEveryNPolls = 10;   // full dump every 30s
        static unsigned long long g_LastPollRdtsc = 0;
        static int g_PollCount = 0;

        // ----------------------------------------------------------------------
        // Hooks: Rigidbody get/set maxDepenetrationVelocity
        // ----------------------------------------------------------------------
        float __fastcall hk_RBGetMaxDepenVel(void* rb) {
            float result = 0.0f;
            __try {
                if (o_RBGetMaxDepenVel) result = o_RBGetMaxDepenVel(rb);
            } __except (EXCEPTION_EXECUTE_HANDLER) { result = 0.0f; }

            if (rb && g_RdtscPerMs > 0) {
                SeenKey k; k.rb = rb; k.value = result; k.is_set = false;
                if (ShouldLog(k)) {
                    std::cout << "[Depen] GET maxDepenVel rb=" << rb
                              << " val=" << result << std::endl;
                }
            }
            return result;
        }

        void __fastcall hk_RBSetMaxDepenVel(void* rb, float value) {
            if (rb && g_RdtscPerMs > 0) {
                SeenKey k; k.rb = rb; k.value = value; k.is_set = true;
                if (ShouldLog(k)) {
                    std::cout << "[Depen] SET maxDepenVel rb=" << rb
                              << " new_val=" << value << std::endl;
                }
            }
            __try {
                if (o_RBSetMaxDepenVel) o_RBSetMaxDepenVel(rb, value);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        // ----------------------------------------------------------------------
        // Hooks: SkillConfigStick / SkillConfigAttraction .ctor (instance collection)
        // ----------------------------------------------------------------------
        void __fastcall hk_StickCtorA(void* thisPtr) {
            TrackCfg(thisPtr, 0);
            __try { if (o_SkillStickCtorA) o_SkillStickCtorA(thisPtr); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        void __fastcall hk_StickCtorB(void* thisPtr) {
            TrackCfg(thisPtr, 1);
            __try { if (o_SkillStickCtorB) o_SkillStickCtorB(thisPtr); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        void __fastcall hk_AttrCtorA(void* thisPtr) {
            TrackCfg(thisPtr, 2);
            __try { if (o_SkillAttrCtorA) o_SkillAttrCtorA(thisPtr); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        void __fastcall hk_AttrCtorB(void* thisPtr) {
            TrackCfg(thisPtr, 3);
            __try { if (o_SkillAttrCtorB) o_SkillAttrCtorB(thisPtr); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        // ----------------------------------------------------------------------
        // Hooks: replacement getters (passive view + late registration fallback)
        // ----------------------------------------------------------------------
        float __fastcall hk_StickGetDepenA(void* thisPtr) {
            TrackCfg(thisPtr, 0);
            float v = 0.0f;
            __try { if (o_SkillStickGetDepenA) v = o_SkillStickGetDepenA(thisPtr); }
            __except (EXCEPTION_EXECUTE_HANDLER) { v = 0.0f; }
            return v;
        }
        float __fastcall hk_StickGetDepenB(void* thisPtr) {
            TrackCfg(thisPtr, 1);
            float v = 0.0f;
            __try { if (o_SkillStickGetDepenB) v = o_SkillStickGetDepenB(thisPtr); }
            __except (EXCEPTION_EXECUTE_HANDLER) { v = 0.0f; }
            return v;
        }
        float __fastcall hk_AttrGetOtherDepenA(void* thisPtr) {
            TrackCfg(thisPtr, 2);
            float v = 0.0f;
            __try { if (o_SkillAttrGetOtherDepenA) v = o_SkillAttrGetOtherDepenA(thisPtr); }
            __except (EXCEPTION_EXECUTE_HANDLER) { v = 0.0f; }
            return v;
        }
        float __fastcall hk_AttrGetOtherDepenB(void* thisPtr) {
            TrackCfg(thisPtr, 3);
            float v = 0.0f;
            __try { if (o_SkillAttrGetOtherDepenB) v = o_SkillAttrGetOtherDepenB(thisPtr); }
            __except (EXCEPTION_EXECUTE_HANDLER) { v = 0.0f; }
            return v;
        }

        uintptr_t ParseHexOffset(const std::string& s) {
            if (s.empty()) return 0;
            uintptr_t v = 0;
            std::stringstream ss;
            ss << std::hex << s;
            ss >> v;
            return v;
        }

        bool CreateHookAt(const std::string& name, const std::string& offsetStr,
                          void* detour, void** orig) {
            uintptr_t off = ParseHexOffset(offsetStr);
            if (off == 0) {
                std::cout << "[WARN] [DepenDiag] offset empty for " << name
                          << ", skip." << std::endl;
                return false;
            }
            void* addr = (void*)((uintptr_t)GetModuleHandle(NULL) + off);
            if (MH_CreateHook(addr, detour, orig) != MH_OK) {
                std::cout << "[ERR] [DepenDiag] MH_CreateHook " << name << " failed." << std::endl;
                return false;
            }
            if (MH_EnableHook(addr) != MH_OK) {
                std::cout << "[ERR] [DepenDiag] MH_EnableHook " << name << " failed." << std::endl;
                return false;
            }
            std::cout << "[SCAN] [DepenDiag] " << name << " @ 0x"
                      << std::hex << (uintptr_t)addr << std::dec << std::endl;
            return true;
        }
    }

    // ----------------------------------------------------------------------
    // Main-thread poller — called from hk_GameUpdate every frame.
    // Internally throttled to every kPollIntervalMs.
    // ----------------------------------------------------------------------
    void Poll() {
        if (!g_Initialized) return;

        unsigned long long now = NowRdtsc();
        if (g_LastPollRdtsc != 0 && g_RdtscPerMs > 0) {
            unsigned long long elapsed_ms = (now - g_LastPollRdtsc) / g_RdtscPerMs;
            if (elapsed_ms < kPollIntervalMs) return;
        }
        g_LastPollRdtsc = now;
        g_PollCount++;
        bool heartbeat = (g_PollCount % kHeartbeatEveryNPolls) == 0;

        // Snapshot the map under lock, then read values WITHOUT holding the
        // lock (getter calls must never run under our mutex).
        std::vector<std::pair<void*, TrackedCfg>> snapshot;
        {
            std::lock_guard<std::mutex> lk(g_CfgMutex);
            snapshot.assign(g_TrackedCfgs.begin(), g_TrackedCfgs.end());
        }

        bool any_change = false;
        for (auto& kv : snapshot) {
            void* cfg = kv.first;
            int tag = kv.second.tag;

            float v = 0.0f;
            bool ok = false;
            __try {
                switch (tag) {
                case 0: if (o_SkillStickGetDepenA)      { v = o_SkillStickGetDepenA(cfg);      ok = true; } break;
                case 1: if (o_SkillStickGetDepenB)      { v = o_SkillStickGetDepenB(cfg);      ok = true; } break;
                case 2: if (o_SkillAttrGetOtherDepenA)  { v = o_SkillAttrGetOtherDepenA(cfg);  ok = true; } break;
                case 3: if (o_SkillAttrGetOtherDepenB)  { v = o_SkillAttrGetOtherDepenB(cfg);  ok = true; } break;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; }
            if (!ok) continue;

            bool changed = !kv.second.has_value || (v != kv.second.last_value);
            if (changed) {
                any_change = true;
                std::cout << "[Depen] " << (kv.second.has_value ? "CHANGED " : "READ    ")
                          << kTagName[tag] << " cfg=" << cfg
                          << " replacement=" << v << std::endl;
                std::lock_guard<std::mutex> lk(g_CfgMutex);
                auto it = g_TrackedCfgs.find(cfg);
                if (it != g_TrackedCfgs.end()) {
                    it->second.last_value = v;
                    it->second.has_value = true;
                }
            } else if (heartbeat) {
                std::cout << "[Depen]   ok " << kTagName[tag] << " cfg=" << cfg
                          << " replacement=" << v << std::endl;
            }
        }

        if (heartbeat) {
            std::cout << "[Depen] POLL heartbeat: " << snapshot.size()
                      << " configs tracked" << std::endl;
        }
    }

    // ----------------------------------------------------------------------
    bool Init() {
        HMODULE hMod = GetModuleHandle(NULL);
        if (!hMod) {
            std::cout << "[WARN] [DepenDiag] GetModuleHandle(NULL) failed." << std::endl;
            return false;
        }

        int okCount = 0;
        // Rigidbody get/set
        okCount += CreateHookAt("RB.get_maxDepenVel",  Offsets::RBGetMaxDepenVelOffset,
                                (void*)hk_RBGetMaxDepenVel,  (void**)&o_RBGetMaxDepenVel) ? 1 : 0;
        okCount += CreateHookAt("RB.set_maxDepenVel",  Offsets::RBSetMaxDepenVelOffset,
                                (void*)hk_RBSetMaxDepenVel,  (void**)&o_RBSetMaxDepenVel) ? 1 : 0;
        // SkillConfig ctors
        okCount += CreateHookAt("StickCtorA", Offsets::StickCtorAOffset,
                                (void*)hk_StickCtorA, (void**)&o_SkillStickCtorA) ? 1 : 0;
        okCount += CreateHookAt("StickCtorB", Offsets::StickCtorBOffset,
                                (void*)hk_StickCtorB, (void**)&o_SkillStickCtorB) ? 1 : 0;
        okCount += CreateHookAt("AttrCtorA",  Offsets::AttrCtorAOffset,
                                (void*)hk_AttrCtorA,  (void**)&o_SkillAttrCtorA) ? 1 : 0;
        okCount += CreateHookAt("AttrCtorB",  Offsets::AttrCtorBOffset,
                                (void*)hk_AttrCtorB,  (void**)&o_SkillAttrCtorB) ? 1 : 0;
        // SkillConfig getters
        okCount += CreateHookAt("StickGetDepenA", Offsets::StickGetDepenAOffset,
                                (void*)hk_StickGetDepenA, (void**)&o_SkillStickGetDepenA) ? 1 : 0;
        okCount += CreateHookAt("StickGetDepenB", Offsets::StickGetDepenBOffset,
                                (void*)hk_StickGetDepenB, (void**)&o_SkillStickGetDepenB) ? 1 : 0;
        okCount += CreateHookAt("AttrGetOtherDepenA", Offsets::AttrGetOtherDepenAOffset,
                                (void*)hk_AttrGetOtherDepenA, (void**)&o_SkillAttrGetOtherDepenA) ? 1 : 0;
        okCount += CreateHookAt("AttrGetOtherDepenB", Offsets::AttrGetOtherDepenBOffset,
                                (void*)hk_AttrGetOtherDepenB, (void**)&o_SkillAttrGetOtherDepenB) ? 1 : 0;

        CalibrateRdtsc();
        g_LastPollRdtsc = 0;
        g_PollCount = 0;
        g_Initialized = true;

        std::cout << "[SCAN] [DepenDiag] Ready. " << okCount
                  << "/10 hooks installed. Active poll every "
                  << kPollIntervalMs << "ms (main thread)." << std::endl;
        return okCount > 0;
    }

    bool IsReady() { return g_Initialized; }
}
