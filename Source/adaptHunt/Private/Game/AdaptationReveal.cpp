#include "Game/AdaptationReveal.h"

namespace
{
FString GetPlayerActionPhrase(const EPlayerCombatAction Action)
{
    switch (Action)
    {
    case EPlayerCombatAction::LightAttack:
        return TEXT("use light attacks");
    case EPlayerCombatAction::HeavyAttack:
        return TEXT("use heavy attacks");
    case EPlayerCombatAction::DodgeLeft:
        return TEXT("dodge left");
    case EPlayerCombatAction::DodgeRight:
        return TEXT("dodge right");
    case EPlayerCombatAction::DodgeBackward:
        return TEXT("dodge backward");
    case EPlayerCombatAction::Block:
        return TEXT("block");
    case EPlayerCombatAction::Heal:
        return TEXT("heal");
    default:
        return FString();
    }
}

FString GetEnemyActionCondition(const EEnemyCombatAction Action)
{
    switch (Action)
    {
    case EEnemyCombatAction::MoveTowardPlayer:
        return TEXT("after the enemy approaches");
    case EEnemyCombatAction::MoveAwayFromPlayer:
        return TEXT("after the enemy retreats");
    case EEnemyCombatAction::StrafeLeft:
        return TEXT("after the enemy strafes left");
    case EEnemyCombatAction::StrafeRight:
        return TEXT("after the enemy strafes right");
    case EEnemyCombatAction::LightAttack:
        return TEXT("after an enemy light attack");
    case EEnemyCombatAction::HeavyAttack:
        return TEXT("after an enemy heavy attack");
    case EEnemyCombatAction::ProjectileAttack:
        return TEXT("after an enemy projectile attack");
    case EEnemyCombatAction::DashAttack:
        return TEXT("after an enemy dash attack");
    case EEnemyCombatAction::Block:
        return TEXT("after the enemy blocks");
    case EEnemyCombatAction::Dodge:
        return TEXT("after the enemy dodges");
    case EEnemyCombatAction::InterruptHeal:
        return TEXT("after an enemy heal interrupt");
    default:
        return FString();
    }
}

FString GetDistanceCondition(const ECombatDistanceCategory Distance)
{
    switch (Distance)
    {
    case ECombatDistanceCategory::Close:
        return TEXT("at close range");
    case ECombatDistanceCategory::Medium:
        return TEXT("at mid range");
    case ECombatDistanceCategory::Far:
        return TEXT("at long range");
    default:
        return FString();
    }
}

FString GetPositionCondition(const ERelativePlayerPosition Position)
{
    switch (Position)
    {
    case ERelativePlayerPosition::Front:
        return TEXT("while in front of the enemy");
    case ERelativePlayerPosition::Left:
        return TEXT("while to the enemy's left");
    case ERelativePlayerPosition::Right:
        return TEXT("while to the enemy's right");
    case ERelativePlayerPosition::Behind:
        return TEXT("while behind the enemy");
    default:
        return FString();
    }
}

FString GetCounterAdjustment(const EEnemyCombatAction Action)
{
    switch (Action)
    {
    case EEnemyCombatAction::LightAttack:
        return TEXT("Adapting: favor quick light pressure against that habit.");
    case EEnemyCombatAction::HeavyAttack:
        return TEXT("Adapting: favor heavy attacks against that habit.");
    case EEnemyCombatAction::ProjectileAttack:
        return TEXT("Adapting: favor ranged pressure against that habit.");
    case EEnemyCombatAction::DashAttack:
        return TEXT("Adapting: close space with dash pressure.");
    case EEnemyCombatAction::Block:
        return TEXT("Adapting: defend and punish that habit.");
    case EEnemyCombatAction::Dodge:
        return TEXT("Adapting: dodge and punish that habit.");
    case EEnemyCombatAction::InterruptHeal:
        return TEXT("Adapting: pursue and interrupt healing.");
    default:
        return FString();
    }
}

bool CanDescribeProfile(const FAdaptiveTacticalProfile& Profile)
{
    const FPredictionResult& Prediction = Profile.EvidencePrediction;
    return Profile.IsActive() && Prediction.bHasPrediction
        && Prediction.PredictedAction == Profile.MostLikelyPlayerAction
        && Profile.SupportingSampleCount > 0
        && (!Prediction.bUsedContext
            || !GetEnemyActionCondition(
                Prediction.ConditioningEnemyAction
            ).IsEmpty())
        && (!Prediction.bUsedDistanceContext
            || !GetDistanceCondition(
                Prediction.ConditioningDistanceCategory
            ).IsEmpty())
        && (!Prediction.bUsedPositionContext
            || !GetPositionCondition(
                Prediction.ConditioningRelativePlayerPosition
            ).IsEmpty())
        && (!Prediction.bUsedPreviousPlayerActionContext
            || !GetPlayerActionPhrase(
                Prediction.ConditioningPreviousPlayerAction
            ).IsEmpty());
}

FString BuildProfileObservation(const FAdaptiveTacticalProfile& Profile)
{
    const FPredictionResult& Prediction = Profile.EvidencePrediction;
    TArray<FString> Conditions;
    if (Prediction.bUsedContext)
    {
        Conditions.Add(GetEnemyActionCondition(
            Prediction.ConditioningEnemyAction
        ));
    }
    if (Prediction.bUsedDistanceContext)
    {
        Conditions.Add(GetDistanceCondition(
            Prediction.ConditioningDistanceCategory
        ));
    }
    if (Prediction.bUsedPositionContext)
    {
        Conditions.Add(GetPositionCondition(
            Prediction.ConditioningRelativePlayerPosition
        ));
    }
    if (Prediction.bUsedPreviousPlayerActionContext)
    {
        Conditions.Add(FString::Printf(
            TEXT("after you previously %s"),
            *GetPlayerActionPhrase(
                Prediction.ConditioningPreviousPlayerAction
            )
        ));
    }

    const FString Context = Conditions.IsEmpty()
        ? FString(TEXT("across the round"))
        : FString::Join(Conditions, TEXT(", "));
    return FString::Printf(
        TEXT("Observed: %s, you most often %s."),
        *Context,
        *GetPlayerActionPhrase(Profile.MostLikelyPlayerAction)
    );
}

FString BuildPatternObservation(
    const FAdaptiveConditionalPattern& Pattern
)
{
    return FString::Printf(
        TEXT("Observed: %s, you most often %s."),
        *GetEnemyActionCondition(Pattern.PreviousEnemyAction),
        *GetPlayerActionPhrase(Pattern.DominantPlayerAction)
    );
}
}

