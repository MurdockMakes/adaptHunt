#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/AllOf.h"
#include "Misc/AutomationTest.h"

#include "Characters/AdaptivePlayerCharacter.h"
#include "Components/PlayerBehaviorTrackerComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptivePlayerBehaviorTrackerOwnershipTest,
    "adaptHunt.Milestone7.TrackerOwnership",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptivePlayerBehaviorTrackerOwnershipTest::RunTest(
    const FString& Parameters
)
{
    const AAdaptivePlayerCharacter* Player =
        GetDefault<AAdaptivePlayerCharacter>();
    const UPlayerBehaviorTrackerComponent* Tracker = Player
        ? Player->GetBehaviorTrackerComponent()
        : nullptr;

    TestNotNull(TEXT("The player owns the behavior tracker"), Tracker);
    if (!Tracker)
    {
        return false;
    }

    TestFalse(
        TEXT("Behavior tracking is event-driven and does not Tick"),
        Tracker->PrimaryComponentTick.bCanEverTick
    );
    TestTrue(
        TEXT("Behavior tracking starts enabled"),
        Tracker->IsTrackingEnabled()
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptivePlayerBehaviorDatasetTest,
    "adaptHunt.Milestone7.DatasetRecording",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptivePlayerBehaviorDatasetTest::RunTest(const FString& Parameters)
{
    UPlayerBehaviorTrackerComponent* Tracker =
        NewObject<UPlayerBehaviorTrackerComponent>();
    TestNotNull(TEXT("An isolated behavior tracker can be created"), Tracker);
    if (!Tracker)
    {
        return false;
    }

    FCombatSnapshot Snapshot;
    Snapshot.CaptureTimeSeconds = 12.5f;
    Snapshot.RoundNumber = 2;
    Snapshot.PreviousPlayerAction = EPlayerCombatAction::HeavyAttack;
    Snapshot.PreviousEnemyAction = EEnemyCombatAction::HeavyAttack;

    for (int32 Index = 0; Index < 3; ++Index)
    {
        TestTrue(
            TEXT("DodgeLeft is recorded after Enemy HeavyAttack"),
            Tracker->RecordPlayerAction(
                Snapshot,
                EPlayerCombatAction::DodgeLeft
            )
        );
    }
    TestTrue(
        TEXT("A different response can share the same condition"),
        Tracker->RecordPlayerAction(Snapshot, EPlayerCombatAction::Block)
    );

    TestEqual(TEXT("Four decisions were recorded"), Tracker->GetSampleCount(), 4);
    TestEqual(
        TEXT("Three DodgeLeft labels were recorded"),
        Tracker->GetActionCount(EPlayerCombatAction::DodgeLeft),
        3
    );
    TestTrue(
        TEXT("DodgeLeft is the most common recorded action"),
        Tracker->GetMostCommonAction() == EPlayerCombatAction::DodgeLeft
    );

    const TArray<FTrainingSample>& Samples =
        Tracker->GetDataset().GetSamples();
    TestTrue(
        TEXT("The enemy-action condition is preserved in every sample"),
        Algo::AllOf(Samples, [](const FTrainingSample& Sample)
        {
            return Sample.Snapshot.PreviousEnemyAction
                == EEnemyCombatAction::HeavyAttack;
        })
    );
    if (!Samples.IsEmpty())
    {
        TestEqual(
            TEXT("The action time comes from the pre-action snapshot"),
            Samples[0].ActionTimeSeconds,
            12.5f
        );
    }

    Tracker->StopTracking();
    TestFalse(
        TEXT("Paused tracking rejects decisions"),
        Tracker->RecordPlayerAction(Snapshot, EPlayerCombatAction::Heal)
    );
    TestEqual(
        TEXT("Paused tracking does not change the dataset"),
        Tracker->GetSampleCount(),
        4
    );

    Tracker->ResetDataset();
    TestEqual(TEXT("Reset clears the dataset"), Tracker->GetSampleCount(), 0);
    return true;
}

#endif
