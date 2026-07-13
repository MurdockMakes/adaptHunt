#include "Data/LearningTelemetry.h"

namespace
{
const EPlayerCombatAction PlayerActions[] = {
    EPlayerCombatAction::LightAttack,
    EPlayerCombatAction::HeavyAttack,
    EPlayerCombatAction::DodgeLeft,
    EPlayerCombatAction::DodgeRight,
    EPlayerCombatAction::DodgeBackward,
    EPlayerCombatAction::Block,
    EPlayerCombatAction::Heal
};

const EEnemyCombatAction EnemyActions[] = {
    EEnemyCombatAction::MoveTowardPlayer,
    EEnemyCombatAction::MoveAwayFromPlayer,
    EEnemyCombatAction::StrafeLeft,
    EEnemyCombatAction::StrafeRight,
    EEnemyCombatAction::LightAttack,
    EEnemyCombatAction::HeavyAttack,
    EEnemyCombatAction::ProjectileAttack,
    EEnemyCombatAction::DashAttack,
    EEnemyCombatAction::Block,
    EEnemyCombatAction::Dodge,
    EEnemyCombatAction::InterruptHeal
};

float SanitizeNonNegative(const float Value)
{
    return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : 0.0f;
}

bool IsUsableTelemetryPrediction(const FPredictionResult& Prediction)
{
    return Prediction.bHasPrediction
        && AdaptiveCombat::IsTrackablePlayerAction(
            Prediction.PredictedAction
        )
        && FMath::IsFinite(Prediction.Confidence)
        && Prediction.SupportingSampleCount >= 0
        && (!Prediction.bUsedContext
            || (Prediction.ConditioningEnemyAction
                    != EEnemyCombatAction::None
                && AdaptiveCombat::IsKnownEnemyAction(
                    Prediction.ConditioningEnemyAction
                )))
        && (!Prediction.bUsedDistanceContext
            || (Prediction.bUsedContext
                && AdaptiveCombat::IsKnownDistanceCategory(
                    Prediction.ConditioningDistanceCategory
                )))
        && (!Prediction.bUsedPositionContext
            || (Prediction.bUsedDistanceContext
                && AdaptiveCombat::IsKnownRelativePosition(
                    Prediction.ConditioningRelativePlayerPosition
                )))
        && (!Prediction.bUsedPreviousPlayerActionContext
            || (Prediction.bUsedPositionContext
                && AdaptiveCombat::IsTrackablePlayerAction(
                    Prediction.ConditioningPreviousPlayerAction
                )));
}

FAdaptiveConditionalPattern BuildPattern(
    const EEnemyCombatAction EnemyAction,
    const TMap<EPlayerCombatAction, int32>& Counts
)
{
    FAdaptiveConditionalPattern Pattern;
    Pattern.PreviousEnemyAction = EnemyAction;

    for (const EPlayerCombatAction PlayerAction : PlayerActions)
    {
        const int32 Count = Counts.FindRef(PlayerAction);
        Pattern.TotalSampleCount += Count;
        if (Count > Pattern.DominantActionCount)
        {
            Pattern.DominantActionCount = Count;
            Pattern.DominantPlayerAction = PlayerAction;
        }
    }

    if (Pattern.TotalSampleCount <= 0
        || Pattern.DominantPlayerAction == EPlayerCombatAction::None)
    {
        return Pattern;
    }

    Pattern.Confidence = static_cast<float>(Pattern.DominantActionCount)
        / static_cast<float>(Pattern.TotalSampleCount);

    struct FPercentageRemainder
    {
        int32 ResultIndex = INDEX_NONE;
        int32 Remainder = 0;
        bool bReceivedRemainderPoint = false;
    };

    TArray<FPercentageRemainder> Remainders;
    int32 AssignedPercentage = 0;
    for (const EPlayerCombatAction PlayerAction : PlayerActions)
    {
        const int32 Count = Counts.FindRef(PlayerAction);
        if (Count <= 0)
        {
            continue;
        }

        const int32 Numerator = Count * 100;
        FAdaptiveActionPercentage& Result =
            Pattern.ActionPercentages.AddDefaulted_GetRef();
        Result.Action = PlayerAction;
        Result.Count = Count;
        Result.Percentage = Numerator / Pattern.TotalSampleCount;
        AssignedPercentage += Result.Percentage;

        FPercentageRemainder& Remainder =
            Remainders.AddDefaulted_GetRef();
        Remainder.ResultIndex = Pattern.ActionPercentages.Num() - 1;
        Remainder.Remainder = Numerator % Pattern.TotalSampleCount;
    }

    int32 RemainingPercentage = 100 - AssignedPercentage;
    while (RemainingPercentage > 0)
    {
        int32 BestRemainderIndex = INDEX_NONE;
        for (int32 Index = 0; Index < Remainders.Num(); ++Index)
        {
            const FPercentageRemainder& Candidate = Remainders[Index];
            if (Candidate.bReceivedRemainderPoint)
            {
                continue;
            }
            if (BestRemainderIndex == INDEX_NONE
                || Candidate.Remainder
                    > Remainders[BestRemainderIndex].Remainder)
            {
                BestRemainderIndex = Index;
            }
        }

        if (BestRemainderIndex == INDEX_NONE)
        {
            break;
        }

        FPercentageRemainder& BestRemainder =
            Remainders[BestRemainderIndex];
        ++Pattern.ActionPercentages[BestRemainder.ResultIndex].Percentage;
        BestRemainder.bReceivedRemainderPoint = true;
        --RemainingPercentage;
    }

    return Pattern;
}

bool IsStrongerPattern(
    const FAdaptiveConditionalPattern& Candidate,
    const FAdaptiveConditionalPattern& Current
)
{
    if (!Candidate.IsValid())
    {
        return false;
    }
    if (!Current.IsValid())
    {
        return true;
    }
    if (Candidate.DominantActionCount != Current.DominantActionCount)
    {
        return Candidate.DominantActionCount > Current.DominantActionCount;
    }
    if (!FMath::IsNearlyEqual(Candidate.Confidence, Current.Confidence))
    {
        return Candidate.Confidence > Current.Confidence;
    }
    return Candidate.TotalSampleCount > Current.TotalSampleCount;
}

FString GetPredictionSourceDescription(
    const FPredictionResult& Prediction
)
{
    if (!IsUsableTelemetryPrediction(Prediction))
    {
        return TEXT("None");
    }

    const auto GetPlayerActionName = [](const EPlayerCombatAction Action)
    {
        const UEnum* ActionEnum = StaticEnum<EPlayerCombatAction>();
        return ActionEnum
            ? ActionEnum->GetDisplayNameTextByValue(
                static_cast<int64>(Action)
            ).ToString()
            : FString(TEXT("Unknown"));
    };
    const auto GetEnemyActionName = [](const EEnemyCombatAction Action)
    {
        const UEnum* ActionEnum = StaticEnum<EEnemyCombatAction>();
        return ActionEnum
            ? ActionEnum->GetDisplayNameTextByValue(
                static_cast<int64>(Action)
            ).ToString()
            : FString(TEXT("Unknown"));
    };

    if (!Prediction.bUsedContext)
    {
        return TEXT("Global Frequency");
    }
    if (!Prediction.bUsedDistanceContext)
    {
        return FString::Printf(
            TEXT("After Enemy %s (Any Range)"),
            *GetEnemyActionName(Prediction.ConditioningEnemyAction)
        );
    }

    const TCHAR* DistanceName = TEXT("Medium");
    if (Prediction.ConditioningDistanceCategory
        == ECombatDistanceCategory::Close)
    {
        DistanceName = TEXT("Close");
    }
    else if (Prediction.ConditioningDistanceCategory
        == ECombatDistanceCategory::Far)
    {
        DistanceName = TEXT("Far");
    }
    FString Result = FString::Printf(
        TEXT("After Enemy %s at %s Range"),
        *GetEnemyActionName(Prediction.ConditioningEnemyAction),
        DistanceName
    );
    if (!Prediction.bUsedPositionContext)
    {
        return Result;
    }

    const TCHAR* PositionName = TEXT("Front");
    if (Prediction.ConditioningRelativePlayerPosition
        == ERelativePlayerPosition::Left)
    {
        PositionName = TEXT("Left");
    }
    else if (Prediction.ConditioningRelativePlayerPosition
        == ERelativePlayerPosition::Right)
    {
        PositionName = TEXT("Right");
    }
    else if (Prediction.ConditioningRelativePlayerPosition
        == ERelativePlayerPosition::Behind)
    {
        PositionName = TEXT("Behind");
    }
    Result += FString::Printf(TEXT(" (Player %s"), PositionName);
    if (Prediction.bUsedPreviousPlayerActionContext)
    {
        Result += FString::Printf(
            TEXT(", after Player %s"),
            *GetPlayerActionName(
                Prediction.ConditioningPreviousPlayerAction
            )
        );
    }
    Result += TEXT(")");
    return Result;
}

const TCHAR* GetProfileEvidenceReason(
    const EAdaptiveProfileEvidenceStatus Status
)
{
    switch (Status)
    {
    case EAdaptiveProfileEvidenceStatus::NoPrediction:
        return TEXT("no supported predictor output");
    case EAdaptiveProfileEvidenceStatus::InsufficientRoundSamples:
        return TEXT("too few round samples");
    case EAdaptiveProfileEvidenceStatus::InsufficientContextSamples:
        return TEXT("too few supporting context samples");
    case EAdaptiveProfileEvidenceStatus::LowConfidence:
        return TEXT("confidence below threshold");
    case EAdaptiveProfileEvidenceStatus::PredictorDisagreement:
        return TEXT("round evidence conflicts with predictor");
    case EAdaptiveProfileEvidenceStatus::Active:
        return TEXT("minimum evidence and confidence met");
    case EAdaptiveProfileEvidenceStatus::NoData:
    default:
        return TEXT("no completed-round evidence");
    }
}
}

