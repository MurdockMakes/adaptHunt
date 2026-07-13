#include "AI/EnemyTacticalPolicy.h"

namespace
{
float SanitizeNormalized(const float Value, const float Fallback)
{
    return FMath::IsFinite(Value)
        ? FMath::Clamp(Value, 0.0f, 1.0f)
        : Fallback;
}

float SanitizeDistance(const float Value, const float Fallback)
{
    return FMath::IsFinite(Value)
        ? FMath::Max(0.0f, Value)
        : Fallback;
}

bool IsRecentPlayerAttack(const FEnemyTacticalContext& Context)
{
    return (Context.RecentPlayerAction == EPlayerCombatAction::LightAttack
        || Context.RecentPlayerAction == EPlayerCombatAction::HeavyAttack)
        && FMath::IsFinite(Context.TimeSincePlayerAttack)
        && Context.TimeSincePlayerAttack >= 0.0f;
}

EEnemyLocomotionIntent SelectOrbitIntent(const bool bOrbitRight)
{
    return bOrbitRight
        ? EEnemyLocomotionIntent::OrbitRight
        : EEnemyLocomotionIntent::OrbitLeft;
}
}

FEnemyTacticalTuning::FEnemyTacticalTuning()
    : LowStaminaThreshold(0.24f)
    , StaminaRecoveryThreshold(0.48f)
    , LowHealthThreshold(0.32f)
    , StateCommitDuration(0.70f)
    , DistanceHysteresis(45.0f)
    , DefensiveThreatDistance(500.0f)
    , PunishWindow(1.10f)
    , PredictionDefendConfidence(0.65f)
    , MaxUtilityAdjustment(0.22f)
    , SequenceStartChance(0.72f)
    , SequenceWindow(1.55f)
    , SequenceExitDistance(260.0f)
    , OrbitDirectionCommitDuration(2.5f)
{
}

FEnemyTacticalTuning FEnemyTacticalTuning::GetSanitized() const
{
    FEnemyTacticalTuning Result;
    Result.LowStaminaThreshold = SanitizeNormalized(
        LowStaminaThreshold,
        Result.LowStaminaThreshold
    );
    Result.StaminaRecoveryThreshold = FMath::Max(
        Result.LowStaminaThreshold,
        SanitizeNormalized(
            StaminaRecoveryThreshold,
            Result.StaminaRecoveryThreshold
        )
    );
    Result.LowHealthThreshold = SanitizeNormalized(
        LowHealthThreshold,
        Result.LowHealthThreshold
    );
    Result.StateCommitDuration = SanitizeDistance(
        StateCommitDuration,
        Result.StateCommitDuration
    );
    Result.DistanceHysteresis = SanitizeDistance(
        DistanceHysteresis,
        Result.DistanceHysteresis
    );
    Result.DefensiveThreatDistance = SanitizeDistance(
        DefensiveThreatDistance,
        Result.DefensiveThreatDistance
    );
    Result.PunishWindow = SanitizeDistance(
        PunishWindow,
        Result.PunishWindow
    );
    Result.PredictionDefendConfidence = SanitizeNormalized(
        PredictionDefendConfidence,
        Result.PredictionDefendConfidence
    );
    Result.MaxUtilityAdjustment = FMath::IsFinite(MaxUtilityAdjustment)
        ? FMath::Clamp(MaxUtilityAdjustment, 0.0f, 0.35f)
        : Result.MaxUtilityAdjustment;
    Result.SequenceStartChance = SanitizeNormalized(
        SequenceStartChance,
        Result.SequenceStartChance
    );
    Result.SequenceWindow = SanitizeDistance(
        SequenceWindow,
        Result.SequenceWindow
    );
    Result.SequenceExitDistance = SanitizeDistance(
        SequenceExitDistance,
        Result.SequenceExitDistance
    );
    Result.OrbitDirectionCommitDuration = SanitizeDistance(
        OrbitDirectionCommitDuration,
        Result.OrbitDirectionCommitDuration
    );
    return Result;
}

bool FEnemyTacticalContext::HasAnyOffensiveAction() const
{
    return bLightAttackAvailable || bHeavyAttackAvailable
        || bProjectileAvailable || bDashAvailable
        || bInterruptAvailable;
}

bool FEnemyTacticalContext::HasAnyDefensiveAction() const
{
    return bBlockAvailable || bDodgeAvailable;
}

