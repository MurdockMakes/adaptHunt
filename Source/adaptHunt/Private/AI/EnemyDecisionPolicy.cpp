#include "AI/EnemyDecisionPolicy.h"

#include "AI/EnemyTacticalPolicy.h"

namespace
{
float SanitizeFiniteRange(
    const float Value,
    const float Minimum,
    const float Maximum,
    const float Fallback
)
{
    return FMath::IsFinite(Value)
        ? FMath::Clamp(Value, Minimum, Maximum)
        : Fallback;
}

bool IsUsableScore(
    const FEnemyActionScore& Score,
    const TSet<EEnemyCombatAction>& ExcludedActions
)
{
    return Score.bAvailable
        && AdaptiveCombat::IsUtilityEnemyAction(Score.Action)
        && !ExcludedActions.Contains(Score.Action)
        && FMath::IsFinite(
            FEnemyActionSelectionPolicy::GetSelectionScore(Score)
        );
}

bool IsFailureResult(
    const EEnemyCombatAction Action,
    const EAdaptiveCounterOutcomeResult Result
)
{
    if (Result == EAdaptiveCounterOutcomeResult::Missed
        || Result
            == EAdaptiveCounterOutcomeResult::InvalidatedBeforeActiveFrame)
    {
        return true;
    }
    if (Result == EAdaptiveCounterOutcomeResult::Blocked)
    {
        return Action != EEnemyCombatAction::Block;
    }
    if (Result == EAdaptiveCounterOutcomeResult::Dodged)
    {
        return Action != EEnemyCombatAction::Dodge;
    }
    return false;
}

bool IsSuccessResult(
    const EEnemyCombatAction Action,
    const EAdaptiveCounterOutcomeResult Result
)
{
    if (Result == EAdaptiveCounterOutcomeResult::Hit)
    {
        return true;
    }
    if (Result == EAdaptiveCounterOutcomeResult::InterruptedHeal)
    {
        return Action == EEnemyCombatAction::InterruptHeal;
    }
    if (Result == EAdaptiveCounterOutcomeResult::Blocked)
    {
        return Action == EEnemyCombatAction::Block;
    }
    if (Result == EAdaptiveCounterOutcomeResult::Dodged)
    {
        return Action == EEnemyCombatAction::Dodge;
    }
    return false;
}

EPlayerCombatAction ToPlayerAction(const EEnemyUrgentThreat Threat)
{
    switch (Threat)
    {
    case EEnemyUrgentThreat::LightAttack:
        return EPlayerCombatAction::LightAttack;
    case EEnemyUrgentThreat::HeavyAttack:
        return EPlayerCombatAction::HeavyAttack;
    case EEnemyUrgentThreat::Heal:
        return EPlayerCombatAction::Heal;
    default:
        return EPlayerCombatAction::None;
    }
}
}


FEnemyActionSelectionTuning::FEnemyActionSelectionTuning()
    : NearBestScoreWindow(0.16f)
    , SelectionTemperature(0.08f)
{
}

FEnemyActionSelectionTuning
FEnemyActionSelectionTuning::GetSanitized() const
{
    FEnemyActionSelectionTuning Result;
    Result.NearBestScoreWindow = SanitizeFiniteRange(
        NearBestScoreWindow,
        0.0f,
        0.5f,
        Result.NearBestScoreWindow
    );
    Result.SelectionTemperature = SanitizeFiniteRange(
        SelectionTemperature,
        0.01f,
        0.5f,
        Result.SelectionTemperature
    );
    return Result;
}


FEnemyActionRepetitionTuning::FEnemyActionRepetitionTuning()
    : HistorySize(5)
    , ImmediateRepeatPenalty(0.22f)
    , AccumulatedRepeatPenalty(0.08f)
    , OffensiveFamilyPenalty(0.015f)
    , MaximumRepetitionAdjustment(0.42f)
{
}

