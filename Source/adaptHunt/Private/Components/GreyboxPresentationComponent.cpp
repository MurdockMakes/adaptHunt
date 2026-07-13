#include "Components/GreyboxPresentationComponent.h"

#include "Components/CombatComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/EnemyDecisionComponent.h"
#include "Components/HealthComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/AdaptiveHuntTuningSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"


namespace
{
const FName GreyboxColorParameter(TEXT("Color"));

float SafePositive(const float Value, const float Fallback)
{
    return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : Fallback;
}
}


UGreyboxPresentationComponent::UGreyboxPresentationComponent()
    : bPresentationEnabled(true)
    , PresentationRole(EGreyboxPresentationRole::Player)
    , PlayerNeutralColor(0.06f, 0.28f, 0.92f, 1.0f)
    , EnemyNeutralColor(0.86f, 0.08f, 0.055f, 1.0f)
    , bPredictionDebugCueEnabled(false)
    , CurrentColor(FLinearColor::White)
    , ObservedPhase(ECombatActionPhase::Idle)
    , ObservedAction(EGreyboxPresentationAction::None)
    , CurrentCue(EGreyboxAnimationCue::Neutral)
    , AnimationTime(0.0f)
    , MovementPhase(0.0f)
    , PhaseElapsedTime(0.0f)
    , RemainingHitFlashTime(0.0f)
    , FeedbackPulseElapsedTime(0.0f)
    , RemainingFeedbackPulseTime(0.0f)
    , ActiveFeedbackCue(ECombatFeedbackCue::None)
    , bNeutralStateCaptured(false)
    , bEventsBound(false)
    , bPredictionCueActive(false)
{
    const FAdaptiveFeedbackTuning Tuning =
        UAdaptiveHuntTuningSettings::Get().Feedback.GetSanitized();
    IdleBreathFrequency = Tuning.IdleBreathFrequency;
    MovementStepFrequency = Tuning.MovementStepFrequency;
    TransformInterpSpeed = Tuning.TransformInterpSpeed;
    ColorInterpSpeed = Tuning.ColorInterpSpeed;
    HitFlashDuration = Tuning.HitFlashDuration;
    ImpactPulseDuration = Tuning.ImpactPulseDuration;
    DeathFallDuration = Tuning.DeathFallDuration;

    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UGreyboxPresentationComponent::SetPresentationMesh(
    UMeshComponent* NewPresentationMesh
)
{
    PresentationMesh = NewPresentationMesh;
}

void UGreyboxPresentationComponent::SetPresentationRole(
    const EGreyboxPresentationRole NewRole
)
{
    PresentationRole = NewRole;
    if (HasBegunPlay())
    {
        CurrentColor = GetNeutralColor();
        ApplyMaterialColor(CurrentColor);
    }
}

void UGreyboxPresentationComponent::SetPresentationEnabled(
    const bool bEnabled
)
{
    bPresentationEnabled = bEnabled;
    if (!bPresentationEnabled)
    {
        ResetPresentation();
    }
    SetComponentTickEnabled(bPresentationEnabled && HasBegunPlay());
}

bool UGreyboxPresentationComponent::IsPresentationEnabled() const
{
    return bPresentationEnabled;
}

void UGreyboxPresentationComponent::ResetPresentation()
{
    ObservedPhase = ECombatActionPhase::Idle;
    ObservedAction = EGreyboxPresentationAction::None;
    AnimationTime = 0.0f;
    MovementPhase = 0.0f;
    PhaseElapsedTime = 0.0f;
    RemainingHitFlashTime = 0.0f;
    FeedbackPulseElapsedTime = 0.0f;
    RemainingFeedbackPulseTime = 0.0f;
    ActiveFeedbackCue = ECombatFeedbackCue::None;
    bPredictionCueActive = false;

    if (PresentationMesh && bNeutralStateCaptured)
    {
        PresentationMesh->SetRelativeTransform(NeutralRelativeTransform);
    }
    CurrentColor = GetNeutralColor();
    ApplyMaterialColor(CurrentColor);
    EmitCue(EGreyboxAnimationCue::Neutral);
}

EGreyboxPresentationRole
UGreyboxPresentationComponent::GetPresentationRole() const
{
    return PresentationRole;
}

EGreyboxAnimationCue UGreyboxPresentationComponent::GetCurrentCue() const
{
    return CurrentCue;
}

FLinearColor UGreyboxPresentationComponent::GetNeutralColor() const
{
    return PresentationRole == EGreyboxPresentationRole::Enemy
        ? EnemyNeutralColor
        : PlayerNeutralColor;
}

UMeshComponent* UGreyboxPresentationComponent::GetPresentationMesh() const
{
    return PresentationMesh;
}

void UGreyboxPresentationComponent::TriggerFeedbackCue(
    const ECombatFeedbackCue Cue
)
{
    if (Cue == ECombatFeedbackCue::None)
    {
        ActiveFeedbackCue = ECombatFeedbackCue::None;
        FeedbackPulseElapsedTime = 0.0f;
        RemainingFeedbackPulseTime = 0.0f;
        return;
    }
    if (RemainingFeedbackPulseTime > 0.0f
        && AdaptiveCombatFeedback::GetCuePriority(Cue)
            < AdaptiveCombatFeedback::GetCuePriority(ActiveFeedbackCue))
    {
        return;
    }

    ActiveFeedbackCue = Cue;
    FeedbackPulseElapsedTime = 0.0f;
    RemainingFeedbackPulseTime = FMath::IsFinite(ImpactPulseDuration)
        ? FMath::Clamp(ImpactPulseDuration, 0.05f, 0.5f)
        : 0.16f;
}

ECombatFeedbackCue
UGreyboxPresentationComponent::GetActiveFeedbackCue() const
{
    return RemainingFeedbackPulseTime > 0.0f
        ? ActiveFeedbackCue
        : ECombatFeedbackCue::None;
}

void UGreyboxPresentationComponent::SetPredictionDebugCueEnabled(
    const bool bEnabled
)
{
#if UE_BUILD_SHIPPING
    bPredictionDebugCueEnabled = false;
#else
    bPredictionDebugCueEnabled = bEnabled;
#endif
    if (!bPredictionDebugCueEnabled)
    {
        bPredictionCueActive = false;
    }
}

bool UGreyboxPresentationComponent::IsPredictionDebugCueEnabled() const
{
    return bPredictionDebugCueEnabled;
}

void UGreyboxPresentationComponent::BeginPlay()
{
    Super::BeginPlay();
    CacheObservedComponents();
    CaptureNeutralState();
    CreatePresentationMaterial();
    BindObservedEvents();

    if (PlayerCombatComponent)
    {
        ObservedPhase = PlayerCombatComponent->GetCurrentPhase();
        ObservedAction = AdaptiveGreyboxPresentation::FromPlayerAction(
            PlayerCombatComponent->GetLastAction()
        );
    }
    else if (EnemyCombatComponent)
    {
        ObservedPhase = EnemyCombatComponent->GetCurrentPhase();
        ObservedAction = AdaptiveGreyboxPresentation::FromEnemyAction(
            EnemyCombatComponent->GetLastAction()
        );
    }
    ResetPresentation();
    SetComponentTickEnabled(bPresentationEnabled && PresentationMesh != nullptr);
}

void UGreyboxPresentationComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    UnbindObservedEvents();
    if (PresentationMesh && bNeutralStateCaptured)
    {
        PresentationMesh->SetRelativeTransform(NeutralRelativeTransform);
        if (OriginalMaterial)
        {
            PresentationMesh->SetMaterial(0, OriginalMaterial);
        }
    }
    PresentationMaterial = nullptr;
    Super::EndPlay(EndPlayReason);
}

void UGreyboxPresentationComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bPresentationEnabled || !PresentationMesh
        || !bNeutralStateCaptured || !FMath::IsFinite(DeltaTime)
        || DeltaTime <= 0.0f)
    {
        return;
    }

    AnimationTime += DeltaTime;
    PhaseElapsedTime += DeltaTime;
    RemainingHitFlashTime = FMath::Max(
        0.0f,
        RemainingHitFlashTime - DeltaTime
    );
    if (RemainingFeedbackPulseTime > 0.0f)
    {
        FeedbackPulseElapsedTime += DeltaTime;
        RemainingFeedbackPulseTime = FMath::Max(
            0.0f,
            RemainingFeedbackPulseTime - DeltaTime
        );
        if (RemainingFeedbackPulseTime <= 0.0f)
        {
            ActiveFeedbackCue = ECombatFeedbackCue::None;
        }
    }

    const AActor* Owner = GetOwner();
    const ACharacter* Character = Cast<ACharacter>(Owner);
    const FVector WorldVelocity = Owner
        ? Owner->GetVelocity()
        : FVector::ZeroVector;
    const FVector LocalVelocity = Owner
        ? Owner->GetActorTransform().InverseTransformVectorNoScale(
            WorldVelocity
        )
        : FVector::ZeroVector;
    const float MaxSpeed = Character && Character->GetCharacterMovement()
        ? FMath::Max(1.0f, Character->GetCharacterMovement()->GetMaxSpeed())
        : 500.0f;
    const float ForwardRatio = FMath::Clamp(
        LocalVelocity.X / MaxSpeed,
        -1.0f,
        1.0f
    );
    const float RightRatio = FMath::Clamp(
        LocalVelocity.Y / MaxSpeed,
        -1.0f,
        1.0f
    );
    const float HorizontalSpeed = WorldVelocity.Size2D();
    const bool bIsMoving = HorizontalSpeed > 5.0f;
    const float SpeedRatio = FMath::Clamp(
        HorizontalSpeed / MaxSpeed,
        0.0f,
        1.0f
    );

    if (bIsMoving)
    {
        MovementPhase += DeltaTime
            * SafePositive(MovementStepFrequency, 7.5f)
            * (0.35f + 0.65f * SpeedRatio);
    }
    const float IdlePhase = AnimationTime
        * SafePositive(IdleBreathFrequency, 1.25f)
        * 2.0f * UE_PI;
    const float MotionWave = FMath::Sin(
        bIsMoving ? MovementPhase : IdlePhase
    );

    UpdateCueFromState(bIsMoving);
    float StateAlpha = 1.0f;
    if (CurrentCue == EGreyboxAnimationCue::Death)
    {
        StateAlpha = FMath::Clamp(
            PhaseElapsedTime / FMath::Max(0.01f, DeathFallDuration),
            0.0f,
            1.0f
        );
    }

    FGreyboxPresentationPose Pose =
        AdaptiveGreyboxPresentation::EvaluatePose(
            CurrentCue,
            ObservedAction,
            GetNeutralColor(),
            ForwardRatio,
            RightRatio,
            MotionWave,
            StateAlpha,
            bPredictionCueActive
        );

    if (ActiveFeedbackCue != ECombatFeedbackCue::None
        && RemainingFeedbackPulseTime > 0.0f)
    {
        const float SafeDuration = FMath::Max(
            0.05f,
            FMath::IsFinite(ImpactPulseDuration)
                ? ImpactPulseDuration
                : 0.16f
        );
        const FCombatFeedbackPulse Pulse =
            AdaptiveCombatFeedback::EvaluatePulse(
                ActiveFeedbackCue,
                FeedbackPulseElapsedTime / SafeDuration
            );
        Pose.ScaleMultiplier *= Pulse.ScaleMultiplier;
        Pose.Color = FLinearColor(
            FMath::Lerp(
                Pose.Color.R,
                Pulse.TintColor.R,
                Pulse.TintStrength
            ),
            FMath::Lerp(
                Pose.Color.G,
                Pulse.TintColor.G,
                Pulse.TintStrength
            ),
            FMath::Lerp(
                Pose.Color.B,
                Pulse.TintColor.B,
                Pulse.TintStrength
            ),
            1.0f
        );
    }

    Pose.Color = AdaptiveGreyboxPresentation::ApplyFeedbackColor(
        Pose.Color,
        RemainingHitFlashTime > 0.0f,
        HealthComponent && HealthComponent->IsInvulnerable(),
        0.5f + 0.5f * FMath::Sin(AnimationTime * 24.0f)
    );

    const FVector NeutralLocation = NeutralRelativeTransform.GetLocation();
    const FRotator NeutralRotation = NeutralRelativeTransform.Rotator();
    const FVector NeutralScale = NeutralRelativeTransform.GetScale3D();
    const FVector TargetLocation = NeutralLocation
        + NeutralRotation.RotateVector(Pose.LocationOffset);
    const FRotator TargetRotation = NeutralRotation + Pose.RotationOffset;
    const FVector TargetScale = NeutralScale * Pose.ScaleMultiplier;
    const float SafeTransformSpeed = FMath::Max(
        0.1f,
        SafePositive(TransformInterpSpeed, 14.0f)
    );

    PresentationMesh->SetRelativeLocation(FMath::VInterpTo(
        PresentationMesh->GetRelativeLocation(),
        TargetLocation,
        DeltaTime,
        SafeTransformSpeed
    ));
    PresentationMesh->SetRelativeRotation(FMath::RInterpTo(
        PresentationMesh->GetRelativeRotation(),
        TargetRotation,
        DeltaTime,
        SafeTransformSpeed
    ));
    PresentationMesh->SetRelativeScale3D(FMath::VInterpTo(
        PresentationMesh->GetRelativeScale3D(),
        TargetScale,
        DeltaTime,
        SafeTransformSpeed
    ));

    const float ColorAlpha = 1.0f - FMath::Exp(
        -FMath::Max(0.1f, SafePositive(ColorInterpSpeed, 18.0f))
            * DeltaTime
    );
    CurrentColor = FLinearColor(
        FMath::Lerp(CurrentColor.R, Pose.Color.R, ColorAlpha),
        FMath::Lerp(CurrentColor.G, Pose.Color.G, ColorAlpha),
        FMath::Lerp(CurrentColor.B, Pose.Color.B, ColorAlpha),
        1.0f
    );
    ApplyMaterialColor(CurrentColor);
}

