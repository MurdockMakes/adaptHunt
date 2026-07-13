#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/EnemyActionScorer.h"
#include "AI/EnemyTacticalPolicy.h"
#include "Components/EnemyDecisionComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEnemyTacticalStateSelectionTest,
    "adaptHunt.Milestone22.StateSelectionAndHysteresis",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEnemyTacticalStateSelectionTest::RunTest(
    const FString& Parameters
)
{
    FEnemyTacticalTuning Tuning;
    FEnemyTacticalContext Context;
    Context.bDashAvailable = true;
    Context.bProjectileAvailable = true;
    Context.DistanceToTarget = 750.0f;

    TestTrue(
        TEXT("A distant target selects approach"),
        FEnemyTacticalPolicy::SelectState(
            Context,
            EEnemyTacticalState::Observe,
            2.0f,
            Tuning
        ) == EEnemyTacticalState::Approach
    );

    Context.DistanceToTarget = 220.0f;
    Context.bLightAttackAvailable = true;
    Context.bHeavyAttackAvailable = true;
    TestTrue(
        TEXT("State commitment holds orbit briefly across a range boundary"),
        FEnemyTacticalPolicy::SelectState(
            Context,
            EEnemyTacticalState::Orbit,
            0.2f,
            Tuning
        ) == EEnemyTacticalState::Orbit
    );
    TestTrue(
        TEXT("The same close target becomes pressure after commitment"),
        FEnemyTacticalPolicy::SelectState(
            Context,
            EEnemyTacticalState::Orbit,
            1.0f,
            Tuning
        ) == EEnemyTacticalState::Pressure
    );

    Context.DistanceToTarget = 120.0f;
    TestTrue(
        TEXT("Crowding bypasses commitment and immediately creates space"),
        FEnemyTacticalPolicy::SelectState(
            Context,
            EEnemyTacticalState::Pressure,
            0.1f,
            Tuning
        ) == EEnemyTacticalState::Retreat
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEnemyTacticalResourcesAndFairnessTest,
    "adaptHunt.Milestone22.ResourcesDefenseAndCommittedInput",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEnemyTacticalResourcesAndFairnessTest::RunTest(
    const FString& Parameters
)
{
    FEnemyTacticalTuning Tuning;
    FEnemyTacticalContext Context;
    Context.DistanceToTarget = 300.0f;
    Context.bLightAttackAvailable = true;
    Context.EnemyStaminaNormalized = 0.15f;

    TestTrue(
        TEXT("Low stamina selects recovery despite an affordable action"),
        FEnemyTacticalPolicy::SelectState(
            Context,
            EEnemyTacticalState::Pressure,
            2.0f,
            Tuning
        ) == EEnemyTacticalState::Recover
    );

    Context.EnemyStaminaNormalized = 0.40f;
    TestTrue(
        TEXT("Recovery hysteresis waits for a useful stamina reserve"),
        FEnemyTacticalPolicy::SelectState(
            Context,
            EEnemyTacticalState::Recover,
            2.0f,
            Tuning
        ) == EEnemyTacticalState::Recover
    );

    Context.EnemyStaminaNormalized = 0.80f;
    Context.bPlayerAttacking = true;
    Context.bBlockAvailable = true;
    TestTrue(
        TEXT("A committed nearby player attack allows defense"),
        FEnemyTacticalPolicy::SelectState(
            Context,
            EEnemyTacticalState::Orbit,
            2.0f,
            Tuning
        ) == EEnemyTacticalState::Defend
    );

    TestTrue(
        TEXT("A player attack is visible once windup has committed"),
        FEnemyTacticalPolicy::IsCommittedPlayerAttack(
            EPlayerCombatAction::HeavyAttack,
            ECombatActionPhase::Windup
        )
    );
    TestFalse(
        TEXT("An idle label cannot expose uncommitted player input"),
        FEnemyTacticalPolicy::IsCommittedPlayerAttack(
            EPlayerCombatAction::HeavyAttack,
            ECombatActionPhase::Idle
        )
    );
    TestFalse(
        TEXT("Non-attacks never become an attacking threat"),
        FEnemyTacticalPolicy::IsCommittedPlayerAttack(
            EPlayerCombatAction::Block,
            ECombatActionPhase::Active
        )
    );

    Context.bPlayerAttacking = false;
    Context.PredictedPlayerAction = EPlayerCombatAction::LightAttack;
    Context.PredictionConfidence = 0.80f;
    TestTrue(
        TEXT("An applied high-confidence prediction can bias defense"),
        FEnemyTacticalPolicy::SelectState(
            Context,
            EEnemyTacticalState::Orbit,
            2.0f,
            Tuning
        ) == EEnemyTacticalState::Defend
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEnemyTacticalUtilityAndSequenceTest,
    "adaptHunt.Milestone22.BoundedUtilityAndAttackSequence",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEnemyTacticalUtilityAndSequenceTest::RunTest(
    const FString& Parameters
)
{
    constexpr float MaximumAdjustment = 0.22f;
    const EEnemyTacticalState States[] = {
        EEnemyTacticalState::Observe,
        EEnemyTacticalState::Approach,
        EEnemyTacticalState::Orbit,
        EEnemyTacticalState::Pressure,
        EEnemyTacticalState::Retreat,
        EEnemyTacticalState::Defend,
        EEnemyTacticalState::Punish,
        EEnemyTacticalState::Recover
    };
    const EEnemyCombatAction Actions[] = {
        EEnemyCombatAction::LightAttack,
        EEnemyCombatAction::HeavyAttack,
        EEnemyCombatAction::ProjectileAttack,
        EEnemyCombatAction::DashAttack,
        EEnemyCombatAction::Block,
        EEnemyCombatAction::Dodge,
        EEnemyCombatAction::InterruptHeal
    };
    for (const EEnemyTacticalState State : States)
    {
        for (const EEnemyCombatAction Action : Actions)
        {
            TestTrue(
                TEXT("Every tactical utility modifier is hard bounded"),
                FMath::Abs(FEnemyTacticalPolicy::GetUtilityModifier(
                    State,
                    Action,
                    MaximumAdjustment
                )) <= MaximumAdjustment + KINDA_SMALL_NUMBER
            );
        }
    }

    FEnemyActionScorer Scorer;
    FEnemyActionScoringContext BaseContext;
    BaseContext.Snapshot.DistanceCategory = ECombatDistanceCategory::Medium;
    const float BaseBlockScore = Scorer.ScoreAction(
        EEnemyCombatAction::Block,
        BaseContext
    ).Score;
    BaseContext.MaximumTacticalUtilityAdjustment = 0.20f;
    BaseContext.TacticalUtilityModifiers.Add(
        EEnemyCombatAction::Block,
        100.0f
    );
    TestTrue(
        TEXT("The scorer independently clamps caller-provided adjustments"),
        FMath::IsNearlyEqual(
            Scorer.ScoreAction(
                EEnemyCombatAction::Block,
                BaseContext
            ).Score,
            BaseBlockScore + 0.20f
        )
    );

    TestTrue(
        TEXT("The seeded sequence choice supports light-light"),
        FEnemyTacticalPolicy::SelectSequenceFollowUp(0.10f)
            == EEnemyCombatAction::LightAttack
    );
    TestTrue(
        TEXT("The seeded sequence choice supports light-heavy"),
        FEnemyTacticalPolicy::SelectSequenceFollowUp(0.90f)
            == EEnemyCombatAction::HeavyAttack
    );

    FEnemyAttackSequenceState Sequence;
    Sequence.Start(EEnemyCombatAction::HeavyAttack, 2.0);
    FEnemyTacticalContext SequenceContext;
    SequenceContext.DistanceToTarget = 180.0f;
    SequenceContext.EnemyStaminaNormalized = 0.80f;
    SequenceContext.bHasLineOfSight = true;
    FEnemyTacticalTuning Tuning;
    TestTrue(
        TEXT("A valid close-range follow-up remains active"),
        FEnemyTacticalPolicy::ShouldContinueSequence(
            Sequence,
            SequenceContext,
            EEnemyTacticalState::Pressure,
            1.0,
            Tuning
        )
    );
    SequenceContext.DistanceToTarget = 400.0f;
    TestFalse(
        TEXT("Leaving close range explicitly exits the sequence"),
        FEnemyTacticalPolicy::ShouldContinueSequence(
            Sequence,
            SequenceContext,
            EEnemyTacticalState::Pressure,
            1.0,
            Tuning
        )
    );
    SequenceContext.DistanceToTarget = 180.0f;
    TestFalse(
        TEXT("The sequence deadline is an explicit exit"),
        FEnemyTacticalPolicy::ShouldContinueSequence(
            Sequence,
            SequenceContext,
            EEnemyTacticalState::Pressure,
            3.0,
            Tuning
        )
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEnemyTacticalSpacingAndLineOfSightTest,
    "adaptHunt.Milestone22.SpacingCooldownsAndLineOfSight",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEnemyTacticalSpacingAndLineOfSightTest::RunTest(
    const FString& Parameters
)
{
    FEnemyTacticalContext Context;
    Context.DistanceToTarget = 150.0f;
    TestTrue(
        TEXT("Recovery retreats until preferred spacing is restored"),
        FEnemyTacticalPolicy::SelectMovementIntent(
            Context,
            EEnemyTacticalState::Recover,
            EEnemyLocomotionIntent::Retreat,
            false
        ) == EEnemyLocomotionIntent::Retreat
    );

    Context.DistanceToTarget = 500.0f;
    TestTrue(
        TEXT("Recovery orbits after creating enough space"),
        FEnemyTacticalPolicy::SelectMovementIntent(
            Context,
            EEnemyTacticalState::Recover,
            EEnemyLocomotionIntent::OrbitLeft,
            true
        ) == EEnemyLocomotionIntent::OrbitRight
    );

    Context.DistanceToTarget = 300.0f;
    TestTrue(
        TEXT("Pressure closes to actual melee range"),
        FEnemyTacticalPolicy::SelectMovementIntent(
            Context,
            EEnemyTacticalState::Pressure,
            EEnemyLocomotionIntent::OrbitLeft,
            false
        ) == EEnemyLocomotionIntent::Approach
    );

    TestFalse(
        TEXT("An available projectile is rejected without line of sight"),
        FEnemyTacticalPolicy::IsProjectilePermitted(false, true)
    );
    TestFalse(
        TEXT("Line of sight cannot bypass projectile cooldown availability"),
        FEnemyTacticalPolicy::IsProjectilePermitted(true, false)
    );
    TestTrue(
        TEXT("Projectile use requires both line of sight and availability"),
        FEnemyTacticalPolicy::IsProjectilePermitted(true, true)
    );

    FEnemyTacticalTuning Tuning;
    Context.bLightAttackAvailable = false;
    Context.bHeavyAttackAvailable = false;
    Context.bProjectileAvailable = false;
    Context.bDashAvailable = false;
    TestTrue(
        TEXT("All offensive cooldowns produce observation instead of spam"),
        FEnemyTacticalPolicy::SelectState(
            Context,
            EEnemyTacticalState::Pressure,
            2.0f,
            Tuning
        ) == EEnemyTacticalState::Observe
    );

    const UEnemyDecisionComponent* Defaults =
        GetDefault<UEnemyDecisionComponent>();
    TestTrue(
        TEXT("Default tactical tuning has useful hysteresis and bounds"),
        Defaults
            && Defaults->GetTacticalTuning().StateCommitDuration > 0.0f
            && Defaults->GetTacticalTuning().DistanceHysteresis > 0.0f
            && Defaults->GetTacticalTuning().MaxUtilityAdjustment > 0.0f
            && Defaults->GetTacticalTuning().MaxUtilityAdjustment <= 0.35f
    );
    return true;
}

#endif
