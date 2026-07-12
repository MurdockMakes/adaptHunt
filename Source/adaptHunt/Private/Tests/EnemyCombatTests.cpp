#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Characters/AdaptiveEnemyCharacter.h"
#include "Combat/EnemyProjectile.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/EnemyDecisionComponent.h"
#include "Components/HealthComponent.h"
#include "Components/StaminaComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Game/AdaptiveGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEnemyDefaultsTest,
    "adaptHunt.Milestone5.EnemyDefaults",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEnemyDefaultsTest::RunTest(const FString& Parameters)
{
    const AAdaptiveEnemyCharacter* Enemy =
        GetDefault<AAdaptiveEnemyCharacter>();
    TestNotNull(TEXT("The adaptive enemy CDO exists"), Enemy);
    if (!Enemy)
    {
        return false;
    }

    TestFalse(
        TEXT("The enemy actor does not run an AI Tick"),
        Enemy->PrimaryActorTick.bCanEverTick
    );
    TestNotNull(TEXT("The enemy has health"), Enemy->GetHealthComponent());
    TestNotNull(
        TEXT("The enemy has stamina"),
        Enemy->GetStaminaComponent()
    );
    TestNotNull(
        TEXT("The enemy has a combat executor"),
        Enemy->GetEnemyCombatComponent()
    );
    TestNotNull(
        TEXT("The enemy has a baseline decision component"),
        Enemy->GetEnemyDecisionComponent()
    );
    TestNotNull(
        TEXT("The enemy greybox body uses a primitive mesh"),
        Enemy->GetBodyMesh()
            ? Enemy->GetBodyMesh()->GetStaticMesh().Get()
            : nullptr
    );
    TestTrue(
        TEXT("The enemy movement speed is slower than the player baseline"),
        Enemy->GetCharacterMovement()->MaxWalkSpeed > 0.0f
            && Enemy->GetCharacterMovement()->MaxWalkSpeed < 500.0f
    );

    const AAdaptiveGameMode* GameMode = GetDefault<AAdaptiveGameMode>();
    TestTrue(
        TEXT("The game mode is configured to create the adaptive enemy"),
        GameMode
            && GameMode->GetEnemyClass()
                == AAdaptiveEnemyCharacter::StaticClass()
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEnemyActionCoverageTest,
    "adaptHunt.Milestone5.EnemyActionCoverage",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEnemyActionCoverageTest::RunTest(const FString& Parameters)
{
    const AAdaptiveEnemyCharacter* Enemy =
        GetDefault<AAdaptiveEnemyCharacter>();
    const UEnemyCombatComponent* Combat = Enemy
        ? Enemy->GetEnemyCombatComponent()
        : nullptr;
    const UEnemyDecisionComponent* Decision = Enemy
        ? Enemy->GetEnemyDecisionComponent()
        : nullptr;
    TestNotNull(TEXT("Enemy combat exists"), Combat);
    TestNotNull(TEXT("Enemy decisions exist"), Decision);
    if (!Combat || !Decision)
    {
        return false;
    }

    const EEnemyCombatAction RequiredActions[] = {
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
    for (const EEnemyCombatAction Action : RequiredActions)
    {
        TestTrue(
            TEXT("Every required enemy action is executable"),
            Combat->SupportsAction(Action)
        );
    }
    TestFalse(
        TEXT("None is not treated as an executable action"),
        Combat->SupportsAction(EEnemyCombatAction::None)
    );

    TestTrue(
        TEXT("Heavy melee is stronger than light melee"),
        Combat->GetHeavyAttackDamage() > Combat->GetLightAttackDamage()
    );
    TestTrue(
        TEXT("Ranged projectiles have damage and speed"),
        Combat->GetProjectileDamage() > 0.0f
            && Combat->GetProjectileSpeed() > 0.0f
    );
    TestFalse(
        TEXT("Decision updates use a timer instead of component Tick"),
        Decision->PrimaryComponentTick.bCanEverTick
    );
    TestTrue(
        TEXT("Decision timing is configured"),
        Decision->GetEvaluationInterval() > 0.0f
            && Decision->GetCombatDecisionInterval() > 0.0f
    );

    TestTrue(
        TEXT("The baseline closes distance when far away"),
        Decision->SelectMovementAction(
            Decision->GetStrafeDistance() + 100.0f
        ) == EEnemyCombatAction::MoveTowardPlayer
    );
    TestTrue(
        TEXT("The baseline retreats when crowded"),
        Decision->SelectMovementAction(
            Decision->GetRetreatDistance() - 1.0f
        ) == EEnemyCombatAction::MoveAwayFromPlayer
    );
    TestTrue(
        TEXT("The baseline can interrupt a nearby heal"),
        Decision->SelectCombatAction(100.0f, true, 0.5f)
            == EEnemyCombatAction::InterruptHeal
    );
    TestTrue(
        TEXT("The baseline uses dash attacks at extreme range"),
        Decision->SelectCombatAction(1200.0f, false, 0.0f)
            == EEnemyCombatAction::DashAttack
    );
    TestTrue(
        TEXT("The close-range baseline includes defensive actions"),
        Decision->SelectCombatAction(100.0f, false, 0.72f)
            == EEnemyCombatAction::Block
    );

    const AEnemyProjectile* Projectile = GetDefault<AEnemyProjectile>();
    TestNotNull(TEXT("The enemy projectile CDO exists"), Projectile);
    TestNotNull(
        TEXT("The projectile has collision"),
        Projectile ? Projectile->GetCollisionSphere() : nullptr
    );
    TestNotNull(
        TEXT("The projectile has deterministic movement"),
        Projectile ? Projectile->GetProjectileMovement() : nullptr
    );

    return true;
}

#endif