bool FAdaptiveConditionalPattern::IsValid() const
{
    if (PreviousEnemyAction == EEnemyCombatAction::None
        || !AdaptiveCombat::IsKnownEnemyAction(PreviousEnemyAction)
        || !AdaptiveCombat::IsTrackablePlayerAction(
            DominantPlayerAction
        )
        || TotalSampleCount <= 0 || DominantActionCount <= 0
        || DominantActionCount > TotalSampleCount
        || !FMath::IsFinite(Confidence)
        || !FMath::IsNearlyEqual(
            Confidence,
            static_cast<float>(DominantActionCount)
                / static_cast<float>(TotalSampleCount)
        ))
    {
        return false;
    }

    int32 CountTotal = 0;
    int32 PercentageTotal = 0;
    for (const FAdaptiveActionPercentage& Entry : ActionPercentages)
    {
        if (!AdaptiveCombat::IsTrackablePlayerAction(Entry.Action)
            || Entry.Count <= 0 || Entry.Percentage < 0
            || Entry.Percentage > 100)
        {
            return false;
        }
        CountTotal += Entry.Count;
        PercentageTotal += Entry.Percentage;
    }
    return CountTotal == TotalSampleCount && PercentageTotal == 100;
}

int32 FAdaptiveConditionalPattern::GetPercentage(
    const EPlayerCombatAction Action
) const
{
    for (const FAdaptiveActionPercentage& Entry : ActionPercentages)
    {
        if (Entry.Action == Action)
        {
            return Entry.Percentage;
        }
    }
    return 0;
}

