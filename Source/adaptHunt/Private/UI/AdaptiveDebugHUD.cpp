#include "UI/AdaptiveDebugHUD.h"

#include "Characters/AdaptiveEnemyCharacter.h"
#include "Characters/AdaptivePlayerCharacter.h"
#include "Components/CombatComponent.h"
#include "Components/CombatFeedbackComponent.h"
#include "Components/EnemyDecisionComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PlayerBehaviorTrackerComponent.h"
#include "Components/StaminaComponent.h"
#include "Data/LearningTelemetry.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Game/AdaptiveGameMode.h"
#include "Game/AdaptationReveal.h"
#include "Game/RoundManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AAdaptiveDebugHUD::AAdaptiveDebugHUD()
    : PanelPosition(24.0f, 158.0f)
    , PanelWidth(680.0f)
    , LineHeight(18.0f)
    , PanelPadding(12.0f)
    , bShowDetailedAdaptiveDebug(false)
    , ResourceBarWidth(360.0f)
    , ResourceBarHeight(14.0f)
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

    const FAdaptiveDebugTelemetrySnapshot Snapshot = CaptureTelemetry();
    DrawCombatReadability(Snapshot);
    if (bShowDetailedAdaptiveDebug)
    {
        DrawDetailedAdaptiveDebug(Snapshot);
    }
}

void AAdaptiveDebugHUD::DebugToggleAdaptiveHUD()
{
#if UE_BUILD_SHIPPING
    bShowDetailedAdaptiveDebug = false;
#else
    bShowDetailedAdaptiveDebug = !bShowDetailedAdaptiveDebug;
#endif
}

bool AAdaptiveDebugHUD::IsDetailedAdaptiveDebugVisible() const
{
    return bShowDetailedAdaptiveDebug;
}

float AAdaptiveDebugHUD::NormalizeResource(
    const float CurrentValue,
    const float MaximumValue
)
{
    return FMath::IsFinite(CurrentValue)
        && FMath::IsFinite(MaximumValue)
        && MaximumValue > 0.0f
        ? FMath::Clamp(CurrentValue / MaximumValue, 0.0f, 1.0f)
        : 0.0f;
}

FString AAdaptiveDebugHUD::FormatRoundStatus(
    const int32 CurrentRound,
    const int32 TotalRounds,
    const EAdaptiveRoundPhase Phase,
    const float IntermissionRemainingTime
)
{
    const int32 SafeTotal = FMath::Max(1, TotalRounds);
    const int32 SafeRound = FMath::Clamp(CurrentRound, 0, SafeTotal);
    switch (Phase)
    {
    case EAdaptiveRoundPhase::PreRoundCountdown:
        return FString::Printf(
            TEXT("ROUND %d / %d  |  PREPARE  |  STARTS IN %.1f"),
            SafeRound,
            SafeTotal,
            FMath::IsFinite(IntermissionRemainingTime)
                ? FMath::Max(0.0f, IntermissionRemainingTime)
                : 0.0f
        );
    case EAdaptiveRoundPhase::InProgress:
        return FString::Printf(
            TEXT("ROUND %d / %d  |  IN PROGRESS"),
            SafeRound,
            SafeTotal
        );
    case EAdaptiveRoundPhase::Intermission:
        return FString::Printf(
            TEXT("ROUND %d / %d  |  INTERMISSION  |  NEXT ROUND IN %.1f"),
            SafeRound,
            SafeTotal,
            FMath::IsFinite(IntermissionRemainingTime)
                ? FMath::Max(0.0f, IntermissionRemainingTime)
                : 0.0f
        );
    case EAdaptiveRoundPhase::MatchComplete:
        return FString::Printf(
            TEXT("ROUND %d / %d  |  MATCH COMPLETE"),
            SafeRound,
            SafeTotal
        );
    default:
        return FString::Printf(
            TEXT("ROUND %d / %d  |  WAITING"),
            SafeRound,
            SafeTotal
        );
    }
}

FString AAdaptiveDebugHUD::FormatPredictionStatus(
    const int32 CurrentRound,
    const bool bPredictionsEnabled
)
{
    if (CurrentRound <= 1)
    {
        return bPredictionsEnabled
            ? TEXT("LIVE LEARNING  |  PREDICTIONS ENABLED")
            : TEXT("LIVE LEARNING  |  GATHERING EVIDENCE");
    }
    return bPredictionsEnabled
        ? TEXT("PREDICTIONS ENABLED")
        : TEXT("PREDICTIONS DISABLED  |  INSUFFICIENT EVIDENCE");
}

