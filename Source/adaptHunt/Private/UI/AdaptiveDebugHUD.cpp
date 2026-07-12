#include "UI/AdaptiveDebugHUD.h"

#include "Characters/AdaptiveEnemyCharacter.h"
#include "Characters/AdaptivePlayerCharacter.h"
#include "Components/CombatComponent.h"
#include "Components/EnemyDecisionComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PlayerBehaviorTrackerComponent.h"
#include "Components/StaminaComponent.h"
#include "Data/LearningTelemetry.h"
#include "Engine/Engine.h"
#include "Game/AdaptiveGameMode.h"
#include "Game/RoundManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AAdaptiveDebugHUD::AAdaptiveDebugHUD()
    : PanelPosition(24.0f, 24.0f)
    , PanelWidth(760.0f)
    , LineHeight(18.0f)
    , PanelPadding(12.0f)
{
    PrimaryActorTick.bCanEverTick = false;
}

void AAdaptiveDebugHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !GEngine)
    {
        return;
    }

    const TArray<FString> Lines =
        FAdaptiveDebugTelemetryFormatter::FormatHudLines(
            CaptureTelemetry()
        );
    const float PanelHeight = PanelPadding * 2.0f
        + LineHeight * static_cast<float>(Lines.Num());
    DrawRect(
        FLinearColor(0.035f, 0.035f, 0.04f, 0.82f),
        PanelPosition.X,
        PanelPosition.Y,
        PanelWidth,
        PanelHeight
    );

    float Y = PanelPosition.Y + PanelPadding;
    for (int32 Index = 0; Index < Lines.Num(); ++Index)
    {
        const bool bSectionHeading = Index == 0
            || Lines[Index].StartsWith(TEXT("ROUND "));
        DrawText(
            Lines[Index],
            bSectionHeading
                ? FLinearColor(0.35f, 0.85f, 1.0f)
                : FLinearColor::White,
            PanelPosition.X + PanelPadding,
            Y,
            GEngine->GetSmallFont(),
            1.0f,
            false
        );
        Y += LineHeight;
    }
}

FAdaptiveDebugTelemetrySnapshot
AAdaptiveDebugHUD::CaptureTelemetry() const
{
    FAdaptiveDebugTelemetrySnapshot Snapshot;
    const AAdaptiveGameMode* GameMode =
        Cast<AAdaptiveGameMode>(UGameplayStatics::GetGameMode(this));
    const URoundManager* RoundManager = GameMode
        ? GameMode->GetRoundManager()
        : nullptr;
    if (RoundManager)
    {
        Snapshot.CurrentRound = RoundManager->GetCurrentRound();
        Snapshot.TotalRounds = RoundManager->GetTotalRounds();
        Snapshot.bPredictionsEnabled =
            RoundManager->ArePredictionsEnabledForCurrentRound();
        Snapshot.CollectedSampleCount =
            RoundManager->GetCollectedSampleCount();
        Snapshot.LastCompletedRound =
            RoundManager->GetLastCompletedRound();
        Snapshot.LastRoundObservedPattern =
            RoundManager->GetLastRoundObservedPattern();
    }

    const APlayerController* PlayerController =
        GetOwningPlayerController();
    const AAdaptivePlayerCharacter* Player = PlayerController
        ? Cast<AAdaptivePlayerCharacter>(PlayerController->GetPawn())
        : nullptr;
    const AAdaptiveEnemyCharacter* Enemy = GameMode
        ? GameMode->GetSpawnedEnemy()
        : nullptr;

    if (Player)
    {
        if (const UHealthComponent* Health = Player->GetHealthComponent())
        {
            Snapshot.PlayerHealth = Health->GetCurrentHealth();
            Snapshot.PlayerMaxHealth = Health->GetMaxHealth();
        }
        if (const UStaminaComponent* Stamina =
            Player->GetStaminaComponent())
        {
            Snapshot.PlayerStamina = Stamina->GetCurrentStamina();
            Snapshot.PlayerMaxStamina = Stamina->GetMaxStamina();
        }
        if (const UCombatComponent* Combat = Player->GetCombatComponent())
        {
            Snapshot.LastPlayerAction = Combat->GetLastAction();
        }
        if (const UPlayerBehaviorTrackerComponent* Tracker =
            Player->GetBehaviorTrackerComponent())
        {
            const FAdaptiveLearningSummary LearningSummary =
                FAdaptiveLearningTelemetryAnalyzer::Analyze(
                    Tracker->GetDataset()
                );
            Snapshot.CollectedSampleCount = LearningSummary.SampleCount;
            Snapshot.MostCommonPlayerAction =
                LearningSummary.MostCommonPlayerAction;
            Snapshot.StrongestLearnedPattern =
                LearningSummary.StrongestConditionalPattern;
        }
    }

    if (Enemy)
    {
        if (const UHealthComponent* Health = Enemy->GetHealthComponent())
        {
            Snapshot.EnemyHealth = Health->GetCurrentHealth();
            Snapshot.EnemyMaxHealth = Health->GetMaxHealth();
        }
        if (const UEnemyDecisionComponent* Decision =
            Enemy->GetEnemyDecisionComponent())
        {
            Snapshot.Prediction = Decision->GetLastPrediction();
            Snapshot.CurrentEnemySelectedAction =
                Decision->GetLastSelectedAction();
        }
    }

    if (!Snapshot.bPredictionsEnabled)
    {
        Snapshot.Prediction = FPredictionResult();
    }
    return Snapshot;
}
