#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/EnemyActionScorer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEnemyUtilityDistanceTest,
    "adaptHunt.Milestone9.DistanceAndStaminaUtility",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEnemyUtilityDistanceTest::RunTest(
    const FString& Parameters
)
{
    FEnemyActionScorer Scorer;
    FEnemyActionScoringContext Context;
    Context.Snapshot.DistanceCategory = ECombatDistanceCategory::Close;

    TestTrue(
        TEXT("A healthy close-range enemy prefers its reliable light attack"),
        Scorer.SelectBestAction(Context)
            == EEnemyCombatAction::LightAttack
    );

    Context.Snapshot.DistanceCategory = ECombatDistanceCategory::Far;
    TestTrue(
        TEXT("High stamina makes the distance-closing dash useful at range"),
        Scorer.SelectBestAction(Context)
            == EEnemyCombatAction::DashAttack
    );

    Context.Snapshot.EnemyStaminaNormalized = 0.10f;
    TestTrue(
        TEXT("Low stamina favors the cheaper projectile at range"),
        Scorer.SelectBestAction(Context)
            == EEnemyCombatAction::ProjectileAttack
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEnemyUtilityDefenseAndHistoryTest,
    "adaptHunt.Milestone9.DefenseAndHistoryUtility",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEnemyUtilityDefenseAndHistoryTest::RunTest(
    const FString& Parameters
)
{
    FEnemyActionScorer Scorer;
    FEnemyActionScoringContext Context;
    Context.Snapshot.DistanceCategory = ECombatDistanceCategory::Close;
    Context.Snapshot.EnemyHealthNormalized = 0.10f;

    TestTrue(
        TEXT("A low-health enemy prioritizes defense"),
        Scorer.SelectBestAction(Context) == EEnemyCombatAction::Block
    );

    Context.Snapshot.EnemyHealthNormalized = 1.0f;
    Context.RepetitionUtilityModifiers.Add(
        EEnemyCombatAction::LightAttack,
        -0.22f
    );
    TestTrue(
        TEXT("A pure repetition modifier changes the best close action"),
        Scorer.SelectBestAction(Context)
            == EEnemyCombatAction::HeavyAttack
    );

    Context.RepetitionUtilityModifiers.Reset();
    Context.Snapshot.PreviousPlayerAction =
        EPlayerCombatAction::HeavyAttack;
    const float DodgeScore = Scorer.ScoreAction(
        EEnemyCombatAction::Dodge,
        Context
    ).Score;
    Context.Snapshot.PreviousPlayerAction = EPlayerCombatAction::None;
    TestTrue(
        TEXT("A recent heavy attack raises dodge utility"),
        DodgeScore > Scorer.ScoreAction(
            EEnemyCombatAction::Dodge,
            Context
        ).Score
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEnemyUtilityAvailabilityAndHealTest,
    "adaptHunt.Milestone9.AvailabilityAndHealUtility",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEnemyUtilityAvailabilityAndHealTest::RunTest(
    const FString& Parameters
)
{
    FEnemyActionScorer Scorer;
    FEnemyActionScoringContext Context;
    Context.Snapshot.DistanceCategory = ECombatDistanceCategory::Close;
    Context.bTargetRecentlyHealing = true;

    TestTrue(
        TEXT("A nearby heal makes interruption the best action"),
        Scorer.SelectBestAction(Context)
            == EEnemyCombatAction::InterruptHeal
    );

    Context.bTargetRecentlyHealing = false;
    Context.Snapshot.DistanceCategory = ECombatDistanceCategory::Far;
    Context.UnavailableActions.Add(EEnemyCombatAction::DashAttack);
    const TArray<FEnemyActionScore> Scores = Scorer.ScoreActions(Context);
    TestEqual(
        TEXT("Every executable combat action receives an observable result"),
        Scores.Num(),
        7
    );
    TestTrue(
        TEXT("An unavailable best action is excluded from selection"),
        Scorer.SelectBestAction(Scores)
            == EEnemyCombatAction::ProjectileAttack
    );

    const FEnemyActionScore* DashScore = Scores.FindByPredicate(
        [](const FEnemyActionScore& Score)
        {
            return Score.Action == EEnemyCombatAction::DashAttack;
        }
    );
    TestTrue(
        TEXT("Availability is visible in score telemetry"),
        DashScore && !DashScore->bAvailable
            && FMath::IsNearlyZero(DashScore->Score)
    );
    return true;
}

#endif