FAdaptiveLearningSummary FAdaptiveLearningTelemetryAnalyzer::Analyze(
    const FCombatDataset& Dataset,
    const int32 RoundNumber
)
{
    FAdaptiveLearningSummary Summary;
    TMap<EPlayerCombatAction, int32> GlobalCounts;
    TMap<EEnemyCombatAction, TMap<EPlayerCombatAction, int32>>
        ConditionalCounts;

    for (const FTrainingSample& Sample : Dataset.GetSamples())
    {
        if (!Sample.IsValid()
            || (RoundNumber > 0
                && Sample.Snapshot.RoundNumber != RoundNumber))
        {
            continue;
        }

        ++Summary.SampleCount;
        ++GlobalCounts.FindOrAdd(Sample.NextPlayerAction);
        if (Sample.Snapshot.PreviousEnemyAction
            != EEnemyCombatAction::None)
        {
            ++ConditionalCounts.FindOrAdd(
                Sample.Snapshot.PreviousEnemyAction
            ).FindOrAdd(Sample.NextPlayerAction);
        }
    }

    int32 HighestGlobalCount = 0;
    for (const EPlayerCombatAction PlayerAction : PlayerActions)
    {
        const int32 Count = GlobalCounts.FindRef(PlayerAction);
        if (Count > HighestGlobalCount)
        {
            HighestGlobalCount = Count;
            Summary.MostCommonPlayerAction = PlayerAction;
        }
    }

    for (const EEnemyCombatAction EnemyAction : EnemyActions)
    {
        const TMap<EPlayerCombatAction, int32>* Counts =
            ConditionalCounts.Find(EnemyAction);
        if (!Counts)
        {
            continue;
        }

        const FAdaptiveConditionalPattern Candidate =
            BuildPattern(EnemyAction, *Counts);
        if (IsStrongerPattern(
            Candidate,
            Summary.StrongestConditionalPattern
        ))
        {
            Summary.StrongestConditionalPattern = Candidate;
        }
    }

    return Summary;
}

