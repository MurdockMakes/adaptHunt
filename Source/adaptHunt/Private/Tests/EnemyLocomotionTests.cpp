#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/EnemyMovementPolicy.h"
#include "AIController.h"
#include "Characters/AdaptiveEnemyCharacter.h"
#include "Components/EnemyLocomotionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEnemyMovementStateSelectionTest,
    "adaptHunt.Milestone18.MovementStateSelection",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEnemyMovementStateSelectionTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("Enemies approach outside the orbit band"),
        FEnemyMovementPolicy::SelectIntent(700.0f, 180.0f, 625.0f, false)
            == EEnemyLocomotionIntent::Approach
    );
    TestTrue(
        TEXT("Enemies retreat when crowded"),
        FEnemyMovementPolicy::SelectIntent(120.0f, 180.0f, 625.0f, false)
            == EEnemyLocomotionIntent::Retreat
    );
    TestTrue(
        TEXT("Enemies orbit left inside the fighting band"),
        FEnemyMovementPolicy::SelectIntent(425.0f, 180.0f, 625.0f, false)
            == EEnemyLocomotionIntent::OrbitLeft
    );
    TestTrue(
        TEXT("The orbit side is an explicit deterministic input"),
        FEnemyMovementPolicy::SelectIntent(425.0f, 180.0f, 625.0f, true)
            == EEnemyLocomotionIntent::OrbitRight
    );
    TestTrue(
        TEXT("Invalid distance safely requests an approach"),
        FEnemyMovementPolicy::SelectIntent(
            std::numeric_limits<float>::quiet_NaN(),
            180.0f,
            625.0f,
            false
        ) == EEnemyLocomotionIntent::Approach
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEnemyLocomotionDefaultsTest,
    "adaptHunt.Milestone18.PossessionAndLocomotionDefaults",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEnemyLocomotionDefaultsTest::RunTest(
    const FString& Parameters
)
{
    const AAdaptiveEnemyCharacter* Enemy =
        GetDefault<AAdaptiveEnemyCharacter>();
    TestNotNull(TEXT("The enemy CDO exists"), Enemy);
    if (!Enemy)
    {
        return false;
    }

    const UEnemyLocomotionComponent* Locomotion =
        Enemy->GetEnemyLocomotionComponent();
    TestNotNull(TEXT("The enemy owns a movement driver"), Locomotion);
    TestTrue(
        TEXT("Placed and runtime-spawned enemies auto-possess AI"),
        Enemy->AutoPossessAI == EAutoPossessAI::PlacedInWorldOrSpawned
    );
    TestTrue(
        TEXT("The configured controller supports navigation requests"),
        Enemy->AIControllerClass
            && Enemy->AIControllerClass->IsChildOf(AAIController::StaticClass())
    );
    if (!Locomotion)
    {
        return false;
    }

    TestTrue(
        TEXT("Preferred spacing is between retreat and orbit limits"),
        Locomotion->GetRetreatRange() < Locomotion->GetPreferredRange()
            && Locomotion->GetPreferredRange() < Locomotion->GetOrbitRange()
    );
    TestTrue(
        TEXT("Navigation acceptance and facing speed are configured"),
        Locomotion->GetAcceptanceRadius() > 0.0f
            && Locomotion->GetTurnSpeed() > 0.0f
    );
    TestTrue(
        TEXT("CharacterMovement consumes the driver tuning"),
        FMath::IsNearlyEqual(
            Enemy->GetCharacterMovement()->MaxWalkSpeed,
            Locomotion->GetMovementSpeed()
        ) && FMath::IsNearlyEqual(
            Enemy->GetCharacterMovement()->MaxAcceleration,
            Locomotion->GetMovementAcceleration()
        )
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEnemyMissingNavigationFallbackTest,
    "adaptHunt.Milestone18.MissingNavigationFallback",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEnemyMissingNavigationFallbackTest::RunTest(
    const FString& Parameters
)
{
    TestFalse(
        TEXT("No nav data prevents a navigation attempt"),
        UEnemyLocomotionComponent::CanAttemptNavigation(true, false)
    );
    TestFalse(
        TEXT("No AI controller prevents a navigation attempt"),
        UEnemyLocomotionComponent::CanAttemptNavigation(false, true)
    );
    TestTrue(
        TEXT("Navigation is used only when controller and data both exist"),
        UEnemyLocomotionComponent::CanAttemptNavigation(true, true)
    );

    const AAdaptiveEnemyCharacter* Enemy =
        GetDefault<AAdaptiveEnemyCharacter>();
    const UEnemyLocomotionComponent* Locomotion = Enemy
        ? Enemy->GetEnemyLocomotionComponent()
        : nullptr;
    TestNotNull(TEXT("The movement driver exists for the fallback"), Locomotion);
    if (Locomotion)
    {
        TestFalse(
            TEXT("A worldless default safely reports missing navigation"),
            Locomotion->HasUsableNavigationData()
        );
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEnemyStuckRecoveryPolicyTest,
    "adaptHunt.Milestone18.StuckRecoveryPolicy",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEnemyStuckRecoveryPolicyTest::RunTest(
    const FString& Parameters
)
{
    TestFalse(
        TEXT("Recovery waits for the timeout"),
        UEnemyLocomotionComponent::ShouldTriggerStuckRecovery(
            0.4f,
            0.0f,
            0.85f,
            10.0f
        )
    );
    TestFalse(
        TEXT("Sufficient progress cancels recovery"),
        UEnemyLocomotionComponent::ShouldTriggerStuckRecovery(
            1.0f,
            12.0f,
            0.85f,
            10.0f
        )
    );
    TestTrue(
        TEXT("No progress after the timeout triggers recovery"),
        UEnemyLocomotionComponent::ShouldTriggerStuckRecovery(
            1.0f,
            2.0f,
            0.85f,
            10.0f
        )
    );
    TestFalse(
        TEXT("Invalid telemetry never triggers recovery"),
        UEnemyLocomotionComponent::ShouldTriggerStuckRecovery(
            std::numeric_limits<float>::quiet_NaN(),
            0.0f,
            0.85f,
            10.0f
        )
    );
    return true;
}

#endif
