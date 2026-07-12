#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "AdaptiveGameMode.generated.h"

class AAdaptiveEnemyCharacter;
class URoundManager;

/** Bootstrap game mode for the adaptive-combat vertical slice. */
UCLASS()
class ADAPTHUNT_API AAdaptiveGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AAdaptiveGameMode();

    virtual void StartPlay() override;

    TSubclassOf<AAdaptiveEnemyCharacter> GetEnemyClass() const;
    AAdaptiveEnemyCharacter* GetSpawnedEnemy() const;
    URoundManager* GetRoundManager() const;

private:
    void SpawnOrFindEnemy();

    UPROPERTY(EditDefaultsOnly, Category = "Adaptive Hunt|Enemy")
    TSubclassOf<AAdaptiveEnemyCharacter> EnemyClass;

    UPROPERTY(Transient)
    TObjectPtr<AAdaptiveEnemyCharacter> SpawnedEnemy;

    /** Authoritative three-round flow; core round logic stays out of pawns. */
    UPROPERTY(VisibleAnywhere, Category = "Adaptive Hunt|Rounds")
    TObjectPtr<URoundManager> RoundManager;
};
