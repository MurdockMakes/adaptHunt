#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/EnemyDecisionPolicy.h"
#include "Components/EnemyDecisionComponent.h"
#include "Game/AdaptiveHuntTuningSettings.h"

#include <limits>

namespace
{
FEnemyActionScore MakeScore(
    const EEnemyCombatAction Action,
    const float Utility,
    const bool bAvailable = true
)
{
    FEnemyActionScore Score;
    Score.Action = Action;
    Score.UnclampedScore = Utility;
    Score.Score = FMath::Clamp(Utility, 0.0f, 1.0f);
    Score.bAvailable = bAvailable;
    return Score;
}

FEnemyCombatActionOutcome MakeOutcome(
    const int32 ActionId,
    const EEnemyCombatAction Action,
    const EAdaptiveCounterOutcomeResult Result
)
{
    FEnemyCombatActionOutcome Outcome;
    Outcome.ActionId = ActionId;
    Outcome.Action = Action;
    Outcome.Result = Result;
    return Outcome;
}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveNearBestSelectionTest,
    "adaptHunt.Milestone28.NearBestSeededSelection",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveNearBestSelectionTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    TArray<FEnemyActionScore> Scores = {
        MakeScore(EEnemyCombatAction::LightAttack, 1.24f),
        MakeScore(EEnemyCombatAction::HeavyAttack, 1.18f),
        MakeScore(EEnemyCombatAction::Dodge, 1.12f),
        MakeScore(EEnemyCombatAction::ProjectileAttack, 0.72f),
        MakeScore(EEnemyCombatAction::DashAttack, 2.0f, false)
    };
    FEnemyActionSelectionTuning Tuning;
    Tuning.NearBestScoreWindow = 0.14f;
    Tuning.SelectionTemperature = 0.09f;

    const FEnemyActionSelectionResult LowRoll =
        FEnemyActionSelectionPolicy::Select(Scores, 0.0f, Tuning);
    const FEnemyActionSelectionResult HighRoll =
        FEnemyActionSelectionPolicy::Select(Scores, 0.98f, Tuning);
    TestTrue(
        TEXT("Different rolls can choose different credible near-best actions"),
        LowRoll.IsValid() && HighRoll.IsValid()
            && LowRoll.Action != HighRoll.Action
    );
    TestFalse(
        TEXT("A clearly inferior utility is outside the candidate window"),
        LowRoll.CandidateActions.Contains(
            EEnemyCombatAction::ProjectileAttack
        )
    );
    TestFalse(
        TEXT("An unavailable action is never a candidate despite its score"),
        LowRoll.CandidateActions.Contains(EEnemyCombatAction::DashAttack)
    );

    FRandomStream FirstStream(90125);
    FRandomStream SecondStream(90125);
    TArray<EEnemyCombatAction> FirstSequence;
    TArray<EEnemyCombatAction> SecondSequence;
    TSet<EEnemyCombatAction> VariedActions;
    for (int32 Index = 0; Index < 32; ++Index)
    {
        const EEnemyCombatAction First =
            FEnemyActionSelectionPolicy::Select(
                Scores,
                FirstStream.FRand(),
                Tuning
            ).Action;
        const EEnemyCombatAction Second =
            FEnemyActionSelectionPolicy::Select(
                Scores,
                SecondStream.FRand(),
                Tuning
            ).Action;
        FirstSequence.Add(First);
        SecondSequence.Add(Second);
        VariedActions.Add(First);
    }
    TestTrue(
        TEXT("The same seed and inputs reproduce the complete sequence"),
        FirstSequence == SecondSequence
    );
    TestTrue(
        TEXT("The reproducible scenario varies among sensible actions"),
        VariedActions.Num() >= 2
    );

    TArray<FEnemyActionScore> SaturatedTies = {
        MakeScore(EEnemyCombatAction::LightAttack, 1.10f),
        MakeScore(EEnemyCombatAction::HeavyAttack, 1.26f)
    };
    TestTrue(
        TEXT("Raw utility preserves ranking after both display scores saturate"),
        FEnemyActionSelectionPolicy::SelectBest(SaturatedTies)
            == EEnemyCombatAction::HeavyAttack
    );

