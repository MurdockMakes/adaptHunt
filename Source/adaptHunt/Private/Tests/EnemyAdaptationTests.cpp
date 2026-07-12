#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/EnemyActionScorer.h"
#include "AI/FrequencyActionPredictor.h"

namespace
{
void AddAdaptationSamples(
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

bool IsAggressiveAction(const EEnemyCombatAction Action)
{
    return Action == EEnemyCombatAction::LightAttack
        || Action == EEnemyCombatAction::HeavyAttack
        || Action == EEnemyCombatAction::ProjectileAttack
        || Action == EEnemyCombatAction::DashAttack
        || Action == EEnemyCombatAction::InterruptHeal;
}

float GetHighestAggressionScore(
    const TArray<FEnemyActionScore>& Scores
)
{
    float HighestScore = 0.0f;
    for (const FEnemyActionScore& Score : Scores)
    {
        if (Score.bAvailable && IsAggressiveAction(Score.Action))
        {
            HighestScore = FMath::Max(HighestScore, Score.Score);
        }
    }
    return HighestScore;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEnemyPredictionChangesDecisionTest,
    "adaptHunt.Milestone10.PredictionChangesDecision",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEnemyPredictionChangesDecisionTest::RunTest(
    const FString& Parameters
)
{
    FCombatDataset Dataset;
    AddAdaptationSamples(Dataset, EPlayerCombatAction::Block, 8);
    AddAdaptationSamples(Dataset, EPlayerCombatAction::LightAttack, 2);

    FFrequencyActionPredictor Predictor;
    Predictor.Train(Dataset);

    FEnemyActionScorer Scorer;
    FEnemyActionScoringContext BaselineContext;
    BaselineContext.Snapshot.DistanceCategory =
        ECombatDistanceCategory::Close;
    TestTrue(
        TEXT("Baseline close-range behavior prefers a light attack"),
        Scorer.SelectBestAction(BaselineContext)
            == EEnemyCombatAction::LightAttack
    );

    FEnemyActionScoringContext AdaptiveContext = BaselineContext;
    AdaptiveContext.Prediction = Predictor.Predict(
        AdaptiveContext.Snapshot
    );
    AdaptiveContext.PredictionInfluence = 1.0f;

    TestTrue(
        TEXT("The model predicts the learned blocking habit"),
        AdaptiveContext.Prediction.PredictedAction
            == EPlayerCombatAction::Block
    );
    TestTrue(
        TEXT("Prediction changes the selected counter to a heavy attack"),
        Scorer.SelectBestAction(AdaptiveContext)
            == EEnemyCombatAction::HeavyAttack
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEnemyPredictedHealAggressionTest,
    "adaptHunt.Milestone10.PredictedHealRaisesAggression",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEnemyPredictedHealAggressionTest::RunTest(
    const FString& Parameters
)
{
    FCombatDataset Dataset;
    AddAdaptationSamples(Dataset, EPlayerCombatAction::Heal, 9);
    AddAdaptationSamples(Dataset, EPlayerCombatAction::Block, 1);

    FFrequencyActionPredictor Predictor;
    Predictor.Train(Dataset);

    FEnemyActionScorer Scorer;
    FEnemyActionScoringContext BaselineContext;
    BaselineContext.Snapshot.PlayerHealthNormalized = 0.20f;
    BaselineContext.Snapshot.DistanceCategory =
        ECombatDistanceCategory::Medium;
    const float BaselineAggression = GetHighestAggressionScore(
        Scorer.ScoreActions(BaselineContext)
    );

    FEnemyActionScoringContext AdaptiveContext = BaselineContext;
    AdaptiveContext.Prediction = Predictor.Predict(
        AdaptiveContext.Snapshot
    );
    AdaptiveContext.PredictionInfluence = 0.75f;
    const TArray<FEnemyActionScore> AdaptiveScores =
        Scorer.ScoreActions(AdaptiveContext);
    const float AdaptiveAggression = GetHighestAggressionScore(
        AdaptiveScores
    );

    TestTrue(
        TEXT("The low-health synthetic pattern predicts Heal"),
        AdaptiveContext.Prediction.PredictedAction
            == EPlayerCombatAction::Heal
    );
    TestTrue(
        TEXT("A predicted heal raises the enemy's aggression score"),
        AdaptiveAggression > BaselineAggression
    );
    TestTrue(
        TEXT("A predicted heal makes pre-emptive interruption useful"),
        Scorer.ScoreAction(
            EEnemyCombatAction::InterruptHeal,
            AdaptiveContext
        ).Score > Scorer.ScoreAction(
            EEnemyCombatAction::InterruptHeal,
            BaselineContext
        ).Score
    );
    return true;
}

#endif
