/*
Copyright (c) FufuLauncher Dev Team. All rights reserved.
Licensed under the AGPL-3.0 License.
*/
#include "FrameLimiter.h"
#include "../Config/Config.h"
#include <iostream>

static LARGE_INTEGER s_QPF = { 0 };      // QPC 频率 (ticks/s)
static LONGLONG      s_LastEnter = 0;     // 上次进入 hk_GameUpdate 的 QPC ticks

namespace FrameLimiter
{
    void Init()
    {
        QueryPerformanceFrequency(&s_QPF);
        std::cout << "[Limiter] Ready. QPF=" << s_QPF.QuadPart
                  << " ticks/sec. Interval="
                  << Config::Get().target_frame_interval_ms << " ms." << '\n';
    }

    // 每帧入口调用：按档位规则阻塞直到目标帧间隔
    void WaitForTarget()
    {
        const auto& cfg = Config::Get();
        if (!cfg.enable_frame_limiter || s_QPF.QuadPart == 0) return;

        LARGE_INTEGER nowQpc{};
        QueryPerformanceCounter(&nowQpc);

        // 首帧不阻塞
        if (s_LastEnter == 0)
        {
            s_LastEnter = nowQpc.QuadPart;
            return;
        }

        // 把 ms 换成 ticks
        double intervalMs = cfg.target_frame_interval_ms;
        LONGLONG intervalTicks = (LONGLONG)(intervalMs * (double)s_QPF.QuadPart / 1000.0);
        LONGLONG oneMsTicks   = s_QPF.QuadPart / 1000;

        // 档位目标：从上次进入时间 + interval 开始
        // 如果已经超时，就每次往后挪 1ms，直到 target >= now
        // 即：2.49 → 3.49 → 4.49 → … 永远等到下一个档位边界
        LONGLONG target = s_LastEnter + intervalTicks;
        while (target < nowQpc.QuadPart)
            target += oneMsTicks;

        // 等到 target
        LONGLONG remaining = target - nowQpc.QuadPart;
        if (remaining > 0)
        {
            LONGLONG sleepMs = (LONGLONG)((double)remaining * 1000.0 / (double)s_QPF.QuadPart);
            if (sleepMs > 1) Sleep((DWORD)(sleepMs - 1));

            while (true)
            {
                LARGE_INTEGER spinQpc{};
                QueryPerformanceCounter(&spinQpc);
                if (spinQpc.QuadPart >= target) break;
            }
        }

        // 基准设为档位边界，保持档位连续性
        s_LastEnter = target;
    }
}