FEnemyActionRepetitionTuning
FEnemyActionRepetitionTuning::GetSanitized() const
{
    FEnemyActionRepetitionTuning Result;
    Result.HistorySize = FMath::Clamp(HistorySize, 1, 8);
    Result.ImmediateRepeatPenalty = SanitizeFiniteRange(
        ImmediateRepeatPenalty,
        0.0f,
        0.5f,
        Result.ImmediateRepeatPenalty
    );
    Result.AccumulatedRepeatPenalty = SanitizeFiniteRange(
        AccumulatedRepeatPenalty,
        0.0f,
        0.25f,
        Result.AccumulatedRepeatPenalty
    );
    Result.OffensiveFamilyPenalty = SanitizeFiniteRange(
        OffensiveFamilyPenalty,
        0.0f,
        0.1f,
        Result.OffensiveFamilyPenalty
    );
    Result.MaximumRepetitionAdjustment = SanitizeFiniteRange(
        MaximumRepetitionAdjustment,
        0.0f,
        0.6f,
        Result.MaximumRepetitionAdjustment
    );
    return Result;
}


FEnemyOffenseDefenseBalanceTuning::FEnemyOffenseDefenseBalanceTuning()
    : HistorySize(6)
    , MinimumHistoryEntries(3)
    , TargetDefensiveRatio(0.45f)
    , RatioDeadZone(0.08f)
    , MaximumBalanceAdjustment(0.35f)
{
}

FEnemyOffenseDefenseBalanceTuning
FEnemyOffenseDefenseBalanceTuning::GetSanitized() const
{
    FEnemyOffenseDefenseBalanceTuning Result;
    Result.HistorySize = FMath::Clamp(HistorySize, 1, 8);
    Result.MinimumHistoryEntries = FMath::Clamp(
        MinimumHistoryEntries,
        1,
        Result.HistorySize
    );
    Result.TargetDefensiveRatio = SanitizeFiniteRange(
        TargetDefensiveRatio,
        0.0f,
        1.0f,
        Result.TargetDefensiveRatio
    );
    Result.RatioDeadZone = SanitizeFiniteRange(
        RatioDeadZone,
        0.0f,
        0.25f,
        Result.RatioDeadZone
    );
    Result.MaximumBalanceAdjustment = SanitizeFiniteRange(
        MaximumBalanceAdjustment,
        0.0f,
        0.5f,
        Result.MaximumBalanceAdjustment
    );
    return Result;
}


FEnemyRecentOutcomeTuning::FEnemyRecentOutcomeTuning()
    : MemorySize(6)
    , FailurePenalty(0.08f)
    , MaximumOutcomeAdjustment(0.24f)
    , SuccessBonus(0.015f)
{
}

FEnemyRecentOutcomeTuning FEnemyRecentOutcomeTuning::GetSanitized() const
{
    FEnemyRecentOutcomeTuning Result;
    Result.MemorySize = FMath::Clamp(MemorySize, 1, 12);
    Result.FailurePenalty = SanitizeFiniteRange(
        FailurePenalty,
        0.0f,
        0.2f,
        Result.FailurePenalty
    );
    Result.MaximumOutcomeAdjustment = SanitizeFiniteRange(
        MaximumOutcomeAdjustment,
        0.0f,
        0.4f,
        Result.MaximumOutcomeAdjustment
    );
    Result.SuccessBonus = SanitizeFiniteRange(
        SuccessBonus,
        0.0f,
        0.05f,
        Result.SuccessBonus
    );
    return Result;
}


FEnemyUrgentDecisionTuning::FEnemyUrgentDecisionTuning()
    : ThreatReactionDelay(0.18f)
    , UrgentDecisionCooldown(0.8f)
{
}

FEnemyUrgentDecisionTuning FEnemyUrgentDecisionTuning::GetSanitized() const
{
    FEnemyUrgentDecisionTuning Result;
    Result.ThreatReactionDelay = SanitizeFiniteRange(
        ThreatReactionDelay,
        0.15f,
        0.25f,
        Result.ThreatReactionDelay
    );
    Result.UrgentDecisionCooldown = SanitizeFiniteRange(
        UrgentDecisionCooldown,
        0.1f,
        3.0f,
        Result.UrgentDecisionCooldown
    );
    return Result;
}


