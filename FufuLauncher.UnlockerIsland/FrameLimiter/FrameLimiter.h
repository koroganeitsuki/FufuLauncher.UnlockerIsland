/*
Copyright (c) FufuLauncher Dev Team. All rights reserved.
Licensed under the AGPL-3.0 License.
*/
#pragma once
#include <Windows.h>

namespace FrameLimiter
{
    void Init();

    // 每帧入口调用：按档位规则阻塞直到目标帧间隔
    // 帧耗时 < interval → Sleep(1) + 自旋等到 interval
    // 帧耗时 >= interval → 直接放行（档位自然后移）
    void WaitForTarget();
}