bool FEnemyTacticalContext::IsActionAvailable(
    const EEnemyCombatAction Action
) const
{
    switch (Action)
    {
    case EEnemyCombatAction::LightAttack:
        return bLightAttackAvailable;
    case EEnemyCombatAction::HeavyAttack:
        return bHeavyAttackAvailable;
    case EEnemyCombatAction::ProjectileAttack:
        return bProjectileAvailable;
    case EEnemyCombatAction::DashAttack:
        return bDashAvailable;
    case EEnemyCombatAction::Block:
        return bBlockAvailable;
    case EEnemyCombatAction::Dodge:
        return bDodgeAvailable;
    case EEnemyCombatAction::InterruptHeal:
        return bInterruptAvailable;
    default:
        return false;
    }
}

void FEnemyAttackSequenceState::Start(
    const EEnemyCombatAction InFollowUpAction,
    const double InExpiresAtTime
)
{
    FollowUpAction = InFollowUpAction == EEnemyCombatAction::HeavyAttack
        ? EEnemyCombatAction::HeavyAttack
        : EEnemyCombatAction::LightAttack;
    ExpiresAtTime = FMath::IsFinite(InExpiresAtTime)
        ? FMath::Max(0.0, InExpiresAtTime)
        : 0.0;
}

void FEnemyAttackSequenceState::Reset()
{
    FollowUpAction = EEnemyCombatAction::None;
    ExpiresAtTime = 0.0;
}

bool FEnemyAttackSequenceState::IsActive() const
{
    return FollowUpAction == EEnemyCombatAction::LightAttack
        || FollowUpAction == EEnemyCombatAction::HeavyAttack;
}

EEnemyTacticalState FEnemyTacticalPolicy::SelectState(
    const FEnemyTacticalContext& Context,
    const EEnemyTacticalState CurrentState,
    const float TimeInCurrentState,
    const FEnemyTacticalTuning& Tuning
)
{
    const FEnemyTacticalTuning SafeTuning = Tuning.GetSanitized();
    const float Distance = SanitizeDistance(
        Context.DistanceToTarget,
        MAX_flt
    );
    const float RetreatDistance = SanitizeDistance(
        Context.RetreatDistance,
        0.0f
    );
    const float OrbitDistance = FMath::Max(
        RetreatDistance,
        SanitizeDistance(Context.OrbitDistance, RetreatDistance)
    );
    const float MeleeDistance = SanitizeDistance(
        Context.MeleeDistance,
        RetreatDistance
    );
    const float Health = SanitizeNormalized(
        Context.EnemyHealthNormalized,
        1.0f
    );
    const float Stamina = SanitizeNormalized(
        Context.EnemyStaminaNormalized,
        0.0f
    );
    const bool bCrowded = Distance < RetreatDistance;

    // Resource and immediate-threat exits intentionally bypass commitment.
    if (Stamina <= SafeTuning.LowStaminaThreshold
        || (CurrentState == EEnemyTacticalState::Recover
            && Stamina < SafeTuning.StaminaRecoveryThreshold))
    {
        return EEnemyTacticalState::Recover;
    }
    if (bCrowded)
    {
        return EEnemyTacticalState::Retreat;
    }
    if (Context.bPlayerAttacking && Context.HasAnyDefensiveAction()
        && Distance <= SafeTuning.DefensiveThreatDistance)
    {
        return EEnemyTacticalState::Defend;
    }
    if (Health <= SafeTuning.LowHealthThreshold
        && Context.HasAnyDefensiveAction()
        && Distance <= OrbitDistance + SafeTuning.DistanceHysteresis)
    {
        return EEnemyTacticalState::Defend;
    }
    if (Context.bTargetRecentlyHealing && Context.HasAnyOffensiveAction())
    {
        return EEnemyTacticalState::Punish;
    }
    if (Context.bAttackSequenceActive && Context.HasAnyOffensiveAction()
        && Context.bHasLineOfSight
        && Distance <= SafeTuning.SequenceExitDistance)
    {
        return EEnemyTacticalState::Pressure;
    }
    if (!Context.HasAnyOffensiveAction())
    {
        return EEnemyTacticalState::Observe;
    }

    const float SafeTimeInState = FMath::IsFinite(TimeInCurrentState)
        ? FMath::Max(0.0f, TimeInCurrentState)
        : SafeTuning.StateCommitDuration;
    if (SafeTimeInState < SafeTuning.StateCommitDuration
        && CurrentState != EEnemyTacticalState::Observe)
    {
        return CurrentState;
    }

    if (IsRecentPlayerAttack(Context)
        && Context.TimeSincePlayerAttack <= SafeTuning.PunishWindow
        && Distance <= SafeTuning.DefensiveThreatDistance)
    {
        return EEnemyTacticalState::Punish;
    }

    const bool bPredictsCommittedAttack =
        Context.PredictedPlayerAction == EPlayerCombatAction::LightAttack
        || Context.PredictedPlayerAction == EPlayerCombatAction::HeavyAttack;
    if (bPredictsCommittedAttack && Context.HasAnyDefensiveAction()
        && FMath::IsFinite(Context.PredictionConfidence)
        && Context.PredictionConfidence
            >= SafeTuning.PredictionDefendConfidence
        && Distance <= SafeTuning.DefensiveThreatDistance)
    {
        return EEnemyTacticalState::Defend;
    }

    if (!Context.bHasLineOfSight
        || Distance > OrbitDistance + SafeTuning.DistanceHysteresis)
    {
        return EEnemyTacticalState::Approach;
    }
    if (Distance <= MeleeDistance + SafeTuning.DistanceHysteresis)
    {
        if (Context.RecentEnemyAction == EEnemyCombatAction::HeavyAttack)
        {
            return EEnemyTacticalState::Orbit;
        }
        return EEnemyTacticalState::Pressure;
    }
    return EEnemyTacticalState::Orbit;
}

