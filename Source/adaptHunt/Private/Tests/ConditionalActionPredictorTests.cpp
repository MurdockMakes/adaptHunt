#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/ConditionalActionPredictor.h"

namespace
{
void AddConditionalSamples(
    FCombatDataset& Dataset,
    const EEnemyCombatAction PreviousEnemyAction,
    const EPlayerCombatAction PlayerAction,
    const int32 Count
)
{
    for (int32 Index = 0; Index < Count; ++Index)
    {
        FTrainingSample Sample;
        Sample.Snapshot.RoundNumber = 1;
        Sample.Snapshot.PreviousEnemyAction = PreviousEnemyAction;
        Sample.NextPlayerAction = PlayerAction;
        Dataset.AddSample(Sample);
    }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveConditionalActionPredictorContextTest,
    "adaptHunt.Milestone14.ContextChangesPrediction",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveConditionalActionPredictorContextTest::RunTest(
    const FString& Parameters
)
{
    FCombatDataset Dataset;
    AddConditionalSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        EPlayerCombatAction::DodgeLeft,
        4
    );
    AddConditionalSamples(
        Dataset,
        EEnemyCombatAction::HeavyAttack,
        EPlayerCombatAction::Block,
        1
    );
    AddConditionalSamples(
        Dataset,
        EEnemyCombatAction::ProjectileAttack,
        EPlayerCombatAction::Heal,
        3
    );
    AddConditionalSamples(
        Dataset,
        EEnemyCombatAction::None,
        EPlayerCombatAction::Heal,
        5
    );

    FConditionalActionPredictor Predictor;
    Predictor.Train(Dataset);

    FCombatSnapshot HeavyAttackContext;
    HeavyAttackContext.PreviousEnemyAction =
        EEnemyCombatAction::HeavyAttack;
    const FPredictionResult ContextualPrediction =
        Predictor.Predict(HeavyAttackContext);
    TestTrue(
        TEXT("The observed enemy action selects a contextual model"),
        ContextualPrediction.bUsedContext
            && ContextualPrediction.ConditioningEnemyAction
                == EEnemyCombatAction::HeavyAttack
    );
    TestTrue(
        TEXT("The contextual response differs from the global habit"),
        ContextualPrediction.PredictedAction
            == EPlayerCombatAction::DodgeLeft
    );
    TestEqual(
        TEXT("Context confidence uses only matching observations"),
        ContextualPrediction.Confidence,
        4.0f / 5.0f
    );
    TestEqual(
        TEXT("Context reports dominant-action support"),
        ContextualPrediction.SupportingSampleCount,
        4
    );

    FCombatSnapshot UnseenContext;
    UnseenContext.PreviousEnemyAction = EEnemyCombatAction::DashAttack;
    const FPredictionResult FallbackPrediction =
        Predictor.Predict(UnseenContext);
    TestTrue(
        TEXT("An unseen context falls back to a global prediction"),
        FallbackPrediction.bHasPrediction
            && !FallbackPrediction.bUsedContext
    );
    TestTrue(
        TEXT("The fallback preserves the global dominant action"),
        FallbackPrediction.PredictedAction == EPlayerCombatAction::Heal
    );
    TestEqual(
        TEXT("The interface exposes the most recent confidence"),
        Predictor.GetPredictionConfidence(),
        FallbackPrediction.Confidence
    );
    TestEqual(
        TEXT("All valid observations train the hierarchy"),
        Predictor.GetTrainedSampleCount(),
        13
    );
    TestEqual(
        TEXT("The context exposes its complete evidence count"),
        Predictor.GetConditionalSampleCount(
            EEnemyCombatAction::HeavyAttack
        ),
        5
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveConditionalActionPredictorFallbackTest,
    "adaptHunt.Milestone14.SparseFallbackAndLifecycle",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveConditionalActionPredictorFallbackTest::RunTest(
    const FString& Parameters
)
{
    FCombatDataset SparseDataset;
    AddConditionalSamples(
        SparseDataset,
        EEnemyCombatAction::Block,
        EPlayerCombatAction::HeavyAttack,
        2
    );
    AddConditionalSamples(
        SparseDataset,
        EEnemyCombatAction::None,
        EPlayerCombatAction::LightAttack,
        3
    );

    FConditionalActionPredictor Predictor(3);
    Predictor.Train(SparseDataset);
    FCombatSnapshot BlockContext;
    BlockContext.PreviousEnemyAction = EEnemyCombatAction::Block;
    FPredictionResult Prediction = Predictor.Predict(BlockContext);
    TestTrue(
        TEXT("A context below its evidence threshold uses fallback"),
        Prediction.bHasPrediction && !Prediction.bUsedContext
    );
    TestTrue(
        TEXT("Sparse fallback selects the global dominant action"),
        Prediction.PredictedAction == EPlayerCombatAction::LightAttack
    );

    FCombatDataset TieDataset;
    AddConditionalSamples(
        TieDataset,
        EEnemyCombatAction::Dodge,
        EPlayerCombatAction::LightAttack,
        1
    );
    AddConditionalSamples(
        TieDataset,
        EEnemyCombatAction::Dodge,
        EPlayerCombatAction::HeavyAttack,
        1
    );
    FConditionalActionPredictor TiePredictor(2);
    TiePredictor.Train(TieDataset);
    FCombatSnapshot DodgeContext;
    DodgeContext.PreviousEnemyAction = EEnemyCombatAction::Dodge;
    Prediction = TiePredictor.Predict(DodgeContext);
    TestTrue(
        TEXT("Context ties use the fixed player-action order"),
        Prediction.bUsedContext
            && Prediction.PredictedAction
                == EPlayerCombatAction::LightAttack
    );

    FCombatDataset ReplacementDataset;
    AddConditionalSamples(
        ReplacementDataset,
        EEnemyCombatAction::DashAttack,
        EPlayerCombatAction::Heal,
        3
    );
    TiePredictor.Train(ReplacementDataset);
    TestEqual(
        TEXT("Retraining removes old contextual evidence"),
        TiePredictor.GetConditionalSampleCount(
            EEnemyCombatAction::Dodge
        ),
        0
    );

    TiePredictor.Reset();
    Prediction = TiePredictor.Predict(DodgeContext);
    TestFalse(
        TEXT("Reset removes both contextual and fallback models"),
        Prediction.bHasPrediction
    );
    TestEqual(
        TEXT("Reset clears trained sample count"),
        TiePredictor.GetTrainedSampleCount(),
        0
    );
    return true;
}

#endif