TArray<FString> FAdaptiveDebugTelemetryFormatter::FormatHudLines(
    const FAdaptiveDebugTelemetrySnapshot& Snapshot
)
{
    const int32 SafeTotalRounds = FMath::Max(1, Snapshot.TotalRounds);
    const int32 SafeCurrentRound = FMath::Clamp(
        Snapshot.CurrentRound,
        0,
        SafeTotalRounds
    );
    const bool bUsablePrediction = IsUsableTelemetryPrediction(
        Snapshot.Prediction
    );
    const float SafePredictionConfidence = bUsablePrediction
        ? FMath::Clamp(Snapshot.Prediction.Confidence, 0.0f, 1.0f)
        : 0.0f;

    TArray<FString> Lines;
    Lines.Reserve(32);
    Lines.Add(TEXT("ADAPTIVE LEARNING DEBUG"));
    Lines.Add(FString::Printf(
        TEXT("Round: %d / %d (%s)"),
        SafeCurrentRound,
        SafeTotalRounds,
        Snapshot.bPredictionsEnabled
            ? TEXT("predictions enabled")
            : TEXT("observation")
    ));
    if (!Snapshot.RoundPresentationStage.IsEmpty())
    {
        Lines.Add(FString::Printf(
            TEXT("Visible Match Stage: %s"),
            *Snapshot.RoundPresentationStage
        ));
    }
    if (!Snapshot.AdaptationObservation.IsEmpty())
    {
        Lines.Add(FString::Printf(
            TEXT("Visible Reveal: %s"),
            *Snapshot.AdaptationObservation
        ));
    }
    if (!Snapshot.AdaptationAdjustment.IsEmpty())
    {
        Lines.Add(FString::Printf(
            TEXT("Visible Reveal: %s"),
            *Snapshot.AdaptationAdjustment
        ));
    }
    Lines.Add(FString::Printf(
        TEXT("Player Health: %.1f / %.1f"),
        SanitizeNonNegative(Snapshot.PlayerHealth),
        SanitizeNonNegative(Snapshot.PlayerMaxHealth)
    ));
    Lines.Add(FString::Printf(
        TEXT("Enemy Health: %.1f / %.1f"),
        SanitizeNonNegative(Snapshot.EnemyHealth),
        SanitizeNonNegative(Snapshot.EnemyMaxHealth)
    ));
    Lines.Add(FString::Printf(
        TEXT("Player Stamina: %.1f / %.1f"),
        SanitizeNonNegative(Snapshot.PlayerStamina),
        SanitizeNonNegative(Snapshot.PlayerMaxStamina)
    ));
    Lines.Add(FString::Printf(
        TEXT("Enemy Stamina: %.1f / %.1f"),
        SanitizeNonNegative(Snapshot.EnemyStamina),
        SanitizeNonNegative(Snapshot.EnemyMaxStamina)
    ));
    Lines.Add(FString::Printf(
        TEXT("Last Player Action: %s"),
        *GetPlayerActionName(Snapshot.LastPlayerAction)
    ));
    Lines.Add(FString::Printf(
        TEXT("Predicted Next Player Action: %s"),
        *GetPlayerActionName(
            bUsablePrediction
                ? Snapshot.Prediction.PredictedAction
                : EPlayerCombatAction::None
        )
    ));
    Lines.Add(FString::Printf(
        TEXT("Prediction Confidence: %d%%"),
        FMath::RoundToInt(
            SafePredictionConfidence * 100.0f
        )
    ));
    const FString PredictionSource = GetPredictionSourceDescription(
        Snapshot.Prediction
    );
    Lines.Add(FString::Printf(
        TEXT("Prediction Source: %s"),
        *PredictionSource
    ));
    Lines.Add(FString::Printf(
        TEXT("Collected Training Samples: %d"),
        FMath::Max(0, Snapshot.CollectedSampleCount)
    ));
    Lines.Add(FString::Printf(
        TEXT("Most Common Player Action: %s"),
        *GetPlayerActionName(Snapshot.MostCommonPlayerAction)
    ));
    Lines.Add(TEXT("Strongest Learned Conditional Pattern:"));
    Lines.Add(FString::Printf(
        TEXT("  %s"),
        *FormatConditionalPattern(Snapshot.StrongestLearnedPattern)
    ));
    Lines.Add(FString::Printf(
        TEXT("Current Enemy Selected Action: %s"),
        *GetEnemyActionName(Snapshot.CurrentEnemySelectedAction)
    ));
    Lines.Add(FString::Printf(
        TEXT("Decision Trigger: %s"),
        Snapshot.bLastDecisionUrgent ? TEXT("URGENT") : TEXT("NORMAL")
    ));

    TArray<FString> CandidateNames;
    for (const EEnemyCombatAction Action :
        Snapshot.LastSelection.CandidateActions)
    {
        CandidateNames.Add(GetEnemyActionName(Action));
    }
    Lines.Add(FString::Printf(
        TEXT(
            "Near-Best Selection: best %.3f, selected %.3f, "
            "%d candidate(s) [%s]"
        ),
        FMath::IsFinite(Snapshot.LastSelection.BestScore)
            ? Snapshot.LastSelection.BestScore
            : 0.0f,
        FMath::IsFinite(Snapshot.LastSelection.SelectedScore)
            ? Snapshot.LastSelection.SelectedScore
            : 0.0f,
        CandidateNames.Num(),
        *FString::Join(CandidateNames, TEXT(", "))
    ));
    for (const FEnemyDecisionModifierTelemetry& Modifier :
        Snapshot.LastDecisionModifiers)
    {
        Lines.Add(FString::Printf(
            TEXT("  %s modifiers: repetition %+.3f, recent outcome %+.3f"),
            *GetEnemyActionName(Modifier.Action),
            FMath::IsFinite(Modifier.RepetitionModifier)
                ? Modifier.RepetitionModifier
                : 0.0f,
            FMath::IsFinite(Modifier.RecentOutcomeModifier)
                ? Modifier.RecentOutcomeModifier
                : 0.0f
        ));
    }
    Lines.Add(FString::Printf(
        TEXT("Short-Term Memory: %d commits, %d resolved outcomes"),
        FMath::Max(0, Snapshot.RecentCommittedActionCount),
        FMath::Max(0, Snapshot.RecentOutcomeMemoryCount)
    ));

    const FAdaptiveTacticalProfile& Profile =
        Snapshot.AdaptiveTacticalProfile;
    const TCHAR* ProfilePlayState = Profile.IsActive()
        ? (Snapshot.bPredictionsEnabled ? TEXT("ACTIVE") : TEXT("READY"))
        : TEXT("BASELINE");
    Lines.Add(FString::Printf(
        TEXT("Adaptive Tactical Profile: %s - %s"),
        ProfilePlayState,
        GetProfileEvidenceReason(Profile.EvidenceStatus)
    ));
    Lines.Add(FString::Printf(
        TEXT("Profile Evidence: %s at %d%% (%d/%d context, %d round)"),
        *GetPlayerActionName(Profile.MostLikelyPlayerAction),
        FMath::RoundToInt(FMath::Clamp(Profile.Confidence, 0.0f, 1.0f)
            * 100.0f),
        FMath::Max(0, Profile.SupportingSampleCount),
        FMath::Max(0, Profile.ContextSampleCount),
        FMath::Max(0, Profile.RoundSampleCount)
    ));
    Lines.Add(FString::Printf(
        TEXT("Profile Context: %s"),
        *GetPredictionSourceDescription(Profile.EvidencePrediction)
    ));

    const TCHAR* OrbitName = TEXT("Seeded");
    if (Profile.OrbitPreference == EAdaptiveOrbitPreference::Left)
    {
        OrbitName = TEXT("Left");
    }
    else if (Profile.OrbitPreference == EAdaptiveOrbitPreference::Right)
    {
        OrbitName = TEXT("Right");
    }
    Lines.Add(FString::Printf(
        TEXT("Profile Counter: %s (%d uses left, %.1fs cooldown)"),
        *GetEnemyActionName(Profile.PreferredCounterAction),
        FMath::Max(0, Snapshot.AdaptiveCounterUsesRemaining),
        SanitizeNonNegative(Snapshot.AdaptiveCounterCooldownRemaining)
    ));
    Lines.Add(FString::Printf(
        TEXT("Profile Changes: spacing %+.0f, aggression %+.0f%%, defense %+.0f%%, heal interrupt +%.0f%%, orbit %s"),
        FMath::IsFinite(Profile.PreferredSpacingAdjustment)
            ? Profile.PreferredSpacingAdjustment
            : 0.0f,
        (FMath::IsFinite(Profile.AggressionAdjustment)
            ? Profile.AggressionAdjustment
            : 0.0f) * 100.0f,
        (FMath::IsFinite(Profile.DefensiveAdjustment)
            ? Profile.DefensiveAdjustment
            : 0.0f) * 100.0f,
        SanitizeNonNegative(Profile.HealInterruptionPriority) * 100.0f,
        OrbitName
    ));

    Lines.Add(FString::Printf(
        TEXT("Resolved Enemy Action Outcomes: %d"),
        FMath::Max(0, Snapshot.CounterOutcomeCount)
    ));
    if (Snapshot.bHasLastCounterOutcome
        && Snapshot.LastCounterOutcome.IsValid())
    {
        const FAdaptiveCounterOutcomeRecord& Outcome =
            Snapshot.LastCounterOutcome;
        const FString PredictedAction = Outcome.Prediction.bHasPrediction
            ? GetPlayerActionName(Outcome.Prediction.PredictedAction)
            : FString(TEXT("None"));
        const TCHAR* Worked = Outcome.IsAdaptiveCounterAttempt()
            ? (Outcome.WasSuccessfulCounter()
                ? TEXT("worked")
                : TEXT("did not work"))
            : TEXT("not an adaptive opportunity");
        Lines.Add(FString::Printf(
            TEXT("Last Counter Evaluation: predicted %s at %d%% -> chose %s -> %s (%s)"),
            *PredictedAction,
            FMath::RoundToInt(FMath::Clamp(
                Outcome.Prediction.Confidence,
                0.0f,
                1.0f
            ) * 100.0f),
            *GetEnemyActionName(Outcome.Action),
            *GetAdaptiveCounterOutcomeName(Outcome.Result),
            Worked
        ));
        Lines.Add(FString::Printf(
            TEXT("  Round %d context: %s; applied=%s, opportunity=%s"),
            Outcome.RoundNumber,
            *GetPredictionSourceDescription(Outcome.Prediction),
            Outcome.bPredictionApplied ? TEXT("true") : TEXT("false"),
            Outcome.bAdaptiveCounterOpportunity
                ? TEXT("true")
                : TEXT("false")
        ));
    }
    else
    {
        Lines.Add(TEXT("Last Counter Evaluation: no resolved enemy action."));
    }

    const FAdaptiveCounterEffectivenessSummary& ActiveEffectiveness =
        Snapshot.ActiveCounterEffectiveness;
    if (ActiveEffectiveness.HasSamples())
    {
        Lines.Add(FString::Printf(
            TEXT("Outcome Secondary Modifier: %s vs predicted %s, %d/%d worked, %s, utility %+.1f%%"),
            *GetEnemyActionName(ActiveEffectiveness.CounterAction),
            *GetPlayerActionName(
                ActiveEffectiveness.PredictedPlayerAction
            ),
            ActiveEffectiveness.SuccessfulCount,
            ActiveEffectiveness.SampleCount,
            ActiveEffectiveness.bMeetsMinimumSamples
                ? TEXT("evidence active")
                : TEXT("insufficient samples"),
            ActiveEffectiveness.UtilityModifier * 100.0f
        ));
    }
    else
    {
        Lines.Add(TEXT("Outcome Secondary Modifier: no matching counter samples."));
    }

    for (const FAdaptiveCounterEffectivenessSummary& Effectiveness :
        Snapshot.LastRoundCounterEffectiveness)
    {
        Lines.Add(FString::Printf(
            TEXT("Round %d Effectiveness: %s vs predicted %s = %d/%d worked (smoothed %d%%, utility %+.1f%%, %s)"),
            Effectiveness.RoundNumber,
            *GetEnemyActionName(Effectiveness.CounterAction),
            *GetPlayerActionName(Effectiveness.PredictedPlayerAction),
            Effectiveness.SuccessfulCount,
            Effectiveness.SampleCount,
            FMath::RoundToInt(
                Effectiveness.SmoothedEffectiveness * 100.0f
            ),
            Effectiveness.UtilityModifier * 100.0f,
            Effectiveness.bMeetsMinimumSamples
                ? TEXT("eligible")
                : TEXT("insufficient")
        ));
    }

    if (Snapshot.LastCompletedRound > 0
        && Snapshot.LastCompletedRound <= SafeTotalRounds)
    {
        Lines.Add(FString::Printf(
            TEXT("ROUND %d OBSERVED PATTERN:"),
            Snapshot.LastCompletedRound
        ));
        Lines.Add(FString::Printf(
            TEXT("  %s"),
            *FormatConditionalPattern(
                Snapshot.LastRoundObservedPattern
            )
        ));
    }

    return Lines;
}

