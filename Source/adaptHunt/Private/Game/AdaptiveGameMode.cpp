#include "Game/AdaptiveGameMode.h"

#include "adaptHunt.h"
#include "Characters/AdaptiveEnemyCharacter.h"
#include "Characters/AdaptivePlayerCharacter.h"
#include "Components/CombatSnapshotComponent.h"
#include "Components/EnemyDecisionComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/RoundManager.h"
#include "Kismet/GameplayStatics.h"
#include "UI/AdaptiveDebugHUD.h"

AAdaptiveGameMode::AAdaptiveGameMode()
    : SpawnedEnemy(nullptr)
{
    DefaultPawnClass = AAdaptivePlayerCharacter::StaticClass();
    HUDClass = AAdaptiveDebugHUD::StaticClass();
    EnemyClass = AAdaptiveEnemyCharacter::StaticClass();
    RoundManager = CreateDefaultSubobject<URoundManager>(TEXT("RoundManager"));
}

void AAdaptiveGameMode::StartPlay()
{
    Super::StartPlay();
    SpawnOrFindEnemy();

    AAdaptivePlayerCharacter* PlayerCharacter =
        Cast<AAdaptivePlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (RoundManager && PlayerCharacter && SpawnedEnemy)
    {
        RoundManager->Initialize(PlayerCharacter, SpawnedEnemy);
    }

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Adaptive Hunt Milestone 13 initialized with game mode %s."),
        *GetName()
    );
}

TSubclassOf<AAdaptiveEnemyCharacter> AAdaptiveGameMode::GetEnemyClass() const
{
    return EnemyClass;
}

AAdaptiveEnemyCharacter* AAdaptiveGameMode::GetSpawnedEnemy() const
{
    return SpawnedEnemy;
}

URoundManager* AAdaptiveGameMode::GetRoundManager() const
{
    return RoundManager;
}

void AAdaptiveGameMode::SpawnOrFindEnemy()
{
    UWorld* World = GetWorld();
    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!World || !PlayerPawn || !EnemyClass)
    {
        UE_LOG(
            LogAdaptHunt,
            Warning,
            TEXT("Could not initialize the Milestone 13 combatants for %s."),
            *GetName()
        );
        return;
    }

    for (TActorIterator<AAdaptiveEnemyCharacter> It(World); It; ++It)
    {
        SpawnedEnemy = *It;
        break;
    }

    if (!SpawnedEnemy)
    {
        const FVector SpawnLocation = PlayerPawn->GetActorLocation()
            + PlayerPawn->GetActorForwardVector() * 850.0f;
        const FRotator SpawnRotation = (
            PlayerPawn->GetActorLocation() - SpawnLocation
        ).Rotation();

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        SpawnedEnemy = World->SpawnActor<AAdaptiveEnemyCharacter>(
            EnemyClass,
            SpawnLocation,
            SpawnRotation,
            SpawnParameters
        );
    }

    if (SpawnedEnemy && SpawnedEnemy->GetEnemyDecisionComponent())
    {
        SpawnedEnemy->GetEnemyDecisionComponent()->SetTargetActor(PlayerPawn);
    }

    AAdaptivePlayerCharacter* PlayerCharacter =
        Cast<AAdaptivePlayerCharacter>(PlayerPawn);
    if (PlayerCharacter && PlayerCharacter->GetCombatSnapshotComponent())
    {
        PlayerCharacter->GetCombatSnapshotComponent()->SetOpponentActor(
            SpawnedEnemy
        );
    }
}
