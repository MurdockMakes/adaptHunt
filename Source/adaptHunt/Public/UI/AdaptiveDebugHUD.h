#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "AdaptiveDebugHUD.generated.h"

struct FAdaptiveDebugTelemetrySnapshot;

/** Simple greybox overlay exposing the adaptive-learning lifecycle. */
UCLASS()
class ADAPTHUNT_API AAdaptiveDebugHUD : public AHUD
{
    GENERATED_BODY()

public:
    AAdaptiveDebugHUD();

    virtual void DrawHUD() override;

private:
    FAdaptiveDebugTelemetrySnapshot CaptureTelemetry() const;

    UPROPERTY(EditDefaultsOnly, Category = "Adaptive Hunt|Debug HUD")
    FVector2D PanelPosition;

    UPROPERTY(EditDefaultsOnly, Category = "Adaptive Hunt|Debug HUD", meta = (ClampMin = "300.0"))
    float PanelWidth;

    UPROPERTY(EditDefaultsOnly, Category = "Adaptive Hunt|Debug HUD", meta = (ClampMin = "12.0"))
    float LineHeight;

    UPROPERTY(EditDefaultsOnly, Category = "Adaptive Hunt|Debug HUD", meta = (ClampMin = "0.0"))
    float PanelPadding;
};
