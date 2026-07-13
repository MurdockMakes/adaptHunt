#include "AI/AdaptiveTacticalProfile.h"

#include "AI/EnemyTacticalPolicy.h"

namespace
{
const EPlayerCombatAction ProfileActions[] = {
    EPlayerCombatAction::LightAttack,
    EPlayerCombatAction::HeavyAttack,
    EPlayerCombatAction::DodgeLeft,
    EPlayerCombatAction::DodgeRight,
    EPlayerCombatAction::DodgeBackward,
    EPlayerCombatAction::Block,
    EPlayerCombatAction::Heal
};

float SanitizeBounded(
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

bool IsUsablePrediction(const FPredictionResult& Prediction)
{
    return Prediction.bHasPrediction
        && AdaptiveCombat::IsTrackablePlayerAction(
            Prediction.PredictedAction
        )
        && FMath::IsFinite(Prediction.Confidence)
        && Prediction.Confidence >= 0.0f
        && Prediction.SupportingSampleCount > 0
        && (!Prediction.bUsedContext
            || (AdaptiveCombat::IsKnownEnemyAction(
                    Prediction.ConditioningEnemyAction
                )
                && Prediction.ConditioningEnemyAction
                    != EEnemyCombatAction::None))
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

bool MatchesPredictionContext(
    const FTrainingSample& Sample,
    const FPredictionResult& Prediction
)
{
    if (Prediction.bUsedContext
        && Sample.Snapshot.PreviousEnemyAction
            != Prediction.ConditioningEnemyAction)
    {
        return false;
    }
    if (Prediction.bUsedDistanceContext
        && Sample.Snapshot.DistanceCategory
            != Prediction.ConditioningDistanceCategory)
    {
        return false;
    }
    if (Prediction.bUsedPositionContext
        && Sample.Snapshot.RelativePlayerPosition
            != Prediction.ConditioningRelativePlayerPosition)
    {
        return false;
    }
    if (Prediction.bUsedPreviousPlayerActionContext
        && Sample.Snapshot.PreviousPlayerAction
            != Prediction.ConditioningPreviousPlayerAction)
    {
        return false;
    }
    return true;
}

int32 GetPredictionSpecificity(const FPredictionResult& Prediction)
{
    if (Prediction.bUsedPreviousPlayerActionContext)
    {
        return 4;
    }
    if (Prediction.bUsedPositionContext)
    {
        return 3;
    }
    if (Prediction.bUsedDistanceContext)
    {
        return 2;
    }
    return Prediction.bUsedContext ? 1 : 0;
}

int32 GetActionOrder(const EPlayerCombatAction Action)
{
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ProfileActions); ++Index)
    {
        if (ProfileActions[Index] == Action)
        {
            return Index;
        }
    }
    return UE_ARRAY_COUNT(ProfileActions);
}

void FillTacticalAdjustments(FAdaptiveTacticalProfile& Profile)
{
    switch (Profile.MostLikelyPlayerAction)
    {
    case EPlayerCombatAction::Block:
        Profile.PreferredCounterAction =
            EEnemyCombatAction::HeavyAttack;
        Profile.PreferredSpacingAdjustment = 80.0f;
        Profile.AggressionAdjustment = 0.10f;
        Profile.DefensiveAdjustment = -0.03f;
        break;
    case EPlayerCombatAction::Heal:
        Profile.PreferredCounterAction =
            EEnemyCombatAction::InterruptHeal;
        Profile.PreferredSpacingAdjustment = -110.0f;
        Profile.AggressionAdjustment = 0.16f;
        Profile.DefensiveAdjustment = -0.05f;
        Profile.HealInterruptionPriority = 0.24f;
        break;
    case EPlayerCombatAction::LightAttack:
        Profile.PreferredCounterAction = EEnemyCombatAction::Block;
        Profile.PreferredSpacingAdjustment = 30.0f;
        Profile.AggressionAdjustment = -0.04f;
        Profile.DefensiveAdjustment = 0.18f;
        break;
    case EPlayerCombatAction::HeavyAttack:
        Profile.PreferredCounterAction = EEnemyCombatAction::Dodge;
        Profile.PreferredSpacingAdjustment = 45.0f;
        Profile.AggressionAdjustment = -0.06f;
        Profile.DefensiveAdjustment = 0.20f;
        break;
    case EPlayerCombatAction::DodgeBackward:
        Profile.PreferredCounterAction = EEnemyCombatAction::DashAttack;
        Profile.PreferredSpacingAdjustment = -100.0f;
        Profile.AggressionAdjustment = 0.14f;
        Profile.DefensiveAdjustment = -0.03f;
        break;
    case EPlayerCombatAction::DodgeLeft:
        Profile.PreferredCounterAction = EEnemyCombatAction::HeavyAttack;
        Profile.PreferredSpacingAdjustment = 20.0f;
        Profile.AggressionAdjustment = 0.06f;
        Profile.OrbitPreference = EAdaptiveOrbitPreference::Right;
        break;
    case EPlayerCombatAction::DodgeRight:
        Profile.PreferredCounterAction = EEnemyCombatAction::HeavyAttack;
        Profile.PreferredSpacingAdjustment = 20.0f;
        Profile.AggressionAdjustment = 0.06f;
        Profile.OrbitPreference = EAdaptiveOrbitPreference::Left;
        break;
    default:
        break;
    }
}
}