bool FEnemyActionSelectionResult::IsValid() const
{
    return AdaptiveCombat::IsUtilityEnemyAction(Action)
        && FMath::IsFinite(BestScore)
        && FMath::IsFinite(SelectedScore)
        && CandidateActions.Contains(Action);
}

float FEnemyActionSelectionPolicy::GetSelectionScore(
    const FEnemyActionScore& Score
)
{
    // Older diagnostics/tests may construct scores without the new raw field.
    return FMath::IsFinite(Score.UnclampedScore)
        && Score.UnclampedScore > -MAX_flt * 0.5f
        ? Score.UnclampedScore
        : Score.Score;
}

FEnemyActionSelectionResult FEnemyActionSelectionPolicy::Select(
    const TArray<FEnemyActionScore>& Scores,
    const float RandomRoll,
    const FEnemyActionSelectionTuning& Tuning
)
{
    const TSet<EEnemyCombatAction> EmptyExclusions;
    return Select(Scores, EmptyExclusions, RandomRoll, Tuning);
}

FEnemyActionSelectionResult FEnemyActionSelectionPolicy::Select(
    const TArray<FEnemyActionScore>& Scores,
    const TSet<EEnemyCombatAction>& ExcludedActions,
    const float RandomRoll,
    const FEnemyActionSelectionTuning& Tuning
)
{
    FEnemyActionSelectionResult Result;
    float BestScore = -MAX_flt;
    for (const FEnemyActionScore& Score : Scores)
    {
        if (IsUsableScore(Score, ExcludedActions))
        {
            BestScore = FMath::Max(BestScore, GetSelectionScore(Score));
        }
    }
    if (BestScore == -MAX_flt)
    {
        return Result;
    }

    const FEnemyActionSelectionTuning SafeTuning = Tuning.GetSanitized();
    Result.BestScore = BestScore;
    TArray<float> Weights;
    float TotalWeight = 0.0f;
    for (const FEnemyActionScore& Score : Scores)
    {
        if (!IsUsableScore(Score, ExcludedActions))
        {
            continue;
        }
        const float Utility = GetSelectionScore(Score);
        if (Utility + SafeTuning.NearBestScoreWindow
            < BestScore - KINDA_SMALL_NUMBER)
        {
            continue;
        }

        Result.CandidateActions.Add(Score.Action);
        const float Weight = FMath::Exp(
            (Utility - BestScore) / SafeTuning.SelectionTemperature
        );
        Weights.Add(Weight);
        TotalWeight += Weight;
    }

    if (Result.CandidateActions.IsEmpty()
        || !FMath::IsFinite(TotalWeight) || TotalWeight <= 0.0f)
    {
        Result.Action = SelectBest(Scores);
        Result.SelectedScore = BestScore;
        if (Result.Action != EEnemyCombatAction::None)
        {
            Result.CandidateActions.AddUnique(Result.Action);
        }
        return Result;
    }

    const float SafeRoll = FMath::IsFinite(RandomRoll)
        ? FMath::Clamp(RandomRoll, 0.0f, 1.0f - KINDA_SMALL_NUMBER)
        : 0.5f;
    const float Threshold = SafeRoll * TotalWeight;
    float AccumulatedWeight = 0.0f;
    int32 SelectedIndex = Result.CandidateActions.Num() - 1;
    for (int32 Index = 0; Index < Weights.Num(); ++Index)
    {
        AccumulatedWeight += Weights[Index];
        if (Threshold < AccumulatedWeight)
        {
            SelectedIndex = Index;
            break;
        }
    }

    Result.Action = Result.CandidateActions[SelectedIndex];
    for (const FEnemyActionScore& Score : Scores)
    {
        if (Score.Action == Result.Action)
        {
            Result.SelectedScore = GetSelectionScore(Score);
            break;
        }
    }
    return Result;
}

