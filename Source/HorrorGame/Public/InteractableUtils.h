// Source/HorrorGame/Public/InteractableUtils.h
#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"

struct FInteractableUtils
{
    // Rotate a widget component to face a camera location (keeps only yaw)
    static void FaceWidgetTowardsCamera(UWidgetComponent* Widget, const FVector& CameraLocation)
    {
        if (!Widget) return;
        const FVector WidgetLoc = Widget->GetComponentLocation();
        FVector ToCamera = CameraLocation - WidgetLoc;
        if (ToCamera.IsNearlyZero()) return;
        FRotator LookAt = ToCamera.Rotation();
        LookAt.Pitch = 0.f; // keep widget upright (only yaw)
        Widget->SetWorldRotation(LookAt);
    }
};