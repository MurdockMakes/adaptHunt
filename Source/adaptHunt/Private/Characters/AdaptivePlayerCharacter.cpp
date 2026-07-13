#include "Characters/AdaptivePlayerCharacter.h"

#include "adaptHunt.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/CombatComponent.h"
#include "Components/CombatFeedbackComponent.h"
#include "Components/CombatSnapshotComponent.h"
#include "Components/GreyboxPresentationComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PlayerBehaviorTrackerComponent.h"
#include "Components/StaminaComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Debug/CombatTargetDummy.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Game/AdaptiveGameMode.h"
#include "Game/AdaptiveHuntTuningSettings.h"
#include "Game/RoundManager.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "UI/AdaptiveDebugHUD.h"
#include "UObject/ConstructorHelpers.h"

AAdaptivePlayerCharacter::AAdaptivePlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    const FAdaptiveMovementTuning MovementTuning =
        UAdaptiveHuntTuningSettings::Get().Movement.GetSanitized();
    const FAdaptiveCombatBalanceTuning CombatBalance =
        UAdaptiveHuntTuningSettings::Get().CombatBalance.GetSanitized();
    MovementAcceleration = MovementTuning.PlayerAcceleration;
    WalkingBrakingDeceleration =
        MovementTuning.PlayerBrakingDeceleration;
    WalkingGroundFriction = MovementTuning.PlayerGroundFriction;
    WalkingBrakingFriction = MovementTuning.PlayerBrakingFriction;
    MovementRotationRateYaw = MovementTuning.PlayerRotationRateYaw;
    MovementAirControl = MovementTuning.PlayerAirControl;
    MinimumAnalogWalkSpeed =
        MovementTuning.PlayerMinimumAnalogWalkSpeed;

    const FAdaptiveCameraTuning CameraTuning =
        UAdaptiveHuntTuningSettings::Get().Camera.GetSanitized();
    bEnableCameraPositionLag = CameraTuning.bEnablePositionLag;
    CameraPositionLagSpeed = CameraTuning.PositionLagSpeed;
    CameraPositionLagMaxDistance = CameraTuning.PositionLagMaxDistance;
    bEnableCameraRotationSmoothing =
        CameraTuning.bEnableRotationSmoothing;
    CameraRotationSmoothingSpeed =
        CameraTuning.RotationSmoothingSpeed;

    GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = true;
    Movement->JumpZVelocity = MovementTuning.PlayerJumpVelocity;
    Movement->MaxWalkSpeed = MovementTuning.PlayerMaxWalkSpeed;

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(GetCapsuleComponent());
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.8f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyPrimitiveMesh(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder")
    );
    if (BodyPrimitiveMesh.Succeeded())
    {
        BodyMesh->SetStaticMesh(BodyPrimitiveMesh.Object);
    }

    HealthComponent = CreateDefaultSubobject<UHealthComponent>(
        TEXT("HealthComponent")
    );
    StaminaComponent = CreateDefaultSubobject<UStaminaComponent>(
        TEXT("StaminaComponent")
    );
    HealthComponent->SetMaxHealth(CombatBalance.PlayerMaxHealth);
    StaminaComponent->SetMaxStamina(CombatBalance.PlayerMaxStamina);
    CombatComponent = CreateDefaultSubobject<UCombatComponent>(
        TEXT("CombatComponent")
    );
    CombatFeedbackComponent =
        CreateDefaultSubobject<UCombatFeedbackComponent>(
            TEXT("CombatFeedbackComponent")
        );
    GreyboxPresentationComponent =
        CreateDefaultSubobject<UGreyboxPresentationComponent>(
            TEXT("GreyboxPresentationComponent")
        );
    GreyboxPresentationComponent->SetPresentationMesh(BodyMesh);
    GreyboxPresentationComponent->SetPresentationRole(
        EGreyboxPresentationRole::Player
    );
    CombatSnapshotComponent =
        CreateDefaultSubobject<UCombatSnapshotComponent>(
            TEXT("CombatSnapshotComponent")
        );
    BehaviorTrackerComponent =
        CreateDefaultSubobject<UPlayerBehaviorTrackerComponent>(
            TEXT("PlayerBehaviorTrackerComponent")
        );

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(
        TEXT("CameraBoom")
    );
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->SetRelativeLocation(
        FVector(0.0f, 0.0f, CameraTuning.BoomHeight)
    );
    CameraBoom->TargetArmLength = CameraTuning.TargetArmLength;
    CameraBoom->SocketOffset = FVector(
        0.0f,
        CameraTuning.SocketSideOffset,
        CameraTuning.SocketHeightOffset
    );
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(
        TEXT("FollowCamera")
    );
    FollowCamera->SetupAttachment(
        CameraBoom,
        USpringArmComponent::SocketName
    );
    FollowCamera->bUsePawnControlRotation = false;

    DefaultMappingContext = CreateDefaultSubobject<UInputMappingContext>(
        TEXT("DefaultMappingContext")
    );

    MoveForwardAction = CreateDefaultSubobject<UInputAction>(
        TEXT("MoveForwardAction")
    );
    MoveForwardAction->ValueType = EInputActionValueType::Axis1D;

    MoveRightAction = CreateDefaultSubobject<UInputAction>(
        TEXT("MoveRightAction")
    );
    MoveRightAction->ValueType = EInputActionValueType::Axis1D;

    LookYawAction = CreateDefaultSubobject<UInputAction>(
        TEXT("LookYawAction")
    );
    LookYawAction->ValueType = EInputActionValueType::Axis1D;

    LookPitchAction = CreateDefaultSubobject<UInputAction>(
        TEXT("LookPitchAction")
    );
    LookPitchAction->ValueType = EInputActionValueType::Axis1D;

    JumpAction = CreateDefaultSubobject<UInputAction>(TEXT("JumpAction"));
    JumpAction->ValueType = EInputActionValueType::Boolean;

    LightAttackAction = CreateDefaultSubobject<UInputAction>(
        TEXT("LightAttackAction")
    );
    LightAttackAction->ValueType = EInputActionValueType::Boolean;

    HeavyAttackAction = CreateDefaultSubobject<UInputAction>(
        TEXT("HeavyAttackAction")
    );
    HeavyAttackAction->ValueType = EInputActionValueType::Boolean;

    BlockAction = CreateDefaultSubobject<UInputAction>(TEXT("BlockAction"));
    BlockAction->ValueType = EInputActionValueType::Boolean;

    DodgeLeftAction = CreateDefaultSubobject<UInputAction>(
        TEXT("DodgeLeftAction")
    );
    DodgeLeftAction->ValueType = EInputActionValueType::Boolean;

    DodgeRightAction = CreateDefaultSubobject<UInputAction>(
        TEXT("DodgeRightAction")
    );
    DodgeRightAction->ValueType = EInputActionValueType::Boolean;

    DodgeBackwardAction = CreateDefaultSubobject<UInputAction>(
        TEXT("DodgeBackwardAction")
    );
    DodgeBackwardAction->ValueType = EInputActionValueType::Boolean;

    HealAction = CreateDefaultSubobject<UInputAction>(TEXT("HealAction"));
    HealAction->ValueType = EInputActionValueType::Boolean;

    RestartMatchAction = CreateDefaultSubobject<UInputAction>(
        TEXT("RestartMatchAction")
    );
    RestartMatchAction->ValueType = EInputActionValueType::Boolean;

    UInputModifierNegate* MoveBackwardModifier =
        CreateDefaultSubobject<UInputModifierNegate>(
            TEXT("MoveBackwardModifier")
        );
    UInputModifierNegate* MoveLeftModifier =
        CreateDefaultSubobject<UInputModifierNegate>(
            TEXT("MoveLeftModifier")
        );

    DefaultMappingContext->MapKey(MoveForwardAction, EKeys::W);
    DefaultMappingContext->MapKey(MoveForwardAction, EKeys::Gamepad_LeftY);
    FEnhancedActionKeyMapping& BackwardMapping =
        DefaultMappingContext->MapKey(MoveForwardAction, EKeys::S);
    BackwardMapping.Modifiers.Add(MoveBackwardModifier);

    DefaultMappingContext->MapKey(MoveRightAction, EKeys::D);
    DefaultMappingContext->MapKey(MoveRightAction, EKeys::Gamepad_LeftX);
    FEnhancedActionKeyMapping& LeftMapping =
        DefaultMappingContext->MapKey(MoveRightAction, EKeys::A);
    LeftMapping.Modifiers.Add(MoveLeftModifier);

    DefaultMappingContext->MapKey(LookYawAction, EKeys::MouseX);
    DefaultMappingContext->MapKey(LookYawAction, EKeys::Gamepad_RightX);
    DefaultMappingContext->MapKey(LookPitchAction, EKeys::MouseY);
    DefaultMappingContext->MapKey(LookPitchAction, EKeys::Gamepad_RightY);

    DefaultMappingContext->MapKey(JumpAction, EKeys::SpaceBar);
    DefaultMappingContext->MapKey(
        JumpAction,
        EKeys::Gamepad_FaceButton_Bottom
    );

    DefaultMappingContext->MapKey(LightAttackAction, EKeys::LeftMouseButton);
    DefaultMappingContext->MapKey(
        LightAttackAction,
        EKeys::Gamepad_RightTriggerAxis
    );
    DefaultMappingContext->MapKey(HeavyAttackAction, EKeys::F);
    DefaultMappingContext->MapKey(
        HeavyAttackAction,
        EKeys::Gamepad_FaceButton_Top
    );
    DefaultMappingContext->MapKey(BlockAction, EKeys::RightMouseButton);
    DefaultMappingContext->MapKey(
        BlockAction,
        EKeys::Gamepad_LeftTriggerAxis
    );
    DefaultMappingContext->MapKey(DodgeLeftAction, EKeys::Q);
    DefaultMappingContext->MapKey(
        DodgeLeftAction,
        EKeys::Gamepad_DPad_Left
    );
    DefaultMappingContext->MapKey(DodgeRightAction, EKeys::E);
    DefaultMappingContext->MapKey(
        DodgeRightAction,
        EKeys::Gamepad_DPad_Right
    );
    DefaultMappingContext->MapKey(DodgeBackwardAction, EKeys::LeftShift);
    DefaultMappingContext->MapKey(
        DodgeBackwardAction,
        EKeys::Gamepad_FaceButton_Right
    );
    DefaultMappingContext->MapKey(HealAction, EKeys::R);
    DefaultMappingContext->MapKey(HealAction, EKeys::Gamepad_DPad_Up);
    DefaultMappingContext->MapKey(RestartMatchAction, EKeys::Enter);
    DefaultMappingContext->MapKey(
        RestartMatchAction,
        EKeys::Gamepad_Special_Right
    );

    ApplyResponsivenessTuning();
}

void AAdaptivePlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    ApplyResponsivenessTuning();
}

void AAdaptivePlayerCharacter::ApplyResponsivenessTuning()
{
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->MaxAcceleration = FMath::Max(
            0.0f,
            MovementAcceleration
        );
        Movement->BrakingDecelerationWalking = FMath::Max(
            0.0f,
            WalkingBrakingDeceleration
        );
        Movement->GroundFriction = FMath::Max(
            0.0f,
            WalkingGroundFriction
        );
        Movement->bUseSeparateBrakingFriction = true;
        Movement->BrakingFriction = FMath::Max(
            0.0f,
            WalkingBrakingFriction
        );
        Movement->RotationRate = FRotator(
            0.0f,
            FMath::Max(0.0f, MovementRotationRateYaw),
            0.0f
        );
        Movement->AirControl = FMath::Clamp(
            MovementAirControl,
            0.0f,
            1.0f
        );
        Movement->MinAnalogWalkSpeed = FMath::Clamp(
            MinimumAnalogWalkSpeed,
            0.0f,
            Movement->MaxWalkSpeed
        );
    }

    if (CameraBoom)
    {
        CameraBoom->bEnableCameraLag = bEnableCameraPositionLag;
        CameraBoom->CameraLagSpeed = FMath::Max(
            0.0f,
            CameraPositionLagSpeed
        );
        CameraBoom->CameraLagMaxDistance = FMath::Max(
            0.0f,
            CameraPositionLagMaxDistance
        );
        CameraBoom->bEnableCameraRotationLag =
            bEnableCameraRotationSmoothing;
        CameraBoom->CameraRotationLagSpeed = FMath::Max(
            0.0f,
            CameraRotationSmoothingSpeed
        );
        CameraBoom->bUseCameraLagSubstepping = true;
        CameraBoom->CameraLagMaxTimeStep = 1.0f / 60.0f;
    }
}

