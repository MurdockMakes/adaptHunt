#include "Characters/AdaptiveEnemyCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/CombatFeedbackComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/EnemyDecisionComponent.h"
#include "Components/EnemyLocomotionComponent.h"
#include "Components/GreyboxPresentationComponent.h"
#include "Components/HealthComponent.h"
#include "Components/StaminaComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "UObject/ConstructorHelpers.h"

AAdaptiveEnemyCharacter::AAdaptiveEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AAIController::StaticClass();

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = false;
    Movement->MaxWalkSpeed = 360.0f;
    Movement->MaxAcceleration = 1600.0f;
    Movement->BrakingDecelerationWalking = 1800.0f;

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(GetCapsuleComponent());
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyMesh->SetRelativeScale3D(FVector(0.9f, 0.9f, 1.8f));

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
    EnemyCombatComponent = CreateDefaultSubobject<UEnemyCombatComponent>(
        TEXT("EnemyCombatComponent")
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
        EGreyboxPresentationRole::Enemy
    );
    EnemyLocomotionComponent =
        CreateDefaultSubobject<UEnemyLocomotionComponent>(
            TEXT("EnemyLocomotionComponent")
        );
    EnemyDecisionComponent = CreateDefaultSubobject<UEnemyDecisionComponent>(
        TEXT("EnemyDecisionComponent")
    );
}

void AAdaptiveEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority() && !GetController())
    {
        SpawnDefaultController();
    }
}

UStaticMeshComponent* AAdaptiveEnemyCharacter::GetBodyMesh() const
{
    return BodyMesh;
}

UHealthComponent* AAdaptiveEnemyCharacter::GetHealthComponent() const
{
    return HealthComponent;
}

UStaminaComponent* AAdaptiveEnemyCharacter::GetStaminaComponent() const
{
    return StaminaComponent;
}

UEnemyCombatComponent*
AAdaptiveEnemyCharacter::GetEnemyCombatComponent() const
{
    return EnemyCombatComponent;
}

UCombatFeedbackComponent*
AAdaptiveEnemyCharacter::GetCombatFeedbackComponent() const
{
    return CombatFeedbackComponent;
}

UEnemyDecisionComponent*
AAdaptiveEnemyCharacter::GetEnemyDecisionComponent() const
{
    return EnemyDecisionComponent;
}

UEnemyLocomotionComponent*
AAdaptiveEnemyCharacter::GetEnemyLocomotionComponent() const
{
    return EnemyLocomotionComponent;
}

UGreyboxPresentationComponent*
AAdaptiveEnemyCharacter::GetGreyboxPresentationComponent() const
{
    return GreyboxPresentationComponent;
}