    SaturatedTies[0].UnclampedScore = 1.2f;
    SaturatedTies[1].UnclampedScore = 1.2f;
    FRandomStream TieStream(73);
    TSet<EEnemyCombatAction> TieChoices;
    for (int32 Index = 0; Index < 24; ++Index)
    {
        TieChoices.Add(FEnemyActionSelectionPolicy::Select(
            SaturatedTies,
            TieStream.FRand(),
            Tuning
        ).Action);
    }
    TestEqual(
        TEXT("Equal saturated scores do not permanently favor enum order"),
        TieChoices.Num(),
        2
    );
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveRepetitionFatigueTest,
    "adaptHunt.Milestone28.BoundedRepetitionFatigue",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveRepetitionFatigueTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FEnemyActionRepetitionTuning Tuning;
    FEnemyCommittedActionHistory History;
    History.Record(EEnemyCombatAction::LightAttack, Tuning.HistorySize);
    const FEnemyRepetitionModifier OneRepeat =
        FEnemyActionRepetitionPolicy::Evaluate(
            EEnemyCombatAction::LightAttack,
            History,
            Tuning
        );
    History.Record(EEnemyCombatAction::LightAttack, Tuning.HistorySize);
    const FEnemyRepetitionModifier TwoRepeats =
        FEnemyActionRepetitionPolicy::Evaluate(
            EEnemyCombatAction::LightAttack,
            History,
            Tuning
        );
    for (int32 Index = 0; Index < 12; ++Index)
    {
        History.Record(EEnemyCombatAction::LightAttack, Tuning.HistorySize);
    }
    const FEnemyRepetitionModifier ManyRepeats =
        FEnemyActionRepetitionPolicy::Evaluate(
            EEnemyCombatAction::LightAttack,
            History,
            Tuning
        );

    TestTrue(
        TEXT("Immediate repetition applies meaningful fatigue"),
        OneRepeat.Total < -0.20f
    );
    TestTrue(
        TEXT("Repeated recent use grows the penalty"),
        TwoRepeats.Total < OneRepeat.Total
    );
    TestTrue(
        TEXT("Repetition fatigue remains hard bounded"),
        ManyRepeats.Total >= -Tuning.MaximumRepetitionAdjustment
            - KINDA_SMALL_NUMBER
    );
    TestEqual(
        TEXT("Committed action history stays within its fixed window"),
        History.Num(),
        Tuning.HistorySize
    );

    TArray<FEnemyActionScore> SoleAction = {
        MakeScore(
            EEnemyCombatAction::LightAttack,
            0.2f + ManyRepeats.Total
        )
    };
    TestTrue(
        TEXT("Fatigue never prevents the sole valid response"),
        FEnemyActionSelectionPolicy::Select(
            SoleAction,
            0.5f,
            FEnemyActionSelectionTuning()
        ).Action == EEnemyCombatAction::LightAttack
    );
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveRecentOutcomeMemoryTest,
    "adaptHunt.Milestone28.RecentOutcomeMemory",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveRecentOutcomeMemoryTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FEnemyRecentOutcomeTuning Tuning;
    Tuning.MemorySize = 4;
    FEnemyRecentOutcomeMemory Memory;

    TestTrue(
        TEXT("A valid committed terminal result enters recent memory"),
        Memory.Record(MakeOutcome(
            1,
            EEnemyCombatAction::LightAttack,
            EAdaptiveCounterOutcomeResult::Blocked
        ), Tuning.MemorySize)
    );
    const float OneFailure = FEnemyRecentOutcomePolicy::Evaluate(
        EEnemyCombatAction::LightAttack,
        Memory,
        Tuning
    ).Total;
    Memory.Record(MakeOutcome(
        2,
        EEnemyCombatAction::LightAttack,
        EAdaptiveCounterOutcomeResult::Dodged
    ), Tuning.MemorySize);
    const float TwoFailures = FEnemyRecentOutcomePolicy::Evaluate(
        EEnemyCombatAction::LightAttack,
        Memory,
        Tuning
    ).Total;
    Memory.Record(MakeOutcome(
        3,
        EEnemyCombatAction::LightAttack,
        EAdaptiveCounterOutcomeResult::Missed
    ), Tuning.MemorySize);
    const float ThreeFailures = FEnemyRecentOutcomePolicy::Evaluate(
        EEnemyCombatAction::LightAttack,
        Memory,
        Tuning
    ).Total;

