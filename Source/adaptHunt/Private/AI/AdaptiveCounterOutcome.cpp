#include "AI/AdaptiveCounterOutcome.h"

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

const EEnemyCombatAction CounterActions[] = {
    EEnemyCombatAction::LightAttack,
    EEnemyCombatAction::HeavyAttack,
    EEnemyCombatAction::ProjectileAttack,
    EEnemyCombatAction::DashAttack,
    EEnemyCombatAction::Block,
    EEnemyCombatAction::Dodge,
    EEnemyCombatAction::InterruptHeal
};

bool IsKnownOutcome(const EAdaptiveCounterOutcomeResult Result)
{
    return Result == EAdaptiveCounterOutcomeResult::Hit
        || Result == EAdaptiveCounterOutcomeResult::Missed
        || Result == EAdaptiveCounterOutcomeResult::Blocked
        || Result == EAdaptiveCounterOutcomeResult::Dodged
        || Result == EAdaptiveCounterOutcomeResult::InterruptedHeal
        || Result
            == EAdaptiveCounterOutcomeResult::InvalidatedBeforeActiveFrame;
}

bool HasUsablePrediction(const FPredictionResult& Prediction)
{
    return Prediction.bHasPrediction
        && AdaptiveCombat::IsTrackablePlayerAction(
            Prediction.PredictedAction
        )
        && FMath::IsFinite(Prediction.Confidence)
        && Prediction.Confidence >= 0.0f
        && Prediction.Confidence <= 1.0f
        && Prediction.SupportingSampleCount >= 0;
}

FAdaptiveCounterEffectivenessSummary BuildSummary(
    const TArray<FAdaptiveCounterOutcomeRecord>& Records,
    const EPlayerCombatAction PredictedPlayerAction,
    const EEnemyCombatAction CounterAction,
    const int32 ExactRound,
    const int32 EvaluatedThroughRound,
    const FAdaptiveCounterEffectivenessTuning& Tuning
)
{
    const FAdaptiveCounterEffectivenessTuning SafeTuning =
        Tuning.GetSanitized();
    FAdaptiveCounterEffectivenessSummary Summary;
    Summary.RoundNumber = FMath::Max(0, ExactRound);
    Summary.EvaluatedThroughRound = FMath::Max(
        Summary.RoundNumber,
        EvaluatedThroughRound
    );
    Summary.PredictedPlayerAction = PredictedPlayerAction;
    Summary.CounterAction = CounterAction;

    if (!AdaptiveCombat::IsTrackablePlayerAction(PredictedPlayerAction)
        || !AdaptiveCombat::IsUtilityEnemyAction(CounterAction))
    {
        return Summary;
    }

    for (const FAdaptiveCounterOutcomeRecord& Record : Records)
    {
        if (!Record.IsAdaptiveCounterAttempt()
            || Record.Prediction.PredictedAction
                != PredictedPlayerAction
            || Record.Action != CounterAction
            || (Summary.RoundNumber > 0
                && Record.RoundNumber != Summary.RoundNumber)
            || (Summary.RoundNumber == 0
                && Summary.EvaluatedThroughRound > 0
                && Record.RoundNumber > Summary.EvaluatedThroughRound))
        {
            continue;
        }

        ++Summary.SampleCount;
        if (Record.WasSuccessfulCounter())
        {
            ++Summary.SuccessfulCount;
        }
    }

    const float Smoothing = SafeTuning.SmoothingSampleWeight;
    const float Denominator = static_cast<float>(Summary.SampleCount)
        + Smoothing * 2.0f;
    Summary.SmoothedEffectiveness = Denominator > 0.0f
        ? (static_cast<float>(Summary.SuccessfulCount) + Smoothing)
            / Denominator
        : 0.5f;
    Summary.SmoothedEffectiveness = FMath::Clamp(
        Summary.SmoothedEffectiveness,
        0.0f,
        1.0f
    );
    Summary.bMeetsMinimumSamples =
        Summary.SampleCount >= SafeTuning.MinimumSamples;
    if (Summary.bMeetsMinimumSamples)
    {
        Summary.UtilityModifier = FMath::Clamp(
            (Summary.SmoothedEffectiveness - 0.5f) * 2.0f
                * SafeTuning.MaximumUtilityAdjustment,
            -SafeTuning.MaximumUtilityAdjustment,
            SafeTuning.MaximumUtilityAdjustment
        );
    }
    return Summary;
}
}

