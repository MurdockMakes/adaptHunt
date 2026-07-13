#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "AdaptiveDebugHUD.generated.h"

struct FAdaptiveDebugTelemetrySnapshot;
enum class EAdaptiveRoundPhase : uint8;
enum class EAdaptiveRoundWinner : uint8;

/** Simple greybox overlay exposing the adaptive-learning lifecycle. */
UCLASS()
class ADAPTHUNT_API AAdaptiveDebugHUD : public AHUD
{
    GENERATED_BODY()

public:
    AAdaptiveDebugHUD();

    virtual void DrawHUD() override;

    /** Keeps detailed learning telemetry opt-in while core combat UI remains. */
    UFUNCTION(Exec)
    void DebugToggleAdaptiveHUD();

    bool IsDetailedAdaptiveDebugVisible() const;

    static float NormalizeResource(float CurrentValue, float MaximumValue);
    static FString FormatRoundStatus(
        int32 CurrentRound,
        int32 TotalRounds,
        EAdaptiveRoundPhase Phase,
        float IntermissionRemainingTime
    );
    static FString FormatPredictionStatus(
        int32 CurrentRound,
        bool bPredictionsEnabled
    );
    static FString FormatMatchScore(int32 PlayerWins, int32 EnemyWins);
    static FString FormatMatchWinner(EAdaptiveRoundWinner Winner);

private:
    FAdaptiveDebugTelemetrySnapshot CaptureTelemetry() const;
    void DrawCombatReadability(
        const FAdaptiveDebugTelemetrySnapshot& Snapshot
    );
    void DrawDetailedAdaptiveDebug(
        const FAdaptiveDebugTelemetrySnapshot& Snapshot
    );
    void DrawCenteredStatusLine(
        const FString& Text,
        float Y,
        const FLinearColor& Color,
        float Scale = 1.0f
    );
    void DrawResourceBar(
        const FString& Label,
        float CurrentValue,
        float MaximumValue,
        const FVector2D& Position,
        float Width,
        const FLinearColor& FillColor
    );

    UPROPERTY(EditDefaultsOnly, Category = "Adaptive Hunt|Debug HUD")
    FVector2D PanelPosition;

    UPROPERTY(EditDefaultsOnly, Category = "Adaptive Hunt|Debug HUD", meta = (ClampMin = "300.0"))
    float PanelWidth;

    UPROPERTY(EditDefaultsOnly, Category = "Adaptive Hunt|Debug HUD", meta = (ClampMin = "12.0"))
    float LineHeight;

    UPROPERTY(EditDefaultsOnly, Category = "Adaptive Hunt|Debug HUD", meta = (ClampMin = "0.0"))
    float PanelPadding;

    UPROPERTY(EditDefaultsOnly, Category = "Adaptive Hunt|Combat HUD")
    bool bShowDetailedAdaptiveDebug;

    UPROPERTY(EditDefaultsOnly, Category = "Adaptive Hunt|Combat HUD", meta = (ClampMin = "180.0"))
    float ResourceBarWidth;

    UPROPERTY(EditDefaultsOnly, Category = "Adaptive Hunt|Combat HUD", meta = (ClampMin = "8.0"))
    float ResourceBarHeight;
};