void AAdaptivePlayerCharacter::SetupPlayerInputComponent(
    UInputComponent* PlayerInputComponent
)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    APlayerController* PlayerController =
        Cast<APlayerController>(GetController());
    ULocalPlayer* LocalPlayer = PlayerController
        ? PlayerController->GetLocalPlayer()
        : nullptr;
    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
        ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
        : nullptr;

    if (InputSubsystem)
    {
        InputSubsystem->RemoveMappingContext(DefaultMappingContext);
        InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
    }
    else
    {
        UE_LOG(
            LogAdaptHunt,
            Warning,
            TEXT("No Enhanced Input subsystem was available for %s."),
            *GetName()
        );
    }

    UEnhancedInputComponent* EnhancedInput =
        Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!EnhancedInput)
    {
        UE_LOG(
            LogAdaptHunt,
            Error,
            TEXT(
                "%s requires EnhancedInputComponent. Check DefaultInput.ini."
            ),
            *GetName()
        );
        return;
    }

    EnhancedInput->BindAction(
        MoveForwardAction,
        ETriggerEvent::Triggered,
        this,
        &AAdaptivePlayerCharacter::MoveForward
    );
    EnhancedInput->BindAction(
        MoveRightAction,
        ETriggerEvent::Triggered,
        this,
        &AAdaptivePlayerCharacter::MoveRight
    );
    EnhancedInput->BindAction(
        LookYawAction,
        ETriggerEvent::Triggered,
        this,
        &AAdaptivePlayerCharacter::LookYaw
    );
    EnhancedInput->BindAction(
        LookPitchAction,
        ETriggerEvent::Triggered,
        this,
        &AAdaptivePlayerCharacter::LookPitch
    );
    EnhancedInput->BindAction(
        JumpAction,
        ETriggerEvent::Started,
        this,
        &ACharacter::Jump
    );
    EnhancedInput->BindAction(
        JumpAction,
        ETriggerEvent::Completed,
        this,
        &ACharacter::StopJumping
    );
    EnhancedInput->BindAction(
        LightAttackAction,
        ETriggerEvent::Started,
        this,
        &AAdaptivePlayerCharacter::LightAttack
    );
    EnhancedInput->BindAction(
        HeavyAttackAction,
        ETriggerEvent::Started,
        this,
        &AAdaptivePlayerCharacter::HeavyAttack
    );
    EnhancedInput->BindAction(
        BlockAction,
        ETriggerEvent::Started,
        this,
        &AAdaptivePlayerCharacter::StartBlock
    );
    EnhancedInput->BindAction(
        BlockAction,
        ETriggerEvent::Completed,
        this,
        &AAdaptivePlayerCharacter::StopBlock
    );
    EnhancedInput->BindAction(
        BlockAction,
        ETriggerEvent::Canceled,
        this,
        &AAdaptivePlayerCharacter::StopBlock
    );
    EnhancedInput->BindAction(
        DodgeLeftAction,
        ETriggerEvent::Started,
        this,
        &AAdaptivePlayerCharacter::DodgeLeft
    );
    EnhancedInput->BindAction(
        DodgeRightAction,
        ETriggerEvent::Started,
        this,
        &AAdaptivePlayerCharacter::DodgeRight
    );
    EnhancedInput->BindAction(
        DodgeBackwardAction,
        ETriggerEvent::Started,
        this,
        &AAdaptivePlayerCharacter::DodgeBackward
    );
    EnhancedInput->BindAction(
        HealAction,
        ETriggerEvent::Started,
        this,
        &AAdaptivePlayerCharacter::Heal
    );
    EnhancedInput->BindAction(
        RestartMatchAction,
        ETriggerEvent::Started,
        this,
        &AAdaptivePlayerCharacter::RestartMatch
    );

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Enhanced Input bindings initialized for %s."),
        *GetName()
    );
}

