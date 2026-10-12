/*
Copyright (c) FufuLauncher Dev Team. All rights reserved.
Licensed under the AGPL-3.0 License.
*/
#include "Patterns.h"
#include <iostream>

std::string GetOwnDllDir();

namespace Offsets {
    std::string GetActiveOffset;
    std::string GetComponent;
    std::string GetText;
    std::string ClockPageOkOffset;
    std::string ClockPageCloseOffset;
    std::string ClockPageFinishOffset;
    std::string ClockPageBackOffset;
    std::string TouchInputOffset;
    std::string InnerDispatcherOffset;
    std::string EventCameraOffset;
    std::string DamageColorAOffset;
    std::string DamageColorBOffset;
    std::string DamageColor1Offset;
    std::string DamageColor2Offset;
    std::string DamageColor3Offset;
    std::string DamageColor4Offset;
    std::string UpdateInnerTargetOffset;
    std::string SetActiveOffset;
    std::string CameraRenderOffset;
    std::string CameraSetSkipRenderingOffset;
    std::string FireOnPreRenderOffset;
    std::string TimeGetDeltaTimeOffset;
    std::string RBGetMaxDepenVelOffset;
    std::string RBSetMaxDepenVelOffset;
    std::string StickCtorAOffset;
    std::string StickGetDepenAOffset;
    std::string AttrCtorAOffset;
    std::string AttrGetOtherDepenAOffset;
    std::string StickCtorBOffset;
    std::string StickGetDepenBOffset;
    std::string AttrCtorBOffset;
    std::string AttrGetOtherDepenBOffset;

    void InitOffsets(bool isOS) {
        if (isOS) {
            GetActiveOffset = Patterns::OS::GetActiveOffset;
            GetComponent = Patterns::OS::GetComponent;
            GetText = Patterns::OS::GetText;
            ClockPageOkOffset = Patterns::OS::ClockPageOkOffset;
            ClockPageCloseOffset = Patterns::OS::ClockPageCloseOffset;
            ClockPageFinishOffset = Patterns::OS::ClockPageFinishOffset;
            ClockPageBackOffset = Patterns::OS::ClockPageBackOffset;
            TouchInputOffset = Patterns::OS::TouchInputOffset;
            InnerDispatcherOffset = Patterns::OS::InnerDispatcherOffset;
            EventCameraOffset = Patterns::OS::EventCameraOffset;
            DamageColorAOffset = Patterns::OS::DamageColorA;
            DamageColorBOffset = Patterns::OS::DamageColorB;
            DamageColor1Offset = Patterns::OS::DamageColor1;
            DamageColor2Offset = Patterns::OS::DamageColor2;
            DamageColor3Offset = Patterns::OS::DamageColor3;
            DamageColor4Offset = Patterns::OS::DamageColor4;
            UpdateInnerTargetOffset = Patterns::OS::UpdateInnerTargetOffset;
            SetActiveOffset = Patterns::OS::SetActiveOffset;
            CameraRenderOffset = Patterns::OS::CameraRenderOffset;
            CameraSetSkipRenderingOffset = Patterns::OS::CameraSetSkipRenderingOffset;
            FireOnPreRenderOffset = Patterns::OS::FireOnPreRenderOffset;
            TimeGetDeltaTimeOffset = Patterns::OS::TimeGetDeltaTimeOffset;
            RBGetMaxDepenVelOffset = Patterns::OS::RBGetMaxDepenVelOffset;
            RBSetMaxDepenVelOffset = Patterns::OS::RBSetMaxDepenVelOffset;
            StickCtorAOffset = Patterns::OS::StickCtorAOffset;
            StickGetDepenAOffset = Patterns::OS::StickGetDepenAOffset;
            AttrCtorAOffset = Patterns::OS::AttrCtorAOffset;
            AttrGetOtherDepenAOffset = Patterns::OS::AttrGetOtherDepenAOffset;
            StickCtorBOffset = Patterns::OS::StickCtorBOffset;
            StickGetDepenBOffset = Patterns::OS::StickGetDepenBOffset;
            AttrCtorBOffset = Patterns::OS::AttrCtorBOffset;
            AttrGetOtherDepenBOffset = Patterns::OS::AttrGetOtherDepenBOffset;
            std::cout << "[INFO] Pre-initialized Global (OS) Offsets from hardcode" << std::endl;
        } else {
            GetActiveOffset = Patterns::CN::GetActiveOffset;
            GetComponent = Patterns::CN::GetComponent;
            GetText = Patterns::CN::GetText;
            ClockPageOkOffset = Patterns::CN::ClockPageOkOffset;
            ClockPageCloseOffset = Patterns::CN::ClockPageCloseOffset;
            ClockPageFinishOffset = Patterns::CN::ClockPageFinishOffset;
            ClockPageBackOffset = Patterns::CN::ClockPageBackOffset;
            TouchInputOffset = Patterns::CN::TouchInputOffset;
            InnerDispatcherOffset = Patterns::CN::InnerDispatcherOffset;
            EventCameraOffset = Patterns::CN::EventCameraOffset;
            DamageColorAOffset = Patterns::CN::DamageColorA;
            DamageColorBOffset = Patterns::CN::DamageColorB;
            DamageColor1Offset = Patterns::CN::DamageColor1;
            DamageColor2Offset = Patterns::CN::DamageColor2;
            DamageColor3Offset = Patterns::CN::DamageColor3;
            DamageColor4Offset = Patterns::CN::DamageColor4;
            UpdateInnerTargetOffset = Patterns::CN::UpdateInnerTargetOffset;
            SetActiveOffset = Patterns::CN::SetActiveOffset;
            CameraRenderOffset = Patterns::CN::CameraRenderOffset;
            CameraSetSkipRenderingOffset = Patterns::CN::CameraSetSkipRenderingOffset;
            FireOnPreRenderOffset = Patterns::CN::FireOnPreRenderOffset;
            TimeGetDeltaTimeOffset = Patterns::CN::TimeGetDeltaTimeOffset;
            RBGetMaxDepenVelOffset = Patterns::CN::RBGetMaxDepenVelOffset;
            RBSetMaxDepenVelOffset = Patterns::CN::RBSetMaxDepenVelOffset;
            StickCtorAOffset = Patterns::CN::StickCtorAOffset;
            StickGetDepenAOffset = Patterns::CN::StickGetDepenAOffset;
            AttrCtorAOffset = Patterns::CN::AttrCtorAOffset;
            AttrGetOtherDepenAOffset = Patterns::CN::AttrGetOtherDepenAOffset;
            StickCtorBOffset = Patterns::CN::StickCtorBOffset;
            StickGetDepenBOffset = Patterns::CN::StickGetDepenBOffset;
            AttrCtorBOffset = Patterns::CN::AttrCtorBOffset;
            AttrGetOtherDepenBOffset = Patterns::CN::AttrGetOtherDepenBOffset;
            std::cout << "[INFO] Pre-initialized China (CN) Offsets from hardcode" << std::endl;
        }
    }
}