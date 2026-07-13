#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Characters/AdaptiveEnemyCharacter.h"
#include "Characters/AdaptivePlayerCharacter.h"
#include "Components/CombatComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Data/CombatTypes.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCombatPhasePolicyTest,
    "adaptHunt.Milestone19.PhasePolicy",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCombatPhasePolicyTest::RunTest(const FString& Parameters)
{
    TestTrue(
        TEXT("Idle allows ordinary movement"),
        AdaptiveCombat::IsMovementAllowed(ECombatActionPhase::Idle)
    );
    TestTrue(
        TEXT("Idle allows ordinary rotation"),
        AdaptiveCombat::IsRotationAllowed(ECombatActionPhase::Idle)
    );
    TestFalse(
        TEXT("Windup prevents translation"),
        AdaptiveCombat::IsMovementAllowed(ECombatActionPhase::Windup)
    );
    TestTrue(
        TEXT("Windup still permits facing the opponent"),
        AdaptiveCombat::IsRotationAllowed(ECombatActionPhase::Windup)
    );
    TestFalse(
        TEXT("Active frames lock ordinary translation"),
        AdaptiveCombat::IsMovementAllowed(ECombatActionPhase::Active)
    );
    TestFalse(
        TEXT("Active frames lock ordinary rotation"),
        AdaptiveCombat::IsRotationAllowed(ECombatActionPhase::Active)
    );
    TestFalse(
        TEXT("Recovery prevents translation"),
        AdaptiveCombat::IsMovementAllowed(ECombatActionPhase::Recovery)
    );
    TestTrue(
        TEXT("Recovery permits smooth facing"),
        AdaptiveCombat::IsRotationAllowed(ECombatActionPhase::Recovery)
    );
    TestFalse(
        TEXT("Blocking prevents translation"),
        AdaptiveCombat::IsMovementAllowed(ECombatActionPhase::Blocking)
    );
    TestTrue(
        TEXT("Blocking permits guard facing"),
        AdaptiveCombat::IsRotationAllowed(ECombatActionPhase::Blocking)
    );
    TestTrue(
        TEXT("Dodging permits its authored launch movement"),
        AdaptiveCombat::IsMovementAllowed(ECombatActionPhase::Dodging)
    );
    TestFalse(
        TEXT("Dodging locks ordinary facing"),
        AdaptiveCombat::IsRotationAllowed(ECombatActionPhase::Dodging)
    );
    TestFalse(
        TEXT("Stagger prevents movement"),
        AdaptiveCombat::IsMovementAllowed(ECombatActionPhase::Staggered)
    );
    TestFalse(
        TEXT("Dead prevents rotation"),
        AdaptiveCombat::IsRotationAllowed(ECombatActionPhase::Dead)
    );
    TestTrue(
        TEXT("Every non-idle, non-dead phase gates new decisions"),
        AdaptiveCombat::IsCommittedPhase(ECombatActionPhase::Windup)
            && AdaptiveCombat::IsCommittedPhase(ECombatActionPhase::Active)
            && AdaptiveCombat::IsCommittedPhase(ECombatActionPhase::Recovery)
            && AdaptiveCombat::IsCommittedPhase(ECombatActionPhase::Blocking)
            && AdaptiveCombat::IsCommittedPhase(ECombatActionPhase::Dodging)
            && AdaptiveCombat::IsCommittedPhase(ECombatActionPhase::Staggered)
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCombatActiveFrameGuardTest,
    "adaptHunt.Milestone19.SingleActiveEffect",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCombatActiveFrameGuardTest::RunTest(
    const FString& Parameters
)
{
    FCombatActionRuntimeState State;
    const uint32 InitialGeneration = State.GetGeneration();
    TestEqual(
        TEXT("Runtime phase state starts Idle"),
        State.GetPhase(),
        ECombatActionPhase::Idle
    );

    State.Begin(ECombatActionPhase::Windup);
    TestEqual(
        TEXT("Each commit advances the generation"),
        State.GetGeneration(),
        InitialGeneration + 1
    );
    TestFalse(
        TEXT("A windup cannot consume the active effect"),
        State.TryConsumeActiveEffect()
    );

    State.TransitionTo(ECombatActionPhase::Active);
    TestTrue(
        TEXT("The first active-frame execution is accepted"),
        State.TryConsumeActiveEffect()
    );
    TestFalse(
        TEXT("The same committed action cannot execute twice"),
        State.TryConsumeActiveEffect()
    );

    State.TransitionTo(ECombatActionPhase::Recovery);
    TestFalse(
        TEXT("Recovery cannot execute a damaging effect"),
        State.TryConsumeActiveEffect()
    );
    State.Reset(ECombatActionPhase::Idle);
    TestEqual(
        TEXT("Reset restores Idle"),
        State.GetPhase(),
        ECombatActionPhase::Idle
    );
    TestEqual(
        TEXT("Reset invalidates the previous action generation"),
        State.GetGeneration(),
        InitialGeneration + 2
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCombatActionTimingTest,
    "adaptHunt.Milestone19.ActionTimings",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCombatActionTimingTest::RunTest(const FString& Parameters)
{
    const AAdaptivePlayerCharacter* Player =
        GetDefault<AAdaptivePlayerCharacter>();
    const AAdaptiveEnemyCharacter* Enemy =
        GetDefault<AAdaptiveEnemyCharacter>();
    const UCombatComponent* PlayerCombat = Player
        ? Player->GetCombatComponent()
        : nullptr;
    const UEnemyCombatComponent* EnemyCombat = Enemy
        ? Enemy->GetEnemyCombatComponent()
        : nullptr;
    TestNotNull(TEXT("Player combat exists"), PlayerCombat);
    TestNotNull(TEXT("Enemy combat exists"), EnemyCombat);
    if (!PlayerCombat || !EnemyCombat)
    {
        return false;
    }

    const FCombatActionTiming PlayerLight =
        PlayerCombat->GetActionTiming(EPlayerCombatAction::LightAttack);
    const FCombatActionTiming PlayerHeavy =
        PlayerCombat->GetActionTiming(EPlayerCombatAction::HeavyAttack);
    TestTrue(
        TEXT("Player light attacks have readable windup/active/recovery"),
        PlayerLight.WindupDuration > 0.0f
            && PlayerLight.ActiveDuration > 0.0f
            && PlayerLight.RecoveryDuration > 0.0f
    );
    TestTrue(
        TEXT("Player heavy attacks are slower than light attacks"),
        PlayerHeavy.GetTotalDuration() > PlayerLight.GetTotalDuration()
    );
    TestTrue(
        TEXT("Player heavy attacks remain more damaging"),
        PlayerCombat->GetHeavyAttackDamage()
            > PlayerCombat->GetLightAttackDamage()
    );

    const EEnemyCombatAction DamagingEnemyActions[] = {
        EEnemyCombatAction::LightAttack,
        EEnemyCombatAction::HeavyAttack,
        EEnemyCombatAction::ProjectileAttack,
        EEnemyCombatAction::DashAttack,
        EEnemyCombatAction::InterruptHeal
    };
    for (const EEnemyCombatAction Action : DamagingEnemyActions)
    {
        const FCombatActionTiming Timing =
            EnemyCombat->GetActionTiming(Action);
        TestTrue(
            TEXT("Every damaging enemy action has three non-zero phases"),
            Timing.WindupDuration > 0.0f
                && Timing.ActiveDuration > 0.0f
                && Timing.RecoveryDuration > 0.0f
        );
    }
    TestTrue(
        TEXT("Enemy heavy attacks are slower than enemy light attacks"),
        EnemyCombat->GetActionTiming(EEnemyCombatAction::HeavyAttack)
                .GetTotalDuration()
            > EnemyCombat->GetActionTiming(EEnemyCombatAction::LightAttack)
                .GetTotalDuration()
    );
    TestTrue(
        TEXT("Enemy heavy attacks remain more damaging"),
        EnemyCombat->GetHeavyAttackDamage()
            > EnemyCombat->GetLightAttackDamage()
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCombatPhaseExposureTest,
    "adaptHunt.Milestone19.PhaseExposureAndDefaults",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCombatPhaseExposureTest::RunTest(
    const FString& Parameters
)
{
    const UCombatComponent* PlayerCombat =
        GetDefault<UCombatComponent>();
    const UEnemyCombatComponent* EnemyCombat =
        GetDefault<UEnemyCombatComponent>();
    TestNotNull(TEXT("Player combat CDO exists"), PlayerCombat);
    TestNotNull(TEXT("Enemy combat CDO exists"), EnemyCombat);
    if (!PlayerCombat || !EnemyCombat)
    {
        return false;
    }

    TestEqual(
        TEXT("Player combat starts Idle"),
        PlayerCombat->GetCurrentPhase(),
        ECombatActionPhase::Idle
    );
    TestEqual(
        TEXT("Enemy combat starts Idle"),
        EnemyCombat->GetCurrentPhase(),
        ECombatActionPhase::Idle
    );
    TestNotNull(
        TEXT("Player phase changes are Blueprint-exposed"),
        FindFProperty<FMulticastDelegateProperty>(
            UCombatComponent::StaticClass(),
            GET_MEMBER_NAME_CHECKED(
                UCombatComponent,
                OnActionPhaseChanged
            )
        )
    );
    TestNotNull(
        TEXT("Enemy phase changes are Blueprint-exposed"),
        FindFProperty<FMulticastDelegateProperty>(
            UEnemyCombatComponent::StaticClass(),
            GET_MEMBER_NAME_CHECKED(
                UEnemyCombatComponent,
                OnActionPhaseChanged
            )
        )
    );
    return true;
}

#endif
