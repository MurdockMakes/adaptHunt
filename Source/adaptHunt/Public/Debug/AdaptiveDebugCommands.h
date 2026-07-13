#pragma once

#include "CoreMinimal.h"
#include "AI/AdaptiveTacticalProfile.h"
#include "Data/CombatTypes.h"


enum class EAdaptiveDebugProfilePreset : uint8
{
    Clear,
    Blocking,
    Healing,
    LightAttacks,
    HeavyAttacks,
    DodgeBackward,
    DodgeLeft,
    DodgeRight
};


enum class EAdaptiveDebugRoundTransition : uint8
{
    Start,
    PlayerWins,
    EnemyWins,
    Advance,
    Restart
};


/** Pure parsing and preset construction shared by console commands and tests. */
struct ADAPTHUNT_API FAdaptiveDebugCommandPolicy
{
    static bool TryParseProfilePreset(
        const FString& Value,
        EAdaptiveDebugProfilePreset& OutPreset
    );

    static bool TryParseActionPhase(
        const FString& Value,
        ECombatActionPhase& OutPhase
    );

    static bool TryParseRoundTransition(
        const FString& Value,
        EAdaptiveDebugRoundTransition& OutTransition
    );

    static FAdaptiveTacticalProfile BuildForcedTacticalProfile(
        EAdaptiveDebugProfilePreset Preset,
        const FAdaptiveTacticalProfileTuning& Tuning
    );
};
