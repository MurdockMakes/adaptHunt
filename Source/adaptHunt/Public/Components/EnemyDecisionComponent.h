#pragma once

#include "CoreMinimal.h"
#include "AI/EnemyActionScorer.h"
#include "AI/PlayerActionPredictor.h"
#include "Components/ActorComponent.h"
#include "Data/CombatTypes.h"
#include "TimerManager.h"

#include "EnemyDecisionComponent.generated.h"

class UEnemyCombatComponent;
class UHealthComponent;
class UPlayerBehaviorTrackerComponent;
struct FCombatDataset;
struct FTrainingSample;

/**
 * Timer-driven enemy brain with prediction-aware utility action selection.
 *
 * Movement remains a simple distance-band policy. Combat actions are ranked
 * by the side-effect-free scorer, prediction remains behind the replaceable
 * IPlayerActionPredictor contract, and action execution remains owned by
 * UEnemyCombatComponent.
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

    /** Runs one deterministic update immediately; useful for debugging. */
    UFUNCTION(BlueprintCallable, Category = "AI|Enemy")
    void EvaluateNow();

    /** Rebuilds the active prediction model from an explicit dataset. */
    void TrainPredictor(const FCombatDataset& Dataset);

    /** Rebuilds the model from the current target's behavior tracker. */
    UFUNCTION(BlueprintCallable, Category = "AI|Adaptation")
    bool TrainPredictorFromTargetHistory();

    UFUNCTION(BlueprintCallable, Category = "AI|Adaptation")
    void ResetPredictor();

    /** Allows later milestones to replace the frequency model. */
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

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void EvaluateDecision();
    void AcquirePlayerTarget();
    void BindBehaviorTracker();
    void UnbindBehaviorTracker();
    void HandleTrainingSampleRecorded(
        UPlayerBehaviorTrackerComponent* Tracker,
        const FTrainingSample& Sample
    );
    void ApplyMovement(EEnemyCombatAction MovementAction);
    FCombatSnapshot CaptureDecisionSnapshot(float DistanceToTarget) const;
    FEnemyActionScoringContext BuildScoringContext(
        const FCombatSnapshot& Snapshot
    ) const;
    bool TryExecuteHighestUtilityAction(
        const FEnemyActionScoringContext& Context
    );
    FPredictionResult PredictPlayerAction(
        const FCombatSnapshot& Snapshot
    ) const;
    bool ShouldApplyPrediction(const FPredictionResult& Prediction);
    bool IsTargetRecentlyHealing() const;
    void HandleOwnerDeath(UHealthComponent*, AActor*);
    double GetDecisionTimeSeconds() const;

    UPROPERTY(Transient)
    TObjectPtr<AActor> TargetActor;

    UPROPERTY(Transient)
    TObjectPtr<UEnemyCombatComponent> EnemyCombatComponent;

    UPROPERTY(Transient)
    TObjectPtr<UHealthComponent> OwnerHealthComponent;

    UPROPERTY(Transient)
    TObjectPtr<UPlayerBehaviorTrackerComponent> BehaviorTrackerComponent;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Baseline", meta = (ClampMin = "0.02"))
    float EvaluationInterval;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Baseline", meta = (ClampMin = "0.1"))
    float CombatDecisionInterval;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
    float RetreatDistance;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
    float StrafeDistance;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.1"))
    float StrafeSwitchInterval;

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

    UPROPERTY(EditDefaultsOnly, Category = "AI|Adaptation")
    int32 AdaptationRandomSeed;

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
    bool bLastPredictionApplied;

    bool bDecisionMakingEnabled;
    bool bAutomaticRetrainingEnabled;
    bool bPredictionUsageEnabled;
    bool bStrafeRight;
    double NextCombatDecisionTime;
    double NextStrafeSwitchTime;
    FEnemyActionScorer ActionScorer;
    TUniquePtr<IPlayerActionPredictor> Predictor;
    FRandomStream AdaptationRandomStream;
    FTimerHandle EvaluationTimer;
};
