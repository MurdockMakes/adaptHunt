#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Characters/AdaptivePlayerCharacter.h"
#include "Components/CombatSnapshotComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCombatSnapshotComponentTest,
    "adaptHunt.Milestone6.SnapshotComponent",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCombatSnapshotComponentTest::RunTest(
    const FString& Parameters
)
{
    const AAdaptivePlayerCharacter* Player =
        GetDefault<AAdaptivePlayerCharacter>();
    const UCombatSnapshotComponent* SnapshotComponent = Player
        ? Player->GetCombatSnapshotComponent()
        : nullptr;

    TestNotNull(
        TEXT("The player owns the snapshot component"),
        SnapshotComponent
    );
    if (!SnapshotComponent)
    {
        return false;
    }

    TestFalse(
        TEXT("Snapshot capture is event-driven and does not Tick"),
        SnapshotComponent->PrimaryComponentTick.bCanEverTick
    );
    TestTrue(
        TEXT("Distance thresholds are ordered"),
        SnapshotComponent->GetCloseDistanceThreshold()
            < SnapshotComponent->GetFarDistanceThreshold()
    );
    TestTrue(
        TEXT("The enemy attack observation window is configured"),
        SnapshotComponent->GetEnemyAttackWindow() > 0.0f
    );

    UCombatSnapshotComponent* IsolatedComponent =
        NewObject<UCombatSnapshotComponent>();
    TestNotNull(TEXT("An isolated snapshot component can be created"), IsolatedComponent);
    if (IsolatedComponent)
    {
        IsolatedComponent->SetRoundNumber(0);
        TestEqual(
            TEXT("Round numbers clamp to one"),
            IsolatedComponent->GetRoundNumber(),
            1
        );
        IsolatedComponent->SetRoundNumber(3);
        TestEqual(
            TEXT("The active round is captured"),
            IsolatedComponent->CaptureSnapshot().RoundNumber,
            3
        );
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCombatSnapshotClassificationTest,
    "adaptHunt.Milestone6.SnapshotClassification",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCombatSnapshotClassificationTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("A nearby opponent is close"),
        UCombatSnapshotComponent::ClassifyDistance(200.0f, 300.0f, 700.0f)
            == ECombatDistanceCategory::Close
    );
    TestTrue(
        TEXT("An opponent inside the distance band is medium"),
        UCombatSnapshotComponent::ClassifyDistance(500.0f, 300.0f, 700.0f)
            == ECombatDistanceCategory::Medium
    );
    TestTrue(
        TEXT("A distant opponent is far"),
        UCombatSnapshotComponent::ClassifyDistance(900.0f, 300.0f, 700.0f)
            == ECombatDistanceCategory::Far
    );

    const FVector EnemyLocation = FVector::ZeroVector;
    const FVector EnemyForward = FVector::ForwardVector;
    TestTrue(
        TEXT("Player in front is classified relative to enemy facing"),
        UCombatSnapshotComponent::ClassifyRelativePosition(
            FVector(100.0f, 0.0f, 0.0f),
            EnemyLocation,
            EnemyForward
        ) == ERelativePlayerPosition::Front
    );
    TestTrue(
        TEXT("Player behind is classified relative to enemy facing"),
        UCombatSnapshotComponent::ClassifyRelativePosition(
            FVector(-100.0f, 0.0f, 0.0f),
            EnemyLocation,
            EnemyForward
        ) == ERelativePlayerPosition::Behind
    );
    TestTrue(
        TEXT("Player on the enemy's right is classified correctly"),
        UCombatSnapshotComponent::ClassifyRelativePosition(
            FVector(0.0f, 100.0f, 0.0f),
            EnemyLocation,
            EnemyForward
        ) == ERelativePlayerPosition::Right
    );
    TestTrue(
        TEXT("Player on the enemy's left is classified correctly"),
        UCombatSnapshotComponent::ClassifyRelativePosition(
            FVector(0.0f, -100.0f, 0.0f),
            EnemyLocation,
            EnemyForward
        ) == ERelativePlayerPosition::Left
    );

    return true;
}

#endif
