#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/AnyOf.h"
#include "Misc/AutomationTest.h"

#include "AI/FrequencyActionPredictor.h"
#include "Characters/AdaptivePlayerCharacter.h"
#include "Components/CombatComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/EnemyDecisionComponent.h"
#include "Components/PlayerBehaviorTrackerComponent.h"
#include "Game/AdaptiveGameMode.h"
#include "Game/RoundManager.h"

namespace
{
void AddRoundSamples(
    UPlayerBehaviorTrackerComponent& Tracker,
    const int32 RoundNumber,
    const EPlayerCombatAction Action,
    const int32 Count
)
{
    FCombatSnapshot Snapshot;
    Snapshot.RoundNumber = RoundNumber;
    Snapshot.DistanceCategory = ECombatDistanceCategory::Medium;
    Snapshot.PreviousPlayerAction = EPlayerCombatAction::HeavyAttack;
    Snapshot.PreviousEnemyAction = EEnemyCombatAction::HeavyAttack;

    for (int32 Index = 0; Index < Count; ++Index)
    {
        Tracker.RecordPlayerAction(Snapshot, Action);
    }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveRoundManagerDefaultsTest,
    "adaptHunt.Milestone11.RoundManagerDefaults",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveRoundManagerDefaultsTest::RunTest(const FString& Parameters)
{
    const AAdaptiveGameMode* GameMode = GetDefault<AAdaptiveGameMode>();
    const URoundManager* RoundManager = GameMode
        ? GameMode->GetRoundManager()
        : nullptr;

    TestNotNull(TEXT("The game mode owns a round manager"), RoundManager);
    if (!RoundManager)
    {
        return false;
    }

    TestFalse(
        TEXT("Round progression is event/timer-driven and does not Tick"),
        RoundManager->PrimaryComponentTick.bCanEverTick
    );
    TestEqual(
        TEXT("The vertical slice contains exactly three rounds"),
        RoundManager->GetTotalRounds(),
        3
    );
    TestTrue(
        TEXT("The CDO waits for live combatants before starting"),
        RoundManager->GetRoundPhase()
            == EAdaptiveRoundPhase::WaitingToStart
    );
    TestTrue(
        TEXT("The intermission delay is configured"),
        RoundManager->GetIntermissionDuration() >= 0.0f
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveThreeRoundProgressionTest,
    "adaptHunt.Milestone11.ThreeRoundProgression",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveThreeRoundProgressionTest::RunTest(const FString& Parameters)
{
    FAdaptiveRoundProgression Progression;
    TestTrue(TEXT("A fresh match begins"), Progression.BeginMatch());
    TestEqual(TEXT("The match begins in Round 1"), Progression.GetCurrentRound(), 1);
    TestTrue(
        TEXT("Round 1 is in progress"),
        Progression.GetPhase() == EAdaptiveRoundPhase::InProgress
    );
    TestFalse(
        TEXT("Round 1 is baseline observation"),
        URoundManager::IsPredictionRound(1)
    );
    TestTrue(
        TEXT("The model trains after Round 1"),
        URoundManager::ShouldTrainAfterRound(1)
    );

    TestTrue(
        TEXT("Round 1 can complete"),
        Progression.CompleteCurrentRound(EAdaptiveRoundWinner::Player)
    );
    TestTrue(
        TEXT("A rematch intermission follows Round 1"),
        Progression.GetPhase() == EAdaptiveRoundPhase::Intermission
    );
    TestTrue(TEXT("Round 2 starts"), Progression.AdvanceToNextRound());
    TestEqual(TEXT("The active round is 2"), Progression.GetCurrentRound(), 2);
    TestTrue(
        TEXT("Round 2 is prediction-eligible"),
        URoundManager::IsPredictionRound(2)
    );
    TestTrue(
        TEXT("The model updates after Round 2"),
        URoundManager::ShouldTrainAfterRound(2)
    );

    TestTrue(
        TEXT("Round 2 can complete"),
        Progression.CompleteCurrentRound(EAdaptiveRoundWinner::Enemy)
    );
    TestTrue(TEXT("Round 3 starts"), Progression.AdvanceToNextRound());
    TestEqual(TEXT("The active round is 3"), Progression.GetCurrentRound(), 3);
    TestTrue(
        TEXT("Round 3 is prediction-eligible"),
        URoundManager::IsPredictionRound(3)
    );
    TestFalse(
        TEXT("No unused model update occurs after the final round"),
        URoundManager::ShouldTrainAfterRound(3)
    );

    TestTrue(
        TEXT("Round 3 can complete"),
        Progression.CompleteCurrentRound(EAdaptiveRoundWinner::Player)
    );
    TestTrue(
        TEXT("The third result completes the match"),
        Progression.GetPhase() == EAdaptiveRoundPhase::MatchComplete
    );
    TestFalse(
        TEXT("A fourth round cannot begin"),
        Progression.AdvanceToNextRound()
    );
    TestEqual(
        TEXT("The final completed round is Round 3"),
        Progression.GetLastCompletedRound(),
        3
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveRoundLearningLifecycleTest,
    "adaptHunt.Milestone11.AccumulatedLearningLifecycle",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveRoundLearningLifecycleTest::RunTest(
    const FString& Parameters
)
{
    UPlayerBehaviorTrackerComponent* Tracker =
        NewObject<UPlayerBehaviorTrackerComponent>();
    UEnemyDecisionComponent* Decision =
        NewObject<UEnemyDecisionComponent>();
    TestNotNull(TEXT("An isolated tracker can be created"), Tracker);
    TestNotNull(TEXT("An isolated decision component can be created"), Decision);
    if (!Tracker || !Decision)
    {
        return false;
    }

    Decision->SetAutomaticRetrainingEnabled(false);
    Decision->SetPredictionUsageEnabled(false);
    Decision->ResetPredictor();
    TestFalse(
        TEXT("The round manager can take ownership of training timing"),
        Decision->IsAutomaticRetrainingEnabled()
    );
    TestFalse(
        TEXT("Round 1 prediction use is disabled"),
        Decision->IsPredictionUsageEnabled()
    );

    AddRoundSamples(*Tracker, 1, EPlayerCombatAction::DodgeLeft, 4);
    Decision->TrainPredictor(Tracker->GetDataset());
    TestEqual(
        TEXT("The first boundary trains on Round 1 samples"),
        Decision->GetTrainedSampleCount(),
        4
    );

    FFrequencyActionPredictor Predictor;
    Predictor.Train(Tracker->GetDataset());
    FCombatSnapshot Query;
    Query.DistanceCategory = ECombatDistanceCategory::Medium;
    Query.PreviousPlayerAction = EPlayerCombatAction::HeavyAttack;
    Query.PreviousEnemyAction = EEnemyCombatAction::HeavyAttack;
    TestTrue(
        TEXT("Round 1 behavior predicts DodgeLeft for Round 2"),
        Predictor.Predict(Query).PredictedAction
            == EPlayerCombatAction::DodgeLeft
    );

    AddRoundSamples(*Tracker, 2, EPlayerCombatAction::Block, 6);
    TestEqual(
        TEXT("Round 2 collection appends instead of clearing Round 1"),
        Tracker->GetSampleCount(),
        10
    );
    Decision->TrainPredictor(Tracker->GetDataset());
    Predictor.Train(Tracker->GetDataset());
    TestEqual(
        TEXT("The second boundary retrains on accumulated samples"),
        Decision->GetTrainedSampleCount(),
        10
    );
    TestTrue(
        TEXT("The updated model responds to the newer Block strategy"),
        Predictor.Predict(Query).PredictedAction
            == EPlayerCombatAction::Block
    );

    const TArray<FTrainingSample>& Samples =
        Tracker->GetDataset().GetSamples();
    TestTrue(
        TEXT("The accumulated dataset retains Round 1 labels"),
        Algo::AnyOf(Samples, [](const FTrainingSample& Sample)
        {
            return Sample.Snapshot.RoundNumber == 1;
        })
    );
    TestTrue(
        TEXT("The accumulated dataset contains Round 2 labels"),
        Algo::AnyOf(Samples, [](const FTrainingSample& Sample)
        {
            return Sample.Snapshot.RoundNumber == 2;
        })
    );

    Decision->SetPredictionUsageEnabled(true);
    TestTrue(
        TEXT("Prediction use can be enabled for the adaptation rounds"),
        Decision->IsPredictionUsageEnabled()
    );

    UCombatComponent* PlayerCombat = NewObject<UCombatComponent>();
    UEnemyCombatComponent* EnemyCombat = NewObject<UEnemyCombatComponent>();
    PlayerCombat->SetCombatEnabled(false);
    EnemyCombat->SetCombatEnabled(false);
    TestFalse(
        TEXT("Player combat can be locked during intermission"),
        PlayerCombat->IsCombatEnabled()
    );
    TestFalse(
        TEXT("Enemy combat can be locked during intermission"),
        EnemyCombat->IsCombatEnabled()
    );
    return true;
}

#endif
