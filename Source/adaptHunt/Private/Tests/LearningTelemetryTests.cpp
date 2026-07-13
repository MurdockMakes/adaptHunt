#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Data/LearningTelemetry.h"
#include "Components/EnemyDecisionComponent.h"
#include "Game/AdaptiveGameMode.h"
#include "UI/AdaptiveDebugHUD.h"

namespace
{
void AddTelemetrySample(
    FCombatDataset& Dataset,
    const int32 RoundNumber,
    const EEnemyCombatAction PreviousEnemyAction,
    const EPlayerCombatAction PlayerAction
)
{
    FTrainingSample Sample;
    Sample.Snapshot.RoundNumber = RoundNumber;
    Sample.Snapshot.PreviousEnemyAction = PreviousEnemyAction;
    Sample.NextPlayerAction = PlayerAction;
    Dataset.AddSample(Sample);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveConditionalLearningTelemetryTest,
    "adaptHunt.Milestone12.ConditionalPatternAnalysis",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveConditionalLearningTelemetryTest::RunTest(
    const FString& Parameters
)
{
    FCombatDataset Dataset;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        AddTelemetrySample(
            Dataset,
            1,
            EEnemyCombatAction::HeavyAttack,
            EPlayerCombatAction::DodgeLeft
        );
    }
    AddTelemetrySample(
        Dataset,
        1,
        EEnemyCombatAction::HeavyAttack,
        EPlayerCombatAction::Block
    );
    for (int32 Index = 0; Index < 2; ++Index)
    {
        AddTelemetrySample(
            Dataset,
            1,
            EEnemyCombatAction::LightAttack,
            EPlayerCombatAction::DodgeRight
        );
    }
    AddTelemetrySample(
        Dataset,
        2,
        EEnemyCombatAction::HeavyAttack,
        EPlayerCombatAction::Heal
    );

    const FAdaptiveLearningSummary RoundOne =
        FAdaptiveLearningTelemetryAnalyzer::Analyze(Dataset, 1);
    TestEqual(TEXT("Only Round 1 samples are analyzed"), RoundOne.SampleCount, 6);
    TestTrue(
        TEXT("Dodge Left is the round's most common player action"),
        RoundOne.MostCommonPlayerAction == EPlayerCombatAction::DodgeLeft
    );

    const FAdaptiveConditionalPattern& Pattern =
        RoundOne.StrongestConditionalPattern;
    TestTrue(TEXT("A conditional pattern is found"), Pattern.IsValid());
    TestTrue(
        TEXT("Enemy Heavy Attack is the strongest observed condition"),
        Pattern.PreviousEnemyAction == EEnemyCombatAction::HeavyAttack
    );
    TestTrue(
        TEXT("Dodge Left is its dominant response"),
        Pattern.DominantPlayerAction == EPlayerCombatAction::DodgeLeft
    );
    TestEqual(TEXT("The pattern has four supporting samples"), Pattern.TotalSampleCount, 4);
    TestEqual(
        TEXT("Dodge Left occupies 75 percent"),
        Pattern.GetPercentage(EPlayerCombatAction::DodgeLeft),
        75
    );
    TestEqual(
        TEXT("Block occupies 25 percent"),
        Pattern.GetPercentage(EPlayerCombatAction::Block),
        25
    );

    int32 PercentageTotal = 0;
    for (const FAdaptiveActionPercentage& Entry : Pattern.ActionPercentages)
    {
        PercentageTotal += Entry.Percentage;
    }
    TestEqual(TEXT("Displayed percentages sum exactly to 100"), PercentageTotal, 100);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveLearningTelemetryDeterminismTest,
    "adaptHunt.Milestone12.DeterministicTiesAndRounding",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveLearningTelemetryDeterminismTest::RunTest(
    const FString& Parameters
)
{
    FCombatDataset Dataset;
    AddTelemetrySample(
        Dataset,
        1,
        EEnemyCombatAction::Block,
        EPlayerCombatAction::LightAttack
    );
    AddTelemetrySample(
        Dataset,
        1,
        EEnemyCombatAction::Block,
        EPlayerCombatAction::HeavyAttack
    );
    AddTelemetrySample(
        Dataset,
        1,
        EEnemyCombatAction::Block,
        EPlayerCombatAction::DodgeLeft
    );

    const FAdaptiveLearningSummary Summary =
        FAdaptiveLearningTelemetryAnalyzer::Analyze(Dataset);
    const FAdaptiveConditionalPattern& Pattern =
        Summary.StrongestConditionalPattern;
    TestTrue(
        TEXT("Fixed action order breaks the global tie"),
        Summary.MostCommonPlayerAction == EPlayerCombatAction::LightAttack
    );
    TestTrue(
        TEXT("Fixed action order breaks the conditional tie"),
        Pattern.DominantPlayerAction == EPlayerCombatAction::LightAttack
    );
    TestEqual(
        TEXT("The first tied action receives the rounding remainder"),
        Pattern.GetPercentage(EPlayerCombatAction::LightAttack),
        34
    );
    TestEqual(
        TEXT("The second tied action remains at 33 percent"),
        Pattern.GetPercentage(EPlayerCombatAction::HeavyAttack),
        33
    );
    TestEqual(
        TEXT("The third tied action remains at 33 percent"),
        Pattern.GetPercentage(EPlayerCombatAction::DodgeLeft),
        33
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveDebugHudTelemetryTest,
    "adaptHunt.Milestone12.DebugHudFormattingAndRegistration",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveDebugHudTelemetryTest::RunTest(const FString& Parameters)
{
    FAdaptiveDebugTelemetrySnapshot Snapshot;
    Snapshot.CurrentRound = 2;
    Snapshot.bPredictionsEnabled = true;
    Snapshot.PlayerHealth = 82.0f;
    Snapshot.PlayerMaxHealth = 100.0f;
    Snapshot.EnemyHealth = 60.0f;
    Snapshot.EnemyMaxHealth = 100.0f;
    Snapshot.PlayerStamina = 45.0f;
    Snapshot.PlayerMaxStamina = 100.0f;
    Snapshot.LastPlayerAction = EPlayerCombatAction::DodgeLeft;
    Snapshot.Prediction.bHasPrediction = true;
    Snapshot.Prediction.PredictedAction = EPlayerCombatAction::Heal;
    Snapshot.Prediction.Confidence = 0.72f;
    Snapshot.Prediction.bUsedContext = true;
    Snapshot.Prediction.ConditioningEnemyAction =
        EEnemyCombatAction::HeavyAttack;
    Snapshot.Prediction.bUsedDistanceContext = true;
    Snapshot.Prediction.ConditioningDistanceCategory =
        ECombatDistanceCategory::Close;
    Snapshot.Prediction.bUsedPositionContext = true;
    Snapshot.Prediction.ConditioningRelativePlayerPosition =
        ERelativePlayerPosition::Behind;
    Snapshot.Prediction.bUsedPreviousPlayerActionContext = true;
    Snapshot.Prediction.ConditioningPreviousPlayerAction =
        EPlayerCombatAction::Block;
    Snapshot.CollectedSampleCount = 12;
    Snapshot.MostCommonPlayerAction = EPlayerCombatAction::DodgeLeft;
    Snapshot.CurrentEnemySelectedAction = EEnemyCombatAction::HeavyAttack;
    Snapshot.bLastDecisionUrgent = true;
    Snapshot.LastSelection.Action = EEnemyCombatAction::HeavyAttack;
    Snapshot.LastSelection.BestScore = 1.18f;
    Snapshot.LastSelection.SelectedScore = 1.12f;
    Snapshot.LastSelection.CandidateActions = {
        EEnemyCombatAction::LightAttack,
        EEnemyCombatAction::HeavyAttack
    };
    FEnemyDecisionModifierTelemetry Modifier;
    Modifier.Action = EEnemyCombatAction::HeavyAttack;
    Modifier.RepetitionModifier = -0.22f;
    Modifier.RecentOutcomeModifier = -0.12f;
    Snapshot.LastDecisionModifiers.Add(Modifier);
    Snapshot.RecentCommittedActionCount = 5;
    Snapshot.RecentOutcomeMemoryCount = 4;
    Snapshot.LastCompletedRound = 1;

    FCombatDataset Dataset;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        AddTelemetrySample(
            Dataset,
            1,
            EEnemyCombatAction::HeavyAttack,
            EPlayerCombatAction::DodgeLeft
        );
    }
    AddTelemetrySample(
        Dataset,
        1,
        EEnemyCombatAction::HeavyAttack,
        EPlayerCombatAction::Block
    );
    const FAdaptiveConditionalPattern Pattern =
        FAdaptiveLearningTelemetryAnalyzer::Analyze(Dataset)
            .StrongestConditionalPattern;
    Snapshot.StrongestLearnedPattern = Pattern;
    Snapshot.LastRoundObservedPattern = Pattern;

    const FString AllLines = FString::Join(
        FAdaptiveDebugTelemetryFormatter::FormatHudLines(Snapshot),
        TEXT("\n")
    );
    const TCHAR* RequiredLabels[] = {
        TEXT("Round: 2 / 3"),
        TEXT("Player Health: 82.0 / 100.0"),
        TEXT("Enemy Health: 60.0 / 100.0"),
        TEXT("Player Stamina: 45.0 / 100.0"),
        TEXT("Last Player Action: Dodge Left"),
        TEXT("Predicted Next Player Action: Heal"),
        TEXT("Prediction Confidence: 72%"),
        TEXT(
            "Prediction Source: After Enemy Heavy Attack at Close Range "
            "(Player Behind, after Player Block)"
        ),
        TEXT("Collected Training Samples: 12"),
        TEXT("Most Common Player Action: Dodge Left"),
        TEXT("Strongest Learned Conditional Pattern:"),
        TEXT("Current Enemy Selected Action: Heavy Attack"),
        TEXT("Decision Trigger: URGENT"),
        TEXT("Near-Best Selection: best 1.180, selected 1.120, 2 candidate(s)"),
        TEXT("Heavy Attack modifiers: repetition -0.220, recent outcome -0.120"),
        TEXT("Short-Term Memory: 5 commits, 4 resolved outcomes"),
        TEXT("ROUND 1 OBSERVED PATTERN:"),
        TEXT("Dodge Left: 75%"),
        TEXT("Block: 25%")
    };
    for (const TCHAR* RequiredLabel : RequiredLabels)
    {
        TestTrue(
            FString::Printf(TEXT("HUD includes '%s'"), RequiredLabel),
            AllLines.Contains(RequiredLabel)
        );
    }

    FAdaptiveDebugTelemetrySnapshot InvalidPositionProvenance = Snapshot;
    InvalidPositionProvenance.Prediction.bUsedDistanceContext = false;
    const FString InvalidPositionLines = FString::Join(
        FAdaptiveDebugTelemetryFormatter::FormatHudLines(
            InvalidPositionProvenance
        ),
        TEXT("\n")
    );
    TestTrue(
        TEXT("Position provenance without distance context is rejected"),
        InvalidPositionLines.Contains(
            TEXT("Predicted Next Player Action: None")
        )
            && InvalidPositionLines.Contains(
                TEXT("Prediction Source: None")
            )
    );

    FAdaptiveDebugTelemetrySnapshot InvalidPlayerActionProvenance = Snapshot;
    InvalidPlayerActionProvenance.Prediction.bUsedPositionContext = false;
    const FString InvalidPlayerActionLines = FString::Join(
        FAdaptiveDebugTelemetryFormatter::FormatHudLines(
            InvalidPlayerActionProvenance
        ),
        TEXT("\n")
    );
    TestTrue(
        TEXT("Player-action provenance without position context is rejected"),
        InvalidPlayerActionLines.Contains(
            TEXT("Predicted Next Player Action: None")
        )
            && InvalidPlayerActionLines.Contains(
                TEXT("Prediction Source: None")
            )
    );

    const AAdaptiveGameMode* GameMode = GetDefault<AAdaptiveGameMode>();
    TestTrue(
        TEXT("The adaptive game mode registers the C++ debug HUD"),
        GameMode
            && GameMode->HUDClass == AAdaptiveDebugHUD::StaticClass()
    );
    const AAdaptiveDebugHUD* Hud = GetDefault<AAdaptiveDebugHUD>();
    TestTrue(
        TEXT("The HUD relies on DrawHUD and does not actor Tick"),
        Hud && !Hud->PrimaryActorTick.bCanEverTick
    );
    const UEnemyDecisionComponent* Decision =
        GetDefault<UEnemyDecisionComponent>();
    TestTrue(
        TEXT("Enemy selected-action telemetry has a safe initial value"),
        Decision
            && Decision->GetLastSelectedAction()
                == EEnemyCombatAction::None
    );
    return true;
}

#endif
