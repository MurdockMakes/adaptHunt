#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Characters/AdaptiveEnemyCharacter.h"
#include "Characters/AdaptivePlayerCharacter.h"
#include "Components/CombatFeedbackComponent.h"
#include "Components/GreyboxPresentationComponent.h"
#include "Game/RoundManager.h"
#include "Presentation/CombatFeedbackTypes.h"
#include "UI/AdaptiveDebugHUD.h"

#include <limits>


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCombatFeedbackPulseTest,
    "adaptHunt.Milestone23.DistinctImpactPulses",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCombatFeedbackPulseTest::RunTest(const FString& Parameters)
{
    using namespace AdaptiveCombatFeedback;
    const FCombatFeedbackPulse Hit = EvaluatePulse(
        ECombatFeedbackCue::Hit,
        0.5f
    );
    const FCombatFeedbackPulse Blocked = EvaluatePulse(
        ECombatFeedbackCue::Blocked,
        0.5f
    );
    const FCombatFeedbackPulse Dodged = EvaluatePulse(
        ECombatFeedbackCue::Dodged,
        0.5f
    );
    const FCombatFeedbackPulse Miss = EvaluatePulse(
        ECombatFeedbackCue::Miss,
        0.5f
    );

    TestTrue(
        TEXT("Hits use an asymmetric impact pulse"),
        !Hit.ScaleMultiplier.Equals(FVector::OneVector)
    );
    TestTrue(
        TEXT("Blocks broaden more than ordinary hits"),
        Blocked.ScaleMultiplier.Y > Hit.ScaleMultiplier.Y
    );
    TestTrue(
        TEXT("Dodges squash vertically and use a unique tint"),
        Dodged.ScaleMultiplier.Z < 1.0f
            && !Dodged.TintColor.Equals(Hit.TintColor)
            && !Dodged.TintColor.Equals(Blocked.TintColor)
    );
    TestTrue(
        TEXT("Misses shrink and use a unique amber cue"),
        Miss.ScaleMultiplier.X < 1.0f
            && !Miss.TintColor.Equals(Hit.TintColor)
    );
    TestTrue(
        TEXT("Death cannot be overwritten by a lower-priority cue"),
        GetCuePriority(ECombatFeedbackCue::Death)
            > GetCuePriority(ECombatFeedbackCue::Hit)
            && GetCuePriority(ECombatFeedbackCue::Hit)
                > GetCuePriority(ECombatFeedbackCue::Miss)
    );
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCombatFeedbackImpulseTest,
    "adaptHunt.Milestone23.BoundedKnockbackAndCameraImpulses",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCombatFeedbackImpulseTest::RunTest(
    const FString& Parameters
)
{
    using namespace AdaptiveCombatFeedback;
    const FVector Knockback = ResolveKnockbackVelocity(
        FVector(100.0f, 0.0f, 40.0f),
        FVector::ZeroVector,
        FVector::RightVector,
        900.0f,
        35.0f,
        420.0f
    );
    TestTrue(
        TEXT("Horizontal knockback points away from the instigator"),
        Knockback.X > 0.0f && FMath::IsNearlyZero(Knockback.Y)
    );
    TestTrue(
        TEXT("Knockback is hard-capped and keeps modest vertical lift"),
        FMath::IsNearlyEqual(Knockback.Size2D(), 420.0f)
            && FMath::IsNearlyEqual(Knockback.Z, 35.0f)
    );
    const FVector Fallback = ResolveKnockbackVelocity(
        FVector::ZeroVector,
        FVector::ZeroVector,
        FVector::RightVector,
        200.0f,
        20.0f,
        420.0f
    );
    TestTrue(
        TEXT("Coincident combatants use a deterministic fallback direction"),
        Fallback.GetSafeNormal2D().Equals(FVector::RightVector)
    );
    const FVector Invalid = ResolveKnockbackVelocity(
        FVector::ZeroVector,
        FVector::ZeroVector,
        FVector::ZeroVector,
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN()
    );
    TestTrue(
        TEXT("Invalid tuning produces a finite zero-strength result"),
        !Invalid.ContainsNaN()
            && FMath::IsNearlyZero(Invalid.Size())
    );

    TestTrue(
        TEXT("Light, heavy, received-hit, and dodge camera impulses exist"),
        !ResolveCameraImpulse(ECombatFeedbackCue::LightAttack).IsNearlyZero()
            && !ResolveCameraImpulse(ECombatFeedbackCue::HeavyAttack).IsNearlyZero()
            && !ResolveCameraImpulse(ECombatFeedbackCue::Hit).IsNearlyZero()
            && !ResolveCameraImpulse(ECombatFeedbackCue::Dodged).IsNearlyZero()
    );
    TestTrue(
        TEXT("Heavy camera impulse exceeds the light impulse"),
        ResolveCameraImpulse(ECombatFeedbackCue::HeavyAttack).Size()
            > ResolveCameraImpulse(ECombatFeedbackCue::LightAttack).Size()
    );
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCombatFeedbackComponentTest,
    "adaptHunt.Milestone23.ComponentIsolationAndReset",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCombatFeedbackComponentTest::RunTest(
    const FString& Parameters
)
{
    const AAdaptivePlayerCharacter* Player =
        GetDefault<AAdaptivePlayerCharacter>();
    const AAdaptiveEnemyCharacter* Enemy =
        GetDefault<AAdaptiveEnemyCharacter>();
    const UCombatFeedbackComponent* PlayerFeedback = Player
        ? Player->GetCombatFeedbackComponent()
        : nullptr;
    const UCombatFeedbackComponent* EnemyFeedback = Enemy
        ? Enemy->GetCombatFeedbackComponent()
        : nullptr;
    TestNotNull(TEXT("Player owns feedback"), PlayerFeedback);
    TestNotNull(TEXT("Enemy owns feedback"), EnemyFeedback);
    if (!PlayerFeedback || !EnemyFeedback)
    {
        return false;
    }
    TestTrue(
        TEXT("Both combatants have configurable bounded knockback"),
        PlayerFeedback->IsKnockbackEnabled()
            && EnemyFeedback->IsKnockbackEnabled()
            && PlayerFeedback->GetKnockbackStrength() > 0.0f
            && PlayerFeedback->GetMaximumKnockbackStrength()
                >= PlayerFeedback->GetKnockbackStrength()
            && PlayerFeedback->GetHeavyKnockbackMultiplier() > 1.0f
            && PlayerFeedback->GetBlockedKnockbackMultiplier() < 1.0f
    );
    TestTrue(
        TEXT("Feedback visibility is brief"),
        PlayerFeedback->GetFeedbackDisplayDuration() <= 0.5f
    );
    TestTrue(
        TEXT("Feedback does not replace authoritative presentation or collision"),
        Player->GetGreyboxPresentationComponent()
            && Enemy->GetGreyboxPresentationComponent()
    );

    UCombatFeedbackComponent* TransientFeedback =
        NewObject<UCombatFeedbackComponent>();
    TransientFeedback->NotifyAttackMissed();
    TestEqual(
        TEXT("A miss becomes immediately readable"),
        TransientFeedback->GetActiveCue(),
        ECombatFeedbackCue::Miss
    );
    TransientFeedback->ResetFeedback();
    TestEqual(
        TEXT("Reset clears transient feedback without a timer"),
        TransientFeedback->GetActiveCue(),
        ECombatFeedbackCue::None
    );
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCombatReadabilityHudTest,
    "adaptHunt.Milestone23.ResourceAndRoundReadability",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCombatReadabilityHudTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("Resource values normalize and clamp safely"),
        FMath::IsNearlyEqual(
            AAdaptiveDebugHUD::NormalizeResource(25.0f, 100.0f),
            0.25f
        )
            && FMath::IsNearlyEqual(
                AAdaptiveDebugHUD::NormalizeResource(150.0f, 100.0f),
                1.0f
            )
            && FMath::IsNearlyZero(
                AAdaptiveDebugHUD::NormalizeResource(
                    std::numeric_limits<float>::quiet_NaN(),
                    100.0f
                )
            )
    );
    TestTrue(
        TEXT("Round 1 clearly identifies live evidence gathering"),
        AAdaptiveDebugHUD::FormatPredictionStatus(1, false).Contains(
            TEXT("LIVE LEARNING")
        )
            && AAdaptiveDebugHUD::FormatPredictionStatus(1, false).Contains(
                TEXT("GATHERING EVIDENCE")
            )
    );
    TestTrue(
        TEXT("Later rounds clearly show enabled predictions"),
        AAdaptiveDebugHUD::FormatPredictionStatus(2, true).Contains(
            TEXT("PREDICTIONS ENABLED")
        )
    );
    TestTrue(
        TEXT("Intermission status contains round, phase, and countdown"),
        AAdaptiveDebugHUD::FormatRoundStatus(
            1,
            3,
            EAdaptiveRoundPhase::Intermission,
            2.45f
        ).Contains(TEXT("ROUND 1 / 3"))
            && AAdaptiveDebugHUD::FormatRoundStatus(
                1,
                3,
                EAdaptiveRoundPhase::Intermission,
                2.45f
            ).Contains(TEXT("INTERMISSION"))
            && AAdaptiveDebugHUD::FormatRoundStatus(
                1,
                3,
                EAdaptiveRoundPhase::Intermission,
                2.45f
            ).Contains(TEXT("2.5"))
    );
    const AAdaptiveDebugHUD* Hud = GetDefault<AAdaptiveDebugHUD>();
    TestTrue(
        TEXT("Detailed adaptive telemetry is hidden by default"),
        Hud && !Hud->IsDetailedAdaptiveDebugVisible()
    );
    return true;
}

#endif
