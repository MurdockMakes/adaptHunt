#include "AI/EnemyActionScorer.h"

#include "AI/EnemyDecisionPolicy.h"

namespace
{
const EEnemyCombatAction CandidateActions[] = {
    EEnemyCombatAction::LightAttack,
    EEnemyCombatAction::HeavyAttack,
    EEnemyCombatAction::ProjectileAttack,
    EEnemyCombatAction::DashAttack,
    EEnemyCombatAction::Block,
    EEnemyCombatAction::Dodge,
    EEnemyCombatAction::InterruptHeal
};

float SanitizeNormalized(const float Value, const float Fallback)
{
    return FMath::IsFinite(Value)
        ? FMath::Clamp(Value, 0.0f, 1.0f)
        : Fallback;
}

bool HasUsablePrediction(const FPredictionResult& Prediction)
{
    return Prediction.bHasPrediction
        && AdaptiveCombat::IsTrackablePlayerAction(
            Prediction.PredictedAction
        )
        && FMath::IsFinite(Prediction.Confidence)
        && Prediction.Confidence > 0.0f
        && Prediction.SupportingSampleCount > 0;
}

bool IsOffensiveAction(const EEnemyCombatAction Action)
{
    return Action == EEnemyCombatAction::LightAttack
        || Action == EEnemyCombatAction::HeavyAttack
        || Action == EEnemyCombatAction::ProjectileAttack
        || Action == EEnemyCombatAction::DashAttack
        || Action == EEnemyCombatAction::InterruptHeal;
}

float GetBaseUtility(const EEnemyCombatAction Action)
{
    switch (Action)
    {
    case EEnemyCombatAction::LightAttack:
        return 0.45f;
    case EEnemyCombatAction::HeavyAttack:
        return 0.38f;
    case EEnemyCombatAction::ProjectileAttack:
        return 0.35f;
    case EEnemyCombatAction::DashAttack:
        return 0.30f;
    case EEnemyCombatAction::Block:
    case EEnemyCombatAction::Dodge:
        return 0.25f;
    case EEnemyCombatAction::InterruptHeal:
        return 0.05f;
    default:
        return 0.0f;
    }
}

float GetDistanceUtility(
    const EEnemyCombatAction Action,
    const ECombatDistanceCategory Distance
)
{
    if (Distance == ECombatDistanceCategory::Close)
    {
        switch (Action)
        {
        case EEnemyCombatAction::LightAttack:
            return 0.35f;
        case EEnemyCombatAction::HeavyAttack:
            return 0.28f;
        case EEnemyCombatAction::ProjectileAttack:
            return -0.30f;
        case EEnemyCombatAction::DashAttack:
            return -0.25f;
        case EEnemyCombatAction::Block:
            return 0.18f;
        case EEnemyCombatAction::Dodge:
            return 0.10f;
        case EEnemyCombatAction::InterruptHeal:
            return 0.15f;
        default:
            return 0.0f;
        }
    }

    if (Distance == ECombatDistanceCategory::Far)
    {
        switch (Action)
        {
        case EEnemyCombatAction::LightAttack:
            return -0.50f;
        case EEnemyCombatAction::HeavyAttack:
            return -0.45f;
        case EEnemyCombatAction::ProjectileAttack:
            return 0.40f;
        case EEnemyCombatAction::DashAttack:
            return 0.50f;
        case EEnemyCombatAction::Block:
            return -0.05f;
        case EEnemyCombatAction::Dodge:
            return 0.05f;
        default:
            return 0.0f;
        }
    }

    switch (Action)
    {
    case EEnemyCombatAction::LightAttack:
        return 0.05f;
    case EEnemyCombatAction::HeavyAttack:
        return 0.10f;
    case EEnemyCombatAction::ProjectileAttack:
        return 0.30f;
    case EEnemyCombatAction::DashAttack:
        return 0.20f;
    case EEnemyCombatAction::Block:
        return 0.05f;
    case EEnemyCombatAction::Dodge:
        return 0.08f;
    case EEnemyCombatAction::InterruptHeal:
        return 0.05f;
    default:
        return 0.0f;
    }
}

float GetStaminaUtility(
    const EEnemyCombatAction Action,
    const float EnemyStaminaNormalized
)
{
    if (EnemyStaminaNormalized < 0.25f)
    {
        switch (Action)
        {
        case EEnemyCombatAction::HeavyAttack:
            return -0.35f;
        case EEnemyCombatAction::ProjectileAttack:
            return -0.08f;
        case EEnemyCombatAction::DashAttack:
            return -0.45f;
        case EEnemyCombatAction::Block:
            return -0.15f;
        case EEnemyCombatAction::Dodge:
            return -0.25f;
        default:
            return 0.0f;
        }
    }

    if (EnemyStaminaNormalized > 0.70f)
    {
        switch (Action)
        {
        case EEnemyCombatAction::HeavyAttack:
            return 0.08f;
        case EEnemyCombatAction::DashAttack:
            return 0.10f;
        case EEnemyCombatAction::Dodge:
            return 0.05f;
        default:
            return 0.0f;
        }
    }

    return 0.0f;
}

float GetPlayerHistoryUtility(
    const EEnemyCombatAction Action,
    const EPlayerCombatAction PreviousPlayerAction
)
{
    if (PreviousPlayerAction == EPlayerCombatAction::HeavyAttack)
    {
        if (Action == EEnemyCombatAction::Dodge)
        {
            return 0.30f;
        }
        if (Action == EEnemyCombatAction::Block)
        {
            return 0.25f;
        }
    }
    else if (PreviousPlayerAction == EPlayerCombatAction::LightAttack)
    {
        if (Action == EEnemyCombatAction::Block)
        {
            return 0.18f;
        }
        if (Action == EEnemyCombatAction::Dodge)
        {
            return 0.12f;
        }
    }
    else if (PreviousPlayerAction == EPlayerCombatAction::Block
        && Action == EEnemyCombatAction::HeavyAttack)
    {
        return 0.18f;
    }

    return 0.0f;
}

float GetPredictionUtility(
    const EEnemyCombatAction Action,
    const EPlayerCombatAction PredictedPlayerAction
)
{
    switch (PredictedPlayerAction)
    {
    case EPlayerCombatAction::LightAttack:
        if (Action == EEnemyCombatAction::Block)
        {
            return 0.24f;
        }
        if (Action == EEnemyCombatAction::Dodge)
        {
            return 0.16f;
        }
        break;

    case EPlayerCombatAction::HeavyAttack:
        if (Action == EEnemyCombatAction::Dodge)
        {
            return 0.42f;
        }
        if (Action == EEnemyCombatAction::Block)
        {
            return 0.22f;
        }
        break;

    case EPlayerCombatAction::DodgeLeft:
    case EPlayerCombatAction::DodgeRight:
        if (Action == EEnemyCombatAction::HeavyAttack)
        {
            return 0.34f;
        }
        if (Action == EEnemyCombatAction::DashAttack)
        {
            return 0.26f;
        }
        if (Action == EEnemyCombatAction::ProjectileAttack)
        {
            return 0.14f;
        }
        break;

    case EPlayerCombatAction::DodgeBackward:
        if (Action == EEnemyCombatAction::DashAttack)
        {
            return 0.44f;
        }
        if (Action == EEnemyCombatAction::ProjectileAttack)
        {
            return 0.28f;
        }
        break;

    case EPlayerCombatAction::Block:
        if (Action == EEnemyCombatAction::HeavyAttack)
        {
            return 0.38f;
        }
        if (Action == EEnemyCombatAction::ProjectileAttack)
        {
            return 0.20f;
        }
        break;

    case EPlayerCombatAction::Heal:
        if (Action == EEnemyCombatAction::InterruptHeal)
        {
            return 0.78f;
        }
        if (Action == EEnemyCombatAction::DashAttack)
        {
            return 0.36f;
        }
        if (Action == EEnemyCombatAction::ProjectileAttack)
        {
            return 0.26f;
        }
        if (Action == EEnemyCombatAction::LightAttack)
        {
            return 0.16f;
        }
        break;

    default:
        break;
    }

    return 0.0f;
}
}