UCameraComponent* AAdaptivePlayerCharacter::GetFollowCamera() const
{
    return FollowCamera;
}

USpringArmComponent* AAdaptivePlayerCharacter::GetCameraBoom() const
{
    return CameraBoom;
}

UStaticMeshComponent* AAdaptivePlayerCharacter::GetBodyMesh() const
{
    return BodyMesh;
}

UHealthComponent* AAdaptivePlayerCharacter::GetHealthComponent() const
{
    return HealthComponent;
}

UStaminaComponent* AAdaptivePlayerCharacter::GetStaminaComponent() const
{
    return StaminaComponent;
}

UCombatComponent* AAdaptivePlayerCharacter::GetCombatComponent() const
{
    return CombatComponent;
}

UCombatFeedbackComponent*
AAdaptivePlayerCharacter::GetCombatFeedbackComponent() const
{
    return CombatFeedbackComponent;
}

UGreyboxPresentationComponent*
AAdaptivePlayerCharacter::GetGreyboxPresentationComponent() const
{
    return GreyboxPresentationComponent;
}

UCombatSnapshotComponent*
AAdaptivePlayerCharacter::GetCombatSnapshotComponent() const
{
    return CombatSnapshotComponent;
}

UPlayerBehaviorTrackerComponent*
AAdaptivePlayerCharacter::GetBehaviorTrackerComponent() const
{
    return BehaviorTrackerComponent;
}

