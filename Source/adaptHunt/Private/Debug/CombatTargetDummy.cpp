#include "Debug/CombatTargetDummy.h"

#include "Components/CapsuleComponent.h"
#include "Components/HealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ACombatTargetDummy::ACombatTargetDummy()
{
    PrimaryActorTick.bCanEverTick = false;

    CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(
        TEXT("CollisionCapsule")
    );
    SetRootComponent(CollisionCapsule);
    CollisionCapsule->InitCapsuleSize(42.0f, 96.0f);
    CollisionCapsule->SetCollisionProfileName(
        UCollisionProfile::Pawn_ProfileName
    );

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(CollisionCapsule);
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
}

UHealthComponent* ACombatTargetDummy::GetHealthComponent() const
{
    return HealthComponent;
}

UCapsuleComponent* ACombatTargetDummy::GetCollisionCapsule() const
{
    return CollisionCapsule;
}