FAdaptiveTacticalProfileTuning::FAdaptiveTacticalProfileTuning()
    : MinimumRoundSamples(5)
    , MinimumContextSamples(4)
    , MinimumSupportingSamples(3)
    , MinimumConfidence(0.65f)
    , MaximumSpacingAdjustment(120.0f)
    , MaximumAggressionAdjustment(0.20f)
    , MaximumDefensiveAdjustment(0.20f)
    , MaximumHealInterruptionPriority(0.25f)
    , MaximumUtilityAdjustment(0.24f)
    , PreferredCounterUtilityBoost(0.18f)
    , CounterCooldown(2.4f)
    , CounterBudgetPerRound(3)
{
}

FAdaptiveTacticalProfileTuning
FAdaptiveTacticalProfileTuning::GetSanitized() const
{
    FAdaptiveTacticalProfileTuning Result;
    Result.MinimumRoundSamples = FMath::Max(1, MinimumRoundSamples);
    Result.MinimumContextSamples = FMath::Max(1, MinimumContextSamples);
    Result.MinimumSupportingSamples = FMath::Max(
        1,
        MinimumSupportingSamples
    );
    Result.MinimumConfidence = SanitizeBounded(
        MinimumConfidence,
        0.0f,
        1.0f,
        Result.MinimumConfidence
    );
    Result.MaximumSpacingAdjustment = SanitizeBounded(
        MaximumSpacingAdjustment,
        0.0f,
        160.0f,
        Result.MaximumSpacingAdjustment
    );
    Result.MaximumAggressionAdjustment = SanitizeBounded(
        MaximumAggressionAdjustment,
        0.0f,
        0.25f,
        Result.MaximumAggressionAdjustment
    );
    Result.MaximumDefensiveAdjustment = SanitizeBounded(
        MaximumDefensiveAdjustment,
        0.0f,
        0.25f,
        Result.MaximumDefensiveAdjustment
    );
    Result.MaximumHealInterruptionPriority = SanitizeBounded(
        MaximumHealInterruptionPriority,
        0.0f,
        0.30f,
        Result.MaximumHealInterruptionPriority
    );
    Result.MaximumUtilityAdjustment = SanitizeBounded(
        MaximumUtilityAdjustment,
        0.0f,
        0.25f,
        Result.MaximumUtilityAdjustment
    );
    Result.PreferredCounterUtilityBoost = SanitizeBounded(
        PreferredCounterUtilityBoost,
        0.0f,
        0.25f,
        Result.PreferredCounterUtilityBoost
    );
    Result.CounterCooldown = FMath::IsFinite(CounterCooldown)
        ? FMath::Max(0.0f, CounterCooldown)
        : Result.CounterCooldown;
    Result.CounterBudgetPerRound = FMath::Clamp(
        CounterBudgetPerRound,
        0,
        8
    );
    return Result;
}