const UInputMappingContext*
AAdaptivePlayerCharacter::GetDefaultMappingContext() const
{
    return DefaultMappingContext;
}

float AAdaptivePlayerCharacter::GetCameraPositionLagSpeed() const
{
    return FMath::Max(0.0f, CameraPositionLagSpeed);
}

float AAdaptivePlayerCharacter::GetCameraRotationSmoothingSpeed() const
{
    return FMath::Max(0.0f, CameraRotationSmoothingSpeed);
}

void AAdaptivePlayerCharacter::DebugDamageSelf(const float DamageAmount)
{
    UGameplayStatics::ApplyDamage(
        this,
        DamageAmount,
        GetController(),
        this,
        nullptr
    );
}

void AAdaptivePlayerCharacter::DebugSpendStamina(const float Cost)
{
    if (!StaminaComponent->TryConsumeStamina(Cost))
    {
        UE_LOG(
            LogAdaptHunt,
            Warning,
            TEXT("Could not spend %.1f stamina; %.1f is available."),
            Cost,
            StaminaComponent->GetCurrentStamina()
        );
    }
}

void AAdaptivePlayerCharacter::DebugResetResources()
{
    CombatComponent->ResetCombatState();
    HealthComponent->ResetHealth();
    StaminaComponent->ResetStamina();
    GreyboxPresentationComponent->ResetPresentation();
}

void AAdaptivePlayerCharacter::DebugLightAttack()
{
    LightAttack();
}

void AAdaptivePlayerCharacter::DebugHeavyAttack()
{
    HeavyAttack();
}

void AAdaptivePlayerCharacter::DebugToggleBlock()
{
    if (CombatComponent->IsBlocking())
    {
        CombatComponent->StopBlock();
    }
    else
    {
        CombatComponent->TryStartBlock();
    }
}

void AAdaptivePlayerCharacter::DebugDodgeLeft()
{
    DodgeLeft();
}

void AAdaptivePlayerCharacter::DebugDodgeRight()
{
    DodgeRight();
}

void AAdaptivePlayerCharacter::DebugDodgeBackward()
{
    DodgeBackward();
}

void AAdaptivePlayerCharacter::DebugHeal()
{
    Heal();
}

