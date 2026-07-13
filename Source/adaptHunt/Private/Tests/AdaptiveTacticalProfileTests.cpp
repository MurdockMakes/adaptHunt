#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/AdaptiveTacticalProfile.h"
#include "AI/EnemyActionScorer.h"
#include "Components/EnemyDecisionComponent.h"
#include "Data/LearningTelemetry.h"

namespace
{
void AddProfileSamples(
    FCombatDataset& Dataset,
    const int32 RoundNumber,
    const EPlayerCombatAction Action,
    const int32 Count,
    const EEnemyCombatAction PreviousEnemyAction =
        EEnemyCombatAction::HeavyAttack
)
{
    for (int32 Index = 0; Index < Count; ++Index)
    {
        FTrainingSample Sample;
        Sample.Snapshot.RoundNumber = RoundNumber;
        Sample.Snapshot.PreviousEnemyAction = PreviousEnemyAction;
        Sample.Snapshot.DistanceCategory = ECombatDistanceCategory::Medium;
        Sample.Snapshot.RelativePlayerPosition =
            ERelativePlayerPosition::Front;
        Sample.Snapshot.PreviousPlayerAction =
            EPlayerCombatAction::HeavyAttack;
        Sample.NextPlayerAction = Action;
        Dataset.AddSample(Sample);
    }
}

FPredictionResult MakeProfilePrediction(
    const EPlayerCombatAction Action,
    const float Confidence,
    const int32 Support,
    const bool bUseContext = true
)
{
    FPredictionResult Prediction;
    Prediction.bHasPrediction = true;
    Prediction.PredictedAction = Action;
    Prediction.Confidence = Confidence;
    Prediction.SupportingSampleCount = Support;
    Prediction.bUsedContext = bUseContext;
    Prediction.ConditioningEnemyAction = bUseContext
        ? EEnemyCombatAction::HeavyAttack
        : EEnemyCombatAction::None;
    Prediction.bUsedDistanceContext = bUseContext;
    Prediction.ConditioningDistanceCategory =
        ECombatDistanceCategory::Medium;
    Prediction.bUsedPositionContext = bUseContext;
    Prediction.ConditioningRelativePlayerPosition =
        ERelativePlayerPosition::Front;
    Prediction.bUsedPreviousPlayerActionContext = bUseContext;
    Prediction.ConditioningPreviousPlayerAction =
        EPlayerCombatAction::HeavyAttack;
    return Prediction;
}

FAdaptiveTacticalProfile DeriveClearHabit(
    const EPlayerCombatAction Action
)
{
    FCombatDataset Dataset;
    AddProfileSamples(Dataset, 1, Action, 6);
    AddProfileSamples(Dataset, 1, EPlayerCombatAction::LightAttack, 2);
    if (Action == EPlayerCombatAction::LightAttack)
    {
        Dataset.Reset();
        AddProfileSamples(Dataset, 1, Action, 6);
        AddProfileSamples(Dataset, 1, EPlayerCombatAction::Block, 2);
    }
    return FAdaptiveTacticalProfilePolicy::Derive(
        Dataset,
        1,
        MakeProfilePrediction(Action, 0.75f, 6),
        FAdaptiveTacticalProfileTuning()
    );
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveTacticalProfileDerivationTest,
    "adaptHunt.Milestone24.DeterministicProfileDerivation",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveTacticalProfileDerivationTest::RunTest(
    const FString& Parameters
)
{
    FCombatDataset Dataset;
    AddProfileSamples(Dataset, 1, EPlayerCombatAction::Block, 6);
    AddProfileSamples(Dataset, 1, EPlayerCombatAction::LightAttack, 2);
    AddProfileSamples(Dataset, 2, EPlayerCombatAction::Heal, 5);
    const FPredictionResult Prediction = MakeProfilePrediction(
        EPlayerCombatAction::Block,
        0.75f,
        6
    );
    const FAdaptiveTacticalProfile First =
        FAdaptiveTacticalProfilePolicy::Derive(
            Dataset,
            1,
            Prediction,
            FAdaptiveTacticalProfileTuning()
        );
    const FAdaptiveTacticalProfile Second =
        FAdaptiveTacticalProfilePolicy::Derive(
            Dataset,
            1,
            Prediction,
            FAdaptiveTacticalProfileTuning()
        );

    TestTrue(TEXT("Clear blocking evidence activates a profile"), First.IsActive());
    TestTrue(
        TEXT("Only the completed round contributes evidence"),
        First.RoundSampleCount == 8 && First.ContextSampleCount == 8
            && First.SupportingSampleCount == 6
    );
    TestTrue(
        TEXT("The profile retains predictor context provenance"),
        First.EvidencePrediction.bUsedPreviousPlayerActionContext
            && First.EvidencePrediction.ConditioningEnemyAction
                == EEnemyCombatAction::HeavyAttack
    );
    TestTrue(
        TEXT("Blocking produces explainable guard pressure"),
        First.MostLikelyPlayerAction == EPlayerCombatAction::Block
            && First.PreferredCounterAction
                == EEnemyCombatAction::HeavyAttack
            && First.PreferredSpacingAdjustment > 0.0f
            && First.AggressionAdjustment > 0.0f
    );
    TestTrue(
        TEXT("Repeated derivation is identical"),
        Second.IsActive()
            && Second.PreferredCounterAction == First.PreferredCounterAction
            && FMath::IsNearlyEqual(Second.Confidence, First.Confidence)
            && FMath::IsNearlyEqual(
                Second.PreferredSpacingAdjustment,
                First.PreferredSpacingAdjustment
            )
    );

    const FAdaptiveTacticalProfile Heal = DeriveClearHabit(
        EPlayerCombatAction::Heal
    );
    const FAdaptiveTacticalProfile Heavy = DeriveClearHabit(
        EPlayerCombatAction::HeavyAttack
    );
    const FAdaptiveTacticalProfile BackDodge = DeriveClearHabit(
        EPlayerCombatAction::DodgeBackward
    );
    const FAdaptiveTacticalProfile LeftDodge = DeriveClearHabit(
        EPlayerCombatAction::DodgeLeft
    );
    TestTrue(
        TEXT("Healing creates pursuit and authoritative interrupt priority"),
        Heal.IsActive()
            && Heal.PreferredCounterAction
                == EEnemyCombatAction::InterruptHeal
            && Heal.PreferredSpacingAdjustment < 0.0f
            && Heal.HealInterruptionPriority > 0.0f
    );
    TestTrue(
        TEXT("Repeated heavy attacks produce a defensive dodge profile"),
        Heavy.IsActive()
            && Heavy.PreferredCounterAction == EEnemyCombatAction::Dodge
            && Heavy.DefensiveAdjustment > 0.0f
    );
    TestTrue(
        TEXT("Backward dodges produce pursuit and ranged pressure"),
        BackDodge.IsActive()
            && BackDodge.PreferredCounterAction
                == EEnemyCombatAction::DashAttack
            && BackDodge.PreferredSpacingAdjustment < 0.0f
    );
    TestTrue(
        TEXT("Lateral dodges deterministically bias orbit direction"),
        LeftDodge.IsActive()
            && LeftDodge.OrbitPreference
                == EAdaptiveOrbitPreference::Right
            && FAdaptiveTacticalProfilePolicy::ResolveOrbitRight(
                LeftDodge,
                false
            )
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveTacticalProfileEvidenceTest,
    "adaptHunt.Milestone24.SparseConflictAndBehaviorChange",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveTacticalProfileEvidenceTest::RunTest(
    const FString& Parameters
)
{
    FCombatDataset SparseDataset;
    AddProfileSamples(
        SparseDataset,
        1,
        EPlayerCombatAction::Block,
        3
    );
    const FAdaptiveTacticalProfile Sparse =
        FAdaptiveTacticalProfilePolicy::Derive(
            SparseDataset,
            1,
            MakeProfilePrediction(EPlayerCombatAction::Block, 1.0f, 3),
            FAdaptiveTacticalProfileTuning()
        );
    TestTrue(
        TEXT("A perfect but sparse habit leaves baseline unchanged"),
        !Sparse.IsActive()
            && Sparse.EvidenceStatus
                == EAdaptiveProfileEvidenceStatus::
                    InsufficientRoundSamples
    );

    FCombatDataset ConflictingDataset;
    AddProfileSamples(
        ConflictingDataset,
        1,
        EPlayerCombatAction::Block,
        3
    );
    AddProfileSamples(
        ConflictingDataset,
        1,
        EPlayerCombatAction::Heal,
        4
    );
    const FAdaptiveTacticalProfile Conflict =
        FAdaptiveTacticalProfilePolicy::Derive(
            ConflictingDataset,
            1,
            MakeProfilePrediction(EPlayerCombatAction::Block, 0.80f, 4),
            FAdaptiveTacticalProfileTuning()
        );
    TestTrue(
        TEXT("Round evidence that contradicts the predictor disables play"),
        !Conflict.IsActive()
            && Conflict.EvidenceStatus
                == EAdaptiveProfileEvidenceStatus::PredictorDisagreement
    );

    FCombatDataset ChangedDataset;
    AddProfileSamples(
        ChangedDataset,
        1,
        EPlayerCombatAction::Block,
        6
    );
    AddProfileSamples(
        ChangedDataset,
        2,
        EPlayerCombatAction::Heal,
        6
    );
    const FAdaptiveTacticalProfile RoundOne =
        FAdaptiveTacticalProfilePolicy::Derive(
            ChangedDataset,
            1,
            MakeProfilePrediction(EPlayerCombatAction::Block, 1.0f, 6),
            FAdaptiveTacticalProfileTuning()
        );
    const FAdaptiveTacticalProfile RoundTwo =
        FAdaptiveTacticalProfilePolicy::Derive(
            ChangedDataset,
            2,
            MakeProfilePrediction(EPlayerCombatAction::Heal, 1.0f, 6),
            FAdaptiveTacticalProfileTuning()
        );
    TestTrue(
        TEXT("Changing tactics replaces the old learned counter in Round 3"),
        RoundOne.IsActive() && RoundTwo.IsActive()
            && RoundOne.PreferredCounterAction
                == EEnemyCombatAction::HeavyAttack
            && RoundTwo.PreferredCounterAction
                == EEnemyCombatAction::InterruptHeal
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveTacticalProfileApplicationTest,
    "adaptHunt.Milestone24.VisibleBoundedTacticalChange",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveTacticalProfileApplicationTest::RunTest(
    const FString& Parameters
)
{
    const FAdaptiveTacticalProfile Profile = DeriveClearHabit(
        EPlayerCombatAction::Block
    );
    const FAdaptiveTacticalProfileTuning Tuning;
    FEnemyActionScorer Scorer;
    FEnemyActionScoringContext Baseline;
    Baseline.Snapshot.DistanceCategory = ECombatDistanceCategory::Close;
    TestTrue(
        TEXT("The competent baseline still prefers a close light attack"),
        Scorer.SelectBestAction(Baseline)
            == EEnemyCombatAction::LightAttack
    );

    FEnemyActionScoringContext Adapted = Baseline;
    Adapted.MaximumAdaptiveUtilityAdjustment =
        Tuning.MaximumUtilityAdjustment;
    const EEnemyCombatAction Actions[] = {
        EEnemyCombatAction::LightAttack,
        EEnemyCombatAction::HeavyAttack,
        EEnemyCombatAction::ProjectileAttack,
        EEnemyCombatAction::DashAttack,
        EEnemyCombatAction::Block,
        EEnemyCombatAction::Dodge,
        EEnemyCombatAction::InterruptHeal
    };
    for (const EEnemyCombatAction Action : Actions)
    {
        const float Modifier =
            FAdaptiveTacticalProfilePolicy::GetUtilityModifier(
                Profile,
                Action,
                true,
                Tuning
            );
        TestTrue(
            TEXT("Every profile utility contribution is independently bounded"),
            FMath::Abs(Modifier)
                <= Tuning.MaximumUtilityAdjustment + KINDA_SMALL_NUMBER
        );
        Adapted.AdaptiveUtilityModifiers.Add(Action, Modifier);
    }
    TestTrue(
        TEXT("A gated block counter visibly changes selection to heavy"),
        Scorer.SelectBestAction(Adapted)
            == EEnemyCombatAction::HeavyAttack
    );
    TestTrue(
        TEXT("Blocking changes preferred spacing without changing melee range"),
        FMath::IsNearlyEqual(
            FAdaptiveTacticalProfilePolicy::AdjustSpacingDistance(
                425.0f,
                Profile,
                1.0f,
                Tuning
            ),
            505.0f
        )
    );

    FEnemyActionScoringContext HardBounded = Baseline;
    HardBounded.MaximumAdaptiveUtilityAdjustment = 0.20f;
    HardBounded.AdaptiveUtilityModifiers.Add(
        EEnemyCombatAction::HeavyAttack,
        100.0f
    );
    const float BaselineHeavy = Scorer.ScoreAction(
        EEnemyCombatAction::HeavyAttack,
        Baseline
    ).Score;
    TestTrue(
        TEXT("The scorer independently clamps malformed profile input"),
        FMath::IsNearlyEqual(
            Scorer.ScoreAction(
                EEnemyCombatAction::HeavyAttack,
                HardBounded
            ).Score,
            BaselineHeavy + 0.20f
        )
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveTacticalProfileFairnessTest,
    "adaptHunt.Milestone24.SeededGateCounterBudgetAndDebug",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveTacticalProfileFairnessTest::RunTest(
    const FString& Parameters
)
{
    const FAdaptiveTacticalProfile Profile = DeriveClearHabit(
        EPlayerCombatAction::Block
    );
    const FPredictionResult Prediction = MakeProfilePrediction(
        EPlayerCombatAction::Block,
        0.80f,
        6
    );
    TestTrue(
        TEXT("The existing confidence and seeded application gate remains"),
        UEnemyDecisionComponent::ShouldApplyPredictionDeterministically(
            Prediction,
            true,
            0.65f,
            3,
            0.75f,
            0.59f
        )
    );
    TestFalse(
        TEXT("A seeded roll outside confidence-scaled chance does not counter"),
        UEnemyDecisionComponent::ShouldApplyPredictionDeterministically(
            Prediction,
            true,
            0.65f,
            3,
            0.75f,
            0.60f
        )
    );

    FAdaptiveCounterBudgetState Budget;
    Budget.Reset(2);
    TestTrue(
        TEXT("The first matched committed-history prediction may counter"),
        FAdaptiveTacticalProfilePolicy::
            IsPredictionMatchedCounterOpportunity(
                Profile,
                Prediction,
                true,
                Budget,
                10.0
            )
    );
    TestTrue(TEXT("The first counter consumes budget"), Budget.Consume(10.0, 2.4f));
    TestFalse(
        TEXT("The short cooldown prevents an omniscient immediate reaction"),
        FAdaptiveTacticalProfilePolicy::
            IsPredictionMatchedCounterOpportunity(
                Profile,
                Prediction,
                true,
                Budget,
                11.0
            )
    );
    TestTrue(
        TEXT("A later matched prediction may use the final counter"),
        FAdaptiveTacticalProfilePolicy::
            IsPredictionMatchedCounterOpportunity(
                Profile,
                Prediction,
                true,
                Budget,
                12.4
            )
    );
    TestTrue(TEXT("The final counter consumes the last use"), Budget.Consume(12.4, 2.4f));
    TestFalse(
        TEXT("The per-round budget prevents further learned counters"),
        FAdaptiveTacticalProfilePolicy::
            IsPredictionMatchedCounterOpportunity(
                Profile,
                Prediction,
                true,
                Budget,
                20.0
            )
    );
    TestFalse(
        TEXT("A prediction that was not seeded for application cannot counter"),
        FAdaptiveTacticalProfilePolicy::
            IsPredictionMatchedCounterOpportunity(
                Profile,
                Prediction,
                false,
                Budget,
                20.0
            )
    );

    FAdaptiveDebugTelemetrySnapshot Snapshot;
    Snapshot.CurrentRound = 2;
    Snapshot.bPredictionsEnabled = true;
    Snapshot.AdaptiveTacticalProfile = Profile;
    Snapshot.AdaptiveCounterUsesRemaining = 2;
    Snapshot.AdaptiveCounterCooldownRemaining = 1.2f;
    const FString DebugText = FString::Join(
        FAdaptiveDebugTelemetryFormatter::FormatHudLines(Snapshot),
        TEXT("\n")
    );
    TestTrue(
        TEXT("Debug output explains evidence, context, counter, and changes"),
        DebugText.Contains(TEXT("Adaptive Tactical Profile: ACTIVE"))
            && DebugText.Contains(TEXT("Profile Evidence: Block at 75%"))
            && DebugText.Contains(TEXT("Profile Context: After Enemy Heavy Attack"))
            && DebugText.Contains(TEXT("Profile Counter: Heavy Attack"))
            && DebugText.Contains(TEXT("Profile Changes: spacing +80"))
    );

    const UEnemyDecisionComponent* Defaults =
        GetDefault<UEnemyDecisionComponent>();
    TestTrue(
        TEXT("Component defaults begin baseline with bounded counter tuning"),
        Defaults && !Defaults->GetAdaptiveTacticalProfile().IsActive()
            && Defaults->GetAdaptiveTacticalProfileTuning()
                .CounterBudgetPerRound > 0
            && Defaults->GetAdaptiveTacticalProfileTuning()
                .CounterCooldown > 0.0f
            && Defaults->GetAdaptiveTacticalProfileTuning()
                .MaximumUtilityAdjustment <= 0.25f
    );
    return true;
}

#endif
