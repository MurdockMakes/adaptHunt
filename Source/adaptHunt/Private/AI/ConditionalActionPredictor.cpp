#include "AI/ConditionalActionPredictor.h"

#include "adaptHunt.h"

namespace
{
const EPlayerCombatAction PredictableActions[] = {
    EPlayerCombatAction::LightAttack,
    EPlayerCombatAction::HeavyAttack,
    EPlayerCombatAction::DodgeLeft,
    EPlayerCombatAction::DodgeRight,
    EPlayerCombatAction::DodgeBackward,
    EPlayerCombatAction::Block,
    EPlayerCombatAction::Heal
};

int32 CountSamples(
    const TMap<EPlayerCombatAction, int32>* ActionCounts
)
{
    if (!ActionCounts)
    {
        return 0;
    }

    int32 Total = 0;
    for (const EPlayerCombatAction Action : PredictableActions)
    {
        Total += ActionCounts->FindRef(Action);
    }
    return Total;
}

FPredictionResult PredictFromCounts(
    const TMap<EPlayerCombatAction, int32>* ActionCounts,
    const int32 MinimumSampleCount,
    const EEnemyCombatAction EnemyAction,
    const bool bUseDistanceContext,
    const ECombatDistanceCategory DistanceCategory,
    const bool bUsePositionContext,
    const ERelativePlayerPosition RelativePlayerPosition,
    const bool bUsePreviousPlayerActionContext,
    const EPlayerCombatAction PreviousPlayerAction
)
{
    FPredictionResult Result;
    const int32 SampleCount = CountSamples(ActionCounts);
    if (!ActionCounts || SampleCount < MinimumSampleCount)
    {
        return Result;
    }

    int32 DominantCount = 0;
    for (const EPlayerCombatAction Action : PredictableActions)
    {
        const int32 Count = ActionCounts->FindRef(Action);
        if (Count > DominantCount)
        {
            Result.PredictedAction = Action;
            DominantCount = Count;
        }
    }

    if (DominantCount <= 0
        || Result.PredictedAction == EPlayerCombatAction::None)
    {
        return FPredictionResult();
    }

    Result.Confidence = FMath::Clamp(
        static_cast<float>(DominantCount)
            / static_cast<float>(SampleCount),
        0.0f,
        1.0f
    );
    Result.SupportingSampleCount = DominantCount;
    Result.bHasPrediction = true;
    Result.bUsedContext = true;
    Result.ConditioningEnemyAction = EnemyAction;
    Result.bUsedDistanceContext = bUseDistanceContext;
    Result.ConditioningDistanceCategory = DistanceCategory;
    Result.bUsedPositionContext = bUsePositionContext;
    Result.ConditioningRelativePlayerPosition = RelativePlayerPosition;
    Result.bUsedPreviousPlayerActionContext =
        bUsePreviousPlayerActionContext;
    Result.ConditioningPreviousPlayerAction = PreviousPlayerAction;
    return Result;
}
}

FConditionalActionPredictor::FConditionalActionPredictor(
    const int32 InMinimumConditionalSampleCount
)
    : TrainedSampleCount(0)
    , MinimumConditionalSampleCount(
        FMath::Max(1, InMinimumConditionalSampleCount)
    )
    , LastPredictionConfidence(0.0f)
{
}

