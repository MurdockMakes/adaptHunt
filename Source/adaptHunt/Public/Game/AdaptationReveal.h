#pragma once

#include "CoreMinimal.h"
#include "AI/AdaptiveTacticalProfile.h"
#include "Data/LearningTelemetry.h"

/** Player-facing role of each round in the learn-adapt-counter-adapt loop. */
enum class EAdaptiveRoundPresentationStage : uint8
{
    Learning,
    Adapting,
    Adapted
};

/** Immutable reveal copy captured at a completed-round boundary. */
struct ADAPTHUNT_API FAdaptiveRevealText
{
    FString Observation;
    FString Adjustment;
    bool bHasSupportedObservation = false;
    bool bHasActiveAdjustment = false;
};

/**
 * Pure formatting policy for the visible adaptation reveal.
 *
 * It only describes conditions present in the immutable predictor provenance
 * or the analyzed conditional pattern. No live or uncommitted input is read.
 */
struct ADAPTHUNT_API FAdaptiveAdaptationRevealPolicy
{
    static EAdaptiveRoundPresentationStage GetRoundStage(int32 RoundNumber);
    static FString FormatRoundStage(int32 RoundNumber);

    static FAdaptiveRevealText Build(
        const FAdaptiveConditionalPattern& ObservedPattern,
        const FAdaptiveTacticalProfile& TacticalProfile
    );
};