EEnemyLocomotionIntent FEnemyTacticalPolicy::SelectMovementIntent(
    const FEnemyTacticalContext& Context,
    const EEnemyTacticalState TacticalState,
    const EEnemyLocomotionIntent BaseIntent,
    const bool bOrbitRight
)
{
    const float Distance = SanitizeDistance(
        Context.DistanceToTarget,
        MAX_flt
    );
    const float RetreatDistance = SanitizeDistance(
        Context.RetreatDistance,
        0.0f
    );
    const float PreferredDistance = FMath::Max(
        RetreatDistance,
        SanitizeDistance(Context.PreferredDistance, RetreatDistance)
    );
    const float MeleeDistance = FMath::Max(
        RetreatDistance,
        SanitizeDistance(Context.MeleeDistance, PreferredDistance)
    );
    const EEnemyLocomotionIntent OrbitIntent =
        SelectOrbitIntent(bOrbitRight);

    switch (TacticalState)
    {
    case EEnemyTacticalState::Approach:
        return Distance < RetreatDistance
            ? EEnemyLocomotionIntent::Retreat
            : EEnemyLocomotionIntent::Approach;
    case EEnemyTacticalState::Pressure:
    case EEnemyTacticalState::Punish:
        if (Distance < RetreatDistance)
        {
            return EEnemyLocomotionIntent::Retreat;
        }
        return Distance > MeleeDistance * 0.90f
            ? EEnemyLocomotionIntent::Approach
            : OrbitIntent;
    case EEnemyTacticalState::Retreat:
        return Distance < PreferredDistance
            ? EEnemyLocomotionIntent::Retreat
            : OrbitIntent;
    case EEnemyTacticalState::Defend:
    case EEnemyTacticalState::Recover:
        return Distance < PreferredDistance
            ? EEnemyLocomotionIntent::Retreat
            : OrbitIntent;
    case EEnemyTacticalState::Observe:
    case EEnemyTacticalState::Orbit:
    default:
        return FEnemyMovementPolicy::IsMovementIntent(BaseIntent)
            ? BaseIntent
            : OrbitIntent;
    }
}