FString FAdaptiveDebugTelemetryFormatter::FormatConditionalPattern(
    const FAdaptiveConditionalPattern& Pattern
)
{
    if (!Pattern.IsValid())
    {
        return TEXT("No conditional player-action pattern observed.");
    }

    FString Result = FString::Printf(
        TEXT("After Enemy %s: "),
        *GetEnemyActionName(Pattern.PreviousEnemyAction)
    );
    for (int32 Index = 0; Index < Pattern.ActionPercentages.Num(); ++Index)
    {
        const FAdaptiveActionPercentage& Entry =
            Pattern.ActionPercentages[Index];
        if (Index > 0)
        {
            Result += TEXT("  ");
        }
        Result += FString::Printf(
            TEXT("%s: %d%%"),
            *GetPlayerActionName(Entry.Action),
            Entry.Percentage
        );
    }
    Result += FString::Printf(
        TEXT(" (%d samples)"),
        Pattern.TotalSampleCount
    );
    return Result;
}

FString FAdaptiveDebugTelemetryFormatter::GetPlayerActionName(
    const EPlayerCombatAction Action
)
{
    switch (Action)
    {
    case EPlayerCombatAction::LightAttack:
        return TEXT("Light Attack");
    case EPlayerCombatAction::HeavyAttack:
        return TEXT("Heavy Attack");
    case EPlayerCombatAction::DodgeLeft:
        return TEXT("Dodge Left");
    case EPlayerCombatAction::DodgeRight:
        return TEXT("Dodge Right");
    case EPlayerCombatAction::DodgeBackward:
        return TEXT("Dodge Backward");
    case EPlayerCombatAction::Block:
        return TEXT("Block");
    case EPlayerCombatAction::Heal:
        return TEXT("Heal");
    default:
        return TEXT("None");
    }
}

FString FAdaptiveDebugTelemetryFormatter::GetEnemyActionName(
    const EEnemyCombatAction Action
)
{
    switch (Action)
    {
    case EEnemyCombatAction::MoveTowardPlayer:
        return TEXT("Move Toward Player");
    case EEnemyCombatAction::MoveAwayFromPlayer:
        return TEXT("Move Away From Player");
    case EEnemyCombatAction::StrafeLeft:
        return TEXT("Strafe Left");
    case EEnemyCombatAction::StrafeRight:
        return TEXT("Strafe Right");
    case EEnemyCombatAction::LightAttack:
        return TEXT("Light Attack");
    case EEnemyCombatAction::HeavyAttack:
        return TEXT("Heavy Attack");
    case EEnemyCombatAction::ProjectileAttack:
        return TEXT("Projectile Attack");
    case EEnemyCombatAction::DashAttack:
        return TEXT("Dash Attack");
    case EEnemyCombatAction::Block:
        return TEXT("Block");
    case EEnemyCombatAction::Dodge:
        return TEXT("Dodge");
    case EEnemyCombatAction::InterruptHeal:
        return TEXT("Interrupt Heal");
    default:
        return TEXT("None");
    }
}
