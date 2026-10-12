/*
Copyright (c) FufuLauncher Dev Team. All rights reserved.
Licensed under the AGPL-3.0 License.
*/
#pragma once

namespace DepenDiag {
    // 1) Hook Rigidbody.get/set_maxDepenetrationVelocity and log calls (rate-limited)
    // 2) Hook SkillConfigStick / SkillConfigAttraction .ctor to collect config instances
    // 3) Poll() — call from the main-thread per-frame hook (hk_GameUpdate);
    //    every 3s it ACTIVELY reads maxDepenetrationVelocityReplacement /
    //    otherMaxDepenetrationVelocityReplacement from every tracked instance
    //    and prints values (change-detected + periodic heartbeat).
    bool Init();
    void Poll();
    bool IsReady();
}