void FConditionalActionPredictor::Train(const FCombatDataset& Dataset)
{
    Reset();
    GlobalPredictor.Train(Dataset);

    for (const FTrainingSample& Sample : Dataset.GetSamples())
    {
        if (!Sample.IsValid())
        {
            continue;
        }

        ++TrainedSampleCount;
        const EEnemyCombatAction EnemyAction =
            Sample.Snapshot.PreviousEnemyAction;
        if (EnemyAction == EEnemyCombatAction::None)
        {
            continue;
        }

        ++ConditionalActionCounts.FindOrAdd(EnemyAction).FindOrAdd(
            Sample.NextPlayerAction
        );
        ++DistanceConditionalActionCounts.FindOrAdd(EnemyAction)
            .FindOrAdd(Sample.Snapshot.DistanceCategory)
            .FindOrAdd(Sample.NextPlayerAction);
        ++PositionConditionalActionCounts.FindOrAdd(EnemyAction)
            .FindOrAdd(Sample.Snapshot.DistanceCategory)
            .FindOrAdd(Sample.Snapshot.RelativePlayerPosition)
            .FindOrAdd(Sample.NextPlayerAction);
        if (AdaptiveCombat::IsTrackablePlayerAction(
            Sample.Snapshot.PreviousPlayerAction
        ))
        {
            ++PreviousPlayerActionConditionalActionCounts.FindOrAdd(
                EnemyAction
            )
                .FindOrAdd(Sample.Snapshot.DistanceCategory)
                .FindOrAdd(Sample.Snapshot.RelativePlayerPosition)
                .FindOrAdd(Sample.Snapshot.PreviousPlayerAction)
                .FindOrAdd(Sample.NextPlayerAction);
        }
    }

    int32 DistanceContextCount = 0;
    for (const TPair<EEnemyCombatAction, FDistanceActionCounts>& Pair
        : DistanceConditionalActionCounts)
    {
        DistanceContextCount += Pair.Value.Num();
    }

    int32 PositionContextCount = 0;
    for (const TPair<EEnemyCombatAction, FDistancePositionActionCounts>& Pair
        : PositionConditionalActionCounts)
    {
        for (const TPair<ECombatDistanceCategory, FPositionActionCounts>&
            DistancePair : Pair.Value)
        {
            PositionContextCount += DistancePair.Value.Num();
        }
    }

    int32 PreviousPlayerActionContextCount = 0;
    for (const TPair<
        EEnemyCombatAction,
        FDistancePositionPreviousPlayerActionCounts
    >& Pair : PreviousPlayerActionConditionalActionCounts)
    {
        for (const TPair<
            ECombatDistanceCategory,
            FPositionPreviousPlayerActionCounts
        >& DistancePair : Pair.Value)
        {
            for (const TPair<
                ERelativePlayerPosition,
                FPreviousPlayerActionCounts
            >& PositionPair : DistancePair.Value)
            {
                PreviousPlayerActionContextCount += PositionPair.Value.Num();
            }
        }
    }

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT(
            "Conditional predictor trained on %d samples across %d "
            "enemy-action contexts, %d distance refinements, %d "
            "position refinements, and %d previous-player-action "
            "refinements."
        ),
        TrainedSampleCount,
        ConditionalActionCounts.Num(),
        DistanceContextCount,
        PositionContextCount,
        PreviousPlayerActionContextCount
    );
}