FString AAdaptiveDebugHUD::FormatMatchScore(
    const int32 PlayerWins,
    const int32 EnemyWins
)
{
    return FString::Printf(
        TEXT("PLAYER %d  -  %d ENEMY"),
        FMath::Max(0, PlayerWins),
        FMath::Max(0, EnemyWins)
    );
}

FString AAdaptiveDebugHUD::FormatMatchWinner(
    const EAdaptiveRoundWinner Winner
)
{
    switch (Winner)
    {
    case EAdaptiveRoundWinner::Player:
        return TEXT("PLAYER WINS THE MATCH");
    case EAdaptiveRoundWinner::Enemy:
        return TEXT("ENEMY WINS THE MATCH");
    default:
        return TEXT("MATCH DRAW");
    }
}

void AAdaptiveDebugHUD::DrawCombatReadability(
    const FAdaptiveDebugTelemetrySnapshot& Snapshot
)
{
    const AAdaptiveGameMode* GameMode =
        Cast<AAdaptiveGameMode>(UGameplayStatics::GetGameMode(this));
    const URoundManager* RoundManager = GameMode
        ? GameMode->GetRoundManager()
        : nullptr;
    const EAdaptiveRoundPhase RoundPhase = RoundManager
        ? RoundManager->GetRoundPhase()
        : EAdaptiveRoundPhase::WaitingToStart;
    const float IntermissionRemaining = RoundManager
        ? (RoundPhase == EAdaptiveRoundPhase::PreRoundCountdown
            ? RoundManager->GetPreRoundCountdownRemainingTime()
            : RoundManager->GetIntermissionRemainingTime())
        : 0.0f;

    const FString RoundText = FormatRoundStatus(
        Snapshot.CurrentRound,
        Snapshot.TotalRounds,
        RoundPhase,
        IntermissionRemaining
    );
    float TextWidth = 0.0f;
    float TextHeight = 0.0f;
    GetTextSize(
        RoundText,
        TextWidth,
        TextHeight,
        GEngine->GetMediumFont(),
        1.0f
    );
    DrawText(
        RoundText,
        FLinearColor::White,
        (Canvas->ClipX - TextWidth) * 0.5f,
        18.0f,
        GEngine->GetMediumFont(),
        1.0f,
        false
    );

    const FString RoundStageText =
        FAdaptiveAdaptationRevealPolicy::FormatRoundStage(
            Snapshot.CurrentRound
        );
    GetTextSize(
        RoundStageText,
        TextWidth,
        TextHeight,
        GEngine->GetSmallFont(),
        1.0f
    );
    DrawText(
        RoundStageText,
        Snapshot.CurrentRound <= 1
            ? FLinearColor(1.0f, 0.78f, 0.16f)
            : (Snapshot.CurrentRound == 2
                ? FLinearColor(0.30f, 0.86f, 1.0f)
                : FLinearColor(0.76f, 0.48f, 1.0f)),
        (Canvas->ClipX - TextWidth) * 0.5f,
        43.0f,
        GEngine->GetSmallFont(),
        1.0f,
        false
    );

    FString PredictionText;
    if (RoundPhase == EAdaptiveRoundPhase::Intermission)
    {
        PredictionText = TEXT("LEARNING UPDATE BETWEEN ROUNDS");
    }
    else if (RoundPhase == EAdaptiveRoundPhase::MatchComplete)
    {
        PredictionText = TEXT("LEARN  ->  ADAPT  ->  COUNTER-ADAPT");
    }
    else if (RoundPhase == EAdaptiveRoundPhase::PreRoundCountdown
        && Snapshot.CurrentRound > 1
        && Snapshot.AdaptiveTacticalProfile.IsActive())
    {
        PredictionText = TEXT("LEARNED TACTICAL PROFILE READY");
    }
    else
    {
        PredictionText = FormatPredictionStatus(
            Snapshot.CurrentRound,
            Snapshot.bPredictionsEnabled
        );
    }
    GetTextSize(
        PredictionText,
        TextWidth,
        TextHeight,
        GEngine->GetSmallFont(),
        1.0f
    );
    DrawText(
        PredictionText,
        Snapshot.bPredictionsEnabled
            || RoundPhase == EAdaptiveRoundPhase::MatchComplete
            || (RoundPhase == EAdaptiveRoundPhase::PreRoundCountdown
                && Snapshot.AdaptiveTacticalProfile.IsActive())
            ? FLinearColor(0.30f, 1.0f, 0.48f)
            : FLinearColor(1.0f, 0.78f, 0.16f),
        (Canvas->ClipX - TextWidth) * 0.5f,
        64.0f,
        GEngine->GetSmallFont(),
        1.0f,
        false
    );

    const float SafeWidth = FMath::Clamp(
        FMath::Min(ResourceBarWidth, Canvas->ClipX * 0.42f),
        200.0f,
        440.0f
    );
    const float EnemyX = (Canvas->ClipX - SafeWidth) * 0.5f;
    DrawResourceBar(
        TEXT("ENEMY HEALTH"),
        Snapshot.EnemyHealth,
        Snapshot.EnemyMaxHealth,
        FVector2D(EnemyX, 89.0f),
        SafeWidth,
        FLinearColor(0.92f, 0.12f, 0.07f)
    );
    DrawResourceBar(
        TEXT("ENEMY STAMINA"),
        Snapshot.EnemyStamina,
        Snapshot.EnemyMaxStamina,
        FVector2D(EnemyX, 115.0f),
        SafeWidth,
        FLinearColor(0.18f, 0.72f, 1.0f)
    );

    const float PlayerY = FMath::Max(132.0f, Canvas->ClipY - 78.0f);
    DrawResourceBar(
        TEXT("PLAYER HEALTH"),
        Snapshot.PlayerHealth,
        Snapshot.PlayerMaxHealth,
        FVector2D(24.0f, PlayerY),
        SafeWidth,
        FLinearColor(0.16f, 0.88f, 0.30f)
    );

    if (RoundManager && RoundPhase == EAdaptiveRoundPhase::Intermission)
    {
        const FAdaptiveRevealText& Reveal =
            RoundManager->GetAdaptationReveal();
        DrawCenteredStatusLine(
            Reveal.Observation,
            Canvas->ClipY * 0.27f,
            Reveal.bHasSupportedObservation
                ? FLinearColor::White
                : FLinearColor(0.75f, 0.75f, 0.78f),
            1.05f
        );
        DrawCenteredStatusLine(
            Reveal.Adjustment,
            Canvas->ClipY * 0.27f + 26.0f,
            Reveal.bHasActiveAdjustment
                ? FLinearColor(0.30f, 1.0f, 0.48f)
                : FLinearColor(1.0f, 0.78f, 0.16f),
            1.05f
        );
    }
    else if (RoundManager
        && RoundPhase == EAdaptiveRoundPhase::PreRoundCountdown)
    {
        const int32 Count = FMath::Max(
            1,
            FMath::CeilToInt(IntermissionRemaining)
        );
        DrawCenteredStatusLine(
            FString::FromInt(Count),
            Canvas->ClipY * 0.30f,
            FLinearColor::White,
            1.8f
        );
    }
    else if (RoundManager
        && RoundPhase == EAdaptiveRoundPhase::MatchComplete)
    {
        DrawCenteredStatusLine(
            FormatMatchWinner(RoundManager->GetMatchWinner()),
            Canvas->ClipY * 0.25f,
            FLinearColor(0.30f, 1.0f, 0.48f),
            1.30f
        );
        DrawCenteredStatusLine(
            FormatMatchScore(
                RoundManager->GetPlayerRoundWins(),
                RoundManager->GetEnemyRoundWins()
            ),
            Canvas->ClipY * 0.25f + 34.0f,
            FLinearColor::White,
            1.15f
        );
        DrawCenteredStatusLine(
            TEXT("PRESS ENTER / GAMEPAD MENU TO RESTART WITH LEARNED HISTORY"),
            Canvas->ClipY * 0.25f + 68.0f,
            FLinearColor(1.0f, 0.78f, 0.16f),
            1.0f
        );
    }
    DrawResourceBar(
        TEXT("PLAYER STAMINA"),
        Snapshot.PlayerStamina,
        Snapshot.PlayerMaxStamina,
        FVector2D(24.0f, PlayerY + 26.0f),
        SafeWidth,
        FLinearColor(0.12f, 0.58f, 1.0f)
    );

    const APlayerController* PlayerController = GetOwningPlayerController();
    const AAdaptivePlayerCharacter* Player = PlayerController
        ? Cast<AAdaptivePlayerCharacter>(PlayerController->GetPawn())
        : nullptr;
    const AAdaptiveEnemyCharacter* Enemy = GameMode
        ? GameMode->GetSpawnedEnemy()
        : nullptr;
    const UCombatFeedbackComponent* PlayerFeedback = Player
        ? Player->GetCombatFeedbackComponent()
        : nullptr;
    const UCombatFeedbackComponent* EnemyFeedback = Enemy
        ? Enemy->GetCombatFeedbackComponent()
        : nullptr;

    FString FeedbackText;
    ECombatFeedbackCue FeedbackCue = ECombatFeedbackCue::None;
    const ECombatFeedbackCue PlayerCue = PlayerFeedback
        ? PlayerFeedback->GetActiveCue()
        : ECombatFeedbackCue::None;
    const ECombatFeedbackCue EnemyCue = EnemyFeedback
        ? EnemyFeedback->GetActiveCue()
        : ECombatFeedbackCue::None;
    if (EnemyCue != ECombatFeedbackCue::None)
    {
        FeedbackCue = EnemyCue;
        switch (EnemyCue)
        {
        case ECombatFeedbackCue::Hit:
            FeedbackText = TEXT("HIT");
            break;
        case ECombatFeedbackCue::Blocked:
            FeedbackText = TEXT("BLOCKED");
            break;
        case ECombatFeedbackCue::Dodged:
            FeedbackText = TEXT("ENEMY DODGED");
            break;
        case ECombatFeedbackCue::Miss:
            FeedbackText = TEXT("ENEMY MISSED");
            break;
        case ECombatFeedbackCue::Death:
            FeedbackText = TEXT("ENEMY DEFEATED");
            break;
        default:
            break;
        }
    }
    if (PlayerCue != ECombatFeedbackCue::None
        && AdaptiveCombatFeedback::GetCuePriority(PlayerCue)
            >= AdaptiveCombatFeedback::GetCuePriority(FeedbackCue))
    {
        FeedbackCue = PlayerCue;
        switch (PlayerCue)
        {
        case ECombatFeedbackCue::Hit:
            FeedbackText = TEXT("HIT RECEIVED");
            break;
        case ECombatFeedbackCue::Blocked:
            FeedbackText = TEXT("BLOCKED");
            break;
        case ECombatFeedbackCue::Dodged:
            FeedbackText = TEXT("DODGE");
            break;
        case ECombatFeedbackCue::Miss:
            FeedbackText = TEXT("MISS");
            break;
        case ECombatFeedbackCue::Death:
            FeedbackText = TEXT("PLAYER DEFEATED");
            break;
        default:
            break;
        }
    }
    if (!FeedbackText.IsEmpty())
    {
        GetTextSize(
            FeedbackText,
            TextWidth,
            TextHeight,
            GEngine->GetMediumFont(),
            1.25f
        );
        DrawText(
            FeedbackText,
            AdaptiveCombatFeedback::GetCueColor(FeedbackCue),
            (Canvas->ClipX - TextWidth) * 0.5f,
            Canvas->ClipY * 0.42f,
            GEngine->GetMediumFont(),
            1.25f,
            false
        );
    }
}

