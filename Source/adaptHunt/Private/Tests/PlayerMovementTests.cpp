#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Camera/CameraComponent.h"
#include "Characters/AdaptivePlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Game/AdaptiveGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputMappingContext.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptivePlayerMovementDefaultsTest,
    "adaptHunt.Milestone2.PlayerMovementDefaults",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptivePlayerMovementDefaultsTest::RunTest(const FString& Parameters)
{
    const AAdaptivePlayerCharacter* Character =
        GetDefault<AAdaptivePlayerCharacter>();
    TestNotNull(TEXT("The player character CDO exists"), Character);
    if (!Character)
    {
        return false;
    }

    const UCharacterMovementComponent* Movement =
        Character->GetCharacterMovement();
    TestNotNull(TEXT("Character movement exists"), Movement);
    if (Movement)
    {
        TestTrue(
            TEXT("Movement rotates the character toward travel direction"),
            Movement->bOrientRotationToMovement
        );
        TestTrue(
            TEXT("Walk speed is the Milestone 2 baseline"),
            FMath::IsNearlyEqual(Movement->MaxWalkSpeed, 500.0f)
        );
        TestTrue(
            TEXT("Jump velocity is configured"),
            FMath::IsNearlyEqual(Movement->JumpZVelocity, 520.0f)
        );
    }

    TestNotNull(TEXT("The camera boom exists"), Character->GetCameraBoom());
    TestNotNull(TEXT("The follow camera exists"), Character->GetFollowCamera());
    TestNotNull(TEXT("The greybox body exists"), Character->GetBodyMesh());
    TestNotNull(
        TEXT("The greybox body uses a primitive mesh"),
        Character->GetBodyMesh()
            ? Character->GetBodyMesh()->GetStaticMesh().Get()
            : nullptr
    );

    const UInputMappingContext* MappingContext =
        Character->GetDefaultMappingContext();
    TestNotNull(TEXT("The input mapping context exists"), MappingContext);
    if (MappingContext)
    {
        TestTrue(
            TEXT("The original movement mappings remain registered"),
            MappingContext->GetMappings().Num() >= 12
        );
    }

    const AAdaptiveGameMode* GameMode = GetDefault<AAdaptiveGameMode>();
    TestTrue(
        TEXT("The game mode spawns the adaptive player character"),
        GameMode
            && GameMode->DefaultPawnClass
                == AAdaptivePlayerCharacter::StaticClass()
    );

    return true;
}

#endif