    TestTrue(
        TEXT("One isolated failure makes only a small adjustment"),
        OneFailure < 0.0f
            && FMath::Abs(OneFailure) < Tuning.FailurePenalty
    );
    TestTrue(
        TEXT("Repeated blocked/dodged/missed outcomes adjust more strongly"),
        TwoFailures < OneFailure && ThreeFailures < TwoFailures
    );
    TestFalse(
        TEXT("A duplicate terminal action ID cannot contaminate memory"),
        Memory.Record(MakeOutcome(
            3,
            EEnemyCombatAction::LightAttack,
            EAdaptiveCounterOutcomeResult::Missed
        ), Tuning.MemorySize)
    );
    TestFalse(
        TEXT("An invalid terminal result cannot contaminate memory"),
        Memory.Record(MakeOutcome(
            4,
            EEnemyCombatAction::LightAttack,
            EAdaptiveCounterOutcomeResult::None
        ), Tuning.MemorySize)
    );

    for (int32 ActionId = 4; ActionId <= 7; ++ActionId)
    {
        Memory.Record(MakeOutcome(
            ActionId,
            EEnemyCombatAction::HeavyAttack,
            EAdaptiveCounterOutcomeResult::Hit
        ), Tuning.MemorySize);
    }
    TestEqual(
        TEXT("Recent result storage remains bounded"),
        Memory.Num(),
        Tuning.MemorySize
    );
    TestTrue(
        TEXT("Old action failures leave the bounded window"),
        FMath::IsNearlyZero(FEnemyRecentOutcomePolicy::Evaluate(
            EEnemyCombatAction::LightAttack,
            Memory,
            Tuning
        ).Total)
    );
    Memory.Reset();
    TestEqual(TEXT("A lifecycle reset clears recent outcomes"), Memory.Num(), 0);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveUrgentDecisionPolicyTest,
    "adaptHunt.Milestone28.FairUrgentDecisionTiming",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveUrgentDecisionPolicyTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FEnemyUrgentDecisionTuning Tuning;
    FEnemyUrgentDecisionState State;

    TestFalse(
        TEXT("An idle action label cannot schedule an urgent decision"),
        State.ObservePhaseEdge(
            EPlayerCombatAction::HeavyAttack,
            ECombatActionPhase::Idle,
            true,
            10.0,
            Tuning
        )
    );
    TestTrue(
        TEXT("A committed attack windup schedules one delayed evaluation"),
        State.ObservePhaseEdge(
            EPlayerCombatAction::LightAttack,
            ECombatActionPhase::Windup,
            true,
            10.0,
            Tuning
        )
    );
    TestFalse(
        TEXT("The reaction cannot fire before its configured delay"),
        State.ConsumeIfReady(
            EPlayerCombatAction::LightAttack,
            ECombatActionPhase::Active,
            true,
            10.17,
            Tuning
        )
    );
    TestTrue(
        TEXT("The reaction can fire once the delay has elapsed"),
        State.ConsumeIfReady(
            EPlayerCombatAction::LightAttack,
            ECombatActionPhase::Active,
            true,
            10.19,
            Tuning
        )
    );

    State.ObservePhaseEdge(
        EPlayerCombatAction::LightAttack,
        ECombatActionPhase::Recovery,
        true,
        10.25,
        Tuning
    );
    TestFalse(
        TEXT("The cooldown rejects a second urgent windup"),
        State.ObservePhaseEdge(
            EPlayerCombatAction::HeavyAttack,
            ECombatActionPhase::Windup,
            true,
            10.3,
            Tuning
        )
    );

    State.Reset();
    State.ObservePhaseEdge(
        EPlayerCombatAction::Heal,
        ECombatActionPhase::Windup,
        true,
        20.0,
        Tuning
    );
    TestFalse(
        TEXT("No urgent decision occurs while the enemy is committed"),
        State.ConsumeIfReady(
            EPlayerCombatAction::Heal,
            ECombatActionPhase::Windup,
            false,
            20.3,
            Tuning
        )
    );

