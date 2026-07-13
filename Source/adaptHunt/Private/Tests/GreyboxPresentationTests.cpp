#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Characters/AdaptiveEnemyCharacter.h"
#include "Characters/AdaptivePlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/GreyboxPresentationComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Presentation/GreyboxPresentationTypes.h"
#include "UObject/UnrealType.h"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveGreyboxCueMappingTest,
    "adaptHunt.Milestone20.CueMapping",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveGreyboxCueMappingTest::RunTest(const FString& Parameters)
{
    using namespace AdaptiveGreyboxPresentation;

    TestEqual(
        TEXT("Player light attacks map into the shared presentation vocabulary"),
        FromPlayerAction(EPlayerCombatAction::LightAttack),
        EGreyboxPresentationAction::LightAttack
    );
    TestEqual(
        TEXT("Enemy interrupts retain a distinct presentation action"),
        FromEnemyAction(EEnemyCombatAction::InterruptHeal),
        EGreyboxPresentationAction::InterruptAttack
    );
    TestEqual(
        TEXT("Moving Idle state selects locomotion"),
        ResolveCue(
            ECombatActionPhase::Idle,
            EGreyboxPresentationAction::None,
            true
        ),
        EGreyboxAnimationCue::Locomotion
    );
    TestEqual(
        TEXT("Light windup is explicit"),
        ResolveCue(
            ECombatActionPhase::Windup,
            EGreyboxPresentationAction::LightAttack,
            false
        ),
        EGreyboxAnimationCue::LightWindup
    );
    TestEqual(
        TEXT("Heavy windup is distinct from light windup"),
        ResolveCue(
            ECombatActionPhase::Windup,
            EGreyboxPresentationAction::HeavyAttack,
            false
        ),
        EGreyboxAnimationCue::HeavyWindup
    );
    TestEqual(
        TEXT("Projectile windup is explicit"),
        ResolveCue(
            ECombatActionPhase::Windup,
            EGreyboxPresentationAction::ProjectileAttack,
            false
        ),
        EGreyboxAnimationCue::ProjectileWindup
    );
    TestEqual(
        TEXT("Dash Active state selects its burst"),
        ResolveCue(
            ECombatActionPhase::Active,
            EGreyboxPresentationAction::DashAttack,
            false
        ),
        EGreyboxAnimationCue::DashBurst
    );
    TestEqual(
        TEXT("Stagger overrides the previous action with a hit reaction"),
        ResolveCue(
            ECombatActionPhase::Staggered,
            EGreyboxPresentationAction::HeavyAttack,
            false
        ),
        EGreyboxAnimationCue::HitReaction
    );
    TestEqual(
        TEXT("Death overrides every action"),
        ResolveCue(
            ECombatActionPhase::Dead,
            EGreyboxPresentationAction::DashAttack,
            true
        ),
        EGreyboxAnimationCue::Death
    );
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveGreyboxTelegraphPoseTest,
    "adaptHunt.Milestone20.DistinctTelegraphPoses",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveGreyboxTelegraphPoseTest::RunTest(
    const FString& Parameters
)
{
    using namespace AdaptiveGreyboxPresentation;
    const FLinearColor Neutral(0.2f, 0.3f, 0.8f, 1.0f);
    const FGreyboxPresentationPose Light = EvaluatePose(
        EGreyboxAnimationCue::LightWindup,
        EGreyboxPresentationAction::LightAttack,
        Neutral,
        0.0f,
        0.0f,
        0.5f,
        1.0f,
        false
    );
    const FGreyboxPresentationPose Heavy = EvaluatePose(
        EGreyboxAnimationCue::HeavyWindup,
        EGreyboxPresentationAction::HeavyAttack,
        Neutral,
        0.0f,
        0.0f,
        0.5f,
        1.0f,
        false
    );
    const FGreyboxPresentationPose Projectile = EvaluatePose(
        EGreyboxAnimationCue::ProjectileWindup,
        EGreyboxPresentationAction::ProjectileAttack,
        Neutral,
        0.0f,
        0.0f,
        0.5f,
        1.0f,
        false
    );
    const FGreyboxPresentationPose Dash = EvaluatePose(
        EGreyboxAnimationCue::DashWindup,
        EGreyboxPresentationAction::DashAttack,
        Neutral,
        0.0f,
        0.0f,
        0.5f,
        1.0f,
        false
    );
    const FGreyboxPresentationPose Interrupt = EvaluatePose(
        EGreyboxAnimationCue::InterruptWindup,
        EGreyboxPresentationAction::InterruptAttack,
        Neutral,
        0.0f,
        0.0f,
        0.5f,
        1.0f,
        false
    );

    TestTrue(
        TEXT("Heavy anticipation is larger and farther back than light anticipation"),
        Heavy.ScaleMultiplier.X > Light.ScaleMultiplier.X
            && FMath::Abs(Heavy.LocationOffset.X)
                > FMath::Abs(Light.LocationOffset.X)
    );
    TestFalse(
        TEXT("Light and heavy anticipation colors are distinguishable"),
        Light.Color.Equals(Heavy.Color)
    );
    TestTrue(
        TEXT("Projectile windup grows vertically"),
        Projectile.ScaleMultiplier.Z > 1.0f
            && Projectile.LocationOffset.Z > 0.0f
    );
    TestTrue(
        TEXT("Dash anticipation has a crouched silhouette"),
        Dash.ScaleMultiplier.Z < 0.75f
            && Dash.LocationOffset.Z < 0.0f
    );
    TestTrue(
        TEXT("Interrupt anticipation is visibly asymmetric"),
        FMath::Abs(Interrupt.RotationOffset.Roll) > 20.0f
            && FMath::Abs(Interrupt.LocationOffset.Y) > 0.0f
    );
    TestTrue(
        TEXT("Heavy, projectile, dash, and interrupt all use distinct colors"),
        !Heavy.Color.Equals(Projectile.Color)
            && !Heavy.Color.Equals(Dash.Color)
            && !Heavy.Color.Equals(Interrupt.Color)
            && !Projectile.Color.Equals(Dash.Color)
            && !Projectile.Color.Equals(Interrupt.Color)
            && !Dash.Color.Equals(Interrupt.Color)
    );
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveGreyboxRequiredMotionTest,
    "adaptHunt.Milestone20.RequiredMotionAndFeedback",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveGreyboxRequiredMotionTest::RunTest(
    const FString& Parameters
)
{
    using namespace AdaptiveGreyboxPresentation;
    const FLinearColor Neutral(0.2f, 0.3f, 0.8f, 1.0f);
    const FGreyboxPresentationPose Idle = EvaluatePose(
        EGreyboxAnimationCue::Neutral,
        EGreyboxPresentationAction::None,
        Neutral,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        false
    );
    const FGreyboxPresentationPose Movement = EvaluatePose(
        EGreyboxAnimationCue::Locomotion,
        EGreyboxPresentationAction::None,
        Neutral,
        0.75f,
        0.60f,
        1.0f,
        1.0f,
        false
    );
    const FGreyboxPresentationPose LightStrike = EvaluatePose(
        EGreyboxAnimationCue::LightStrike,
        EGreyboxPresentationAction::LightAttack,
        Neutral,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        false
    );
    const FGreyboxPresentationPose HeavyStrike = EvaluatePose(
        EGreyboxAnimationCue::HeavyStrike,
        EGreyboxPresentationAction::HeavyAttack,
        Neutral,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        false
    );
    const FGreyboxPresentationPose Block = EvaluatePose(
        EGreyboxAnimationCue::Block,
        EGreyboxPresentationAction::Block,
        Neutral,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        false
    );
    const FGreyboxPresentationPose Dodge = EvaluatePose(
        EGreyboxAnimationCue::Dodge,
        EGreyboxPresentationAction::DodgeLeft,
        Neutral,
        0.0f,
        -1.0f,
        0.0f,
        1.0f,
        false
    );
    const FGreyboxPresentationPose Hit = EvaluatePose(
        EGreyboxAnimationCue::HitReaction,
        EGreyboxPresentationAction::None,
        Neutral,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        false
    );
    const FGreyboxPresentationPose Death = EvaluatePose(
        EGreyboxAnimationCue::Death,
        EGreyboxPresentationAction::None,
        Neutral,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        false
    );

    TestTrue(
        TEXT("Idle has a subtle breathing offset and scale"),
        Idle.LocationOffset.Z > 0.0f
            && Idle.ScaleMultiplier.Z > 1.0f
    );
    TestTrue(
        TEXT("Movement provides forward and strafe lean plus a step"),
        Movement.LocationOffset.Z > 0.0f
            && !FMath::IsNearlyZero(Movement.RotationOffset.Pitch)
            && !FMath::IsNearlyZero(Movement.RotationOffset.Roll)
    );
    TestTrue(
        TEXT("Light and heavy strikes both extend forward and remain distinct"),
        LightStrike.LocationOffset.X > 0.0f
            && HeavyStrike.LocationOffset.X > LightStrike.LocationOffset.X
            && !LightStrike.Color.Equals(HeavyStrike.Color)
    );
    TestTrue(
        TEXT("Block broadens the guard silhouette"),
        Block.ScaleMultiplier.Y > 1.25f
            && Block.ScaleMultiplier.X < 1.0f
    );
    TestTrue(
        TEXT("Dodge squashes and leans laterally"),
        Dodge.ScaleMultiplier.Z < 0.75f
            && FMath::Abs(Dodge.RotationOffset.Roll) > 20.0f
    );
    TestTrue(
        TEXT("Hit reaction recoils from neutral"),
        Hit.LocationOffset.X < 0.0f
            && FMath::Abs(Hit.RotationOffset.Roll) > 0.0f
    );
    TestTrue(
        TEXT("Death completes a visible fall and collapse"),
        Death.LocationOffset.Z < -50.0f
            && FMath::Abs(Death.RotationOffset.Roll) > 80.0f
    );

    const FLinearColor HitFlash = ApplyFeedbackColor(
        Neutral,
        true,
        false,
        0.0f
    );
    const FLinearColor Invulnerable = ApplyFeedbackColor(
        Neutral,
        false,
        true,
        1.0f
    );
    TestTrue(TEXT("Hit flash is briefly white"), HitFlash.Equals(FLinearColor::White));
    TestFalse(
        TEXT("Invulnerability feedback changes the current cue color"),
        Invulnerable.Equals(Neutral)
    );

    const FGreyboxPresentationPose Predicted = EvaluatePose(
        EGreyboxAnimationCue::HeavyWindup,
        EGreyboxPresentationAction::HeavyAttack,
        Neutral,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        true
    );
    const FGreyboxPresentationPose Ordinary = EvaluatePose(
        EGreyboxAnimationCue::HeavyWindup,
        EGreyboxPresentationAction::HeavyAttack,
        Neutral,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        false
    );
    TestFalse(
        TEXT("The optional prediction cue is subtle but visible"),
        Predicted.Color.Equals(Ordinary.Color)
    );
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveGreyboxIsolationDefaultsTest,
    "adaptHunt.Milestone20.IsolationResetAndHooks",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveGreyboxIsolationDefaultsTest::RunTest(
    const FString& Parameters
)
{
    const AAdaptivePlayerCharacter* Player =
        GetDefault<AAdaptivePlayerCharacter>();
    const AAdaptiveEnemyCharacter* Enemy =
        GetDefault<AAdaptiveEnemyCharacter>();
    const UGreyboxPresentationComponent* PlayerPresentation = Player
        ? Player->GetGreyboxPresentationComponent()
        : nullptr;
    const UGreyboxPresentationComponent* EnemyPresentation = Enemy
        ? Enemy->GetGreyboxPresentationComponent()
        : nullptr;
    TestNotNull(TEXT("Player owns an isolated presentation component"), PlayerPresentation);
    TestNotNull(TEXT("Enemy owns an isolated presentation component"), EnemyPresentation);
    if (!Player || !Enemy || !PlayerPresentation || !EnemyPresentation)
    {
        return false;
    }

    TestTrue(
        TEXT("Presentation defaults enabled for both combatants"),
        PlayerPresentation->IsPresentationEnabled()
            && EnemyPresentation->IsPresentationEnabled()
    );
    TestTrue(
        TEXT("Player and enemy use different neutral roles and colors"),
        PlayerPresentation->GetPresentationRole()
                == EGreyboxPresentationRole::Player
            && EnemyPresentation->GetPresentationRole()
                == EGreyboxPresentationRole::Enemy
            && !PlayerPresentation->GetNeutralColor().Equals(
                EnemyPresentation->GetNeutralColor()
            )
    );
    TestTrue(
        TEXT("Presentation targets only each collision-free visible body"),
        PlayerPresentation->GetPresentationMesh() == Player->GetBodyMesh()
            && EnemyPresentation->GetPresentationMesh() == Enemy->GetBodyMesh()
            && Player->GetBodyMesh()->GetCollisionEnabled()
                == ECollisionEnabled::NoCollision
            && Enemy->GetBodyMesh()->GetCollisionEnabled()
                == ECollisionEnabled::NoCollision
    );
    TestTrue(
        TEXT("Capsules remain authoritative while body presentation is isolated"),
        Player->GetCapsuleComponent()->GetCollisionEnabled()
                != ECollisionEnabled::NoCollision
            && Enemy->GetCapsuleComponent()->GetCollisionEnabled()
                != ECollisionEnabled::NoCollision
    );
    TestFalse(
        TEXT("Prediction cue is debug-only and disabled by default"),
        EnemyPresentation->IsPredictionDebugCueEnabled()
    );
    TestNotNull(
        TEXT("Future skeletal animation hooks are Blueprint-assignable"),
        FindFProperty<FMulticastDelegateProperty>(
            UGreyboxPresentationComponent::StaticClass(),
            GET_MEMBER_NAME_CHECKED(
                UGreyboxPresentationComponent,
                OnAnimationCue
            )
        )
    );

    const FLinearColor Neutral(0.2f, 0.3f, 0.8f, 1.0f);
    const FGreyboxPresentationPose ResetPose =
        AdaptiveGreyboxPresentation::EvaluatePose(
            EGreyboxAnimationCue::Neutral,
            EGreyboxPresentationAction::None,
            Neutral,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            false
        );
    TestTrue(
        TEXT("Reset target is exactly the captured neutral transform and color"),
        ResetPose.LocationOffset.IsNearlyZero()
            && ResetPose.RotationOffset.IsNearlyZero()
            && ResetPose.ScaleMultiplier.Equals(FVector::OneVector)
            && ResetPose.Color.Equals(Neutral)
    );
    return true;
}

#endif