FPredictionResult FConditionalActionPredictor::Predict(
    const FCombatSnapshot& Snapshot
) const
{
    const EEnemyCombatAction EnemyAction = Snapshot.PreviousEnemyAction;
    const bool bKnownContext = EnemyAction != EEnemyCombatAction::None
        && AdaptiveCombat::IsKnownEnemyAction(EnemyAction);
    const bool bKnownDistance = AdaptiveCombat::IsKnownDistanceCategory(
        Snapshot.DistanceCategory
    );
    const bool bKnownPosition = AdaptiveCombat::IsKnownRelativePosition(
        Snapshot.RelativePlayerPosition
    );
    if (bKnownContext && bKnownDistance && bKnownPosition
        && AdaptiveCombat::IsTrackablePlayerAction(
            Snapshot.PreviousPlayerAction
        ))
    {
        const FDistancePositionPreviousPlayerActionCounts*
            DistancePositionPreviousPlayerActionCounts =
                PreviousPlayerActionConditionalActionCounts.Find(
                    EnemyAction
                );
        const FPositionPreviousPlayerActionCounts*
            PositionPreviousPlayerActionCounts =
                DistancePositionPreviousPlayerActionCounts
                    ? DistancePositionPreviousPlayerActionCounts->Find(
                        Snapshot.DistanceCategory
                    )
                    : nullptr;
        const FPreviousPlayerActionCounts* PreviousPlayerActionCounts =
            PositionPreviousPlayerActionCounts
                ? PositionPreviousPlayerActionCounts->Find(
                    Snapshot.RelativePlayerPosition
                )
                : nullptr;
        const FActionCounts* ExactCounts = PreviousPlayerActionCounts
            ? PreviousPlayerActionCounts->Find(
                Snapshot.PreviousPlayerAction
            )
            : nullptr;
        FPredictionResult ExactResult = PredictFromCounts(
            ExactCounts,
            MinimumConditionalSampleCount,
            EnemyAction,
            true,
            Snapshot.DistanceCategory,
            true,
            Snapshot.RelativePlayerPosition,
            true,
            Snapshot.PreviousPlayerAction
        );
        if (ExactResult.bHasPrediction)
        {
            LastPredictionConfidence = ExactResult.Confidence;
            return ExactResult;
        }
    }

    if (bKnownContext && bKnownDistance
        && bKnownPosition)
    {
        const FDistancePositionActionCounts* DistancePositionCounts =
            PositionConditionalActionCounts.Find(EnemyAction);
        const FPositionActionCounts* PositionCounts =
            DistancePositionCounts
                ? DistancePositionCounts->Find(Snapshot.DistanceCategory)
                : nullptr;
        const FActionCounts* ExactCounts = PositionCounts
            ? PositionCounts->Find(Snapshot.RelativePlayerPosition)
            : nullptr;
        FPredictionResult ExactResult = PredictFromCounts(
            ExactCounts,
            MinimumConditionalSampleCount,
            EnemyAction,
            true,
            Snapshot.DistanceCategory,
            true,
            Snapshot.RelativePlayerPosition,
            false,
            Snapshot.PreviousPlayerAction
        );
        if (ExactResult.bHasPrediction)
        {
            LastPredictionConfidence = ExactResult.Confidence;
            return ExactResult;
        }
    }

    if (bKnownContext
        && bKnownDistance)
    {
        const FDistanceActionCounts* DistanceCounts =
            DistanceConditionalActionCounts.Find(EnemyAction);
        const FActionCounts* ExactCounts = DistanceCounts
            ? DistanceCounts->Find(Snapshot.DistanceCategory)
            : nullptr;
        FPredictionResult ExactResult = PredictFromCounts(
            ExactCounts,
            MinimumConditionalSampleCount,
            EnemyAction,
            true,
            Snapshot.DistanceCategory,
            false,
            Snapshot.RelativePlayerPosition,
            false,
            Snapshot.PreviousPlayerAction
        );
        if (ExactResult.bHasPrediction)
        {
            LastPredictionConfidence = ExactResult.Confidence;
            return ExactResult;
        }
    }

    if (bKnownContext)
    {
        FPredictionResult EnemyActionResult = PredictFromCounts(
            ConditionalActionCounts.Find(EnemyAction),
            MinimumConditionalSampleCount,
            EnemyAction,
            false,
            Snapshot.DistanceCategory,
            false,
            Snapshot.RelativePlayerPosition,
            false,
            Snapshot.PreviousPlayerAction
        );
        if (EnemyActionResult.bHasPrediction)
        {
            LastPredictionConfidence = EnemyActionResult.Confidence;
            return EnemyActionResult;
        }
    }

    FPredictionResult Result = GlobalPredictor.Predict(Snapshot);
    Result.bUsedContext = false;
    Result.ConditioningEnemyAction = EEnemyCombatAction::None;
    Result.bUsedDistanceContext = false;
    Result.bUsedPositionContext = false;
    Result.bUsedPreviousPlayerActionContext = false;
    Result.ConditioningPreviousPlayerAction = EPlayerCombatAction::None;
    LastPredictionConfidence = Result.Confidence;
    return Result;
}

void FConditionalActionPredictor::Reset()
{
    GlobalPredictor.Reset();
    ConditionalActionCounts.Reset();
    DistanceConditionalActionCounts.Reset();
    PositionConditionalActionCounts.Reset();
    PreviousPlayerActionConditionalActionCounts.Reset();
    TrainedSampleCount = 0;
    LastPredictionConfidence = 0.0f;
}

float FConditionalActionPredictor::GetPredictionConfidence() const
{
    return LastPredictionConfidence;
}

int32 FConditionalActionPredictor::GetTrainedSampleCount() const
{
    return TrainedSampleCount;
}

int32 FConditionalActionPredictor::GetMinimumConditionalSampleCount() const
{
    return MinimumConditionalSampleCount;
}

int32 FConditionalActionPredictor::GetConditionalSampleCount(
    const EEnemyCombatAction EnemyAction
) const
{
    return CountSamples(ConditionalActionCounts.Find(EnemyAction));
}

int32 FConditionalActionPredictor::GetConditionalActionCount(
    const EEnemyCombatAction EnemyAction,
    const EPlayerCombatAction PlayerAction
) const
{
    const TMap<EPlayerCombatAction, int32>* Counts =
        ConditionalActionCounts.Find(EnemyAction);
    return Counts ? Counts->FindRef(PlayerAction) : 0;
}

int32 FConditionalActionPredictor::GetDistanceConditionalSampleCount(
    const EEnemyCombatAction EnemyAction,
    const ECombatDistanceCategory DistanceCategory
) const
{
    const FDistanceActionCounts* DistanceCounts =
        DistanceConditionalActionCounts.Find(EnemyAction);
    return CountSamples(
        DistanceCounts ? DistanceCounts->Find(DistanceCategory) : nullptr
    );
}