TArray<FEnemyActionScore> FEnemyActionScorer::ScoreActions(
    const FEnemyActionScoringContext& Context
) const
{
    TArray<FEnemyActionScore> Scores;
    Scores.Reserve(UE_ARRAY_COUNT(CandidateActions));
    for (const EEnemyCombatAction Action : CandidateActions)
    {
        Scores.Add(ScoreAction(Action, Context));
    }
    return Scores;
}

FEnemyActionScore FEnemyActionScorer::ScoreAction(
    const EEnemyCombatAction Action,
    const FEnemyActionScoringContext& Context
) const
{
    FEnemyActionScore Result;
    Result.Action = Action;
    Result.bAvailable = AdaptiveCombat::IsUtilityEnemyAction(Action)
        && !Context.UnavailableActions.Contains(Action);
    if (!Result.bAvailable)
    {
        return Result;
    }

    const FCombatSnapshot& Snapshot = Context.Snapshot;
    const float PlayerHealth = SanitizeNormalized(
        Snapshot.PlayerHealthNormalized,
        1.0f
    );
    const float EnemyHealth = SanitizeNormalized(
        Snapshot.EnemyHealthNormalized,
        1.0f
    );
    const float EnemyStamina = SanitizeNormalized(
        Snapshot.EnemyStaminaNormalized,
        0.0f
    );

    float Utility = GetBaseUtility(Action);
    Utility += GetDistanceUtility(Action, Snapshot.DistanceCategory);
    Utility += GetStaminaUtility(Action, EnemyStamina);
    Utility += GetPlayerHistoryUtility(
        Action,
        Snapshot.PreviousPlayerAction
    );

    const float DefensiveUrgency = 1.0f - EnemyHealth;
    if (Action == EEnemyCombatAction::Block)
    {
        Utility += DefensiveUrgency * 0.50f;
    }
    else if (Action == EEnemyCombatAction::Dodge)
    {
        Utility += DefensiveUrgency * 0.42f;
    }
    else if (IsOffensiveAction(Action))
    {
        Utility += (1.0f - PlayerHealth) * 0.20f;
        Utility -= DefensiveUrgency * 0.18f;
    }

    if (Snapshot.RelativePlayerPosition == ERelativePlayerPosition::Behind)
    {
        if (Action == EEnemyCombatAction::Dodge)
        {
            Utility += 0.25f;
        }
        else if (Action == EEnemyCombatAction::Block)
        {
            Utility += 0.10f;
        }
        else if (IsOffensiveAction(Action))
        {
            Utility -= 0.10f;
        }
    }

    if (Action == EEnemyCombatAction::InterruptHeal)
    {
        Utility = Context.bTargetRecentlyHealing
            ? Utility + 0.80f
            : 0.0f;
    }
    else if (Context.bTargetRecentlyHealing
        && (Action == EEnemyCombatAction::DashAttack
            || Action == EEnemyCombatAction::ProjectileAttack))
    {
        Utility += 0.12f;
    }

    if (HasUsablePrediction(Context.Prediction))
    {
        const float PredictionConfidence = FMath::Clamp(
            Context.Prediction.Confidence,
            0.0f,
            1.0f
        );
        const float PredictionInfluence = FMath::IsFinite(
            Context.PredictionInfluence
        )
            ? FMath::Clamp(Context.PredictionInfluence, 0.0f, 1.0f)
            : 0.0f;
        const float PredictionStrength = PredictionConfidence
            * PredictionInfluence;
        Utility += GetPredictionUtility(
            Action,
            Context.Prediction.PredictedAction
        ) * PredictionStrength;
    }

    const float LightAttackPressure = SanitizeNormalized(
        Context.RecentLightAttackPressure,
        0.0f
    );
    const float MaximumPatternAdjustment = FMath::IsFinite(
        Context.MaximumLightAttackPatternAdjustment
    )
        ? FMath::Clamp(
            Context.MaximumLightAttackPatternAdjustment,
            0.0f,
            0.65f
        )
        : 0.0f;
    if (LightAttackPressure > 0.0f)
    {
        float PatternResponseScale = 0.0f;
        if (Action == EEnemyCombatAction::Block)
        {
            PatternResponseScale = 1.0f;
        }
        else if (Action == EEnemyCombatAction::Dodge)
        {
            PatternResponseScale = 0.75f;
        }
        else if (IsOffensiveAction(Action))
        {
            // Stop damage racing once repeated light attacks are established.
            PatternResponseScale = -0.65f;
        }
        Utility += PatternResponseScale * MaximumPatternAdjustment
            * LightAttackPressure;
    }

    const float SafeMaximumTacticalAdjustment = FMath::IsFinite(
        Context.MaximumTacticalUtilityAdjustment
    )
        ? FMath::Clamp(
            Context.MaximumTacticalUtilityAdjustment,
            0.0f,
            0.35f
        )
        : 0.0f;
    if (const float* TacticalAdjustment =
        Context.TacticalUtilityModifiers.Find(Action))
    {
        Utility += FMath::IsFinite(*TacticalAdjustment)
            ? FMath::Clamp(
                *TacticalAdjustment,
                -SafeMaximumTacticalAdjustment,
                SafeMaximumTacticalAdjustment
            )
            : 0.0f;
    }

    const float SafeMaximumAdaptiveAdjustment = FMath::IsFinite(
        Context.MaximumAdaptiveUtilityAdjustment
    )
        ? FMath::Clamp(
            Context.MaximumAdaptiveUtilityAdjustment,
            0.0f,
            0.25f
        )
        : 0.0f;
    if (const float* AdaptiveAdjustment =
        Context.AdaptiveUtilityModifiers.Find(Action))
    {
        Utility += FMath::IsFinite(*AdaptiveAdjustment)
            ? FMath::Clamp(
                *AdaptiveAdjustment,
                -SafeMaximumAdaptiveAdjustment,
                SafeMaximumAdaptiveAdjustment
            )
            : 0.0f;
    }

    const float SafeMaximumOutcomeAdjustment = FMath::IsFinite(
        Context.MaximumOutcomeEffectivenessUtilityAdjustment
    )
        ? FMath::Clamp(
            Context.MaximumOutcomeEffectivenessUtilityAdjustment,
            0.0f,
            0.10f
        )
        : 0.0f;
    if (const float* OutcomeAdjustment =
        Context.OutcomeEffectivenessUtilityModifiers.Find(Action))
    {
        Utility += FMath::IsFinite(*OutcomeAdjustment)
            ? FMath::Clamp(
                *OutcomeAdjustment,
                -SafeMaximumOutcomeAdjustment,
                SafeMaximumOutcomeAdjustment
            )
            : 0.0f;
    }

    const float SafeMaximumRepetitionAdjustment = FMath::IsFinite(
        Context.MaximumRepetitionUtilityAdjustment
    )
        ? FMath::Clamp(
            Context.MaximumRepetitionUtilityAdjustment,
            0.0f,
            0.6f
        )
        : 0.0f;
    if (const float* RepetitionAdjustment =
        Context.RepetitionUtilityModifiers.Find(Action))
    {
        Utility += FMath::IsFinite(*RepetitionAdjustment)
            ? FMath::Clamp(
                *RepetitionAdjustment,
                -SafeMaximumRepetitionAdjustment,
                SafeMaximumRepetitionAdjustment
            )
            : 0.0f;
    }

    const float SafeMaximumBalanceAdjustment = FMath::IsFinite(
        Context.MaximumOffenseDefenseBalanceUtilityAdjustment
    )
        ? FMath::Clamp(
            Context.MaximumOffenseDefenseBalanceUtilityAdjustment,
            0.0f,
            0.5f
        )
        : 0.0f;
    if (const float* BalanceAdjustment =
        Context.OffenseDefenseBalanceUtilityModifiers.Find(Action))
    {
        Utility += FMath::IsFinite(*BalanceAdjustment)
            ? FMath::Clamp(
                *BalanceAdjustment,
                -SafeMaximumBalanceAdjustment,
                SafeMaximumBalanceAdjustment
            )
            : 0.0f;
    }

    const float SafeMaximumRecentOutcomeAdjustment = FMath::IsFinite(
        Context.MaximumRecentOutcomeUtilityAdjustment
    )
        ? FMath::Clamp(
            Context.MaximumRecentOutcomeUtilityAdjustment,
            0.0f,
            0.4f
        )
        : 0.0f;
    if (const float* RecentOutcomeAdjustment =
        Context.RecentOutcomeUtilityModifiers.Find(Action))
    {
        Utility += FMath::IsFinite(*RecentOutcomeAdjustment)
            ? FMath::Clamp(
                *RecentOutcomeAdjustment,
                -SafeMaximumRecentOutcomeAdjustment,
                SafeMaximumRecentOutcomeAdjustment
            )
            : 0.0f;
    }

    Result.UnclampedScore = FMath::IsFinite(Utility) ? Utility : 0.0f;
    Result.Score = FMath::Clamp(Result.UnclampedScore, 0.0f, 1.0f);
    return Result;
}

EEnemyCombatAction FEnemyActionScorer::SelectBestAction(
    const FEnemyActionScoringContext& Context
) const
{
    return SelectBestAction(ScoreActions(Context));
}

EEnemyCombatAction FEnemyActionScorer::SelectBestAction(
    const TArray<FEnemyActionScore>& Scores
) const
{
    return FEnemyActionSelectionPolicy::SelectBest(Scores);
}