EAdaptiveRoundPresentationStage
FAdaptiveAdaptationRevealPolicy::GetRoundStage(const int32 RoundNumber)
{
    if (RoundNumber <= 1)
    {
        return EAdaptiveRoundPresentationStage::Learning;
    }
    return RoundNumber == 2
        ? EAdaptiveRoundPresentationStage::Adapting
        : EAdaptiveRoundPresentationStage::Adapted;
}

FString FAdaptiveAdaptationRevealPolicy::FormatRoundStage(
    const int32 RoundNumber
)
{
    switch (GetRoundStage(RoundNumber))
    {
    case EAdaptiveRoundPresentationStage::Adapting:
        return TEXT("ADAPTING  |  ENEMY TESTS A LEARNED COUNTER");
    case EAdaptiveRoundPresentationStage::Adapted:
        return TEXT("ADAPTED  |  COUNTER-ADAPT: CHANGE YOUR HABIT");
    case EAdaptiveRoundPresentationStage::Learning:
    default:
        return TEXT("LIVE LEARNING  |  HISTORY + NEW COMMITTED ACTIONS");
    }
}

FAdaptiveRevealText FAdaptiveAdaptationRevealPolicy::Build(
    const FAdaptiveConditionalPattern& ObservedPattern,
    const FAdaptiveTacticalProfile& TacticalProfile
)
{
    FAdaptiveRevealText Reveal;
    if (CanDescribeProfile(TacticalProfile))
    {
        Reveal.Observation = BuildProfileObservation(TacticalProfile);
        Reveal.bHasSupportedObservation = true;
    }
    else if (ObservedPattern.IsValid())
    {
        Reveal.Observation = BuildPatternObservation(ObservedPattern);
        Reveal.bHasSupportedObservation = true;
    }
    else
    {
        Reveal.Observation = TEXT(
            "Observed: no reliable contextual habit yet."
        );
    }

    if (TacticalProfile.IsActive())
    {
        Reveal.Adjustment = GetCounterAdjustment(
            TacticalProfile.PreferredCounterAction
        );
        Reveal.bHasActiveAdjustment = !Reveal.Adjustment.IsEmpty();
    }
    if (!Reveal.bHasActiveAdjustment)
    {
        Reveal.Adjustment = TEXT(
            "Adapting: baseline tactics retained; evidence is not strong enough."
        );
    }
    return Reveal;
}
