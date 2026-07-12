#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/FrequencyActionPredictor.h"

namespace
{
void AddSamples(
    FCombatDataset& Dataset,
    const EPlayerCombatAction Action,
    const int32 Count
)
{
    for (int32 Index = 0; Index < Count; ++Index)
    {
        FTrainingSample Sample;
        Sample.Snapshot.RoundNumber = 1;
        Sample.NextPlayerAction = Action;
        Dataset.AddSample(Sample);
    }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveFrequencyActionPredictorDominantPatternTest,
    "adaptHunt.Milestone8.DominantPattern",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveFrequencyActionPredictorDominantPatternTest::RunTest(
    const FString& Parameters
)
{
    FCombatDataset Dataset;
    AddSamples(Dataset, EPlayerCombatAction::DodgeLeft, 5);
    AddSamples(Dataset, EPlayerCombatAction::Block, 2);

    TUniquePtr<IPlayerActionPredictor> Predictor =
        MakeUnique<FFrequencyActionPredictor>();
    Predictor->Train(Dataset);

    FCombatSnapshot CurrentState;
    CurrentState.PlayerHealthNormalized = 0.25f;
    CurrentState.PreviousEnemyAction = EEnemyCombatAction::HeavyAttack;
    const FPredictionResult Prediction = Predictor->Predict(CurrentState);

    TestTrue(
        TEXT("A trained frequency model produces a prediction"),
        Prediction.bHasPrediction
    );
    TestTrue(
        TEXT("The dominant synthetic pattern is predicted"),
        Prediction.PredictedAction == EPlayerCombatAction::DodgeLeft
    );
    TestEqual(
        TEXT("The prediction reports its supporting observations"),
        Prediction.SupportingSampleCount,
        5
    );
    TestEqual(
        TEXT("Confidence is the dominant action's observed frequency"),
        Prediction.Confidence,
        5.0f / 7.0f
    );
    TestEqual(
        TEXT("The interface exposes the last prediction confidence"),
        Predictor->GetPredictionConfidence(),
        Prediction.Confidence
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveFrequencyActionPredictorLifecycleTest,
    "adaptHunt.Milestone8.Lifecycle",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveFrequencyActionPredictorLifecycleTest::RunTest(
    const FString& Parameters
)
{
    FFrequencyActionPredictor Predictor;
    const FCombatSnapshot Snapshot;

    Predictor.Train(FCombatDataset());
    FPredictionResult Prediction = Predictor.Predict(Snapshot);
    TestFalse(
        TEXT("An empty dataset produces no prediction"),
        Prediction.bHasPrediction
    );
    TestEqual(
        TEXT("An empty model has zero confidence"),
        Predictor.GetPredictionConfidence(),
        0.0f
    );

    FCombatDataset FirstDataset;
    AddSamples(FirstDataset, EPlayerCombatAction::HeavyAttack, 1);
    AddSamples(FirstDataset, EPlayerCombatAction::LightAttack, 1);
    Predictor.Train(FirstDataset);
    Prediction = Predictor.Predict(Snapshot);
    TestTrue(
        TEXT("Ties use the documented fixed action order"),
        Prediction.PredictedAction == EPlayerCombatAction::LightAttack
    );

    FCombatDataset ReplacementDataset;
    AddSamples(ReplacementDataset, EPlayerCombatAction::Heal, 3);
    Predictor.Train(ReplacementDataset);
    Prediction = Predictor.Predict(Snapshot);
    TestTrue(
        TEXT("Retraining replaces old frequencies"),
        Prediction.PredictedAction == EPlayerCombatAction::Heal
    );
    TestEqual(
        TEXT("Only replacement samples remain trained"),
        Predictor.GetTrainedSampleCount(),
        3
    );
    TestEqual(
        TEXT("Replaced action counts are cleared"),
        Predictor.GetActionCount(EPlayerCombatAction::LightAttack),
        0
    );

    Predictor.Reset();
    Prediction = Predictor.Predict(Snapshot);
    TestFalse(
        TEXT("Reset removes the learned prediction"),
        Prediction.bHasPrediction
    );
    TestEqual(
        TEXT("Reset clears the trained sample count"),
        Predictor.GetTrainedSampleCount(),
        0
    );
    return true;
}

#endif