    State.Reset();
    State.ObservePhaseEdge(
        EPlayerCombatAction::HeavyAttack,
        ECombatActionPhase::Windup,
        true,
        30.0,
        Tuning
    );
    TestFalse(
        TEXT("An action that is already over cannot trigger a late reaction"),
        State.ConsumeIfReady(
            EPlayerCombatAction::HeavyAttack,
            ECombatActionPhase::Idle,
            true,
            30.3,
            Tuning
        )
    );
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveDecisionPolicySanitizationTest,
    "adaptHunt.Milestone28.DecisionTuningSanitization",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveDecisionPolicySanitizationTest::RunTest(
    const FString& Parameters
)
{
    static_cast<void>(Parameters);
    const float Infinity = std::numeric_limits<float>::infinity();

    FEnemyActionSelectionTuning Selection;
    Selection.NearBestScoreWindow = Infinity;
    Selection.SelectionTemperature = -10.0f;
    const FEnemyActionSelectionTuning SafeSelection =
        Selection.GetSanitized();
    TestTrue(
        TEXT("Selection tuning rejects non-finite and out-of-range values"),
        FMath::IsFinite(SafeSelection.NearBestScoreWindow)
            && SafeSelection.SelectionTemperature >= 0.01f
    );

    FEnemyActionRepetitionTuning Repetition;
    Repetition.HistorySize = 100;
    Repetition.ImmediateRepeatPenalty = Infinity;
    Repetition.AccumulatedRepeatPenalty = -1.0f;
    const FEnemyActionRepetitionTuning SafeRepetition =
        Repetition.GetSanitized();
    TestTrue(
        TEXT("Repetition tuning is finite and hard bounded"),
        SafeRepetition.HistorySize == 8
            && FMath::IsFinite(SafeRepetition.ImmediateRepeatPenalty)
            && SafeRepetition.AccumulatedRepeatPenalty >= 0.0f
    );

    FEnemyRecentOutcomeTuning Outcome;
    Outcome.MemorySize = -4;
    Outcome.FailurePenalty = Infinity;
    Outcome.MaximumOutcomeAdjustment = 4.0f;
    const FEnemyRecentOutcomeTuning SafeOutcome = Outcome.GetSanitized();
    TestTrue(
        TEXT("Recent-outcome tuning is finite and independently bounded"),
        SafeOutcome.MemorySize == 1
            && FMath::IsFinite(SafeOutcome.FailurePenalty)
            && SafeOutcome.MaximumOutcomeAdjustment <= 0.4f
    );

    FEnemyUrgentDecisionTuning Urgent;
    Urgent.ThreatReactionDelay = Infinity;
    Urgent.UrgentDecisionCooldown = -1.0f;
    const FEnemyUrgentDecisionTuning SafeUrgent = Urgent.GetSanitized();
    TestTrue(
        TEXT("Urgent tuning remains in a fair human-scale range"),
        SafeUrgent.ThreatReactionDelay >= 0.15f
            && SafeUrgent.ThreatReactionDelay <= 0.25f
            && SafeUrgent.UrgentDecisionCooldown >= 0.1f
    );

    FAdaptiveTacticalRuntimeTuning Runtime;
    Runtime.Selection = Selection;
    Runtime.Repetition = Repetition;
    Runtime.RecentOutcome = Outcome;
    Runtime.UrgentReactions = Urgent;
    const FAdaptiveTacticalRuntimeTuning SafeRuntime =
        Runtime.GetSanitized();
    TestTrue(
        TEXT("The central Adaptive Hunt settings sanitize every new channel"),
        FMath::IsFinite(SafeRuntime.Selection.NearBestScoreWindow)
            && FMath::IsFinite(
                SafeRuntime.Repetition.ImmediateRepeatPenalty
            )
            && FMath::IsFinite(SafeRuntime.RecentOutcome.FailurePenalty)
            && SafeRuntime.UrgentReactions.ThreatReactionDelay >= 0.15f
    );

    const UEnemyDecisionComponent* Defaults =
        GetDefault<UEnemyDecisionComponent>();
    TestTrue(
        TEXT("The decision component consumes safe native decision defaults"),
        Defaults
            && Defaults->GetActionSelectionTuning().NearBestScoreWindow
                == FEnemyActionSelectionTuning().NearBestScoreWindow
            && Defaults->GetRepetitionTuning().HistorySize
                == FEnemyActionRepetitionTuning().HistorySize
            && Defaults->GetRecentOutcomeTuning().MemorySize
                == FEnemyRecentOutcomeTuning().MemorySize
            && Defaults->GetUrgentDecisionTuning().ThreatReactionDelay
                == FEnemyUrgentDecisionTuning().ThreatReactionDelay
    );
    return true;
}

#endif