float FEnemyTacticalPolicy::GetUtilityModifier(
    const EEnemyTacticalState TacticalState,
    const EEnemyCombatAction Action,
    const float MaximumAdjustment
)
{
    const float SafeMaximum = FMath::IsFinite(MaximumAdjustment)
        ? FMath::Clamp(MaximumAdjustment, 0.0f, 0.35f)
        : 0.0f;
    float Scale = 0.0f;

    switch (TacticalState)
    {
    case EEnemyTacticalState::Observe:
        Scale = IsOffensiveAction(Action) ? -0.45f
            : (IsDefensiveAction(Action) ? 0.20f : 0.0f);
        break;
    case EEnemyTacticalState::Approach:
        if (Action == EEnemyCombatAction::DashAttack)
        {
            Scale = 0.55f;
        }
        else if (Action == EEnemyCombatAction::ProjectileAttack)
        {
            Scale = 0.35f;
        }
        else if (Action == EEnemyCombatAction::LightAttack
            || Action == EEnemyCombatAction::HeavyAttack)
        {
            Scale = -0.55f;
        }
        break;
    case EEnemyTacticalState::Orbit:
        Scale = Action == EEnemyCombatAction::ProjectileAttack ? 0.45f
            : (IsDefensiveAction(Action) ? 0.18f : 0.0f);
        break;
    case EEnemyTacticalState::Pressure:
        if (Action == EEnemyCombatAction::LightAttack)
        {
            Scale = 1.0f;
        }
        else if (Action == EEnemyCombatAction::HeavyAttack)
        {
            Scale = 0.65f;
        }
        else if (Action == EEnemyCombatAction::ProjectileAttack
            || Action == EEnemyCombatAction::DashAttack)
        {
            Scale = -0.75f;
        }
        break;
    case EEnemyTacticalState::Retreat:
        Scale = IsDefensiveAction(Action) ? 0.70f
            : (IsOffensiveAction(Action) ? -0.70f : 0.0f);
        break;
    case EEnemyTacticalState::Defend:
        if (Action == EEnemyCombatAction::Block)
        {
            Scale = 1.0f;
        }
        else if (Action == EEnemyCombatAction::Dodge)
        {
            Scale = 0.80f;
        }
        else if (IsOffensiveAction(Action))
        {
            Scale = -0.75f;
        }
        break;
    case EEnemyTacticalState::Punish:
        if (Action == EEnemyCombatAction::InterruptHeal)
        {
            Scale = 1.0f;
        }
        else if (Action == EEnemyCombatAction::HeavyAttack)
        {
            Scale = 0.80f;
        }
        else if (Action == EEnemyCombatAction::LightAttack)
        {
            Scale = 0.55f;
        }
        else if (IsDefensiveAction(Action))
        {
            Scale = -0.35f;
        }
        break;
    case EEnemyTacticalState::Recover:
        Scale = AdaptiveCombat::IsUtilityEnemyAction(Action) ? -1.0f : 0.0f;
        break;
    default:
        break;
    }

    return FMath::Clamp(Scale * SafeMaximum, -SafeMaximum, SafeMaximum);
}

bool FEnemyTacticalPolicy::IsCommittedPlayerAttack(
    const EPlayerCombatAction Action,
    const ECombatActionPhase Phase
)
{
    const bool bAttack = Action == EPlayerCombatAction::LightAttack
        || Action == EPlayerCombatAction::HeavyAttack;
    return bAttack && (Phase == ECombatActionPhase::Windup
        || Phase == ECombatActionPhase::Active);
}

EEnemyCombatAction FEnemyTacticalPolicy::SelectSequenceFollowUp(
    const float RandomValue
)
{
    const float Choice = FMath::IsFinite(RandomValue)
        ? FMath::Clamp(RandomValue, 0.0f, 1.0f)
        : 0.5f;
    return Choice < 0.55f
        ? EEnemyCombatAction::LightAttack
        : EEnemyCombatAction::HeavyAttack;
}

bool FEnemyTacticalPolicy::ShouldContinueSequence(
    const FEnemyAttackSequenceState& Sequence,
    const FEnemyTacticalContext& Context,
    const EEnemyTacticalState TacticalState,
    const double CurrentTime,
    const FEnemyTacticalTuning& Tuning
)
{
    if (!Sequence.IsActive() || !FMath::IsFinite(CurrentTime)
        || CurrentTime > Sequence.ExpiresAtTime)
    {
        return false;
    }

    const FEnemyTacticalTuning SafeTuning = Tuning.GetSanitized();
    const float Distance = SanitizeDistance(
        Context.DistanceToTarget,
        MAX_flt
    );
    const float Stamina = SanitizeNormalized(
        Context.EnemyStaminaNormalized,
        0.0f
    );
    const bool bSequenceTactic =
        TacticalState == EEnemyTacticalState::Pressure
        || TacticalState == EEnemyTacticalState::Punish;
    return bSequenceTactic && Context.bHasLineOfSight
        && !Context.bPlayerAttacking
        && Distance <= SafeTuning.SequenceExitDistance
        && Stamina > SafeTuning.LowStaminaThreshold;
}

bool FEnemyTacticalPolicy::IsProjectilePermitted(
    const bool bHasLineOfSight,
    const bool bExecutorAvailable
)
{
    return bHasLineOfSight && bExecutorAvailable;
}

bool FEnemyTacticalPolicy::IsOffensiveAction(
    const EEnemyCombatAction Action
)
{
    return Action == EEnemyCombatAction::LightAttack
        || Action == EEnemyCombatAction::HeavyAttack
        || Action == EEnemyCombatAction::ProjectileAttack
        || Action == EEnemyCombatAction::DashAttack
        || Action == EEnemyCombatAction::InterruptHeal;
}

bool FEnemyTacticalPolicy::IsDefensiveAction(
    const EEnemyCombatAction Action
)
{
    return Action == EEnemyCombatAction::Block
        || Action == EEnemyCombatAction::Dodge;
}