void UGreyboxPresentationComponent::CacheObservedComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    if (!PlayerCombatComponent)
    {
        PlayerCombatComponent = Owner->FindComponentByClass<UCombatComponent>();
    }
    if (!EnemyCombatComponent)
    {
        EnemyCombatComponent =
            Owner->FindComponentByClass<UEnemyCombatComponent>();
    }
    if (!EnemyDecisionComponent)
    {
        EnemyDecisionComponent =
            Owner->FindComponentByClass<UEnemyDecisionComponent>();
    }
    if (!HealthComponent)
    {
        HealthComponent = Owner->FindComponentByClass<UHealthComponent>();
    }
    if (!PresentationMesh)
    {
        TArray<UMeshComponent*> MeshComponents;
        Owner->GetComponents<UMeshComponent>(MeshComponents);
        for (UMeshComponent* MeshComponent : MeshComponents)
        {
            if (MeshComponent && MeshComponent->GetNumMaterials() > 0)
            {
                PresentationMesh = MeshComponent;
                break;
            }
        }
    }
}

void UGreyboxPresentationComponent::BindObservedEvents()
{
    if (bEventsBound)
    {
        return;
    }
    if (PlayerCombatComponent)
    {
        PlayerCombatComponent->OnActionCommitted.AddUObject(
            this,
            &UGreyboxPresentationComponent::HandlePlayerActionCommitted
        );
        PlayerCombatComponent->OnActionPhaseChanged.AddDynamic(
            this,
            &UGreyboxPresentationComponent::HandleActionPhaseChanged
        );
    }
    if (EnemyCombatComponent)
    {
        EnemyCombatComponent->OnActionCommitted.AddUObject(
            this,
            &UGreyboxPresentationComponent::HandleEnemyActionCommitted
        );
        EnemyCombatComponent->OnActionPhaseChanged.AddDynamic(
            this,
            &UGreyboxPresentationComponent::HandleActionPhaseChanged
        );
    }
    if (HealthComponent)
    {
        HealthComponent->OnHealthChanged.AddUObject(
            this,
            &UGreyboxPresentationComponent::HandleHealthChanged
        );
        HealthComponent->OnDeath.AddUObject(
            this,
            &UGreyboxPresentationComponent::HandleDeath
        );
    }
    bEventsBound = true;
}