bool FEnemyCombatActionOutcome::IsValid() const
{
    return ActionId > 0 && AdaptiveCombat::IsUtilityEnemyAction(Action)
        && IsKnownOutcome(Result);
}

bool FAdaptiveCounterOutcomeRecord::IsValid() const
{
    if (ActionId <= 0 || RoundNumber <= 0
        || ContextSnapshot.RoundNumber != RoundNumber
        || !AdaptiveCombat::IsUtilityEnemyAction(Action)
        || !IsKnownOutcome(Result)
        || !FMath::IsFinite(Prediction.Confidence))
    {
        return false;
    }

    if (Prediction.bHasPrediction && !HasUsablePrediction(Prediction))
    {
        return false;
    }
    if ((bPredictionApplied || bAdaptiveCounterOpportunity)
        && !HasUsablePrediction(Prediction))
    {
        return false;
    }
    if (bAdaptiveCounterOpportunity && !bPredictionApplied)
    {
        return false;
    }
    return ProfilePreferredCounterAction == EEnemyCombatAction::None
        || AdaptiveCombat::IsUtilityEnemyAction(
            ProfilePreferredCounterAction
        );
}

bool FAdaptiveCounterOutcomeRecord::IsAdaptiveCounterAttempt() const
{
    return IsValid() && bPredictionApplied
        && bAdaptiveCounterOpportunity
        && HasUsablePrediction(Prediction);
}

bool FAdaptiveCounterOutcomeRecord::WasSuccessfulCounter() const
{
    if (!IsAdaptiveCounterAttempt())
    {
        return false;
    }

    if (Action == EEnemyCombatAction::Block)
    {
        return Result == EAdaptiveCounterOutcomeResult::Blocked;
    }
    if (Action == EEnemyCombatAction::Dodge)
    {
        return Result == EAdaptiveCounterOutcomeResult::Dodged;
    }
    if (Action == EEnemyCombatAction::InterruptHeal)
    {
        return Result == EAdaptiveCounterOutcomeResult::InterruptedHeal;
    }
    return Result == EAdaptiveCounterOutcomeResult::Hit;
}

FAdaptiveCounterEffectivenessTuning::
    FAdaptiveCounterEffectivenessTuning()
    : MinimumSamples(3)
    , MaximumUtilityAdjustment(0.08f)
    , SmoothingSampleWeight(1.0f)
{
}

FAdaptiveCounterEffectivenessTuning
FAdaptiveCounterEffectivenessTuning::GetSanitized() const
{
    FAdaptiveCounterEffectivenessTuning Result;
    Result.MinimumSamples = FMath::Clamp(MinimumSamples, 2, 12);
    Result.MaximumUtilityAdjustment =
        FMath::IsFinite(MaximumUtilityAdjustment)
        ? FMath::Clamp(MaximumUtilityAdjustment, 0.0f, 0.10f)
        : Result.MaximumUtilityAdjustment;
    Result.SmoothingSampleWeight = FMath::IsFinite(SmoothingSampleWeight)
        ? FMath::Clamp(SmoothingSampleWeight, 0.0f, 4.0f)
        : Result.SmoothingSampleWeight;
    return Result;
}

bool FAdaptiveCounterEffectivenessSummary::HasSamples() const
{
    return SampleCount > 0
        && SuccessfulCount >= 0
        && SuccessfulCount <= SampleCount
        && AdaptiveCombat::IsTrackablePlayerAction(
            PredictedPlayerAction
        )
        && AdaptiveCombat::IsUtilityEnemyAction(CounterAction);
}

