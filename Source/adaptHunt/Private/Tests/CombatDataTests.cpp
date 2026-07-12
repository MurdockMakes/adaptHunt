#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Data/CombatDataset.h"
#include "Data/PredictionResult.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCombatDataDefaultsTest,
    "adaptHunt.Milestone1.CombatDataDefaults",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCombatDataDefaultsTest::RunTest(const FString& Parameters)
{
    const FCombatSnapshot Snapshot;
    TestTrue(
        TEXT("A new snapshot starts in round one"),
        Snapshot.RoundNumber == 1
    );
    TestTrue(
        TEXT("A new snapshot has no previous player action"),
        Snapshot.PreviousPlayerAction == EPlayerCombatAction::None
    );

    FCombatDataset Dataset;
    TestTrue(TEXT("A new dataset is empty"), Dataset.IsEmpty());

    const FTrainingSample UnlabeledSample;
    TestFalse(
        TEXT("The dataset rejects an unlabeled sample"),
        Dataset.AddSample(UnlabeledSample)
    );

    FTrainingSample LabeledSample;
    LabeledSample.NextPlayerAction = EPlayerCombatAction::LightAttack;
    TestTrue(
        TEXT("The dataset accepts a labeled sample"),
        Dataset.AddSample(LabeledSample)
    );
    TestEqual(TEXT("The dataset contains one sample"), Dataset.Num(), 1);
    TestEqual(
        TEXT("The light-attack count is one"),
        Dataset.GetActionCount(EPlayerCombatAction::LightAttack),
        1
    );

    const FPredictionResult Prediction;
    TestFalse(
        TEXT("A default prediction explicitly reports no prediction"),
        Prediction.bHasPrediction
    );
    TestTrue(
        TEXT("A default prediction uses the None action"),
        Prediction.PredictedAction == EPlayerCombatAction::None
    );

    return true;
}

#endif