void AAdaptivePlayerCharacter::DebugSpawnCombatDummy(const float Distance)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FVector SpawnLocation = GetActorLocation()
        + GetActorForwardVector() * FMath::Max(150.0f, Distance);
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    World->SpawnActor<ACombatTargetDummy>(
        ACombatTargetDummy::StaticClass(),
        SpawnLocation,
        GetActorRotation(),
        SpawnParameters
    );
}

void AAdaptivePlayerCharacter::DebugCombatSnapshot()
{
    if (CombatSnapshotComponent)
    {
        CombatSnapshotComponent->LogSnapshot();
    }
}

void AAdaptivePlayerCharacter::DebugBehaviorDataset()
{
    if (BehaviorTrackerComponent)
    {
        BehaviorTrackerComponent->LogDataset();
    }
}

void AAdaptivePlayerCharacter::DebugResetBehaviorDataset()
{
    if (BehaviorTrackerComponent)
    {
        BehaviorTrackerComponent->ResetDataset();
    }
}

void AAdaptivePlayerCharacter::DebugToggleAdaptiveHUD()
{
    const APlayerController* PlayerController = Cast<APlayerController>(
        GetController()
    );
    AAdaptiveDebugHUD* Hud = PlayerController
        ? Cast<AAdaptiveDebugHUD>(PlayerController->GetHUD())
        : nullptr;
    if (Hud)
    {
        Hud->DebugToggleAdaptiveHUD();
    }
}

void AAdaptivePlayerCharacter::RestartMatch()
{
    AAdaptiveGameMode* GameMode = Cast<AAdaptiveGameMode>(
        UGameplayStatics::GetGameMode(this)
    );
    URoundManager* RoundManager = GameMode
        ? GameMode->GetRoundManager()
        : nullptr;
    if (RoundManager && RoundManager->RestartMatch())
    {
        UE_LOG(
            LogAdaptHunt,
            Log,
            TEXT("Player requested a clean match restart.")
        );
    }
}

void AAdaptivePlayerCharacter::MoveForward(const FInputActionValue& Value)
{
    if (!Controller || (CombatComponent
        && !CombatComponent->IsMovementAllowed()))
    {
        return;
    }

    const float AxisValue = Value.Get<float>();
    const FRotator ControlRotation = Controller->GetControlRotation();
    const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);
    AddMovementInput(
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X),
        AxisValue
    );
}

void AAdaptivePlayerCharacter::MoveRight(const FInputActionValue& Value)
{
    if (!Controller || (CombatComponent
        && !CombatComponent->IsMovementAllowed()))
    {
        return;
    }

    const float AxisValue = Value.Get<float>();
    const FRotator ControlRotation = Controller->GetControlRotation();
    const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);
    AddMovementInput(
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y),
        AxisValue
    );
}

void AAdaptivePlayerCharacter::LookYaw(const FInputActionValue& Value)
{
    AddControllerYawInput(Value.Get<float>());
}

void AAdaptivePlayerCharacter::LookPitch(const FInputActionValue& Value)
{
    AddControllerPitchInput(-Value.Get<float>());
}

void AAdaptivePlayerCharacter::LightAttack()
{
    CombatComponent->TryLightAttack();
}

void AAdaptivePlayerCharacter::HeavyAttack()
{
    CombatComponent->TryHeavyAttack();
}

void AAdaptivePlayerCharacter::StartBlock()
{
    CombatComponent->TryStartBlock();
}

void AAdaptivePlayerCharacter::StopBlock()
{
    CombatComponent->StopBlock();
}

void AAdaptivePlayerCharacter::DodgeLeft()
{
    CombatComponent->TryDodge(EPlayerCombatAction::DodgeLeft);
}

void AAdaptivePlayerCharacter::DodgeRight()
{
    CombatComponent->TryDodge(EPlayerCombatAction::DodgeRight);
}

void AAdaptivePlayerCharacter::DodgeBackward()
{
    CombatComponent->TryDodge(EPlayerCombatAction::DodgeBackward);
}

void AAdaptivePlayerCharacter::Heal()
{
    CombatComponent->TryHeal();
}