void UGreyboxPresentationComponent::UnbindObservedEvents()
{
    if (!bEventsBound)
    {
        return;
    }
    if (PlayerCombatComponent)
    {
        PlayerCombatComponent->OnActionCommitted.RemoveAll(this);
        PlayerCombatComponent->OnActionPhaseChanged.RemoveDynamic(
            this,
            &UGreyboxPresentationComponent::HandleActionPhaseChanged
        );
    }
    if (EnemyCombatComponent)
    {
        EnemyCombatComponent->OnActionCommitted.RemoveAll(this);
        EnemyCombatComponent->OnActionPhaseChanged.RemoveDynamic(
            this,
            &UGreyboxPresentationComponent::HandleActionPhaseChanged
        );
    }
    if (HealthComponent)
    {
        HealthComponent->OnHealthChanged.RemoveAll(this);
        HealthComponent->OnDeath.RemoveAll(this);
    }
    bEventsBound = false;
}

void UGreyboxPresentationComponent::CaptureNeutralState()
{
    if (!PresentationMesh || bNeutralStateCaptured)
    {
        return;
    }
    NeutralRelativeTransform = PresentationMesh->GetRelativeTransform();
    OriginalMaterial = PresentationMesh->GetMaterial(0);
    bNeutralStateCaptured = true;
}

