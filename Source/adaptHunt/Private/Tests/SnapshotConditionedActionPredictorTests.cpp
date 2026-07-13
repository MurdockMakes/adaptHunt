#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/ConditionalActionPredictor.h"

namespace
{
void AddSnapshotConditionedSamples(
    FCombatDataset& Dataset,
    const EEnemyCombatAction PreviousEnemyAction,
    const ECombatDistanceCategory DistanceCategory,
    const EPlayerCombatAction PlayerAction,
    const int32 Count
)
{
    for (int32 Index = 0; Index < Count; ++Index)
    {
        FTrainingSample Sample;
        Sample.Snapshot.RoundNumber = 1;
        Sample.Snapshot.PreviousEnemyAction = PreviousEnemyAction;
        Sample.Snapshot.DistanceCategory = DistanceCategory;
        Sample.NextPlayerAction = PlayerAction;
        Dataset.AddSample(Sample);
    }
}

void AddPositionConditionedSamples(
    FCombatDataset& Dataset,
    const EEnemyCombatAction PreviousEnemyAction,
    const ECombatDistanceCategory DistanceCategory,
    const ERelativePlayerPosition RelativePlayerPosition,
    const EPlayerCombatAction PlayerAction,
    const int32 Count
)
{
    for (int32 Index = 0; Index < Count; ++Index)
    {
        FTrainingSample Sample;
        Sample.Snapshot.RoundNumber = 1;
        Sample.Snapshot.PreviousEnemyAction = PreviousEnemyAction;
        Sample.Snapshot.DistanceCategory = DistanceCategory;
        Sample.Snapshot.RelativePlayerPosition = RelativePlayerPosition;
        Sample.NextPlayerAction = PlayerAction;
        Dataset.AddSample(Sample);
    }
}

void AddPreviousPlayerActionConditionedSamples(
    FCombatDataset& Dataset,
    const EEnemyCombatAction PreviousEnemyAction,
    const ECombatDistanceCategory DistanceCategory,
    const ERelativePlayerPosition RelativePlayerPosition,
    const EPlayerCombatAction PreviousPlayerAction,
    const EPlayerCombatAction NextPlayerAction,
    const int32 Count
)
{
    for (int32 Index = 0; Index < Count; ++Index)
    {
        FTrainingSample Sample;
        Sample.Snapshot.RoundNumber = 1;
        Sample.Snapshot.PreviousEnemyAction = PreviousEnemyAction;
        Sample.Snapshot.DistanceCategory = DistanceCategory;
        Sample.Snapshot.RelativePlayerPosition = RelativePlayerPosition;
        Sample.Snapshot.PreviousPlayerAction = PreviousPlayerAction;
        Sample.NextPlayerAction = NextPlayerAction;
        Dataset.AddSample(Sample);
    }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveSnapshotConditionedDistanceTest,
    "adaptHunt.Milestone15.DistanceChangesPrediction",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveSnapshotConditionedDistanceTest::RunTest(
    const FString& Parameters
)
{
    FCombatDataset Dataset;
    AddSnapshotConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Close,
        EPlayerCombatAction::Block,
        3
    );
    AddSnapshotConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Far,
        EPlayerCombatAction::Heal,
        2
    );
    AddSnapshotConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Far,
        EPlayerCombatAction::DodgeLeft,
        2
    );

    FConditionalActionPredictor Predictor;
    Predictor.Train(Dataset);

    FCombatSnapshot CloseSnapshot;
    CloseSnapshot.PreviousEnemyAction = EEnemyCombatAction::HeavyAttack;
    CloseSnapshot.DistanceCategory = ECombatDistanceCategory::Close;
    const FPredictionResult ClosePrediction =
        Predictor.Predict(CloseSnapshot);
    TestTrue(
        TEXT("The exact enemy-action and distance context is selected"),
        ClosePrediction.bUsedContext
            && ClosePrediction.bUsedDistanceContext
            && ClosePrediction.ConditioningEnemyAction
                == EEnemyCombatAction::HeavyAttack
            && ClosePrediction.ConditioningDistanceCategory
                == ECombatDistanceCategory::Close
    );
    TestTrue(
        TEXT("Close-range Heavy Attack responses predict Block"),
        ClosePrediction.PredictedAction == EPlayerCombatAction::Block
    );
    TestEqual(
        TEXT("Exact-context confidence uses only exact observations"),
        ClosePrediction.Confidence,
        1.0f
    );

    FCombatSnapshot FarSnapshot = CloseSnapshot;
    FarSnapshot.DistanceCategory = ECombatDistanceCategory::Far;
    const FPredictionResult FarPrediction = Predictor.Predict(FarSnapshot);
    TestTrue(
        TEXT("Changing only distance changes the prediction"),
        FarPrediction.bUsedDistanceContext
            && FarPrediction.PredictedAction
                != ClosePrediction.PredictedAction
    );
    TestTrue(
        TEXT("Exact-context ties use the fixed player-action order"),
        FarPrediction.PredictedAction == EPlayerCombatAction::DodgeLeft
    );
    TestEqual(
        TEXT("The exact context exposes all of its evidence"),
        Predictor.GetDistanceConditionalSampleCount(
            EEnemyCombatAction::HeavyAttack,
            ECombatDistanceCategory::Far
        ),
        4
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveSnapshotConditionedFallbackTest,
    "adaptHunt.Milestone15.HierarchicalFallback",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveSnapshotConditionedFallbackTest::RunTest(
    const FString& Parameters
)
{
    FCombatDataset Dataset;
    AddSnapshotConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Close,
        EPlayerCombatAction::Block,
        1
    );
    AddSnapshotConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Far,
        EPlayerCombatAction::DodgeLeft,
        3
    );
    AddSnapshotConditionedSamples(
        Dataset,
        EEnemyCombatAction::None,
        ECombatDistanceCategory::Medium,
        EPlayerCombatAction::Heal,
        5
    );

    FConditionalActionPredictor Predictor(2);
    Predictor.Train(Dataset);

    FCombatSnapshot SparseExactSnapshot;
    SparseExactSnapshot.PreviousEnemyAction =
        EEnemyCombatAction::HeavyAttack;
    SparseExactSnapshot.DistanceCategory = ECombatDistanceCategory::Close;
    FPredictionResult Prediction = Predictor.Predict(SparseExactSnapshot);
    TestTrue(
        TEXT("Sparse exact evidence falls back to enemy-action evidence"),
        Prediction.bUsedContext
            && !Prediction.bUsedDistanceContext
            && Prediction.ConditioningEnemyAction
                == EEnemyCombatAction::HeavyAttack
            && Prediction.PredictedAction
                == EPlayerCombatAction::DodgeLeft
    );
    TestEqual(
        TEXT("Enemy-action fallback confidence uses the broader table"),
        Prediction.Confidence,
        3.0f / 4.0f
    );

    FCombatSnapshot UnseenEnemySnapshot;
    UnseenEnemySnapshot.PreviousEnemyAction = EEnemyCombatAction::Dodge;
    UnseenEnemySnapshot.DistanceCategory = ECombatDistanceCategory::Close;
    Prediction = Predictor.Predict(UnseenEnemySnapshot);
    TestTrue(
        TEXT("Unseen enemy actions fall back to the global predictor"),
        Prediction.bHasPrediction
            && !Prediction.bUsedContext
            && !Prediction.bUsedDistanceContext
            && Prediction.PredictedAction == EPlayerCombatAction::Heal
    );

    Predictor.Reset();
    Prediction = Predictor.Predict(SparseExactSnapshot);
    TestFalse(
        TEXT("Reset clears every level of the prediction hierarchy"),
        Prediction.bHasPrediction
    );
    TestEqual(
        TEXT("Reset clears exact-context evidence"),
        Predictor.GetDistanceConditionalSampleCount(
            EEnemyCombatAction::HeavyAttack,
            ECombatDistanceCategory::Close
        ),
        0
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptivePositionConditionedPredictionTest,
    "adaptHunt.Milestone16.PositionChangesPrediction",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptivePositionConditionedPredictionTest::RunTest(
    const FString& Parameters
)
{
    FCombatDataset Dataset;
    AddPositionConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Close,
        ERelativePlayerPosition::Front,
        EPlayerCombatAction::Block,
        3
    );
    AddPositionConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Close,
        ERelativePlayerPosition::Behind,
        EPlayerCombatAction::Heal,
        2
    );
    AddPositionConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Close,
        ERelativePlayerPosition::Behind,
        EPlayerCombatAction::DodgeLeft,
        2
    );

    FConditionalActionPredictor Predictor;
    Predictor.Train(Dataset);

    FCombatSnapshot FrontSnapshot;
    FrontSnapshot.PreviousEnemyAction = EEnemyCombatAction::HeavyAttack;
    FrontSnapshot.DistanceCategory = ECombatDistanceCategory::Close;
    FrontSnapshot.RelativePlayerPosition = ERelativePlayerPosition::Front;
    const FPredictionResult FrontPrediction =
        Predictor.Predict(FrontSnapshot);
    TestTrue(
        TEXT("The exact position context exposes complete provenance"),
        FrontPrediction.bUsedContext
            && FrontPrediction.bUsedDistanceContext
            && FrontPrediction.bUsedPositionContext
            && FrontPrediction.ConditioningEnemyAction
                == EEnemyCombatAction::HeavyAttack
            && FrontPrediction.ConditioningDistanceCategory
                == ECombatDistanceCategory::Close
            && FrontPrediction.ConditioningRelativePlayerPosition
                == ERelativePlayerPosition::Front
    );
    TestTrue(
        TEXT("A player in front after a close Heavy Attack predicts Block"),
        FrontPrediction.PredictedAction == EPlayerCombatAction::Block
    );
    TestEqual(
        TEXT("Position confidence uses only exact observations"),
        FrontPrediction.Confidence,
        1.0f
    );

    FCombatSnapshot BehindSnapshot = FrontSnapshot;
    BehindSnapshot.RelativePlayerPosition =
        ERelativePlayerPosition::Behind;
    const FPredictionResult BehindPrediction =
        Predictor.Predict(BehindSnapshot);
    TestTrue(
        TEXT("Changing only relative position changes the prediction"),
        BehindPrediction.bUsedPositionContext
            && BehindPrediction.PredictedAction
                != FrontPrediction.PredictedAction
    );
    TestTrue(
        TEXT("Position-context ties use the fixed player-action order"),
        BehindPrediction.PredictedAction
            == EPlayerCombatAction::DodgeLeft
    );
    TestEqual(
        TEXT("The exact position context exposes all of its evidence"),
        Predictor.GetPositionConditionalSampleCount(
            EEnemyCombatAction::HeavyAttack,
            ECombatDistanceCategory::Close,
            ERelativePlayerPosition::Behind
        ),
        4
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptivePositionConditionedFallbackTest,
    "adaptHunt.Milestone16.HierarchicalFallback",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptivePositionConditionedFallbackTest::RunTest(
    const FString& Parameters
)
{
    FCombatDataset Dataset;
    AddPositionConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Close,
        ERelativePlayerPosition::Front,
        EPlayerCombatAction::Block,
        1
    );
    AddPositionConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Close,
        ERelativePlayerPosition::Behind,
        EPlayerCombatAction::DodgeLeft,
        3
    );
    AddPositionConditionedSamples(
        Dataset,
        EEnemyCombatAction::None,
        ECombatDistanceCategory::Medium,
        ERelativePlayerPosition::Front,
        EPlayerCombatAction::Heal,
        5
    );

    FConditionalActionPredictor Predictor(2);
    Predictor.Train(Dataset);

    FCombatSnapshot SparsePositionSnapshot;
    SparsePositionSnapshot.PreviousEnemyAction =
        EEnemyCombatAction::HeavyAttack;
    SparsePositionSnapshot.DistanceCategory =
        ECombatDistanceCategory::Close;
    SparsePositionSnapshot.RelativePlayerPosition =
        ERelativePlayerPosition::Front;
    FPredictionResult Prediction = Predictor.Predict(
        SparsePositionSnapshot
    );
    TestTrue(
        TEXT("Sparse position evidence falls back to distance evidence"),
        Prediction.bUsedContext
            && Prediction.bUsedDistanceContext
            && !Prediction.bUsedPositionContext
            && Prediction.PredictedAction
                == EPlayerCombatAction::DodgeLeft
    );
    TestEqual(
        TEXT("Distance fallback confidence uses its broader table"),
        Prediction.Confidence,
        3.0f / 4.0f
    );

    FCombatSnapshot UnseenDistanceSnapshot = SparsePositionSnapshot;
    UnseenDistanceSnapshot.DistanceCategory =
        ECombatDistanceCategory::Medium;
    Prediction = Predictor.Predict(UnseenDistanceSnapshot);
    TestTrue(
        TEXT("Unseen distance falls back to enemy-action evidence"),
        Prediction.bUsedContext
            && !Prediction.bUsedDistanceContext
            && !Prediction.bUsedPositionContext
            && Prediction.PredictedAction
                == EPlayerCombatAction::DodgeLeft
    );

    FCombatSnapshot UnseenEnemySnapshot = SparsePositionSnapshot;
    UnseenEnemySnapshot.PreviousEnemyAction = EEnemyCombatAction::Dodge;
    Prediction = Predictor.Predict(UnseenEnemySnapshot);
    TestTrue(
        TEXT("Unseen enemy action falls back to global frequency"),
        Prediction.bHasPrediction
            && !Prediction.bUsedContext
            && !Prediction.bUsedDistanceContext
            && !Prediction.bUsedPositionContext
            && Prediction.PredictedAction == EPlayerCombatAction::Heal
    );

    Predictor.Reset();
    Prediction = Predictor.Predict(SparsePositionSnapshot);
    TestFalse(
        TEXT("Reset clears every position-conditioned hierarchy level"),
        Prediction.bHasPrediction
    );
    TestEqual(
        TEXT("Reset clears exact position evidence"),
        Predictor.GetPositionConditionalSampleCount(
            EEnemyCombatAction::HeavyAttack,
            ECombatDistanceCategory::Close,
            ERelativePlayerPosition::Front
        ),
        0
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptivePreviousPlayerActionConditionedPredictionTest,
    "adaptHunt.Milestone17.PreviousPlayerActionChangesPrediction",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptivePreviousPlayerActionConditionedPredictionTest::RunTest(
    const FString& Parameters
)
{
    FCombatDataset Dataset;
    AddPreviousPlayerActionConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Close,
        ERelativePlayerPosition::Front,
        EPlayerCombatAction::LightAttack,
        EPlayerCombatAction::Block,
        3
    );
    AddPreviousPlayerActionConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Close,
        ERelativePlayerPosition::Front,
        EPlayerCombatAction::HeavyAttack,
        EPlayerCombatAction::Heal,
        2
    );
    AddPreviousPlayerActionConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Close,
        ERelativePlayerPosition::Front,
        EPlayerCombatAction::HeavyAttack,
        EPlayerCombatAction::DodgeLeft,
        2
    );

    FConditionalActionPredictor Predictor;
    Predictor.Train(Dataset);

    FCombatSnapshot AfterLightAttackSnapshot;
    AfterLightAttackSnapshot.PreviousEnemyAction =
        EEnemyCombatAction::HeavyAttack;
    AfterLightAttackSnapshot.DistanceCategory =
        ECombatDistanceCategory::Close;
    AfterLightAttackSnapshot.RelativePlayerPosition =
        ERelativePlayerPosition::Front;
    AfterLightAttackSnapshot.PreviousPlayerAction =
        EPlayerCombatAction::LightAttack;
    const FPredictionResult AfterLightAttackPrediction =
        Predictor.Predict(AfterLightAttackSnapshot);
    TestTrue(
        TEXT("The exact previous-player-action context exposes provenance"),
        AfterLightAttackPrediction.bUsedContext
            && AfterLightAttackPrediction.bUsedDistanceContext
            && AfterLightAttackPrediction.bUsedPositionContext
            && AfterLightAttackPrediction.bUsedPreviousPlayerActionContext
            && AfterLightAttackPrediction.ConditioningPreviousPlayerAction
                == EPlayerCombatAction::LightAttack
    );
    TestTrue(
        TEXT("A previous Light Attack predicts Block in the exact state"),
        AfterLightAttackPrediction.PredictedAction
            == EPlayerCombatAction::Block
    );
    TestEqual(
        TEXT("Previous-player-action confidence uses exact observations"),
        AfterLightAttackPrediction.Confidence,
        1.0f
    );

    FCombatSnapshot AfterHeavyAttackSnapshot = AfterLightAttackSnapshot;
    AfterHeavyAttackSnapshot.PreviousPlayerAction =
        EPlayerCombatAction::HeavyAttack;
    const FPredictionResult AfterHeavyAttackPrediction =
        Predictor.Predict(AfterHeavyAttackSnapshot);
    TestTrue(
        TEXT("Changing only the previous player action changes prediction"),
        AfterHeavyAttackPrediction.bUsedPreviousPlayerActionContext
            && AfterHeavyAttackPrediction.PredictedAction
                != AfterLightAttackPrediction.PredictedAction
    );
    TestTrue(
        TEXT("Exact previous-player-action ties use fixed action order"),
        AfterHeavyAttackPrediction.PredictedAction
            == EPlayerCombatAction::DodgeLeft
    );
    TestEqual(
        TEXT("The exact previous-player-action context exposes evidence"),
        Predictor.GetPreviousPlayerActionConditionalSampleCount(
            EEnemyCombatAction::HeavyAttack,
            ECombatDistanceCategory::Close,
            ERelativePlayerPosition::Front,
            EPlayerCombatAction::HeavyAttack
        ),
        4
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptivePreviousPlayerActionConditionedFallbackTest,
    "adaptHunt.Milestone17.HierarchicalFallback",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptivePreviousPlayerActionConditionedFallbackTest::RunTest(
    const FString& Parameters
)
{
    FCombatDataset Dataset;
    AddPreviousPlayerActionConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Close,
        ERelativePlayerPosition::Front,
        EPlayerCombatAction::LightAttack,
        EPlayerCombatAction::Block,
        1
    );
    AddPreviousPlayerActionConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Close,
        ERelativePlayerPosition::Front,
        EPlayerCombatAction::HeavyAttack,
        EPlayerCombatAction::DodgeLeft,
        3
    );
    AddPreviousPlayerActionConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Close,
        ERelativePlayerPosition::Behind,
        EPlayerCombatAction::Block,
        EPlayerCombatAction::Heal,
        5
    );
    AddPreviousPlayerActionConditionedSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        ECombatDistanceCategory::Far,
        ERelativePlayerPosition::Right,
        EPlayerCombatAction::Block,
        EPlayerCombatAction::Block,
        8
    );
    AddPreviousPlayerActionConditionedSamples(
        Dataset,
        EEnemyCombatAction::None,
        ECombatDistanceCategory::Medium,
        ERelativePlayerPosition::Front,
        EPlayerCombatAction::Heal,
        EPlayerCombatAction::LightAttack,
        12
    );

    FConditionalActionPredictor Predictor(2);
    Predictor.Train(Dataset);

    FCombatSnapshot SparseExactSnapshot;
    SparseExactSnapshot.PreviousEnemyAction =
        EEnemyCombatAction::HeavyAttack;
    SparseExactSnapshot.DistanceCategory = ECombatDistanceCategory::Close;
    SparseExactSnapshot.RelativePlayerPosition =
        ERelativePlayerPosition::Front;
    SparseExactSnapshot.PreviousPlayerAction =
        EPlayerCombatAction::LightAttack;
    FPredictionResult Prediction = Predictor.Predict(SparseExactSnapshot);
    TestTrue(
        TEXT("Sparse player-action evidence falls back to position"),
        Prediction.bUsedPositionContext
            && !Prediction.bUsedPreviousPlayerActionContext
            && Prediction.PredictedAction
                == EPlayerCombatAction::DodgeLeft
    );
    TestEqual(
        TEXT("Position fallback confidence uses its broader table"),
        Prediction.Confidence,
        3.0f / 4.0f
    );

    FCombatSnapshot UnseenPositionSnapshot = SparseExactSnapshot;
    UnseenPositionSnapshot.RelativePlayerPosition =
        ERelativePlayerPosition::Left;
    Prediction = Predictor.Predict(UnseenPositionSnapshot);
    TestTrue(
        TEXT("Unseen position falls back to distance evidence"),
        Prediction.bUsedDistanceContext
            && !Prediction.bUsedPositionContext
            && !Prediction.bUsedPreviousPlayerActionContext
            && Prediction.PredictedAction == EPlayerCombatAction::Heal
    );

    FCombatSnapshot UnseenDistanceSnapshot = SparseExactSnapshot;
    UnseenDistanceSnapshot.DistanceCategory =
        ECombatDistanceCategory::Medium;
    Prediction = Predictor.Predict(UnseenDistanceSnapshot);
    TestTrue(
        TEXT("Unseen distance falls back to enemy-action evidence"),
        Prediction.bUsedContext
            && !Prediction.bUsedDistanceContext
            && !Prediction.bUsedPositionContext
            && !Prediction.bUsedPreviousPlayerActionContext
            && Prediction.PredictedAction == EPlayerCombatAction::Block
    );

    FCombatSnapshot UnseenEnemySnapshot = SparseExactSnapshot;
    UnseenEnemySnapshot.PreviousEnemyAction = EEnemyCombatAction::Dodge;
    Prediction = Predictor.Predict(UnseenEnemySnapshot);
    TestTrue(
        TEXT("Unseen enemy action falls back to global frequency"),
        Prediction.bHasPrediction
            && !Prediction.bUsedContext
            && !Prediction.bUsedDistanceContext
            && !Prediction.bUsedPositionContext
            && !Prediction.bUsedPreviousPlayerActionContext
            && Prediction.PredictedAction
                == EPlayerCombatAction::LightAttack
    );

    Predictor.Reset();
    Prediction = Predictor.Predict(SparseExactSnapshot);
    TestFalse(
        TEXT("Reset clears every previous-player-action hierarchy level"),
        Prediction.bHasPrediction
    );
    TestEqual(
        TEXT("Reset clears exact previous-player-action evidence"),
        Predictor.GetPreviousPlayerActionConditionalSampleCount(
            EEnemyCombatAction::HeavyAttack,
            ECombatDistanceCategory::Close,
            ERelativePlayerPosition::Front,
            EPlayerCombatAction::LightAttack
        ),
        0
    );
    return true;
}

#endif
