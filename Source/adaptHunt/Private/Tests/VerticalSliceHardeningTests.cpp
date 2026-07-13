#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/AdaptiveCounterOutcome.h"
#include "AI/AdaptiveTacticalProfile.h"
#include "Characters/AdaptiveEnemyCharacter.h"
#include "Characters/AdaptivePlayerCharacter.h"
#include "Components/CombatComponent.h"
#include "Components/CombatFeedbackComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/EnemyDecisionComponent.h"
#include "Components/EnemyLocomotionComponent.h"
#include "Components/HealthComponent.h"
#include "Components/StaminaComponent.h"
#include "Data/CombatDataset.h"
#include "Debug/AdaptiveDebugCommands.h"
#include "Engine/World.h"
#include "Game/AdaptiveHuntTuningSettings.h"
#include "Game/RoundManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"

#include <limits>

namespace
{
template <typename ComponentType>
ComponentType* AddTestComponent(AActor* Owner, const FName Name)
{
    ComponentType* Component = NewObject<ComponentType>(Owner, Name);
    if (Owner && Component)
    {
        Owner->AddInstanceComponent(Component);
        Component->RegisterComponent();
    }
    return Component;
}

UWorld* CreateHardeningTestWorld()
{
    FWorldInitializationValues Values;
    Values.AllowAudioPlayback(false)
        .RequiresHitProxies(false)
        .CreateNavigation(false)
        .CreateAISystem(false)
        .ShouldSimulatePhysics(false)
        .CreateFXSystem(false)
        .SetTransactional(false);
    return UWorld::CreateWorld(
        EWorldType::Game,
        false,
        TEXT("AdaptiveHuntHardeningTestWorld"),
        nullptr,
        true,
        ERHIFeatureLevel::Num,
        &Values
    );
}

FAdaptiveCounterOutcomeRecord MakeDuplicateGuardRecord()
{
    FAdaptiveCounterOutcomeRecord Record;
    Record.ActionId = 27;
    Record.RoundNumber = 2;
    Record.Action = EEnemyCombatAction::HeavyAttack;
    Record.ProfilePreferredCounterAction =
        EEnemyCombatAction::HeavyAttack;
    Record.Result = EAdaptiveCounterOutcomeResult::Hit;
    Record.ContextSnapshot.RoundNumber = 2;
    Record.Prediction.bHasPrediction = true;
    Record.Prediction.PredictedAction = EPlayerCombatAction::Block;
    Record.Prediction.Confidence = 0.8f;
    Record.Prediction.SupportingSampleCount = 5;
    Record.bPredictionApplied = true;
    Record.bAdaptiveCounterOpportunity = true;
    return Record;
}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCentralTuningTest,
    "adaptHunt.Milestone27.CentralTuningAndInvalidValues",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCentralTuningTest::RunTest(const FString& Parameters)
{
    const UAdaptiveHuntTuningSettings* Settings =
        GetDefault<UAdaptiveHuntTuningSettings>();
    TestNotNull(TEXT("The asset-free project tuning CDO exists"), Settings);
    if (!Settings)
    {
        return false;
    }

    const FAdaptiveMovementTuning Movement =
        Settings->Movement.GetSanitized();
    TestTrue(
        TEXT("Player and enemy movement defaults form usable ranges"),
        Movement.PlayerMaxWalkSpeed > 0.0f
            && Movement.PlayerAcceleration > 0.0f
            && Movement.EnemyRetreatRange
                < Movement.EnemyPreferredRange
            && Movement.EnemyPreferredRange < Movement.EnemyOrbitRange
            && Movement.EnemyMovementSpeed > 0.0f
    );

    const FAdaptiveCombatBalanceTuning CombatBalance =
        Settings->CombatBalance.GetSanitized();
    TestTrue(
        TEXT("Enemy resource defaults retain the requested power ratios"),
        FMath::IsNearlyEqual(
            CombatBalance.GetEnemyMaxHealth(),
            CombatBalance.PlayerMaxHealth * 2.0f
        ) && FMath::IsNearlyEqual(
            CombatBalance.GetEnemyMaxStamina(),
            CombatBalance.PlayerMaxStamina * 1.5f
        )
    );
    TestTrue(
        TEXT("Enemy core attacks retain the requested damage ratio"),
        FMath::IsNearlyEqual(
            CombatBalance.GetEnemyLightAttackDamage(),
            CombatBalance.PlayerLightAttackDamage * 2.0f
        ) && FMath::IsNearlyEqual(
            CombatBalance.GetEnemyHeavyAttackDamage(),
            CombatBalance.PlayerHeavyAttackDamage * 2.0f
        )
    );

    const FAdaptiveActionTimingTuning Timings =
        Settings->ActionTiming.GetSanitized();
    TestTrue(
        TEXT("Central attack defaults retain readable active timelines"),
        Timings.PlayerLightAttack.WindupDuration > 0.0f
            && Timings.PlayerHeavyAttack.GetTotalDuration()
                > Timings.PlayerLightAttack.GetTotalDuration()
            && Timings.EnemyHeavyAttack.GetTotalDuration()
                > Timings.EnemyLightAttack.GetTotalDuration()
            && Timings.EnemyProjectileAttack.ActiveDuration > 0.0f
    );
    TestTrue(
        TEXT("The single player input buffer stays inside the intended window"),
        Timings.PlayerInputBufferDuration >= 0.10f
            && Timings.PlayerInputBufferDuration <= 0.18f
    );

    const FAdaptiveLearningTuning Learning =
        Settings->Adaptation.GetSanitized();
    TestTrue(
        TEXT("Fairness defaults retain the committed cooldown and budget"),
        FMath::IsNearlyEqual(Learning.Profile.CounterCooldown, 2.4f)
            && Learning.Profile.CounterBudgetPerRound == 3
    );
    TestTrue(
        TEXT("Outcome learning retains its three-sample smoothed 0.08 bound"),
        Learning.Outcome.MinimumSamples == 3
            && FMath::IsNearlyEqual(
                Learning.Outcome.SmoothingSampleWeight,
                1.0f
            )
            && FMath::IsNearlyEqual(
                Learning.Outcome.MaximumUtilityAdjustment,
                0.08f
            )
    );

    const float NotANumber = std::numeric_limits<float>::quiet_NaN();
    const float Infinity = std::numeric_limits<float>::infinity();
    FAdaptiveMovementTuning InvalidMovement;
    InvalidMovement.PlayerAcceleration = NotANumber;
    InvalidMovement.PlayerAirControl = Infinity;
    InvalidMovement.EnemyRetreatRange = 500.0f;
    InvalidMovement.EnemyPreferredRange = 100.0f;
    InvalidMovement.EnemyOrbitRange = -20.0f;
    const FAdaptiveMovementTuning SafeMovement =
        InvalidMovement.GetSanitized();
    TestTrue(
        TEXT("Invalid movement values recover to finite ordered defaults"),
        FMath::IsFinite(SafeMovement.PlayerAcceleration)
            && SafeMovement.PlayerAirControl >= 0.0f
            && SafeMovement.PlayerAirControl <= 1.0f
            && SafeMovement.EnemyRetreatRange
                < SafeMovement.EnemyPreferredRange
            && SafeMovement.EnemyPreferredRange
                < SafeMovement.EnemyOrbitRange
    );

    FAdaptiveActionTimingTuning InvalidTimings;
    InvalidTimings.PlayerLightAttack.WindupDuration = NotANumber;
    InvalidTimings.EnemyHeavyAttack.ActiveDuration = Infinity;
    InvalidTimings.PlayerInputBufferDuration = 4.0f;
    const FAdaptiveActionTimingTuning SafeTimings =
        InvalidTimings.GetSanitized();
    TestTrue(
        TEXT("Invalid timing values retain finite safe timelines"),
        FMath::IsFinite(SafeTimings.PlayerLightAttack.WindupDuration)
            && SafeTimings.PlayerLightAttack.WindupDuration > 0.0f
            && FMath::IsFinite(
                SafeTimings.EnemyHeavyAttack.ActiveDuration
            )
            && SafeTimings.EnemyHeavyAttack.ActiveDuration > 0.0f
            && FMath::IsNearlyEqual(
                SafeTimings.PlayerInputBufferDuration,
                0.5f
            )
    );

    FAdaptiveCombatBalanceTuning InvalidCombatBalance;
    InvalidCombatBalance.PlayerMaxHealth = NotANumber;
    InvalidCombatBalance.PlayerMaxStamina = -5.0f;
    InvalidCombatBalance.EnemyHealthMultiplier = Infinity;
    InvalidCombatBalance.EnemyDamageMultiplier = -2.0f;
    const FAdaptiveCombatBalanceTuning SafeCombatBalance =
        InvalidCombatBalance.GetSanitized();
    TestTrue(
        TEXT("Invalid combat balance values recover to finite hard bounds"),
        FMath::IsFinite(SafeCombatBalance.PlayerMaxHealth)
            && SafeCombatBalance.PlayerMaxStamina >= 1.0f
            && FMath::IsFinite(SafeCombatBalance.EnemyHealthMultiplier)
            && SafeCombatBalance.EnemyDamageMultiplier >= 0.1f
    );

    FAdaptiveFeedbackTuning InvalidFeedback;
    InvalidFeedback.KnockbackStrength = 600.0f;
    InvalidFeedback.MaximumKnockbackStrength = 100.0f;
    InvalidFeedback.CameraReturnSpeed = NotANumber;
    const FAdaptiveFeedbackTuning SafeFeedback =
        InvalidFeedback.GetSanitized();
    TestTrue(
        TEXT("Feedback sanitization preserves caps and finite decay"),
        SafeFeedback.MaximumKnockbackStrength
            >= SafeFeedback.KnockbackStrength
            && FMath::IsFinite(SafeFeedback.CameraReturnSpeed)
            && SafeFeedback.CameraReturnSpeed > 0.0f
    );
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveVerticalSliceDefaultWiringTest,
    "adaptHunt.Milestone27.VerticalSliceDefaultWiring",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveVerticalSliceDefaultWiringTest::RunTest(
    const FString& Parameters
)
{
    const UAdaptiveHuntTuningSettings& Settings =
        UAdaptiveHuntTuningSettings::Get();
    const AAdaptivePlayerCharacter* Player =
        GetDefault<AAdaptivePlayerCharacter>();
    const AAdaptiveEnemyCharacter* Enemy =
        GetDefault<AAdaptiveEnemyCharacter>();
    TestNotNull(TEXT("Player native defaults exist"), Player);
    TestNotNull(TEXT("Enemy native defaults exist"), Enemy);
    if (!Player || !Enemy)
    {
        return false;
    }

    const FAdaptiveMovementTuning Movement =
        Settings.Movement.GetSanitized();
    TestTrue(
        TEXT("Player CharacterMovement consumes the central defaults"),
        FMath::IsNearlyEqual(
            Player->GetCharacterMovement()->MaxWalkSpeed,
            Movement.PlayerMaxWalkSpeed
        ) && FMath::IsNearlyEqual(
            Player->GetCharacterMovement()->MaxAcceleration,
            Movement.PlayerAcceleration
        )
    );
    TestTrue(
        TEXT("Camera responsiveness consumes the central defaults"),
        FMath::IsNearlyEqual(
            Player->GetCameraPositionLagSpeed(),
            Settings.Camera.GetSanitized().PositionLagSpeed
        ) && FMath::IsNearlyEqual(
            Player->GetCameraRotationSmoothingSpeed(),
            Settings.Camera.GetSanitized().RotationSmoothingSpeed
        )
    );

    const UEnemyLocomotionComponent* Locomotion =
        Enemy->GetEnemyLocomotionComponent();
    const UEnemyCombatComponent* EnemyCombat =
        Enemy->GetEnemyCombatComponent();
    const UCombatComponent* PlayerCombat = Player->GetCombatComponent();
    const UEnemyDecisionComponent* Decision =
        Enemy->GetEnemyDecisionComponent();
    TestTrue(
        TEXT("Enemy locomotion consumes central speed and spacing"),
        Locomotion
            && FMath::IsNearlyEqual(
                Locomotion->GetPreferredRange(),
                Movement.EnemyPreferredRange
            )
            && FMath::IsNearlyEqual(
                Locomotion->GetMovementSpeed(),
                Movement.EnemyMovementSpeed
            )
    );
    TestTrue(
        TEXT("Both combat executors consume central action timelines"),
        PlayerCombat && EnemyCombat
            && FMath::IsNearlyEqual(
                PlayerCombat->GetActionTiming(
                    EPlayerCombatAction::LightAttack
                ).WindupDuration,
                Settings.ActionTiming.GetSanitized()
                    .PlayerLightAttack.WindupDuration
            )
            && FMath::IsNearlyEqual(
                EnemyCombat->GetActionTiming(
                    EEnemyCombatAction::HeavyAttack
                ).RecoveryDuration,
                Settings.ActionTiming.GetSanitized()
                    .EnemyHeavyAttack.RecoveryDuration
            )
    );
    const FAdaptiveCombatBalanceTuning CombatBalance =
        Settings.CombatBalance.GetSanitized();
    TestTrue(
        TEXT("Character resources consume the central combat ratios"),
        Player->GetHealthComponent()
            && Player->GetStaminaComponent()
            && Enemy->GetHealthComponent()
            && Enemy->GetStaminaComponent()
            && FMath::IsNearlyEqual(
                Player->GetHealthComponent()->GetMaxHealth(),
                CombatBalance.PlayerMaxHealth
            )
            && FMath::IsNearlyEqual(
                Enemy->GetHealthComponent()->GetMaxHealth(),
                CombatBalance.GetEnemyMaxHealth()
            )
            && FMath::IsNearlyEqual(
                Enemy->GetStaminaComponent()->GetMaxStamina(),
                CombatBalance.GetEnemyMaxStamina()
            )
    );
    TestTrue(
        TEXT("Combat executors consume the exact two-times damage ratio"),
        PlayerCombat && EnemyCombat
            && FMath::IsNearlyEqual(
                EnemyCombat->GetLightAttackDamage(),
                PlayerCombat->GetLightAttackDamage() * 2.0f
            )
            && FMath::IsNearlyEqual(
                EnemyCombat->GetHeavyAttackDamage(),
                PlayerCombat->GetHeavyAttackDamage() * 2.0f
            )
    );
    TestTrue(
        TEXT("Decision cadence and adaptation consume central defaults"),
        Decision
            && FMath::IsNearlyEqual(
                Decision->GetEvaluationInterval(),
                Settings.Tactical.GetSanitized().EvaluationInterval
            )
            && FMath::IsNearlyEqual(
                Decision->GetPredictionInfluence(),
                Settings.Adaptation.GetSanitized().PredictionInfluence
            )
    );

    const UCombatFeedbackComponent* Feedback =
        Player->GetCombatFeedbackComponent();
    const URoundManager* RoundManager = GetDefault<URoundManager>();
    TestTrue(
        TEXT("Feedback and match flow consume central defaults"),
        Feedback && RoundManager
            && FMath::IsNearlyEqual(
                Feedback->GetKnockbackStrength(),
                Settings.Feedback.GetSanitized().KnockbackStrength
            )
            && FMath::IsNearlyEqual(
                RoundManager->GetPreRoundCountdownDuration(),
                Settings.MatchFlow.GetSanitized()
                    .PreRoundCountdownDuration
            )
    );
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveLifecycleTimerHardeningTest,
    "adaptHunt.Milestone27.ResetDeathTargetLossAndTimerCleanup",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveLifecycleTimerHardeningTest::RunTest(
    const FString& Parameters
)
{
    UWorld* World = CreateHardeningTestWorld();
    TestNotNull(TEXT("A transient timer test world can be created"), World);
    if (!World)
    {
        return false;
    }

    AActor* PlayerOwner = World->SpawnActor<AActor>();
    UHealthComponent* PlayerHealth = AddTestComponent<UHealthComponent>(
        PlayerOwner,
        TEXT("PlayerHealth")
    );
    AddTestComponent<UStaminaComponent>(
        PlayerOwner,
        TEXT("PlayerStamina")
    );
    UCombatComponent* PlayerCombat = AddTestComponent<UCombatComponent>(
        PlayerOwner,
        TEXT("PlayerCombat")
    );
    PlayerOwner->DispatchBeginPlay();

    TestTrue(
        TEXT("A forced player phase schedules its cleanup timer"),
        PlayerCombat
            && PlayerCombat->DebugForceActionPhase(
                ECombatActionPhase::Windup,
                30.0f
            )
            && PlayerCombat->HasActivePhaseTimer()
    );
    PlayerCombat->ResetCombatState();
    TestTrue(
        TEXT("Reset clears the player timer and returns Idle"),
        !PlayerCombat->HasActivePhaseTimer()
            && PlayerCombat->GetCurrentPhase()
                == ECombatActionPhase::Idle
    );

    PlayerCombat->DebugForceActionPhase(
        ECombatActionPhase::Recovery,
        30.0f
    );
    PlayerHealth->ApplyDamage(1000.0f);
    TestTrue(
        TEXT("Actual death clears the action timer and enters Dead"),
        PlayerHealth->IsDead()
            && PlayerCombat->GetCurrentPhase()
                == ECombatActionPhase::Dead
            && !PlayerCombat->HasActivePhaseTimer()
    );

    AActor* EnemyOwner = World->SpawnActor<AActor>();
    AddTestComponent<UHealthComponent>(EnemyOwner, TEXT("EnemyHealth"));
    AddTestComponent<UStaminaComponent>(EnemyOwner, TEXT("EnemyStamina"));
    UEnemyCombatComponent* EnemyCombat =
        AddTestComponent<UEnemyCombatComponent>(
            EnemyOwner,
            TEXT("EnemyCombat")
        );
    EnemyOwner->DispatchBeginPlay();
    EnemyCombat->DebugForceActionPhase(
        ECombatActionPhase::Windup,
        30.0f
    );
    TestTrue(
        TEXT("Enemy phase timer is active before target loss"),
        EnemyCombat->HasActivePhaseTimer()
    );
    EnemyCombat->HandleTargetLost();
    TestTrue(
        TEXT("Target loss clears the timer and target-dependent phase"),
        !EnemyCombat->HasActivePhaseTimer()
            && EnemyCombat->GetCurrentPhase()
                == ECombatActionPhase::Idle
    );

    AActor* DecisionOwner = World->SpawnActor<AActor>();
    UHealthComponent* DecisionHealth = AddTestComponent<UHealthComponent>(
        DecisionOwner,
        TEXT("DecisionHealth")
    );
    UEnemyDecisionComponent* Decision =
        AddTestComponent<UEnemyDecisionComponent>(
            DecisionOwner,
            TEXT("Decision")
        );
    DecisionOwner->DispatchBeginPlay();
    TestTrue(
        TEXT("The enabled enemy brain owns one repeating evaluation timer"),
        Decision->HasActiveEvaluationTimer()
    );
    Decision->SetDecisionMakingEnabled(false);
    TestFalse(
        TEXT("Disabling decisions clears the repeating timer"),
        Decision->HasActiveEvaluationTimer()
    );
    Decision->SetDecisionMakingEnabled(true);
    TestTrue(
        TEXT("Re-enabling decisions restores exactly one timer"),
        Decision->HasActiveEvaluationTimer()
    );
    DecisionHealth->ApplyDamage(1000.0f);
    TestTrue(
        TEXT("Enemy death disables decisions and clears their timer"),
        !Decision->IsDecisionMakingEnabled()
            && !Decision->HasActiveEvaluationTimer()
    );

    World->DestroyWorld(false);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveEvidenceAndDebugHardeningTest,
    "adaptHunt.Milestone27.EvidenceNavigationDuplicatesAndDebugCommands",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveEvidenceAndDebugHardeningTest::RunTest(
    const FString& Parameters
)
{
    TestFalse(
        TEXT("Missing navigation data fails closed without blocking fallback"),
        UEnemyLocomotionComponent::CanAttemptNavigation(true, false)
    );

    const FCombatDataset EmptyDataset;
    const FAdaptiveTacticalProfile EmptyProfile =
        FAdaptiveTacticalProfilePolicy::Derive(
            EmptyDataset,
            1,
            FPredictionResult(),
            FAdaptiveTacticalProfileTuning()
        );
    TestTrue(
        TEXT("Insufficient learning data retains baseline tactics"),
        !EmptyProfile.IsActive()
            && EmptyProfile.EvidenceStatus
                == EAdaptiveProfileEvidenceStatus::NoData
    );

    UEnemyDecisionComponent* IsolatedDecision =
        NewObject<UEnemyDecisionComponent>();
    TestFalse(
        TEXT("Missing target history cannot train a predictor"),
        IsolatedDecision->TrainPredictorFromTargetHistory()
    );

    FAdaptiveCounterOutcomeHistory History;
    const FAdaptiveCounterOutcomeRecord Record = MakeDuplicateGuardRecord();
    TestTrue(TEXT("The first terminal outcome is accepted"),
        History.Record(Record));
    TestFalse(TEXT("A duplicate terminal outcome is rejected"),
        History.Record(Record));

    FAdaptiveRoundProgression Progression;
    TestTrue(TEXT("Progression begins its baseline countdown"),
        Progression.BeginMatch());
    TestTrue(TEXT("The baseline round starts once"),
        Progression.StartCurrentRound());
    TestTrue(TEXT("The first death result completes the round"),
        Progression.CompleteCurrentRound(EAdaptiveRoundWinner::Player));
    TestFalse(TEXT("A duplicate death result cannot overwrite the winner"),
        Progression.CompleteCurrentRound(EAdaptiveRoundWinner::Enemy));

    EAdaptiveDebugProfilePreset Preset;
    ECombatActionPhase Phase;
    EAdaptiveDebugRoundTransition Transition;
    TestTrue(
        TEXT("Debug profile, phase, and round parsers accept useful aliases"),
        FAdaptiveDebugCommandPolicy::TryParseProfilePreset(
            TEXT("dodge-left"),
            Preset
        ) && Preset == EAdaptiveDebugProfilePreset::DodgeLeft
            && FAdaptiveDebugCommandPolicy::TryParseActionPhase(
                TEXT("stagger"),
                Phase
            ) && Phase == ECombatActionPhase::Staggered
            && FAdaptiveDebugCommandPolicy::TryParseRoundTransition(
                TEXT("next"),
                Transition
            ) && Transition == EAdaptiveDebugRoundTransition::Advance
    );
    TestFalse(
        TEXT("Unknown debug values are rejected without changing state"),
        FAdaptiveDebugCommandPolicy::TryParseActionPhase(
            TEXT("instant damage"),
            Phase
        )
    );

    const FAdaptiveTacticalProfile Forced =
        FAdaptiveDebugCommandPolicy::BuildForcedTacticalProfile(
            EAdaptiveDebugProfilePreset::Blocking,
            FAdaptiveTacticalProfileTuning()
        );
    TestTrue(
        TEXT("Forced presets still produce validated bounded profile data"),
        Forced.IsActive()
            && Forced.MostLikelyPlayerAction == EPlayerCombatAction::Block
            && Forced.PreferredCounterAction
                == EEnemyCombatAction::HeavyAttack
    );
    TestFalse(
        TEXT("The clear preset returns the normal inactive profile"),
        FAdaptiveDebugCommandPolicy::BuildForcedTacticalProfile(
            EAdaptiveDebugProfilePreset::Clear,
            FAdaptiveTacticalProfileTuning()
        ).IsActive()
    );

    IConsoleManager& Console = IConsoleManager::Get();
    TestTrue(
        TEXT("All three vertical-slice debug commands are registered"),
        Console.FindConsoleObject(TEXT("adapthunt.ForceTacticalProfile"))
            && Console.FindConsoleObject(TEXT("adapthunt.ForceActionPhase"))
            && Console.FindConsoleObject(
                TEXT("adapthunt.ForceRoundTransition")
            )
    );
    return true;
}

#endif