EEnemyCombatAction FEnemyActionSelectionPolicy::SelectBest(
    const TArray<FEnemyActionScore>& Scores
)
{
    EEnemyCombatAction BestAction = EEnemyCombatAction::None;
    float BestScore = -MAX_flt;
    for (const FEnemyActionScore& Score : Scores)
    {
        const float Utility = GetSelectionScore(Score);
        if (Score.bAvailable
            && AdaptiveCombat::IsUtilityEnemyAction(Score.Action)
            && FMath::IsFinite(Utility) && Utility > BestScore)
        {
            BestAction = Score.Action;
            BestScore = Utility;
        }
    }
    return BestAction;
}


void FEnemyCommittedActionHistory::Record(
    const EEnemyCombatAction Action,
    const int32 MaximumEntries
)
{
    if (!AdaptiveCombat::IsUtilityEnemyAction(Action))
    {
        return;
    }
    const int32 SafeMaximum = FMath::Clamp(MaximumEntries, 1, 8);
    Actions.Add(Action);
    if (Actions.Num() > SafeMaximum)
    {
        Actions.RemoveAt(0, Actions.Num() - SafeMaximum, EAllowShrinking::No);
    }
}

void FEnemyCommittedActionHistory::Reset()
{
    Actions.Reset();
}

int32 FEnemyCommittedActionHistory::Num() const
{
    return Actions.Num();
}

const TArray<EEnemyCombatAction>&
FEnemyCommittedActionHistory::GetActions() const
{
    return Actions;
}

FEnemyRepetitionModifier FEnemyActionRepetitionPolicy::Evaluate(
    const EEnemyCombatAction Action,
    const FEnemyCommittedActionHistory& History,
    const FEnemyActionRepetitionTuning& Tuning
)
{
    FEnemyRepetitionModifier Result;
    if (!AdaptiveCombat::IsUtilityEnemyAction(Action))
    {
        return Result;
    }

    const FEnemyActionRepetitionTuning SafeTuning = Tuning.GetSanitized();
    int32 SameActionCount = 0;
    int32 OffensiveCount = 0;
    const TArray<EEnemyCombatAction>& Actions = History.GetActions();
    const int32 StartIndex = FMath::Max(
        0,
        Actions.Num() - SafeTuning.HistorySize
    );
    for (int32 Index = StartIndex; Index < Actions.Num(); ++Index)
    {
        const EEnemyCombatAction PreviousAction = Actions[Index];
        SameActionCount += PreviousAction == Action ? 1 : 0;
        OffensiveCount += FEnemyTacticalPolicy::IsOffensiveAction(
            PreviousAction
        ) ? 1 : 0;
    }

    if (!Actions.IsEmpty() && Actions.Last() == Action)
    {
        Result.Immediate = -SafeTuning.ImmediateRepeatPenalty;
    }
    Result.Accumulated = -SafeTuning.AccumulatedRepeatPenalty
        * static_cast<float>(FMath::Max(0, SameActionCount - 1));
    if (FEnemyTacticalPolicy::IsOffensiveAction(Action))
    {
        Result.OffensiveFamily = -SafeTuning.OffensiveFamilyPenalty
            * static_cast<float>(FMath::Max(0, OffensiveCount - 2));
    }
    Result.Total = FMath::Clamp(
        Result.Immediate + Result.Accumulated + Result.OffensiveFamily,
        -SafeTuning.MaximumRepetitionAdjustment,
        0.0f
    );
    return Result;
}