bool FAdaptiveCounterOutcomeHistory::Record(
    const FAdaptiveCounterOutcomeRecord& Record
)
{
    if (!Record.IsValid() || RecordedActionIds.Contains(Record.ActionId))
    {
        return false;
    }
    RecordedActionIds.Add(Record.ActionId);
    Records.Add(Record);
    return true;
}

void FAdaptiveCounterOutcomeHistory::Reset()
{
    Records.Reset();
    RecordedActionIds.Reset();
}

int32 FAdaptiveCounterOutcomeHistory::Num() const
{
    return Records.Num();
}

const TArray<FAdaptiveCounterOutcomeRecord>&
FAdaptiveCounterOutcomeHistory::GetRecords() const
{
    return Records;
}

const FAdaptiveCounterOutcomeRecord*
FAdaptiveCounterOutcomeHistory::GetLastRecord() const
{
    return Records.Num() > 0 ? &Records.Last() : nullptr;
}

TArray<FAdaptiveCounterEffectivenessSummary>
FAdaptiveCounterEffectivenessPolicy::AnalyzeRound(
    const TArray<FAdaptiveCounterOutcomeRecord>& Records,
    const int32 RoundNumber,
    const FAdaptiveCounterEffectivenessTuning& Tuning
)
{
    TArray<FAdaptiveCounterEffectivenessSummary> Results;
    if (RoundNumber <= 0)
    {
        return Results;
    }

    for (const EPlayerCombatAction PlayerAction : PlayerActions)
    {
        for (const EEnemyCombatAction CounterAction : CounterActions)
        {
            FAdaptiveCounterEffectivenessSummary Summary = BuildSummary(
                Records,
                PlayerAction,
                CounterAction,
                RoundNumber,
                RoundNumber,
                Tuning
            );
            if (Summary.HasSamples())
            {
                Results.Add(Summary);
            }
        }
    }
    return Results;
}

FAdaptiveCounterEffectivenessSummary
FAdaptiveCounterEffectivenessPolicy::AnalyzeForCounter(
    const TArray<FAdaptiveCounterOutcomeRecord>& Records,
    const EPlayerCombatAction PredictedPlayerAction,
    const EEnemyCombatAction CounterAction,
    const int32 EvaluatedThroughRound,
    const FAdaptiveCounterEffectivenessTuning& Tuning
)
{
    return BuildSummary(
        Records,
        PredictedPlayerAction,
        CounterAction,
        0,
        FMath::Max(0, EvaluatedThroughRound),
        Tuning
    );
}

float FAdaptiveCounterEffectivenessPolicy::GetUtilityModifier(
    const FAdaptiveCounterEffectivenessSummary& Summary,
    const EEnemyCombatAction Action,
    const bool bAdaptiveCounterOpportunity,
    const FAdaptiveCounterEffectivenessTuning& Tuning
)
{
    if (!bAdaptiveCounterOpportunity || !Summary.HasSamples()
        || !Summary.bMeetsMinimumSamples
        || Action != Summary.CounterAction)
    {
        return 0.0f;
    }
    const float Maximum =
        Tuning.GetSanitized().MaximumUtilityAdjustment;
    return FMath::IsFinite(Summary.UtilityModifier)
        ? FMath::Clamp(Summary.UtilityModifier, -Maximum, Maximum)
        : 0.0f;
}

FString GetAdaptiveCounterOutcomeName(
    const EAdaptiveCounterOutcomeResult Result
)
{
    switch (Result)
    {
    case EAdaptiveCounterOutcomeResult::Hit:
        return TEXT("Hit");
    case EAdaptiveCounterOutcomeResult::Missed:
        return TEXT("Missed");
    case EAdaptiveCounterOutcomeResult::Blocked:
        return TEXT("Blocked");
    case EAdaptiveCounterOutcomeResult::Dodged:
        return TEXT("Dodged");
    case EAdaptiveCounterOutcomeResult::InterruptedHeal:
        return TEXT("Interrupted Heal");
    case EAdaptiveCounterOutcomeResult::InvalidatedBeforeActiveFrame:
        return TEXT("Invalidated Before Active Frame");
    default:
        return TEXT("None");
    }
}
