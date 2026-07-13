#pragma once

#include "CoreMinimal.h"
#include "AI/AdaptiveCounterOutcome.h"
#include "AI/AdaptiveTacticalProfile.h"
#include "AI/EnemyActionScorer.h"
#include "AI/EnemyDecisionPolicy.h"
#include "AI/EnemyMovementPolicy.h"
#include "AI/EnemyTacticalPolicy.h"
#include "AI/PlayerActionPredictor.h"
#include "Components/ActorComponent.h"
#include "Data/CombatTypes.h"
#include "Data/CombatDataset.h"
#include "TimerManager.h"

#include "EnemyDecisionComponent.generated.h"

class UEnemyCombatComponent;
class UEnemyLocomotionComponent;
class UCombatComponent;
class UHealthComponent;
class UPlayerBehaviorTrackerComponent;
struct FTrainingSample;

/**
 * Timer-driven enemy brain with prediction-aware utility action selection.
 *
 * A deterministic tactical policy modifies spacing and bounded utility
 * weights without replacing the scorer. Prediction remains behind the
 * replaceable IPlayerActionPredictor contract, and action execution remains
 * owned by UEnemyCombatComponent.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class ADAPTHUNT_API UEnemyDecisionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEnemyDecisionComponent();

    UFUNCTION(BlueprintCallable, Category = "AI|Enemy")
    void SetTargetActor(AActor* NewTargetActor);

    AActor* GetTargetActor() const;

    UFUNCTION(BlueprintCallable, Category = "AI|Enemy")
    void SetDecisionMakingEnabled(bool bEnabled);

    bool IsDecisionMakingEnabled() const;
    bool HasActiveEvaluationTimer() const;

    /** Runs one deterministic update immediately; useful for debugging. */
    UFUNCTION(BlueprintCallable, Category = "AI|Enemy")
    void EvaluateNow();

    /** Rebuilds the active prediction model from an explicit dataset. */
    void TrainPredictor(const FCombatDataset& Dataset);

    /** Supplies prior-session samples kept separate from live round telemetry. */
    void SetPersistentTrainingDataset(const FCombatDataset& Dataset);
    void ClearPersistentTrainingDataset();

    /** Derives the next round's explainable profile from committed history. */
    void RebuildAdaptiveTacticalProfile(
        const FCombatDataset& Dataset,
        int32 CompletedRound
    );

    UFUNCTION(BlueprintCallable, Category = "AI|Adaptation")
    void ResetAdaptiveTacticalProfile();

    /** Accepts only an active validated profile or an explicit empty profile. */
    bool DebugForceAdaptiveTacticalProfile(
        const FAdaptiveTacticalProfile& Profile
    );

    /** Explicit new-match reset; normal round transitions retain history. */
    UFUNCTION(BlueprintCallable, Category = "AI|Adaptation|Outcome")
    void ResetCounterOutcomeTracking();

    /** Clears moment-to-moment action/result memory at a combat boundary. */
    UFUNCTION(BlueprintCallable, Category = "AI|Enemy|Memory")
    void ResetShortTermDecisionMemory();

    /** Rebuilds the model from the current target's behavior tracker. */
    UFUNCTION(BlueprintCallable, Category = "AI|Adaptation")
    bool TrainPredictorFromTargetHistory();

    UFUNCTION(BlueprintCallable, Category = "AI|Adaptation")
    void ResetPredictor();

    /** Allows experiments to replace the default conditional model. */
    void SetPredictor(TUniquePtr<IPlayerActionPredictor> NewPredictor);

    UFUNCTION(BlueprintCallable, Category = "AI|Adaptation")
    void SetAutomaticRetrainingEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "AI|Adaptation")
    void SetPredictionUsageEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "AI|Adaptation")
    bool IsAutomaticRetrainingEnabled() const;

    UFUNCTION(BlueprintPure, Category = "AI|Adaptation")
    bool IsPredictionUsageEnabled() const;

    EEnemyCombatAction SelectMovementAction(float DistanceToTarget) const;
    EEnemyLocomotionIntent SelectMovementIntent(
        float DistanceToTarget
    ) const;
    EEnemyCombatAction SelectCombatAction(
        float DistanceToTarget,
        bool bTargetRecentlyHealed,
        float RandomValue
    ) const;

    /** Pure prediction gate used for deterministic tuning and edge tests. */
    static bool ShouldApplyPredictionDeterministically(
        const FPredictionResult& Prediction,
        bool bUsageEnabled,
        float MinimumConfidence,
        int32 MinimumSupportingSamples,
        float ApplicationChance,
        float RandomValue
    );

    float GetEvaluationInterval() const;
    float GetCombatDecisionInterval() const;
    float GetRetreatDistance() const;
    float GetStrafeDistance() const;
    float GetPredictionInfluence() const;
    float GetMinimumPredictionConfidence() const;
    float GetPredictionApplicationChance() const;
    int32 GetMinimumPredictionSupportingSamples() const;
    const TArray<FEnemyActionScore>& GetLastActionScores() const;
    EEnemyCombatAction GetLastSelectedUtilityAction() const;
    /** Last utility or fallback combat action successfully selected. */
    EEnemyCombatAction GetLastSelectedAction() const;
    const FPredictionResult& GetLastPrediction() const;
    bool WasLastPredictionApplied() const;
    int32 GetTrainedSampleCount() const;
    int32 GetPersistentTrainingSampleCount() const;
    float GetRecentLightAttackPressure() const;
    const FAdaptiveTacticalProfile& GetAdaptiveTacticalProfile() const;
    const FAdaptiveTacticalProfileTuning&
        GetAdaptiveTacticalProfileTuning() const;
    int32 GetAdaptiveCounterUsesRemaining() const;
    float GetAdaptiveCounterCooldownRemaining() const;
    int32 GetCounterOutcomeCount() const;
    const FAdaptiveCounterOutcomeRecord* GetLastCounterOutcome() const;
    const TArray<FAdaptiveCounterEffectivenessSummary>&
        GetLastRoundCounterEffectiveness() const;
    const FAdaptiveCounterEffectivenessSummary&
        GetActiveCounterEffectiveness() const;
    const FAdaptiveCounterEffectivenessTuning&
        GetCounterEffectivenessTuning() const;

    UFUNCTION(BlueprintPure, Category = "AI|Tactics")
    EEnemyTacticalState GetTacticalState() const;

    UFUNCTION(BlueprintPure, Category = "AI|Tactics")
    bool HasActiveAttackSequence() const;

    UFUNCTION(BlueprintPure, Category = "AI|Tactics")
    EEnemyCombatAction GetAttackSequenceFollowUp() const;

    UFUNCTION(BlueprintPure, Category = "AI|Tactics")
    bool WasLastLineOfSightClear() const;

    const FEnemyTacticalTuning& GetTacticalTuning() const;
    const FEnemyActionSelectionTuning& GetActionSelectionTuning() const;
    const FEnemyActionRepetitionTuning& GetRepetitionTuning() const;
    const FEnemyRecentOutcomeTuning& GetRecentOutcomeTuning() const;
    const FEnemyUrgentDecisionTuning& GetUrgentDecisionTuning() const;
    const FEnemyActionSelectionResult& GetLastSelectionResult() const;
    const TMap<EEnemyCombatAction, float>&
        GetLastRepetitionModifiers() const;
    const TMap<EEnemyCombatAction, float>&
        GetLastRecentOutcomeModifiers() const;
    int32 GetRecentCommittedActionCount() const;
    int32 GetRecentOutcomeMemoryCount() const;
    bool WasLastDecisionUrgent() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void EvaluateDecision();
    void RefreshEvaluationTimer();
    void AcquirePlayerTarget();
    void BindBehaviorTracker();
    void UnbindBehaviorTracker();
    void BindTargetCombat();
    void UnbindTargetCombat();
    void HandleTrainingSampleRecorded(
        UPlayerBehaviorTrackerComponent* Tracker,
        const FTrainingSample& Sample
    );
    void ApplyMovement(EEnemyLocomotionIntent MovementIntent);
    FCombatSnapshot CaptureDecisionSnapshot(float DistanceToTarget) const;
    FEnemyTacticalContext BuildTacticalContext(
        const FCombatSnapshot& Snapshot,
        float DistanceToTarget,
        bool bHasLineOfSight
    ) const;
    FEnemyActionScoringContext BuildScoringContext(
        const FCombatSnapshot& Snapshot,
        const FEnemyTacticalContext& TacticalContext,
        bool bAdaptiveCounterOpportunity
    ) const;
    bool TryExecuteUtilityAction(
        const FEnemyActionScoringContext& Context,
        const FEnemyTacticalContext& TacticalContext,
        double CurrentTime,
        bool bAdaptiveCounterOpportunity
    );
    void RegisterOutcomeContext(
        int32 ActionId,
        EEnemyCombatAction Action,
        const FCombatSnapshot& Snapshot,
        bool bAdaptiveCounterOpportunity
    );
    void HandleEnemyActionResolved(
        const FEnemyCombatActionOutcome& Outcome
    );
    void UpdateTacticalState(
        const FEnemyTacticalContext& Context,
        double CurrentTime
    );
    void UpdateAttackSequence(
        const FEnemyTacticalContext& Context,
        double CurrentTime,
        bool bCombatDecisionDue
    );
    void HandleExecutedCombatAction(
        EEnemyCombatAction Action,
        const FEnemyTacticalContext& Context,
        double CurrentTime
    );
    void ResetTacticalState(double CurrentTime);
    bool HasLineOfSightToTarget() const;
    float GetPreferredDistance() const;
    static EEnemyCombatAction ToMovementAction(
        EEnemyLocomotionIntent MovementIntent
    );
    FPredictionResult PredictPlayerAction(
        const FCombatSnapshot& Snapshot
    ) const;
    bool ShouldApplyPrediction(const FPredictionResult& Prediction);
    bool IsTargetRecentlyHealing() const;
    void HandleOwnerDeath(UHealthComponent*, AActor*);
    double GetDecisionTimeSeconds() const;

    UFUNCTION()
    void HandleTargetActionPhaseChanged(
        ECombatActionPhase PreviousPhase,
        ECombatActionPhase NewPhase
    );

    UPROPERTY(Transient)
    TObjectPtr<AActor> TargetActor;

    UPROPERTY(Transient)
    TObjectPtr<UEnemyCombatComponent> EnemyCombatComponent;

    UPROPERTY(Transient)
    TObjectPtr<UEnemyLocomotionComponent> EnemyLocomotionComponent;

    UPROPERTY(Transient)
    TObjectPtr<UHealthComponent> OwnerHealthComponent;

    UPROPERTY(Transient)
    TObjectPtr<UPlayerBehaviorTrackerComponent> BehaviorTrackerComponent;

    UPROPERTY(Transient)
    TObjectPtr<UCombatComponent> TargetCombatComponent;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Baseline", meta = (ClampMin = "0.02"))
    float EvaluationInterval;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Baseline", meta = (ClampMin = "0.1"))
    float CombatDecisionInterval;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Combat", meta = (ClampMin = "0.0"))
    float RangedCombatDistance;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Combat", meta = (ClampMin = "0.0"))
    float DashCombatDistance;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Combat", meta = (ClampMin = "0.0"))
    float InterruptDistance;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Adaptation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PredictionInfluence;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Adaptation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumPredictionConfidence;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Adaptation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PredictionApplicationChance;

    /** Prevents a one-off action from becoming an apparent learned habit. */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Adaptation", meta = (ClampMin = "1"))
    int32 MinimumPredictionSupportingSamples;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Adaptation", meta = (ClampMin = "1"))
    int32 MinimumTrainingSamples;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Adaptation", meta = (ClampMin = "1"))
    int32 RetrainSampleInterval;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Adaptation|Pattern", meta = (ClampMin = "3", ClampMax = "16"))
    int32 RecentPlayerPatternWindow;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Adaptation|Pattern", meta = (ClampMin = "2", ClampMax = "16"))
    int32 MinimumRepeatedLightAttacks;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Adaptation|Pattern", meta = (ClampMin = "0.0", ClampMax = "0.65"))
    float MaximumLightAttackPatternAdjustment;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Adaptation")
    int32 AdaptationRandomSeed;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Adaptation")
    FAdaptiveTacticalProfileTuning AdaptiveProfileTuning;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Adaptation|Outcome")
    FAdaptiveCounterEffectivenessTuning CounterEffectivenessTuning;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Tactics")
    FEnemyTacticalTuning TacticalTuning;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Tactics")
    int32 TacticalRandomSeed;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Selection")
    FEnemyActionSelectionTuning ActionSelectionTuning;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Selection|Repetition")
    FEnemyActionRepetitionTuning RepetitionTuning;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Selection|Outcome")
    FEnemyRecentOutcomeTuning RecentOutcomeTuning;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Selection|Urgent")
    FEnemyUrgentDecisionTuning UrgentDecisionTuning;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Utility")
    TArray<FEnemyActionScore> LastActionScores;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Utility")
    EEnemyCombatAction LastSelectedUtilityAction;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Utility")
    EEnemyCombatAction LastSelectedAction;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Adaptation")
    FPredictionResult LastPrediction;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Adaptation")
    int32 TrainedSampleCount;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Adaptation")
    int32 LiveSampleCountAtLastTraining;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Adaptation")
    bool bLastPredictionApplied;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Adaptation")
    FAdaptiveTacticalProfile AdaptiveTacticalProfile;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Adaptation|Outcome")
    TArray<FAdaptiveCounterEffectivenessSummary>
        LastRoundCounterEffectiveness;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Adaptation|Outcome")
    FAdaptiveCounterEffectivenessSummary ActiveCounterEffectiveness;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Tactics")
    EEnemyTacticalState TacticalState;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Tactics")
    bool bLastLineOfSightClear;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Utility")
    bool bLastDecisionUrgent;

    bool bDecisionMakingEnabled;
    bool bAutomaticRetrainingEnabled;
    bool bPredictionUsageEnabled;
    bool bStrafeRight;
    double NextCombatDecisionTime;
    double TacticalStateEnteredTime;
    double NextOrbitDirectionChangeTime;
    FEnemyActionScorer ActionScorer;
    TUniquePtr<IPlayerActionPredictor> Predictor;
    FCombatDataset PersistentTrainingDataset;
    FRandomStream AdaptationRandomStream;
    FRandomStream TacticalRandomStream;
    FRandomStream SelectionRandomStream;
    FEnemyAttackSequenceState AttackSequence;
    FEnemyCommittedActionHistory CommittedActionHistory;
    FEnemyRecentOutcomeMemory RecentOutcomeMemory;
    FEnemyUrgentDecisionState UrgentDecisionState;
    FEnemyActionSelectionResult LastSelectionResult;
    TMap<EEnemyCombatAction, float> LastRepetitionModifiers;
    TMap<EEnemyCombatAction, float> LastRecentOutcomeModifiers;
    FAdaptiveCounterBudgetState AdaptiveCounterBudget;
    FAdaptiveCounterOutcomeHistory CounterOutcomeHistory;
    TMap<int32, FAdaptiveCounterOutcomeRecord> PendingOutcomeRecords;
    FTimerHandle EvaluationTimer;
    FTimerHandle UrgentDecisionTimer;
};
