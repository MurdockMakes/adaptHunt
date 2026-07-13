#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "Misc/AutomationTest.h"

#include "AI/AdaptiveCounterOutcome.h"
#include "AI/EnemyActionScorer.h"
#include "Components/EnemyDecisionComponent.h"
#include "Data/LearningTelemetry.h"

namespace
{
FAdaptiveCounterOutcomeRecord MakeOutcomeRecord(
    const int32 ActionId,
    const int32 RoundNumber,
    const EPlayerCombatAction PredictedAction,
    const EEnemyCombatAction CounterAction,
    const EAdaptiveCounterOutcomeResult Result,
    const bool bAdaptiveOpportunity = true
)
{
    FAdaptiveCounterOutcomeRecord Record;
    Record.ActionId = ActionId;
    Record.RoundNumber = RoundNumber;
    Record.Action = CounterAction;
    Record.ProfilePreferredCounterAction = CounterAction;
    Record.Result = Result;
    Record.ContextSnapshot.RoundNumber = RoundNumber;
    Record.ContextSnapshot.DistanceCategory =
        ECombatDistanceCategory::Close;
    Record.ContextSnapshot.RelativePlayerPosition =
        ERelativePlayerPosition::Front;
    if (PredictedAction != EPlayerCombatAction::None)
    {
        Record.Prediction.bHasPrediction = true;
        Record.Prediction.PredictedAction = PredictedAction;
        Record.Prediction.Confidence = 0.8f;
        Record.Prediction.SupportingSampleCount = 5;
        Record.Prediction.bUsedContext = true;
        Record.Prediction.ConditioningEnemyAction =
            EEnemyCombatAction::LightAttack;
        Record.bPredictionApplied = bAdaptiveOpportunity;
        Record.bAdaptiveCounterOpportunity = bAdaptiveOpportunity;
    }
    return Record;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCounterOutcomeAssociationTest,
    "adaptHunt.Milestone25.AssociationDuplicateGuardAndMatchReset",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCounterOutcomeAssociationTest::RunTest(
    const FString& Parameters
)
{
    FEnemyCombatActionOutcome RawOutcome;
    RawOutcome.ActionId = 7;
    RawOutcome.Action = EEnemyCombatAction::HeavyAttack;
    RawOutcome.Result = EAdaptiveCounterOutcomeResult::Hit;
    TestTrue(TEXT("A raw executor result has a stable action ID"),
        RawOutcome.IsValid());

    FAdaptiveCounterOutcomeHistory History;
    const FAdaptiveCounterOutcomeRecord RoundOne = MakeOutcomeRecord(
        7,
        1,
        EPlayerCombatAction::Block,
        EEnemyCombatAction::HeavyAttack,
        EAdaptiveCounterOutcomeResult::Hit
    );
    TestTrue(TEXT("The associated result is accepted once"),
        History.Record(RoundOne));
    TestFalse(TEXT("The same committed action cannot resolve twice"),
        History.Record(RoundOne));

    FAdaptiveCounterOutcomeRecord Baseline = MakeOutcomeRecord(
        8,
        2,
        EPlayerCombatAction::None,
        EEnemyCombatAction::LightAttack,
        EAdaptiveCounterOutcomeResult::Missed,
        false
    );
    Baseline.ProfilePreferredCounterAction = EEnemyCombatAction::None;
    TestTrue(TEXT("Baseline committed actions are retained as telemetry"),
        History.Record(Baseline));
    TestEqual(TEXT("Outcome history persists across round numbers"),
        History.Num(), 2);
    TestTrue(TEXT("The last result retains its round and action context"),
        History.GetLastRecord()
            && History.GetLastRecord()->RoundNumber == 2
            && History.GetLastRecord()->Action
                == EEnemyCombatAction::LightAttack);

    FAdaptiveCounterOutcomeRecord Invalidated = MakeOutcomeRecord(
        9,
        2,
        EPlayerCombatAction::Block,
        EEnemyCombatAction::HeavyAttack,
        EAdaptiveCounterOutcomeResult::InvalidatedBeforeActiveFrame
    );
    TestTrue(TEXT("Pre-active invalidation is a terminal valid result"),
        Invalidated.IsValid());
    TestFalse(TEXT("Invalidation never counts as a successful counter"),
        Invalidated.WasSuccessfulCounter());

    History.Reset();
    TestEqual(TEXT("An explicit new-match reset clears all outcome data"),
        History.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCounterEffectivenessAggregationTest,
    "adaptHunt.Milestone25.DeterministicPerRoundEffectiveness",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCounterEffectivenessAggregationTest::RunTest(
    const FString& Parameters
)
{
    const FAdaptiveCounterEffectivenessTuning Tuning;
    TArray<FAdaptiveCounterOutcomeRecord> Sparse;
    Sparse.Add(MakeOutcomeRecord(
        1,
        2,
        EPlayerCombatAction::Block,
        EEnemyCombatAction::HeavyAttack,
        EAdaptiveCounterOutcomeResult::Hit
    ));
    const FAdaptiveCounterEffectivenessSummary SparseSummary =
        FAdaptiveCounterEffectivenessPolicy::AnalyzeForCounter(
            Sparse,
            EPlayerCombatAction::Block,
            EEnemyCombatAction::HeavyAttack,
            2,
            Tuning
        );
    TestTrue(TEXT("One successful counter remains below the evidence gate"),
        SparseSummary.SampleCount == 1
            && !SparseSummary.bMeetsMinimumSamples
            && FMath::IsNearlyZero(SparseSummary.UtilityModifier));

    TArray<FAdaptiveCounterOutcomeRecord> Records = Sparse;
    Records.Add(MakeOutcomeRecord(
        2,
        2,
        EPlayerCombatAction::Block,
        EEnemyCombatAction::HeavyAttack,
        EAdaptiveCounterOutcomeResult::Hit
    ));
    Records.Add(MakeOutcomeRecord(
        3,
        2,
        EPlayerCombatAction::Block,
        EEnemyCombatAction::HeavyAttack,
        EAdaptiveCounterOutcomeResult::Blocked
    ));
    Records.Add(MakeOutcomeRecord(
        4,
        1,
        EPlayerCombatAction::Block,
        EEnemyCombatAction::HeavyAttack,
        EAdaptiveCounterOutcomeResult::Hit,
        false
    ));

    const TArray<FAdaptiveCounterEffectivenessSummary> RoundTwo =
        FAdaptiveCounterEffectivenessPolicy::AnalyzeRound(
            Records,
            2,
            Tuning
        );
    TestEqual(TEXT("Round telemetry groups one deterministic counter pair"),
        RoundTwo.Num(), 1);
    if (RoundTwo.Num() != 1)
    {
        return false;
    }

    const FAdaptiveCounterEffectivenessSummary& Summary = RoundTwo[0];
    TestTrue(TEXT("Only adaptive attempts in the requested round aggregate"),
        Summary.RoundNumber == 2 && Summary.SampleCount == 3
            && Summary.SuccessfulCount == 2);
    TestTrue(TEXT("Symmetric smoothing keeps a small perfect streak bounded"),
        FMath::IsNearlyEqual(Summary.SmoothedEffectiveness, 0.6f)
            && Summary.bMeetsMinimumSamples
            && FMath::IsNearlyEqual(Summary.UtilityModifier, 0.016f));

    Algo::Reverse(Records);
    const TArray<FAdaptiveCounterEffectivenessSummary> Reversed =
        FAdaptiveCounterEffectivenessPolicy::AnalyzeRound(
            Records,
            2,
            Tuning
        );
    TestTrue(TEXT("Aggregation is independent of event insertion order"),
        Reversed.Num() == 1
            && Reversed[0].SampleCount == Summary.SampleCount
            && Reversed[0].SuccessfulCount == Summary.SuccessfulCount
            && FMath::IsNearlyEqual(
                Reversed[0].UtilityModifier,
                Summary.UtilityModifier
            ));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCounterEffectivenessScoringTest,
    "adaptHunt.Milestone25.BoundedSecondaryScoringAndSafety",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCounterEffectivenessScoringTest::RunTest(
    const FString& Parameters
)
{
    FEnemyActionScorer Scorer;
    FEnemyActionScoringContext Baseline;
    Baseline.Snapshot.DistanceCategory = ECombatDistanceCategory::Close;
    const float BaselineHeavy = Scorer.ScoreAction(
        EEnemyCombatAction::HeavyAttack,
        Baseline
    ).Score;

    FEnemyActionScoringContext Malformed = Baseline;
    Malformed.MaximumOutcomeEffectivenessUtilityAdjustment = 0.08f;
    Malformed.OutcomeEffectivenessUtilityModifiers.Add(
        EEnemyCombatAction::HeavyAttack,
        100.0f
    );
    TestTrue(TEXT("The scorer independently clamps outcome influence"),
        FMath::IsNearlyEqual(
            Scorer.ScoreAction(
                EEnemyCombatAction::HeavyAttack,
                Malformed
            ).Score,
            BaselineHeavy + 0.08f
        ));

    Malformed.UnavailableActions.Add(EEnemyCombatAction::HeavyAttack);
    const FEnemyActionScore Unavailable = Scorer.ScoreAction(
        EEnemyCombatAction::HeavyAttack,
        Malformed
    );
    TestTrue(TEXT("Outcome learning cannot make an unavailable action legal"),
        !Unavailable.bAvailable && FMath::IsNearlyZero(Unavailable.Score));

    FAdaptiveCounterEffectivenessSummary Summary;
    Summary.PredictedPlayerAction = EPlayerCombatAction::Block;
    Summary.CounterAction = EEnemyCombatAction::HeavyAttack;
    Summary.SampleCount = 3;
    Summary.SuccessfulCount = 3;
    Summary.SmoothedEffectiveness = 0.8f;
    Summary.UtilityModifier = 100.0f;
    Summary.bMeetsMinimumSamples = true;
    const FAdaptiveCounterEffectivenessTuning Tuning;
    TestTrue(TEXT("The pure policy also clamps malformed summary input"),
        FMath::IsNearlyEqual(
            FAdaptiveCounterEffectivenessPolicy::GetUtilityModifier(
                Summary,
                EEnemyCombatAction::HeavyAttack,
                true,
                Tuning
            ),
            Tuning.MaximumUtilityAdjustment
        ));
    TestTrue(TEXT("No existing profile opportunity means no outcome modifier"),
        FMath::IsNearlyZero(
            FAdaptiveCounterEffectivenessPolicy::GetUtilityModifier(
                Summary,
                EEnemyCombatAction::HeavyAttack,
                false,
                Tuning
            )
        ));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCounterOutcomeDebugTest,
    "adaptHunt.Milestone25.DebugExplainsPredictionCounterAndResult",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCounterOutcomeDebugTest::RunTest(
    const FString& Parameters
)
{
    FAdaptiveDebugTelemetrySnapshot Snapshot;
    Snapshot.CurrentRound = 3;
    Snapshot.bPredictionsEnabled = true;
    Snapshot.CounterOutcomeCount = 3;
    Snapshot.bHasLastCounterOutcome = true;
    Snapshot.LastCounterOutcome = MakeOutcomeRecord(
        3,
        2,
        EPlayerCombatAction::Block,
        EEnemyCombatAction::HeavyAttack,
        EAdaptiveCounterOutcomeResult::Hit
    );

    FAdaptiveCounterEffectivenessSummary Summary;
    Summary.RoundNumber = 2;
    Summary.EvaluatedThroughRound = 2;
    Summary.PredictedPlayerAction = EPlayerCombatAction::Block;
    Summary.CounterAction = EEnemyCombatAction::HeavyAttack;
    Summary.SampleCount = 3;
    Summary.SuccessfulCount = 2;
    Summary.SmoothedEffectiveness = 0.6f;
    Summary.UtilityModifier = 0.016f;
    Summary.bMeetsMinimumSamples = true;
    Snapshot.LastRoundCounterEffectiveness.Add(Summary);
    Summary.RoundNumber = 0;
    Snapshot.ActiveCounterEffectiveness = Summary;

    const FString Text = FString::Join(
        FAdaptiveDebugTelemetryFormatter::FormatHudLines(Snapshot),
        TEXT("\n")
    );
    TestTrue(TEXT("Telemetry states the prediction, chosen counter, and result"),
        Text.Contains(TEXT(
            "Last Counter Evaluation: predicted Block at 80% -> chose Heavy Attack -> Hit (worked)"
        )));
    TestTrue(TEXT("Telemetry shows deterministic round effectiveness"),
        Text.Contains(TEXT(
            "Round 2 Effectiveness: Heavy Attack vs predicted Block = 2/3 worked"
        )));
    TestTrue(TEXT("Telemetry exposes the bounded secondary contribution"),
        Text.Contains(TEXT("Outcome Secondary Modifier:"))
            && Text.Contains(TEXT("utility +1.6%")));

    const UEnemyDecisionComponent* Defaults =
        GetDefault<UEnemyDecisionComponent>();
    TestTrue(TEXT("Default outcome evidence needs multiple samples"),
        Defaults
            && Defaults->GetCounterEffectivenessTuning().MinimumSamples >= 2
            && Defaults->GetCounterEffectivenessTuning()
                .MaximumUtilityAdjustment <= 0.10f
            && Defaults->GetCounterEffectivenessTuning()
                .MaximumUtilityAdjustment
                < Defaults->GetAdaptiveTacticalProfileTuning()
                    .MaximumUtilityAdjustment);
    return true;
}

#endif