void UGreyboxPresentationComponent::CreatePresentationMaterial()
{
    if (!PresentationMesh)
    {
        return;
    }
    PresentationMaterial = PresentationMesh->CreateDynamicMaterialInstance(
        0,
        OriginalMaterial
    );
    CurrentColor = GetNeutralColor();
    ApplyMaterialColor(CurrentColor);
}

void UGreyboxPresentationComponent::ApplyMaterialColor(
    const FLinearColor& Color
)
{
    if (PresentationMaterial)
    {
        PresentationMaterial->SetVectorParameterValue(
            GreyboxColorParameter,
            Color
        );
    }
}

void UGreyboxPresentationComponent::EmitCue(
    const EGreyboxAnimationCue Cue
)
{
    if (CurrentCue == Cue)
    {
        return;
    }
    CurrentCue = Cue;
    OnAnimationCue.Broadcast(CurrentCue);
}

void UGreyboxPresentationComponent::UpdateCueFromState(
    const bool bIsMoving
)
{
    EmitCue(AdaptiveGreyboxPresentation::ResolveCue(
        ObservedPhase,
        ObservedAction,
        bIsMoving
    ));
}

void UGreyboxPresentationComponent::HandlePlayerActionCommitted(
    UCombatComponent*,
    const EPlayerCombatAction Action
)
{
    ObservedAction = AdaptiveGreyboxPresentation::FromPlayerAction(Action);
    bPredictionCueActive = false;
}

void UGreyboxPresentationComponent::HandleEnemyActionCommitted(
    UEnemyCombatComponent*,
    const EEnemyCombatAction Action
)
{
    ObservedAction = AdaptiveGreyboxPresentation::FromEnemyAction(Action);
#if UE_BUILD_SHIPPING
    bPredictionCueActive = false;
#else
    bPredictionCueActive = bPredictionDebugCueEnabled
        && EnemyDecisionComponent
        && EnemyDecisionComponent->WasLastPredictionApplied()
        && AdaptiveGreyboxPresentation::IsAttackAction(ObservedAction);
#endif
}

void UGreyboxPresentationComponent::HandleActionPhaseChanged(
    const ECombatActionPhase,
    const ECombatActionPhase NewPhase
)
{
    ObservedPhase = NewPhase;
    PhaseElapsedTime = 0.0f;
    if (NewPhase == ECombatActionPhase::Idle)
    {
        ObservedAction = EGreyboxPresentationAction::None;
        bPredictionCueActive = false;
    }
    UpdateCueFromState(false);
}

void UGreyboxPresentationComponent::HandleHealthChanged(
    UHealthComponent*,
    const float OldHealth,
    const float NewHealth
)
{
    if (NewHealth < OldHealth)
    {
        RemainingHitFlashTime = FMath::Max(
            0.0f,
            SafePositive(HitFlashDuration, 0.12f)
        );
    }
    else if (NewHealth > OldHealth
        && ObservedPhase == ECombatActionPhase::Idle)
    {
        // Round/resource reset happens after combat reset; ordinary healing
        // occurs during its committed Active phase and does not enter here.
        ResetPresentation();
    }
}

void UGreyboxPresentationComponent::HandleDeath(
    UHealthComponent*,
    AActor*
)
{
    ObservedPhase = ECombatActionPhase::Dead;
    PhaseElapsedTime = 0.0f;
    bPredictionCueActive = false;
    UpdateCueFromState(false);
}