bool FAdaptiveTacticalProfile::IsActive() const
{
    return bEvidenceStrongEnough
        && EvidenceStatus == EAdaptiveProfileEvidenceStatus::Active
        && SourceRound > 0
        && AdaptiveCombat::IsTrackablePlayerAction(
            MostLikelyPlayerAction
        )
        && AdaptiveCombat::IsUtilityEnemyAction(PreferredCounterAction)
        && FMath::IsFinite(Confidence)
        && Confidence > 0.0f
        && SupportingSampleCount > 0
        && ContextSampleCount >= SupportingSampleCount
        && RoundSampleCount >= ContextSampleCount;
}

void FAdaptiveCounterBudgetState::Reset(const int32 MaximumUses)
{
    RemainingUses = FMath::Max(0, MaximumUses);
    NextAllowedTime = 0.0;
}

bool FAdaptiveCounterBudgetState::CanConsume(
    const double CurrentTime
) const
{
    return RemainingUses > 0 && FMath::IsFinite(CurrentTime)
        && CurrentTime + static_cast<double>(KINDA_SMALL_NUMBER)
            >= NextAllowedTime;
}

bool FAdaptiveCounterBudgetState::Consume(
    const double CurrentTime,
    const float Cooldown
)
{
    if (!CanConsume(CurrentTime))
    {
        return false;
    }

    --RemainingUses;
    const float SafeCooldown = FMath::IsFinite(Cooldown)
        ? FMath::Max(0.0f, Cooldown)
        : 0.0f;
    NextAllowedTime = CurrentTime + SafeCooldown;
    return true;
}

float FAdaptiveCounterBudgetState::GetRemainingCooldown(
    const double CurrentTime
) const
{
    if (!FMath::IsFinite(CurrentTime))
    {
        return 0.0f;
    }
    return static_cast<float>(FMath::Max(
        0.0,
        NextAllowedTime - CurrentTime
    ));
}

FAdaptiveTacticalProfile FAdaptiveTacticalProfilePolicy::Derive(
    const FCombatDataset& Dataset,
    const int32 CompletedRound,
    const FPredictionResult& Prediction,
    const FAdaptiveTacticalProfileTuning& Tuning
)
{
    const FAdaptiveTacticalProfileTuning SafeTuning =
        Tuning.GetSanitized();
    FAdaptiveTacticalProfile Profile;
    Profile.SourceRound = FMath::Max(0, CompletedRound);

    TMap<EPlayerCombatAction, int32> ContextCounts;
    for (const FTrainingSample& Sample : Dataset.GetSamples())
    {
        if (!Sample.IsValid()
            || Sample.Snapshot.RoundNumber != CompletedRound)
        {
            continue;
        }

        ++Profile.RoundSampleCount;
        if (IsUsablePrediction(Prediction)
            && MatchesPredictionContext(Sample, Prediction))
        {
            ++ContextCounts.FindOrAdd(Sample.NextPlayerAction);
            ++Profile.ContextSampleCount;
        }
    }

    if (Profile.RoundSampleCount <= 0 || CompletedRound <= 0)
    {
        Profile.EvidenceStatus = EAdaptiveProfileEvidenceStatus::NoData;
        return Profile;
    }
    if (Profile.RoundSampleCount < SafeTuning.MinimumRoundSamples)
    {
        Profile.EvidenceStatus =
            EAdaptiveProfileEvidenceStatus::InsufficientRoundSamples;
        return Profile;
    }
    if (!IsUsablePrediction(Prediction))
    {
        Profile.EvidenceStatus =
            EAdaptiveProfileEvidenceStatus::NoPrediction;
        return Profile;
    }

    Profile.EvidencePrediction = Prediction;
    Profile.MostLikelyPlayerAction = Prediction.PredictedAction;
    Profile.SupportingSampleCount = ContextCounts.FindRef(
        Prediction.PredictedAction
    );
    const float RoundContextConfidence = Profile.ContextSampleCount > 0
        ? static_cast<float>(Profile.SupportingSampleCount)
            / static_cast<float>(Profile.ContextSampleCount)
        : 0.0f;
    Profile.Confidence = FMath::Min(
        FMath::Clamp(Prediction.Confidence, 0.0f, 1.0f),
        FMath::Clamp(RoundContextConfidence, 0.0f, 1.0f)
    );

    EPlayerCombatAction RoundDominantAction = EPlayerCombatAction::None;
    int32 RoundDominantCount = 0;
    for (const EPlayerCombatAction Action : ProfileActions)
    {
        const int32 Count = ContextCounts.FindRef(Action);
        if (Count > RoundDominantCount)
        {
            RoundDominantAction = Action;
            RoundDominantCount = Count;
        }
    }

    if (Profile.ContextSampleCount < SafeTuning.MinimumContextSamples
        || Profile.SupportingSampleCount
            < SafeTuning.MinimumSupportingSamples)
    {
        Profile.EvidenceStatus =
            EAdaptiveProfileEvidenceStatus::InsufficientContextSamples;
        return Profile;
    }
    if (RoundDominantAction != Prediction.PredictedAction)
    {
        Profile.EvidenceStatus =
            EAdaptiveProfileEvidenceStatus::PredictorDisagreement;
        return Profile;
    }
    if (Profile.Confidence < SafeTuning.MinimumConfidence)
    {
        Profile.EvidenceStatus =
            EAdaptiveProfileEvidenceStatus::LowConfidence;
        return Profile;
    }

    FillTacticalAdjustments(Profile);
    Profile.PreferredSpacingAdjustment = FMath::Clamp(
        Profile.PreferredSpacingAdjustment,
        -SafeTuning.MaximumSpacingAdjustment,
        SafeTuning.MaximumSpacingAdjustment
    );
    Profile.AggressionAdjustment = FMath::Clamp(
        Profile.AggressionAdjustment,
        -SafeTuning.MaximumAggressionAdjustment,
        SafeTuning.MaximumAggressionAdjustment
    );
    Profile.DefensiveAdjustment = FMath::Clamp(
        Profile.DefensiveAdjustment,
        -SafeTuning.MaximumDefensiveAdjustment,
        SafeTuning.MaximumDefensiveAdjustment
    );
    Profile.HealInterruptionPriority = FMath::Clamp(
        Profile.HealInterruptionPriority,
        0.0f,
        SafeTuning.MaximumHealInterruptionPriority
    );
    Profile.bEvidenceStrongEnough = AdaptiveCombat::IsUtilityEnemyAction(
        Profile.PreferredCounterAction
    );
    Profile.EvidenceStatus = Profile.bEvidenceStrongEnough
        ? EAdaptiveProfileEvidenceStatus::Active
        : EAdaptiveProfileEvidenceStatus::NoPrediction;
    return Profile;
}