FEnemyOffenseDefenseBalanceModifier
FEnemyOffenseDefenseBalancePolicy::Evaluate(
    const EEnemyCombatAction Action,
    const FEnemyCommittedActionHistory& History,
    const FEnemyOffenseDefenseBalanceTuning& Tuning
)
{
    FEnemyOffenseDefenseBalanceModifier Result;
    const bool bOffensive = FEnemyTacticalPolicy::IsOffensiveAction(Action);
    const bool bDefensive = FEnemyTacticalPolicy::IsDefensiveAction(Action);
    if (!bOffensive && !bDefensive)
    {
        return Result;
    }

    const FEnemyOffenseDefenseBalanceTuning SafeTuning =
        Tuning.GetSanitized();
    const TArray<EEnemyCombatAction>& Actions = History.GetActions();
    const int32 StartIndex = FMath::Max(
        0,
        Actions.Num() - SafeTuning.HistorySize
    );
    int32 DefensiveCount = 0;
    for (int32 Index = StartIndex; Index < Actions.Num(); ++Index)
    {
        const EEnemyCombatAction PreviousAction = Actions[Index];
        if (FEnemyTacticalPolicy::IsDefensiveAction(PreviousAction))
        {
            ++DefensiveCount;
            ++Result.EvidenceCount;
        }
        else if (FEnemyTacticalPolicy::IsOffensiveAction(PreviousAction))
        {
            ++Result.EvidenceCount;
        }
    }

    if (Result.EvidenceCount < SafeTuning.MinimumHistoryEntries)
    {
        return Result;
    }

    Result.DefensiveRatio = static_cast<float>(DefensiveCount)
        / static_cast<float>(Result.EvidenceCount);
    const float DefensiveDeficit = SafeTuning.TargetDefensiveRatio
        - Result.DefensiveRatio;
    const float Magnitude = FMath::Clamp(
        FMath::Abs(DefensiveDeficit) - SafeTuning.RatioDeadZone,
        0.0f,
        SafeTuning.MaximumBalanceAdjustment
    );
    if (FMath::IsNearlyZero(Magnitude))
    {
        return Result;
    }

    const bool bNeedsMoreDefense = DefensiveDeficit > 0.0f;
    Result.Total = (bDefensive == bNeedsMoreDefense)
        ? Magnitude
        : -Magnitude;
    return Result;
}


bool FEnemyRecentOutcomeMemory::Record(
    const FEnemyCombatActionOutcome& Outcome,
    const int32 MaximumEntries
)
{
    if (!Outcome.IsValid() || Outcome.ActionId <= HighestRecordedActionId)
    {
        return false;
    }

    HighestRecordedActionId = Outcome.ActionId;
    FEnemyRecentOutcomeRecord Record;
    Record.ActionId = Outcome.ActionId;
    Record.Action = Outcome.Action;
    Record.Result = Outcome.Result;
    Records.Add(Record);

    const int32 SafeMaximum = FMath::Clamp(MaximumEntries, 1, 12);
    if (Records.Num() > SafeMaximum)
    {
        Records.RemoveAt(0, Records.Num() - SafeMaximum, EAllowShrinking::No);
    }
    return true;
}

void FEnemyRecentOutcomeMemory::Reset()
{
    Records.Reset();
    HighestRecordedActionId = 0;
}

int32 FEnemyRecentOutcomeMemory::Num() const
{
    return Records.Num();
}

const TArray<FEnemyRecentOutcomeRecord>&
FEnemyRecentOutcomeMemory::GetRecords() const
{
    return Records;
}

FEnemyRecentOutcomeModifier FEnemyRecentOutcomePolicy::Evaluate(
    const EEnemyCombatAction Action,
    const FEnemyRecentOutcomeMemory& Memory,
    const FEnemyRecentOutcomeTuning& Tuning
)
{
    FEnemyRecentOutcomeModifier Result;
    if (!AdaptiveCombat::IsUtilityEnemyAction(Action))
    {
        return Result;
    }

    for (const FEnemyRecentOutcomeRecord& Record : Memory.GetRecords())
    {
        if (Record.Action != Action)
        {
            continue;
        }
        Result.FailureCount += IsFailureResult(Action, Record.Result) ? 1 : 0;
        Result.SuccessCount += IsSuccessResult(Action, Record.Result) ? 1 : 0;
    }

    const FEnemyRecentOutcomeTuning SafeTuning = Tuning.GetSanitized();
    const float FailureUnits = Result.FailureCount > 0
        ? 0.5f + static_cast<float>(Result.FailureCount - 1)
        : 0.0f;
    Result.Total = FMath::Clamp(
        -FailureUnits * SafeTuning.FailurePenalty
            + static_cast<float>(Result.SuccessCount)
                * SafeTuning.SuccessBonus,
        -SafeTuning.MaximumOutcomeAdjustment,
        SafeTuning.MaximumOutcomeAdjustment
    );
    return Result;
}


