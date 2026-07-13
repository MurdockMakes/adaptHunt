#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Characters/AdaptivePlayerCharacter.h"
#include "Combat/PlayerResponsivenessPolicy.h"
#include "Components/CombatComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputMappingContext.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptivePlayerResponsivenessDefaultsTest,
    "adaptHunt.Milestone21.MovementAndCameraDefaults",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptivePlayerResponsivenessDefaultsTest::RunTest(
    const FString& Parameters
)
{
    const AAdaptivePlayerCharacter* Player =
        GetDefault<AAdaptivePlayerCharacter>();
    TestNotNull(TEXT("The player CDO exists"), Player);
    if (!Player)
    {
        return false;
    }

    const UCharacterMovementComponent* Movement =
        Player->GetCharacterMovement();
    TestNotNull(TEXT("Character movement exists"), Movement);
    if (Movement)
    {
        TestTrue(
            TEXT("Acceleration is responsive without being instantaneous"),
            Movement->MaxAcceleration >= 2200.0f
                && Movement->MaxAcceleration <= 3200.0f
        );
        TestTrue(
            TEXT("Walking braking prevents excess sliding"),
            Movement->BrakingDecelerationWalking >= 2200.0f
                && Movement->bUseSeparateBrakingFriction
                && Movement->BrakingFriction > 0.0f
        );
        TestTrue(
            TEXT("Travel-facing rotation is quick but interpolated"),
            Movement->bOrientRotationToMovement
                && Movement->RotationRate.Yaw >= 800.0f
        );
        TestTrue(
            TEXT("Air control is useful but remains bounded"),
            Movement->AirControl >= 0.35f
                && Movement->AirControl <= 0.5f
        );
        TestTrue(
            TEXT("Low analog input preserves a slow movement range"),
            Movement->MinAnalogWalkSpeed > 0.0f
                && Movement->MinAnalogWalkSpeed <= 10.0f
        );
    }

    const USpringArmComponent* CameraBoom = Player->GetCameraBoom();
    TestNotNull(TEXT("The camera boom exists"), CameraBoom);
    if (CameraBoom)
    {
        TestTrue(
            TEXT("Subtle position lag is enabled"),
            CameraBoom->bEnableCameraLag
                && CameraBoom->CameraLagSpeed >= 15.0f
                && CameraBoom->CameraLagMaxDistance <= 30.0f
        );
        TestTrue(
            TEXT("Responsive camera rotation smoothing is enabled"),
            CameraBoom->bEnableCameraRotationLag
                && CameraBoom->CameraRotationLagSpeed >= 15.0f
        );
        TestTrue(
            TEXT("Camera lag is sub-stepped for frame-rate stability"),
            CameraBoom->bUseCameraLagSubstepping
        );
        TestTrue(
            TEXT("The exposed position-lag tuning drives the boom"),
            FMath::IsNearlyEqual(
                CameraBoom->CameraLagSpeed,
                Player->GetCameraPositionLagSpeed()
            )
        );
        TestTrue(
            TEXT("The exposed rotation smoothing drives the boom"),
            FMath::IsNearlyEqual(
                CameraBoom->CameraRotationLagSpeed,
                Player->GetCameraRotationSmoothingSpeed()
            )
        );
    }

    const UInputMappingContext* MappingContext =
        Player->GetDefaultMappingContext();
    TestTrue(
        TEXT("Keyboard, mouse, gamepad, and restart mappings are preserved"),
        MappingContext && MappingContext->GetMappings().Num() == 28
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptivePlayerDodgeDirectionTest,
    "adaptHunt.Milestone21.CameraRelativeDodgeDirection",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptivePlayerDodgeDirectionTest::RunTest(
    const FString& Parameters
)
{
    const FVector ActorForward = FVector::ForwardVector;
    const FVector ActorRight = FVector::RightVector;
    const FVector CameraForward = FVector::RightVector;
    const FVector CameraRight = -FVector::ForwardVector;

    const FVector Left =
        AdaptivePlayerResponsiveness::ResolveDodgeDirection(
            EPlayerCombatAction::DodgeLeft,
            ActorForward,
            ActorRight,
            CameraForward,
            CameraRight,
            true
        );
    const FVector Right =
        AdaptivePlayerResponsiveness::ResolveDodgeDirection(
            EPlayerCombatAction::DodgeRight,
            ActorForward,
            ActorRight,
            CameraForward,
            CameraRight,
            true
        );
    const FVector Backward =
        AdaptivePlayerResponsiveness::ResolveDodgeDirection(
            EPlayerCombatAction::DodgeBackward,
            ActorForward,
            ActorRight,
            CameraForward,
            CameraRight,
            true
        );
    TestTrue(
        TEXT("Left dodge follows camera-left"),
        Left.Equals(FVector::ForwardVector)
    );
    TestTrue(
        TEXT("Right dodge follows camera-right"),
        Right.Equals(-FVector::ForwardVector)
    );
    TestTrue(
        TEXT("Backward dodge follows camera-back"),
        Backward.Equals(-FVector::RightVector)
    );
    TestTrue(
        TEXT("Resolved dodge directions stay horizontal and normalized"),
        FMath::IsNearlyZero(Left.Z)
            && FMath::IsNearlyEqual(Left.Size(), 1.0f)
            && FMath::IsNearlyZero(Right.Z)
            && FMath::IsNearlyEqual(Backward.Size(), 1.0f)
    );

    const FVector ActorRelativeLeft =
        AdaptivePlayerResponsiveness::ResolveDodgeDirection(
            EPlayerCombatAction::DodgeLeft,
            ActorForward,
            ActorRight,
            FVector::ZeroVector,
            FVector::ZeroVector,
            false
        );
    TestTrue(
        TEXT("Missing camera data falls back to player-relative axes"),
        ActorRelativeLeft.Equals(-ActorRight)
    );
    TestTrue(
        TEXT("Non-dodge actions never produce launch direction"),
        AdaptivePlayerResponsiveness::ResolveDodgeDirection(
            EPlayerCombatAction::LightAttack,
            ActorForward,
            ActorRight,
            CameraForward,
            CameraRight,
            true
        ).IsNearlyZero()
    );
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptivePlayerCombatInputBufferTest,
    "adaptHunt.Milestone21.SingleCombatInputBuffer",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptivePlayerCombatInputBufferTest::RunTest(
    const FString& Parameters
)
{
    TestTrue(
        TEXT("Input inside the final recovery window is bufferable"),
        AdaptivePlayerResponsiveness::CanBufferDuringRecovery(
            ECombatActionPhase::Recovery,
            0.12f,
            0.14f,
            true,
            true
        )
    );
    TestFalse(
        TEXT("Input earlier in recovery is not bufferable"),
        AdaptivePlayerResponsiveness::CanBufferDuringRecovery(
            ECombatActionPhase::Recovery,
            0.2f,
            0.14f,
            true,
            true
        )
    );
    TestFalse(
        TEXT("Windup cannot be canceled through the buffer"),
        AdaptivePlayerResponsiveness::CanBufferDuringRecovery(
            ECombatActionPhase::Windup,
            0.05f,
            0.14f,
            true,
            true
        )
    );
    TestFalse(
        TEXT("Combat lock rejects buffered input"),
        AdaptivePlayerResponsiveness::CanBufferDuringRecovery(
            ECombatActionPhase::Recovery,
            0.05f,
            0.14f,
            false,
            true
        )
    );
    TestFalse(
        TEXT("Death rejects buffered input"),
        AdaptivePlayerResponsiveness::CanBufferDuringRecovery(
            ECombatActionPhase::Recovery,
            0.05f,
            0.14f,
            true,
            false
        )
    );

    FPlayerCombatInputBuffer Buffer;
    TestTrue(
        TEXT("The first eligible action occupies the buffer"),
        Buffer.TryStore(EPlayerCombatAction::LightAttack, 10.0)
    );
    TestTrue(
        TEXT("Repeated input is accepted without a duplicate entry"),
        Buffer.TryStore(EPlayerCombatAction::LightAttack, 20.0)
    );
    TestTrue(
        TEXT("Repeated input does not refresh the original request"),
        FMath::IsNearlyEqual(Buffer.GetExpirationTime(), 10.0)
    );
    TestFalse(
        TEXT("A second action cannot replace the buffered action"),
        Buffer.TryStore(EPlayerCombatAction::HeavyAttack, 10.0)
    );
    TestEqual(
        TEXT("The valid buffered action is consumed exactly once"),
        Buffer.ConsumeIfValid(9.0),
        EPlayerCombatAction::LightAttack
    );
    TestEqual(
        TEXT("A second consume cannot duplicate the commit"),
        Buffer.ConsumeIfValid(9.0),
        EPlayerCombatAction::None
    );
    TestTrue(
        TEXT("Cancellation clears pending input"),
        Buffer.TryStore(EPlayerCombatAction::DodgeLeft, 20.0)
    );
    Buffer.Clear();
    TestFalse(TEXT("Cancellation leaves no action"), Buffer.HasAction());
    Buffer.TryStore(EPlayerCombatAction::HeavyAttack, 1.0);
    TestEqual(
        TEXT("Expired input is discarded instead of committed"),
        Buffer.ConsumeIfValid(2.0),
        EPlayerCombatAction::None
    );

    const UCombatComponent* Combat = GetDefault<UCombatComponent>();
    TestNotNull(TEXT("Player combat CDO exists"), Combat);
    if (Combat)
    {
        TestTrue(
            TEXT("Default buffer duration is between 100 and 180 ms"),
            Combat->GetInputBufferDuration() >= 0.1f
                && Combat->GetInputBufferDuration() <= 0.18f
        );
        TestFalse(
            TEXT("Combat starts without stale buffered input"),
            Combat->HasBufferedInput()
        );
        TestFalse(
            TEXT("Timer-driven buffering does not add component Tick"),
            Combat->PrimaryComponentTick.bCanEverTick
        );
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptivePlayerMeleeFacingAssistTest,
    "adaptHunt.Milestone21.MeleeFacingAssist",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptivePlayerMeleeFacingAssistTest::RunTest(
    const FString& Parameters
)
{
    float Correction = 0.0f;
    const FVector TargetAtThirtyDegrees = FRotator(
        0.0f,
        30.0f,
        0.0f
    ).Vector();
    TestTrue(
        TEXT("A nearby target inside the cone receives assistance"),
        AdaptivePlayerResponsiveness::ComputeMeleeFacingCorrection(
            FVector::ForwardVector,
            TargetAtThirtyDegrees,
            45.0f,
            18.0f,
            Correction
        )
    );
    TestTrue(
        TEXT("Facing assistance is bounded to a modest correction"),
        FMath::IsNearlyEqual(Correction, 18.0f)
    );

    TestFalse(
        TEXT("Targets outside the configured cone are ignored"),
        AdaptivePlayerResponsiveness::ComputeMeleeFacingCorrection(
            FVector::ForwardVector,
            FRotator(0.0f, 60.0f, 0.0f).Vector(),
            45.0f,
            18.0f,
            Correction
        )
    );
    TestFalse(
        TEXT("Targets behind the player are never assisted"),
        AdaptivePlayerResponsiveness::ComputeMeleeFacingCorrection(
            FVector::ForwardVector,
            -FVector::ForwardVector,
            180.0f,
            180.0f,
            Correction
        )
    );

    const UCombatComponent* Combat = GetDefault<UCombatComponent>();
    TestTrue(
        TEXT("Melee facing assistance has safe configurable defaults"),
        Combat && Combat->IsMeleeFacingAssistEnabled()
            && Combat->GetMeleeFacingAssistDistance() > 0.0f
            && Combat->GetMeleeFacingAssistConeHalfAngle() < 90.0f
            && Combat->GetMeleeFacingAssistMaxCorrection()
                < Combat->GetMeleeFacingAssistConeHalfAngle()
    );
    return true;
}

#endif