bool FAdaptiveTacticalProfilePolicy::IsStrongerCandidate(
    const FAdaptiveTacticalProfile& Candidate,
    const FAdaptiveTacticalProfile& Current
)
{
    if (Candidate.IsActive() != Current.IsActive())
    {
        return Candidate.IsActive();
    }
    if (Candidate.SupportingSampleCount != Current.SupportingSampleCount)
    {
        return Candidate.SupportingSampleCount
            > Current.SupportingSampleCount;
    }
    if (!FMath::IsNearlyEqual(Candidate.Confidence, Current.Confidence))
    {
        return Candidate.Confidence > Current.Confidence;
    }
    const int32 CandidateSpecificity = GetPredictionSpecificity(
        Candidate.EvidencePrediction
    );
    const int32 CurrentSpecificity = GetPredictionSpecificity(
        Current.EvidencePrediction
    );
    if (CandidateSpecificity != CurrentSpecificity)
    {
        return CandidateSpecificity > CurrentSpecificity;
    }
    return GetActionOrder(Candidate.MostLikelyPlayerAction)
        < GetActionOrder(Current.MostLikelyPlayerAction);
}

float FAdaptiveTacticalProfilePolicy::GetUtilityModifier(
    const FAdaptiveTacticalProfile& Profile,
    const EEnemyCombatAction Action,
    const bool bPreferredCounterOpportunity,
    const FAdaptiveTacticalProfileTuning& Tuning
)
{
    if (!Profile.IsActive()
        || !AdaptiveCombat::IsUtilityEnemyAction(Action))
    {
        return 0.0f;
    }

    const FAdaptiveTacticalProfileTuning SafeTuning =
        Tuning.GetSanitized();
    float Adjustment = 0.0f;
    if (FEnemyTacticalPolicy::IsOffensiveAction(Action))
    {
        Adjustment += FMath::Clamp(
            Profile.AggressionAdjustment,
            -SafeTuning.MaximumAggressionAdjustment,
            SafeTuning.MaximumAggressionAdjustment
        );
    }
    if (FEnemyTacticalPolicy::IsDefensiveAction(Action))
    {
        Adjustment += FMath::Clamp(
            Profile.DefensiveAdjustment,
            -SafeTuning.MaximumDefensiveAdjustment,
            SafeTuning.MaximumDefensiveAdjustment
        );
    }
    if (Action == EEnemyCombatAction::InterruptHeal)
    {
        Adjustment += FMath::Clamp(
            Profile.HealInterruptionPriority,
            0.0f,
            SafeTuning.MaximumHealInterruptionPriority
        );
    }

    switch (Profile.MostLikelyPlayerAction)
    {
    case EPlayerCombatAction::Block:
        if (Action == EEnemyCombatAction::HeavyAttack)
        {
            Adjustment += 0.10f;
        }
        else if (Action == EEnemyCombatAction::ProjectileAttack)
        {
            Adjustment += 0.08f;
        }
        else if (Action == EEnemyCombatAction::LightAttack)
        {
            Adjustment -= 0.06f;
        }
        break;
    case EPlayerCombatAction::DodgeBackward:
        if (Action == EEnemyCombatAction::ProjectileAttack)
        {
            Adjustment += 0.08f;
        }
        break;
    case EPlayerCombatAction::DodgeLeft:
    case EPlayerCombatAction::DodgeRight:
        if (Action == EEnemyCombatAction::ProjectileAttack)
        {
            Adjustment += 0.04f;
        }
        break;
    default:
        break;
    }

    if (bPreferredCounterOpportunity
        && Action == Profile.PreferredCounterAction)
    {
        Adjustment += SafeTuning.PreferredCounterUtilityBoost;
    }

    return FMath::Clamp(
        Adjustment,
        -SafeTuning.MaximumUtilityAdjustment,
        SafeTuning.MaximumUtilityAdjustment
    );
}

