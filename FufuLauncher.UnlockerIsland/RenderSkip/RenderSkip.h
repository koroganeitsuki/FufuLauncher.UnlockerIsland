
#pragma once

namespace RenderSkip {
    bool Init();
    void Tick();        // Called from hk_ChangeFov — marks a new logical frame
    bool ShouldSkip();  // Called from hk_CameraRender — returns true to skip rendering
    bool IsReady();
    void ResetState();
}