int32 FConditionalActionPredictor::GetDistanceConditionalActionCount(
    const EEnemyCombatAction EnemyAction,
    const ECombatDistanceCategory DistanceCategory,
    const EPlayerCombatAction PlayerAction
) const
{
    const FDistanceActionCounts* DistanceCounts =
        DistanceConditionalActionCounts.Find(EnemyAction);
    const FActionCounts* Counts = DistanceCounts
        ? DistanceCounts->Find(DistanceCategory)
        : nullptr;
    return Counts ? Counts->FindRef(PlayerAction) : 0;
}

int32 FConditionalActionPredictor::GetPositionConditionalSampleCount(
    const EEnemyCombatAction EnemyAction,
    const ECombatDistanceCategory DistanceCategory,
    const ERelativePlayerPosition RelativePlayerPosition
) const
{
    const FDistancePositionActionCounts* DistancePositionCounts =
        PositionConditionalActionCounts.Find(EnemyAction);
    const FPositionActionCounts* PositionCounts = DistancePositionCounts
        ? DistancePositionCounts->Find(DistanceCategory)
        : nullptr;
    return CountSamples(
        PositionCounts ? PositionCounts->Find(RelativePlayerPosition) : nullptr
    );
}

int32 FConditionalActionPredictor::GetPositionConditionalActionCount(
    const EEnemyCombatAction EnemyAction,
    const ECombatDistanceCategory DistanceCategory,
    const ERelativePlayerPosition RelativePlayerPosition,
    const EPlayerCombatAction PlayerAction
) const
{
    const FDistancePositionActionCounts* DistancePositionCounts =
        PositionConditionalActionCounts.Find(EnemyAction);
    const FPositionActionCounts* PositionCounts = DistancePositionCounts
        ? DistancePositionCounts->Find(DistanceCategory)
        : nullptr;
    const FActionCounts* Counts = PositionCounts
        ? PositionCounts->Find(RelativePlayerPosition)
        : nullptr;
    return Counts ? Counts->FindRef(PlayerAction) : 0;
}

int32 FConditionalActionPredictor::
    GetPreviousPlayerActionConditionalSampleCount(
        const EEnemyCombatAction EnemyAction,
        const ECombatDistanceCategory DistanceCategory,
        const ERelativePlayerPosition RelativePlayerPosition,
        const EPlayerCombatAction PreviousPlayerAction
    ) const
{
    const FDistancePositionPreviousPlayerActionCounts*
        DistancePositionPreviousPlayerActionCounts =
            PreviousPlayerActionConditionalActionCounts.Find(EnemyAction);
    const FPositionPreviousPlayerActionCounts*
        PositionPreviousPlayerActionCounts =
            DistancePositionPreviousPlayerActionCounts
                ? DistancePositionPreviousPlayerActionCounts->Find(
                    DistanceCategory
                )
                : nullptr;
    const FPreviousPlayerActionCounts* PreviousPlayerActionCounts =
        PositionPreviousPlayerActionCounts
            ? PositionPreviousPlayerActionCounts->Find(
                RelativePlayerPosition
            )
            : nullptr;
    return CountSamples(
        PreviousPlayerActionCounts
            ? PreviousPlayerActionCounts->Find(PreviousPlayerAction)
            : nullptr
    );
}

int32 FConditionalActionPredictor::
    GetPreviousPlayerActionConditionalActionCount(
        const EEnemyCombatAction EnemyAction,
        const ECombatDistanceCategory DistanceCategory,
        const ERelativePlayerPosition RelativePlayerPosition,
        const EPlayerCombatAction PreviousPlayerAction,
        const EPlayerCombatAction NextPlayerAction
    ) const
{
    const FDistancePositionPreviousPlayerActionCounts*
        DistancePositionPreviousPlayerActionCounts =
            PreviousPlayerActionConditionalActionCounts.Find(EnemyAction);
    const FPositionPreviousPlayerActionCounts*
        PositionPreviousPlayerActionCounts =
            DistancePositionPreviousPlayerActionCounts
                ? DistancePositionPreviousPlayerActionCounts->Find(
                    DistanceCategory
                )
                : nullptr;
    const FPreviousPlayerActionCounts* PreviousPlayerActionCounts =
        PositionPreviousPlayerActionCounts
            ? PositionPreviousPlayerActionCounts->Find(
                RelativePlayerPosition
            )
            : nullptr;
    const FActionCounts* Counts = PreviousPlayerActionCounts
        ? PreviousPlayerActionCounts->Find(PreviousPlayerAction)
        : nullptr;
    return Counts ? Counts->FindRef(NextPlayerAction) : 0;
}
