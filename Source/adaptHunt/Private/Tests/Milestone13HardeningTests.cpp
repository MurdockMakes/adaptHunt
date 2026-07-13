#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/EnemyActionScorer.h"
#include "Characters/AdaptivePlayerCharacter.h"
#include "Components/CombatSnapshotComponent.h"
#include "Components/EnemyDecisionComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PlayerBehaviorTrackerComponent.h"
#include "Components/StaminaComponent.h"
#include "Data/LearningTelemetry.h"
#include "Game/RoundManager.h"

#include <limits>

namespace
{
FTrainingSample MakeValidSample(
    const int32 RoundNumber,
    const EPlayerCombatAction Action
)
{
    FTrainingSample Sample;
    Sample.Snapshot.RoundNumber = RoundNumber;
    Sample.Snapshot.PreviousPlayerAction =
        EPlayerCombatAction::HeavyAttack;
    Sample.Snapshot.PreviousEnemyAction =
        EEnemyCombatAction::HeavyAttack;
    Sample.NextPlayerAction = Action;
    return Sample;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveGameplayResourceEdgeCasesTest,
    "adaptHunt.Milestone13.GameplayResourceEdgeCases",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveGameplayResourceEdgeCasesTest::RunTest(
    const FString& Parameters
)
{
    const float NotANumber = std::numeric_limits<float>::quiet_NaN();
    const float Infinity = std::numeric_limits<float>::infinity();

    UHealthComponent* Health = NewObject<UHealthComponent>();
    UStaminaComponent* Stamina = NewObject<UStaminaComponent>();
    TestNotNull(TEXT("An isolated health component can be created"), Health);
    TestNotNull(TEXT("An isolated stamina component can be created"), Stamina);
    if (!Health || !Stamina)
    {
        return false;
    }

    int32 HealthChanges = 0;
    Health->OnHealthChanged.AddLambda(
        [&HealthChanges](UHealthComponent*, float, float)
        {
            ++HealthChanges;
        }
    );
    TestTrue(
        TEXT("NaN damage is rejected"),
        FMath::IsNearlyZero(Health->ApplyDamage(NotANumber))
    );
    TestTrue(
        TEXT("Infinite damage is rejected"),
        FMath::IsNearlyZero(Health->ApplyDamage(Infinity))
    );
    TestTrue(
        TEXT("Negative healing is rejected"),
        FMath::IsNearlyZero(Health->Heal(-10.0f))
    );
    TestEqual(
        TEXT("Rejected health inputs do not broadcast changes"),
        HealthChanges,
        0
    );
    TestTrue(
        TEXT("Rejected health inputs leave health finite and full"),
        FMath::IsFinite(Health->GetCurrentHealth())
            && FMath::IsNearlyEqual(Health->GetCurrentHealth(), 100.0f)
    );

    Health->SetDamageReduction(NotANumber);
    TestTrue(
        TEXT("Invalid mitigation falls back to no reduction"),
        FMath::IsNearlyZero(Health->GetDamageReduction())
    );
    TestTrue(
        TEXT("A valid controlled hit still applies after invalid input"),
        FMath::IsNearlyEqual(Health->ApplyDamage(25.0f), 25.0f)
    );

    TestFalse(
        TEXT("NaN stamina cost is rejected"),
        Stamina->TryConsumeStamina(NotANumber)
    );
    TestFalse(
        TEXT("Infinite stamina cost is rejected"),
        Stamina->TryConsumeStamina(Infinity)
    );
    TestFalse(
        TEXT("Negative stamina cost is rejected"),
        Stamina->TryConsumeStamina(-1.0f)
    );
    TestTrue(
        TEXT("Invalid stamina costs leave the resource unchanged"),
        FMath::IsNearlyEqual(Stamina->GetCurrentStamina(), 100.0f)
    );
    TestTrue(
        TEXT("Invalid stamina restoration is rejected"),
        FMath::IsNearlyZero(Stamina->RestoreStamina(Infinity))
    );
    TestTrue(
        TEXT("A valid stamina cost remains exact"),
        Stamina->TryConsumeStamina(20.0f)
            && FMath::IsNearlyEqual(Stamina->GetCurrentStamina(), 80.0f)
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveLearningInputEdgeCasesTest,
    "adaptHunt.Milestone13.LearningInputAndTelemetryEdgeCases",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveLearningInputEdgeCasesTest::RunTest(
    const FString& Parameters
)
{
    const float NotANumber = std::numeric_limits<float>::quiet_NaN();
    FCombatDataset Dataset;

    const FTrainingSample Valid = MakeValidSample(
        1,
        EPlayerCombatAction::DodgeLeft
    );
    TestTrue(TEXT("A complete behavior sample is accepted"), Dataset.AddSample(Valid));

    FTrainingSample InvalidAction = Valid;
    InvalidAction.NextPlayerAction = static_cast<EPlayerCombatAction>(255);
    TestFalse(
        TEXT("An unknown player-action label is rejected"),
        Dataset.AddSample(InvalidAction)
    );

    FTrainingSample InvalidRound = Valid;
    InvalidRound.Snapshot.RoundNumber = 0;
    TestFalse(
        TEXT("A non-positive round number is rejected"),
        Dataset.AddSample(InvalidRound)
    );

    FTrainingSample InvalidFeature = Valid;
    InvalidFeature.Snapshot.PlayerHealthNormalized = NotANumber;
    TestFalse(
        TEXT("A non-finite learning feature is rejected"),
        Dataset.AddSample(InvalidFeature)
    );
    TestEqual(
        TEXT("Rejected learning rows do not contaminate the dataset"),
        Dataset.Num(),
        1
    );

    const FAdaptiveLearningSummary Summary =
        FAdaptiveLearningTelemetryAnalyzer::Analyze(Dataset);
    TestEqual(TEXT("Telemetry counts only the valid row"), Summary.SampleCount, 1);
    TestTrue(
        TEXT("The valid HeavyAttack to DodgeLeft relationship remains observable"),
        Summary.StrongestConditionalPattern.IsValid()
            && Summary.StrongestConditionalPattern.PreviousEnemyAction
                == EEnemyCombatAction::HeavyAttack
            && Summary.StrongestConditionalPattern.DominantPlayerAction
                == EPlayerCombatAction::DodgeLeft
    );

    FAdaptiveDebugTelemetrySnapshot Telemetry;
    Telemetry.CurrentRound = 99;
    Telemetry.TotalRounds = 3;
    Telemetry.PlayerHealth = NotANumber;
    Telemetry.PlayerMaxHealth = 100.0f;
    Telemetry.Prediction.bHasPrediction = true;
    Telemetry.Prediction.PredictedAction =
        static_cast<EPlayerCombatAction>(255);
    Telemetry.Prediction.Confidence = NotANumber;
    Telemetry.Prediction.bUsedContext = true;
    Telemetry.Prediction.ConditioningEnemyAction =
        static_cast<EEnemyCombatAction>(255);
    const FString Lines = FString::Join(
        FAdaptiveDebugTelemetryFormatter::FormatHudLines(Telemetry),
        TEXT("\n")
    );
    TestTrue(
        TEXT("HUD round values clamp to the three-round slice"),
        Lines.Contains(TEXT("Round: 3 / 3"))
    );
    TestTrue(
        TEXT("HUD replaces non-finite health with a safe value"),
        Lines.Contains(TEXT("Player Health: 0.0 / 100.0"))
    );
    TestTrue(
        TEXT("HUD hides a malformed prediction"),
        Lines.Contains(TEXT("Predicted Next Player Action: None"))
            && Lines.Contains(TEXT("Prediction Confidence: 0%"))
            && Lines.Contains(TEXT("Prediction Source: None"))
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveSnapshotAndUtilityEdgeCasesTest,
    "adaptHunt.Milestone13.SnapshotAndUtilityEdgeCases",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveSnapshotAndUtilityEdgeCasesTest::RunTest(
    const FString& Parameters
)
{
    const float NotANumber = std::numeric_limits<float>::quiet_NaN();
    const float Infinity = std::numeric_limits<float>::infinity();

    TestTrue(
        TEXT("A non-finite distance uses the safe far classification"),
        UCombatSnapshotComponent::ClassifyDistance(
            NotANumber,
            300.0f,
            700.0f
        ) == ECombatDistanceCategory::Far
    );
    TestTrue(
        TEXT("Reversed thresholds collapse deterministically"),
        UCombatSnapshotComponent::ClassifyDistance(101.0f, 100.0f, 50.0f)
            == ECombatDistanceCategory::Far
    );
    TestTrue(
        TEXT("Negative distances clamp to close"),
        UCombatSnapshotComponent::ClassifyDistance(-1.0f, 300.0f, 700.0f)
            == ECombatDistanceCategory::Close
    );
    TestTrue(
        TEXT("Invalid position vectors fall back to neutral front"),
        UCombatSnapshotComponent::ClassifyRelativePosition(
            FVector(NotANumber, 0.0f, 0.0f),
            FVector::ZeroVector,
            FVector::ForwardVector
        ) == ERelativePlayerPosition::Front
    );

    FEnemyActionScorer Scorer;
    FEnemyActionScoringContext Context;
    Context.Snapshot.PlayerHealthNormalized = NotANumber;
    Context.Snapshot.EnemyHealthNormalized = Infinity;
    Context.Snapshot.EnemyStaminaNormalized = NotANumber;
    Context.Snapshot.DistanceCategory =
        static_cast<ECombatDistanceCategory>(255);
    Context.Prediction.bHasPrediction = true;
    Context.Prediction.PredictedAction = EPlayerCombatAction::Heal;
    Context.Prediction.Confidence = Infinity;
    Context.Prediction.SupportingSampleCount = 5;
    Context.PredictionInfluence = NotANumber;

    const TArray<FEnemyActionScore> Scores = Scorer.ScoreActions(Context);
    bool bAllScoresSafe = Scores.Num() == 7;
    for (const FEnemyActionScore& Score : Scores)
    {
        bAllScoresSafe = bAllScoresSafe
            && FMath::IsFinite(Score.Score)
            && Score.Score >= 0.0f
            && Score.Score <= 1.0f;
    }
    TestTrue(TEXT("Malformed context still produces finite bounded utility"), bAllScoresSafe);

    const FEnemyActionScore MovementScore = Scorer.ScoreAction(
        EEnemyCombatAction::MoveTowardPlayer,
        Context
    );
    TestFalse(
        TEXT("Movement cannot enter the combat utility candidate set"),
        MovementScore.bAvailable
    );

    TArray<FEnemyActionScore> MalformedScores;
    FEnemyActionScore InvalidCandidate;
    InvalidCandidate.Action = EEnemyCombatAction::MoveTowardPlayer;
    InvalidCandidate.Score = 1.0f;
    InvalidCandidate.bAvailable = true;
    MalformedScores.Add(InvalidCandidate);
    InvalidCandidate.Action = EEnemyCombatAction::HeavyAttack;
    InvalidCandidate.Score = NotANumber;
    MalformedScores.Add(InvalidCandidate);
    TestTrue(
        TEXT("Malformed score arrays fail closed"),
        Scorer.SelectBestAction(MalformedScores)
            == EEnemyCombatAction::None
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptivePredictionTuningTest,
    "adaptHunt.Milestone13.PredictionTuningAndDeterminism",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptivePredictionTuningTest::RunTest(const FString& Parameters)
{
    const UEnemyDecisionComponent* Defaults =
        GetDefault<UEnemyDecisionComponent>();
    TestNotNull(TEXT("Enemy decision defaults are available"), Defaults);
    if (!Defaults)
    {
        return false;
    }

    TestEqual(TEXT("Prediction influence is deliberately sub-perfect"), Defaults->GetPredictionInfluence(), 0.40f);
    TestEqual(TEXT("Weak predictions are filtered at forty percent"), Defaults->GetMinimumPredictionConfidence(), 0.40f);
    TestEqual(TEXT("Prediction application retains controlled randomness"), Defaults->GetPredictionApplicationChance(), 0.80f);
    TestEqual(TEXT("A learned habit needs repeated support"), Defaults->GetMinimumPredictionSupportingSamples(), 2);

    FPredictionResult Prediction;
    Prediction.bHasPrediction = true;
    Prediction.PredictedAction = EPlayerCombatAction::Block;
    Prediction.Confidence = 0.75f;
    Prediction.SupportingSampleCount = 3;

    TestTrue(
        TEXT("A roll below confidence-scaled chance applies prediction"),
        UEnemyDecisionComponent::ShouldApplyPredictionDeterministically(
            Prediction,
            true,
            0.40f,
            2,
            0.80f,
            0.59f
        )
    );
    TestFalse(
        TEXT("A roll at the exclusive probability boundary does not apply"),
        UEnemyDecisionComponent::ShouldApplyPredictionDeterministically(
            Prediction,
            true,
            0.40f,
            2,
            0.80f,
            0.60f
        )
    );

    Prediction.SupportingSampleCount = 1;
    TestFalse(
        TEXT("A one-off observation cannot drive adaptation"),
        UEnemyDecisionComponent::ShouldApplyPredictionDeterministically(
            Prediction,
            true,
            0.40f,
            2,
            0.80f,
            0.0f
        )
    );
    Prediction.SupportingSampleCount = 3;
    Prediction.Confidence = 0.39f;
    TestFalse(
        TEXT("A below-threshold prediction cannot drive adaptation"),
        UEnemyDecisionComponent::ShouldApplyPredictionDeterministically(
            Prediction,
            true,
            0.40f,
            2,
            0.80f,
            0.0f
        )
    );

    FEnemyActionScorer Scorer;
    FEnemyActionScoringContext Baseline;
    Baseline.Snapshot.DistanceCategory = ECombatDistanceCategory::Close;
    TestTrue(
        TEXT("The close-range baseline remains LightAttack"),
        Scorer.SelectBestAction(Baseline)
            == EEnemyCombatAction::LightAttack
    );

    FEnemyActionScoringContext WeakAdaptation = Baseline;
    WeakAdaptation.Prediction.bHasPrediction = true;
    WeakAdaptation.Prediction.PredictedAction = EPlayerCombatAction::Block;
    WeakAdaptation.Prediction.Confidence = 0.39f;
    WeakAdaptation.Prediction.SupportingSampleCount = 4;
    WeakAdaptation.PredictionInfluence = 0.40f;
    TestTrue(
        TEXT("Weak evidence does not overwhelm baseline combat"),
        Scorer.SelectBestAction(WeakAdaptation)
            == EEnemyCombatAction::LightAttack
    );

    FEnemyActionScoringContext StrongAdaptation = WeakAdaptation;
    StrongAdaptation.Prediction.Confidence = 0.80f;
    TestTrue(
        TEXT("Strong repeated Block evidence selects the HeavyAttack counter"),
        Scorer.SelectBestAction(StrongAdaptation)
            == EEnemyCombatAction::HeavyAttack
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveRoundAndTargetEdgeCasesTest,
    "adaptHunt.Milestone13.RoundAndTargetEdgeCases",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveRoundAndTargetEdgeCasesTest::RunTest(
    const FString& Parameters
)
{
    FAdaptiveRoundProgression Progression;
    TestTrue(TEXT("A match begins for transition guard tests"), Progression.BeginMatch());
    TestTrue(
        TEXT("The first round begins after its countdown"),
        Progression.StartCurrentRound()
    );
    TestFalse(
        TEXT("A round cannot advance before it ends"),
        Progression.AdvanceToNextRound()
    );
    TestFalse(
        TEXT("None cannot complete a round"),
        Progression.CompleteCurrentRound(EAdaptiveRoundWinner::None)
    );
    TestFalse(
        TEXT("An unknown winner cannot complete a round"),
        Progression.CompleteCurrentRound(
            static_cast<EAdaptiveRoundWinner>(255)
        )
    );
    TestTrue(
        TEXT("Rejected winners leave Round 1 in progress"),
        Progression.GetCurrentRound() == 1
            && Progression.GetPhase() == EAdaptiveRoundPhase::InProgress
    );
    TestTrue(
        TEXT("A valid winner completes Round 1"),
        Progression.CompleteCurrentRound(EAdaptiveRoundWinner::Player)
    );
    TestFalse(
        TEXT("Duplicate death events cannot complete the same round twice"),
        Progression.CompleteCurrentRound(EAdaptiveRoundWinner::Enemy)
    );
    TestTrue(
        TEXT("The first valid result remains authoritative"),
        Progression.GetLastWinner() == EAdaptiveRoundWinner::Player
            && Progression.GetLastCompletedRound() == 1
    );

    UEnemyDecisionComponent* Decision =
        NewObject<UEnemyDecisionComponent>();
    TestNotNull(TEXT("An isolated decision component can be created"), Decision);
    if (!Decision)
    {
        return false;
    }

    AAdaptivePlayerCharacter* FirstTarget =
        GetMutableDefault<AAdaptivePlayerCharacter>();
    Decision->SetTargetActor(FirstTarget);
    FCombatDataset Dataset;
    Dataset.AddSample(MakeValidSample(1, EPlayerCombatAction::Heal));
    Dataset.AddSample(MakeValidSample(1, EPlayerCombatAction::Heal));
    Dataset.AddSample(MakeValidSample(1, EPlayerCombatAction::Block));
    Decision->TrainPredictor(Dataset);
    TestEqual(TEXT("The target model is trained"), Decision->GetTrainedSampleCount(), 3);

    Decision->SetTargetActor(FirstTarget);
    TestEqual(
        TEXT("Reassigning the same target preserves accumulated learning"),
        Decision->GetTrainedSampleCount(),
        3
    );
    Decision->SetTargetActor(nullptr);
    TestEqual(
        TEXT("Changing targets clears stale player-specific learning"),
        Decision->GetTrainedSampleCount(),
        0
    );
    return true;
}

#endif