float FAdaptiveTacticalProfilePolicy::AdjustSpacingDistance(
    const float BaseDistance,
    const FAdaptiveTacticalProfile& Profile,
    const float AdjustmentScale,
    const FAdaptiveTacticalProfileTuning& Tuning
)
{
    const float SafeBaseDistance = FMath::IsFinite(BaseDistance)
        ? FMath::Max(0.0f, BaseDistance)
        : 0.0f;
    if (!Profile.IsActive())
    {
        return SafeBaseDistance;
    }
    const FAdaptiveTacticalProfileTuning SafeTuning =
        Tuning.GetSanitized();
    const float SafeScale = FMath::IsFinite(AdjustmentScale)
        ? FMath::Clamp(AdjustmentScale, 0.0f, 1.0f)
        : 0.0f;
    const float Adjustment = FMath::Clamp(
        Profile.PreferredSpacingAdjustment,
        -SafeTuning.MaximumSpacingAdjustment,
        SafeTuning.MaximumSpacingAdjustment
    ) * SafeScale;
    return FMath::Max(0.0f, SafeBaseDistance + Adjustment);
}

bool FAdaptiveTacticalProfilePolicy::ResolveOrbitRight(
    const FAdaptiveTacticalProfile& Profile,
    const bool bSeededOrbitRight
)
{
    if (!Profile.IsActive())
    {
        return bSeededOrbitRight;
    }
    if (Profile.OrbitPreference == EAdaptiveOrbitPreference::Right)
    {
        return true;
    }
    if (Profile.OrbitPreference == EAdaptiveOrbitPreference::Left)
    {
        return false;
    }
    return bSeededOrbitRight;
}

bool FAdaptiveTacticalProfilePolicy::
    IsPredictionMatchedCounterOpportunity(
        const FAdaptiveTacticalProfile& Profile,
        const FPredictionResult& Prediction,
        const bool bPredictionApplied,
        const FAdaptiveCounterBudgetState& CounterBudget,
        const double CurrentTime
    )
{
    return Profile.IsActive() && bPredictionApplied
        && IsUsablePrediction(Prediction)
        && Prediction.PredictedAction == Profile.MostLikelyPlayerAction
        && CounterBudget.CanConsume(CurrentTime);
}
