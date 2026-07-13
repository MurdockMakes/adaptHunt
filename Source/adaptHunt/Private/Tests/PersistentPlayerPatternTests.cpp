#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/EnemyActionScorer.h"
#include "Components/EnemyDecisionComponent.h"
#include "Data/PersistentPlayerPatterns.h"

namespace
{
FTrainingSample MakePatternSample(
    const EPlayerCombatAction Action,
    const EPlayerCombatAction PreviousAction =
        EPlayerCombatAction::LightAttack
)
{
    FTrainingSample Sample;
    Sample.Snapshot.RoundNumber = 2;
    Sample.Snapshot.DistanceCategory = ECombatDistanceCategory::Close;
    Sample.Snapshot.RelativePlayerPosition =
        ERelativePlayerPosition::Front;
    Sample.Snapshot.PreviousPlayerAction = PreviousAction;
    Sample.Snapshot.PreviousEnemyAction = EEnemyCombatAction::Block;
    Sample.NextPlayerAction = Action;
    return Sample;
}

void AddPatternSamples(
    FCombatDataset& Dataset,
    const EPlayerCombatAction Action,
    const int32 Count
)
{
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Dataset.AddSample(MakePatternSample(Action));
    }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPersistentPlayerPatternRetentionTest,
    "adaptHunt.Milestone29.PersistentPatterns.RetentionAndReconstruction",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FPersistentPlayerPatternRetentionTest::RunTest(
    const FString& Parameters
)
{
    static_cast<void>(Parameters);
    FCombatDataset Source;
    Source.AddSample(MakePatternSample(EPlayerCombatAction::HeavyAttack));
    Source.AddSample(MakePatternSample(EPlayerCombatAction::Block));
    Source.AddSample(MakePatternSample(EPlayerCombatAction::LightAttack));
    Source.AddSample(MakePatternSample(EPlayerCombatAction::LightAttack));

    TArray<FPersistentPlayerPattern> Patterns;
    TestEqual(
        TEXT("Every valid committed sample is accepted"),
        FAdaptivePlayerPatternPolicy::AppendDataset(
            Patterns,
            Source,
            0,
            3
        ),
        4
    );
    TestEqual(TEXT("The history obeys its FIFO cap"), Patterns.Num(), 3);
    TestTrue(
        TEXT("The oldest retained label is the second source sample"),
        Patterns[0].NextPlayerAction == EPlayerCombatAction::Block
    );

    const FCombatDataset Restored =
        FAdaptivePlayerPatternPolicy::BuildDataset(Patterns);
    TestEqual(TEXT("Every retained pattern reconstructs"), Restored.Num(), 3);
    if (!Restored.IsEmpty())
    {
        const FTrainingSample& RestoredSample =
            Restored.GetSamples()[0];
        TestTrue(TEXT("Reconstructed rows remain valid"), RestoredSample.IsValid());
        TestEqual(
            TEXT("Historical rows use a stable synthetic profile round"),
            RestoredSample.Snapshot.RoundNumber,
            1
        );
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRecentLightAttackPressureTest,
    "adaptHunt.Milestone29.PersistentPatterns.LightSpamPressure",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FRecentLightAttackPressureTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FCombatDataset Persistent;
    FCombatDataset Current;
    AddPatternSamples(Persistent, EPlayerCombatAction::LightAttack, 2);
    AddPatternSamples(Current, EPlayerCombatAction::LightAttack, 2);

    const float Pressure = FAdaptivePlayerPatternPolicy::
        CalculateRecentLightAttackPressure(Persistent, Current, 6, 3);
    TestTrue(
        TEXT("Prior and current committed actions combine into full pressure"),
        FMath::IsNearlyEqual(Pressure, 1.0f)
    );

    FCombatDataset Insufficient;
    AddPatternSamples(Insufficient, EPlayerCombatAction::LightAttack, 2);
    TestTrue(
        TEXT("Two attacks do not create an anti-spam response"),
        FMath::IsNearlyZero(
            FAdaptivePlayerPatternPolicy::
                CalculateRecentLightAttackPressure(
                    FCombatDataset(),
                    Insufficient,
                    6,
                    3
                )
        )
    );

    FEnemyActionScoringContext BaselineContext;
    BaselineContext.Snapshot.DistanceCategory =
        ECombatDistanceCategory::Close;
    BaselineContext.Snapshot.PreviousPlayerAction =
        EPlayerCombatAction::None;
    BaselineContext.Snapshot.EnemyHealthNormalized = 1.0f;
    BaselineContext.Snapshot.PlayerHealthNormalized = 1.0f;
    BaselineContext.Snapshot.EnemyStaminaNormalized = 1.0f;
    FEnemyActionScorer Scorer;
    TestTrue(
        TEXT("Close-range baseline still prefers its light attack"),
        Scorer.ScoreAction(
            EEnemyCombatAction::LightAttack,
            BaselineContext
        ).UnclampedScore > Scorer.ScoreAction(
            EEnemyCombatAction::Block,
            BaselineContext
        ).UnclampedScore
    );

    BaselineContext.RecentLightAttackPressure = 1.0f;
    TestTrue(
        TEXT("Established light spam makes block outrank damage racing"),
        Scorer.ScoreAction(
            EEnemyCombatAction::Block,
            BaselineContext
        ).UnclampedScore > Scorer.ScoreAction(
            EEnemyCombatAction::LightAttack,
            BaselineContext
        ).UnclampedScore
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPersistentAndLivePredictorHistoryTest,
    "adaptHunt.Milestone29.PersistentPatterns.CombinedPredictorHistory",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FPersistentAndLivePredictorHistoryTest::RunTest(
    const FString& Parameters
)
{
    static_cast<void>(Parameters);
    FCombatDataset Persistent;
    AddPatternSamples(Persistent, EPlayerCombatAction::LightAttack, 3);
    FCombatDataset Live;
    AddPatternSamples(Live, EPlayerCombatAction::Block, 2);

    UEnemyDecisionComponent* Decision =
        NewObject<UEnemyDecisionComponent>();
    TestNotNull(TEXT("A decision component can be created"), Decision);
    if (!Decision)
    {
        return false;
    }

    Decision->SetPersistentTrainingDataset(Persistent);
    TestEqual(
        TEXT("Persistent evidence trains before a live round"),
        Decision->GetTrainedSampleCount(),
        3
    );
    Decision->TrainPredictor(Live);
    TestEqual(
        TEXT("Retraining combines prior-run and current-session samples"),
        Decision->GetTrainedSampleCount(),
        5
    );
    TestEqual(
        TEXT("Persistent telemetry remains separately inspectable"),
        Decision->GetPersistentTrainingSampleCount(),
        3
    );
    return true;
}

#endif
