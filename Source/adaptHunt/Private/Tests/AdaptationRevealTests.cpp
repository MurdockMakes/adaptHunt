#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/AnyOf.h"
#include "EnhancedActionKeyMapping.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"

#include "Characters/AdaptivePlayerCharacter.h"
#include "Game/AdaptationReveal.h"
#include "Game/RoundManager.h"
#include "UI/AdaptiveDebugHUD.h"

namespace
{
FAdaptiveConditionalPattern MakeObservedBlockPattern()
{
    FAdaptiveConditionalPattern Pattern;
    Pattern.PreviousEnemyAction = EEnemyCombatAction::LightAttack;
    Pattern.DominantPlayerAction = EPlayerCombatAction::Block;
    Pattern.TotalSampleCount = 5;
    Pattern.DominantActionCount = 3;
    Pattern.Confidence = 0.6f;

    FAdaptiveActionPercentage Block;
    Block.Action = EPlayerCombatAction::Block;
    Block.Count = 3;
    Block.Percentage = 60;
    Pattern.ActionPercentages.Add(Block);

    FAdaptiveActionPercentage Heal;
    Heal.Action = EPlayerCombatAction::Heal;
    Heal.Count = 2;
    Heal.Percentage = 40;
    Pattern.ActionPercentages.Add(Heal);
    return Pattern;
}

FAdaptiveTacticalProfile MakeCloseBlockProfile()
{
    FAdaptiveTacticalProfile Profile;
    Profile.SourceRound = 1;
    Profile.EvidenceStatus = EAdaptiveProfileEvidenceStatus::Active;
    Profile.MostLikelyPlayerAction = EPlayerCombatAction::Block;
    Profile.Confidence = 0.75f;
    Profile.SupportingSampleCount = 6;
    Profile.ContextSampleCount = 8;
    Profile.RoundSampleCount = 12;
    Profile.PreferredCounterAction = EEnemyCombatAction::HeavyAttack;
    Profile.PreferredSpacingAdjustment = 80.0f;
    Profile.AggressionAdjustment = 0.10f;
    Profile.bEvidenceStrongEnough = true;

    Profile.EvidencePrediction.bHasPrediction = true;
    Profile.EvidencePrediction.PredictedAction =
        EPlayerCombatAction::Block;
    Profile.EvidencePrediction.Confidence = 0.75f;
    Profile.EvidencePrediction.SupportingSampleCount = 6;
    Profile.EvidencePrediction.bUsedContext = true;
    Profile.EvidencePrediction.ConditioningEnemyAction =
        EEnemyCombatAction::LightAttack;
    Profile.EvidencePrediction.bUsedDistanceContext = true;
    Profile.EvidencePrediction.ConditioningDistanceCategory =
        ECombatDistanceCategory::Close;
    return Profile;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveMilestone26RoundStageTest,
    "adaptHunt.Milestone26.RoundStagesAndCountdown",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveMilestone26RoundStageTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("Round 1 visibly identifies live learning"),
        FAdaptiveAdaptationRevealPolicy::FormatRoundStage(1).Contains(
            TEXT("LIVE LEARNING")
        )
    );
    TestTrue(
        TEXT("Round 2 is visibly adapting"),
        FAdaptiveAdaptationRevealPolicy::FormatRoundStage(2).Contains(
            TEXT("ADAPTING")
        )
    );
    TestTrue(
        TEXT("Round 3 is visibly adapted and asks for counter-adaptation"),
        FAdaptiveAdaptationRevealPolicy::FormatRoundStage(3).Contains(
            TEXT("ADAPTED")
        )
            && FAdaptiveAdaptationRevealPolicy::FormatRoundStage(3)
                .Contains(TEXT("COUNTER-ADAPT"))
    );
    TestTrue(
        TEXT("The countdown status is explicit"),
        AAdaptiveDebugHUD::FormatRoundStatus(
            1,
            3,
            EAdaptiveRoundPhase::PreRoundCountdown,
            1.95f
        ).Contains(TEXT("STARTS IN 2.0"))
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveMilestone26EvidenceRevealTest,
    "adaptHunt.Milestone26.EvidenceBackedReveal",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveMilestone26EvidenceRevealTest::RunTest(
    const FString& Parameters
)
{
    const FAdaptiveRevealText Reveal =
        FAdaptiveAdaptationRevealPolicy::Build(
            MakeObservedBlockPattern(),
            MakeCloseBlockProfile()
        );
    TestTrue(
        TEXT("Validated contextual evidence produces an observation"),
        Reveal.bHasSupportedObservation
            && Reveal.Observation.Contains(TEXT("enemy light attack"))
            && Reveal.Observation.Contains(TEXT("close range"))
            && Reveal.Observation.Contains(TEXT("block"))
    );
    TestFalse(
        TEXT("Unused position context is not manufactured"),
        Reveal.Observation.Contains(TEXT("enemy's left"))
            || Reveal.Observation.Contains(TEXT("enemy's right"))
    );
    TestTrue(
        TEXT("The stated adjustment is the profile's actual counter"),
        Reveal.bHasActiveAdjustment
            && Reveal.Adjustment.Contains(TEXT("heavy attacks"))
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveMilestone26InsufficientRevealTest,
    "adaptHunt.Milestone26.InsufficientEvidenceReveal",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveMilestone26InsufficientRevealTest::RunTest(
    const FString& Parameters
)
{
    FAdaptiveTacticalProfile InactiveProfile;
    InactiveProfile.SourceRound = 1;
    InactiveProfile.EvidenceStatus =
        EAdaptiveProfileEvidenceStatus::InsufficientContextSamples;

    const FAdaptiveRevealText PatternOnly =
        FAdaptiveAdaptationRevealPolicy::Build(
            MakeObservedBlockPattern(),
            InactiveProfile
        );
    TestTrue(
        TEXT("An analyzed pattern may be reported without claiming adaptation"),
        PatternOnly.bHasSupportedObservation
            && !PatternOnly.bHasActiveAdjustment
            && PatternOnly.Observation.Contains(TEXT("block"))
            && PatternOnly.Adjustment.Contains(TEXT("baseline tactics"))
    );

    const FAdaptiveRevealText NoEvidence =
        FAdaptiveAdaptationRevealPolicy::Build(
            FAdaptiveConditionalPattern(),
            FAdaptiveTacticalProfile()
        );
    TestTrue(
        TEXT("No evidence produces an honest no-habit message"),
        !NoEvidence.bHasSupportedObservation
            && !NoEvidence.bHasActiveAdjustment
            && NoEvidence.Observation.Contains(TEXT("no reliable"))
            && NoEvidence.Adjustment.Contains(TEXT("baseline tactics"))
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveMilestone26MatchRestartTest,
    "adaptHunt.Milestone26.MatchResultsAndCleanRestart",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveMilestone26MatchRestartTest::RunTest(
    const FString& Parameters
)
{
    FAdaptiveRoundProgression Progression;
    TestTrue(TEXT("The match enters the Round 1 countdown"), Progression.BeginMatch());
    TestTrue(TEXT("Round 1 begins"), Progression.StartCurrentRound());
    TestTrue(
        TEXT("The player wins Round 1"),
        Progression.CompleteCurrentRound(EAdaptiveRoundWinner::Player)
    );
    TestTrue(TEXT("Round 2 prepares"), Progression.AdvanceToNextRound());
    TestTrue(TEXT("Round 2 begins"), Progression.StartCurrentRound());
    TestTrue(
        TEXT("The enemy wins Round 2"),
        Progression.CompleteCurrentRound(EAdaptiveRoundWinner::Enemy)
    );
    TestTrue(TEXT("Round 3 prepares"), Progression.AdvanceToNextRound());
    TestTrue(TEXT("Round 3 begins"), Progression.StartCurrentRound());
    TestTrue(
        TEXT("The player wins Round 3"),
        Progression.CompleteCurrentRound(EAdaptiveRoundWinner::Player)
    );
    TestTrue(
        TEXT("Match results aggregate all three rounds"),
        Progression.GetPhase() == EAdaptiveRoundPhase::MatchComplete
            && Progression.GetMatchWinner()
                == EAdaptiveRoundWinner::Player
            && Progression.GetPlayerRoundWins() == 2
            && Progression.GetEnemyRoundWins() == 1
            && AAdaptiveDebugHUD::FormatMatchScore(2, 1)
                .Contains(TEXT("PLAYER 2  -  1 ENEMY"))
    );

    TestTrue(
        TEXT("Starting again resets progression to a clean Round 1 countdown"),
        Progression.BeginMatch()
    );
    TestTrue(
        TEXT("The restarted progression has no retained result"),
        Progression.GetCurrentRound() == 1
            && Progression.GetLastCompletedRound() == 0
            && Progression.GetLastWinner() == EAdaptiveRoundWinner::None
            && Progression.GetMatchWinner() == EAdaptiveRoundWinner::None
            && Progression.GetPlayerRoundWins() == 0
            && Progression.GetEnemyRoundWins() == 0
            && Progression.GetPhase()
                == EAdaptiveRoundPhase::PreRoundCountdown
    );

    const AAdaptivePlayerCharacter* Player =
        GetDefault<AAdaptivePlayerCharacter>();
    const UInputMappingContext* Mapping = Player
        ? Player->GetDefaultMappingContext()
        : nullptr;
    TestTrue(
        TEXT("Keyboard and gamepad expose a match restart action"),
        Mapping
            && Algo::AnyOf(
                Mapping->GetMappings(),
                [](const FEnhancedActionKeyMapping& Entry)
                {
                    return Entry.Key == EKeys::Enter;
                }
            )
            && Algo::AnyOf(
                Mapping->GetMappings(),
                [](const FEnhancedActionKeyMapping& Entry)
                {
                    return Entry.Key == EKeys::Gamepad_Special_Right;
                }
            )
    );
    return true;
}

#endif