void AAdaptiveDebugHUD::DrawCenteredStatusLine(
    const FString& Text,
    const float Y,
    const FLinearColor& Color,
    const float Scale
)
{
    if (Text.IsEmpty() || !Canvas || !GEngine)
    {
        return;
    }

    float Width = 0.0f;
    float Height = 0.0f;
    GetTextSize(
        Text,
        Width,
        Height,
        GEngine->GetMediumFont(),
        Scale
    );
    DrawText(
        Text,
        Color,
        (Canvas->ClipX - Width) * 0.5f,
        Y,
        GEngine->GetMediumFont(),
        Scale,
        false
    );
}

void AAdaptiveDebugHUD::DrawDetailedAdaptiveDebug(
    const FAdaptiveDebugTelemetrySnapshot& Snapshot
)
{
    const TArray<FString> Lines =
        FAdaptiveDebugTelemetryFormatter::FormatHudLines(Snapshot);
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

void AAdaptiveDebugHUD::DrawResourceBar(
    const FString& Label,
    const float CurrentValue,
    const float MaximumValue,
    const FVector2D& Position,
    const float Width,
    const FLinearColor& FillColor
)
{
    const float Height = FMath::Max(8.0f, ResourceBarHeight);
    DrawRect(
        FLinearColor(0.015f, 0.015f, 0.02f, 0.86f),
        Position.X - 2.0f,
        Position.Y - 2.0f,
        Width + 4.0f,
        Height + 4.0f
    );
    DrawRect(
        FLinearColor(0.10f, 0.10f, 0.12f, 0.94f),
        Position.X,
        Position.Y,
        Width,
        Height
    );
    DrawRect(
        FillColor,
        Position.X,
        Position.Y,
        Width * NormalizeResource(CurrentValue, MaximumValue),
        Height
    );
    DrawText(
        FString::Printf(
            TEXT("%s  %.0f / %.0f"),
            *Label,
            FMath::IsFinite(CurrentValue) ? FMath::Max(0.0f, CurrentValue) : 0.0f,
            FMath::IsFinite(MaximumValue) ? FMath::Max(0.0f, MaximumValue) : 0.0f
        ),
        FLinearColor::White,
        Position.X + 4.0f,
        Position.Y - 1.0f,
        GEngine->GetSmallFont(),
        0.85f,
        false
    );
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
        Snapshot.RoundPresentationStage =
            FAdaptiveAdaptationRevealPolicy::FormatRoundStage(
                Snapshot.CurrentRound
            );
        Snapshot.AdaptationObservation =
            RoundManager->GetAdaptationReveal().Observation;
        Snapshot.AdaptationAdjustment =
            RoundManager->GetAdaptationReveal().Adjustment;
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
        if (const UStaminaComponent* Stamina =
            Enemy->GetStaminaComponent())
        {
            Snapshot.EnemyStamina = Stamina->GetCurrentStamina();
            Snapshot.EnemyMaxStamina = Stamina->GetMaxStamina();
        }
        if (const UEnemyDecisionComponent* Decision =
            Enemy->GetEnemyDecisionComponent())
        {
            Snapshot.Prediction = Decision->GetLastPrediction();
            Snapshot.CurrentEnemySelectedAction =
                Decision->GetLastSelectedAction();
            Snapshot.bLastDecisionUrgent =
                Decision->WasLastDecisionUrgent();
            Snapshot.LastSelection = Decision->GetLastSelectionResult();
            Snapshot.RecentCommittedActionCount =
                Decision->GetRecentCommittedActionCount();
            Snapshot.RecentOutcomeMemoryCount =
                Decision->GetRecentOutcomeMemoryCount();
            for (const EEnemyCombatAction Action :
                Snapshot.LastSelection.CandidateActions)
            {
                FEnemyDecisionModifierTelemetry Modifier;
                Modifier.Action = Action;
                Modifier.RepetitionModifier =
                    Decision->GetLastRepetitionModifiers().FindRef(Action);
                Modifier.RecentOutcomeModifier =
                    Decision->GetLastRecentOutcomeModifiers().FindRef(Action);
                Snapshot.LastDecisionModifiers.Add(Modifier);
            }
            Snapshot.AdaptiveTacticalProfile =
                Decision->GetAdaptiveTacticalProfile();
            Snapshot.AdaptiveCounterUsesRemaining =
                Decision->GetAdaptiveCounterUsesRemaining();
            Snapshot.AdaptiveCounterCooldownRemaining =
                Decision->GetAdaptiveCounterCooldownRemaining();
            Snapshot.CounterOutcomeCount =
                Decision->GetCounterOutcomeCount();
            if (const FAdaptiveCounterOutcomeRecord* Outcome =
                Decision->GetLastCounterOutcome())
            {
                Snapshot.bHasLastCounterOutcome = true;
                Snapshot.LastCounterOutcome = *Outcome;
            }
            Snapshot.LastRoundCounterEffectiveness =
                Decision->GetLastRoundCounterEffectiveness();
            Snapshot.ActiveCounterEffectiveness =
                Decision->GetActiveCounterEffectiveness();
        }
    }

    if (!Snapshot.bPredictionsEnabled)
    {
        Snapshot.Prediction = FPredictionResult();
    }
    return Snapshot;
}