EEnemyUrgentThreat FEnemyUrgentDecisionState::ClassifyWindup(
    const EPlayerCombatAction Action,
    const ECombatActionPhase Phase
)
{
    if (Phase != ECombatActionPhase::Windup)
    {
        return EEnemyUrgentThreat::None;
    }
    switch (Action)
    {
    case EPlayerCombatAction::LightAttack:
        return EEnemyUrgentThreat::LightAttack;
    case EPlayerCombatAction::HeavyAttack:
        return EEnemyUrgentThreat::HeavyAttack;
    case EPlayerCombatAction::Heal:
        return EEnemyUrgentThreat::Heal;
    default:
        return EEnemyUrgentThreat::None;
    }
}

bool FEnemyUrgentDecisionState::IsThreatStillCommitted(
    const EEnemyUrgentThreat Threat,
    const EPlayerCombatAction Action,
    const ECombatActionPhase Phase
)
{
    const bool bObservableCommittedPhase =
        Phase == ECombatActionPhase::Windup
        || Phase == ECombatActionPhase::Active
        || Phase == ECombatActionPhase::Recovery;
    return Threat != EEnemyUrgentThreat::None
        && Action == ToPlayerAction(Threat)
        && bObservableCommittedPhase;
}

bool FEnemyUrgentDecisionState::ObservePhaseEdge(
    const EPlayerCombatAction Action,
    const ECombatActionPhase Phase,
    const bool bEnemyCanDecide,
    const double CurrentTime,
    const FEnemyUrgentDecisionTuning& Tuning
)
{
    const EEnemyUrgentThreat Threat = ClassifyWindup(Action, Phase);
    const bool bEdge = Threat != EEnemyUrgentThreat::None
        && Threat != LastObservedWindup;
    LastObservedWindup = Threat;
    if (Threat == EEnemyUrgentThreat::None)
    {
        if (!IsThreatStillCommitted(PendingThreat, Action, Phase))
        {
            PendingThreat = EEnemyUrgentThreat::None;
        }
        return false;
    }
    if (!bEdge || !bEnemyCanDecide || !FMath::IsFinite(CurrentTime)
        || CurrentTime < NextAllowedTime)
    {
        return false;
    }

    PendingThreat = Threat;
    ReadyTime = CurrentTime
        + Tuning.GetSanitized().ThreatReactionDelay;
    return true;
}

bool FEnemyUrgentDecisionState::ConsumeIfReady(
    const EPlayerCombatAction Action,
    const ECombatActionPhase Phase,
    const bool bEnemyCanDecide,
    const double CurrentTime,
    const FEnemyUrgentDecisionTuning& Tuning
)
{
    if (!bEnemyCanDecide
        || !IsThreatStillCommitted(PendingThreat, Action, Phase))
    {
        PendingThreat = EEnemyUrgentThreat::None;
        return false;
    }
    if (!FMath::IsFinite(CurrentTime) || CurrentTime < ReadyTime
        || CurrentTime < NextAllowedTime)
    {
        return false;
    }

    PendingThreat = EEnemyUrgentThreat::None;
    NextAllowedTime = CurrentTime
        + Tuning.GetSanitized().UrgentDecisionCooldown;
    return true;
}

void FEnemyUrgentDecisionState::Reset()
{
    LastObservedWindup = EEnemyUrgentThreat::None;
    PendingThreat = EEnemyUrgentThreat::None;
    ReadyTime = 0.0;
    NextAllowedTime = 0.0;
}

bool FEnemyUrgentDecisionState::IsPending() const
{
    return PendingThreat != EEnemyUrgentThreat::None;
}

double FEnemyUrgentDecisionState::GetReadyTime() const
{
    return ReadyTime;
}
