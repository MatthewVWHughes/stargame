#include "Space/SystemSpacePresentationActor.h"

#include "Components/ChildActorComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Flight/SpaceFlightPawn.h"
#include "GameFramework/Pawn.h"
#include "Space/OrbitRouteFrameQueryService.h"
#include "Space/SectorStarAnchorActor.h"
#include "Space/StarSystemSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASystemSpacePresentationActor::ASystemSpacePresentationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SystemOriginMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NativeSystemOriginMarker"));
	SystemOriginMesh->SetupAttachment(SceneRoot);
	SystemOriginMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SystemOriginMesh->SetGenerateOverlapEvents(false);
	SystemOriginMesh->SetCanEverAffectNavigation(false);
	SystemOriginMesh->SetRelativeScale3D(FVector(12.0));
	SystemOriginMesh->SetVisibility(false);
	SystemOriginMesh->SetHiddenInGame(true);

	AmbientDebugLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("NativeSystemAmbientLight"));
	AmbientDebugLight->SetupAttachment(SceneRoot);
	AmbientDebugLight->SetIntensity(8.0f);
	AmbientDebugLight->SetCastShadows(false);

	StarDirectionalLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("NativeStarDirectionalLight"));
	StarDirectionalLight->SetupAttachment(SceneRoot);
	StarDirectionalLight->Mobility = EComponentMobility::Movable;
	StarDirectionalLight->SetIntensity(12.0f);
	StarDirectionalLight->SetLightColor(FLinearColor(1.0f, 0.86f, 0.62f));
	StarDirectionalLight->SetLightSourceAngle(0.5357f);
	StarDirectionalLight->SetCastShadows(false);

	SkySphereMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NativeMilkyWaySkySphere"));
	SkySphereMesh->SetupAttachment(SceneRoot);
	SkySphereMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkySphereMesh->SetGenerateOverlapEvents(false);
	SkySphereMesh->SetCanEverAffectNavigation(false);
	SkySphereMesh->SetCastShadow(false);
	SkySphereMesh->SetReceivesDecals(false);
	SkySphereMesh->SetRelativeScale3D(FVector(10000000.0));
	SkySphereMesh->SetVisibility(true);
	SkySphereMesh->SetHiddenInGame(false);

	SpacePostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("NativeSpacePostProcess"));
	SpacePostProcess->SetupAttachment(SceneRoot);
	SpacePostProcess->bUnbound = true;
	SpacePostProcess->BlendWeight = 1.0f;
	SpacePostProcess->Settings.bOverride_AutoExposureMethod = true;
	SpacePostProcess->Settings.AutoExposureMethod = AEM_Manual;
	SpacePostProcess->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	SpacePostProcess->Settings.AutoExposureApplyPhysicalCameraExposure = false;
	SpacePostProcess->Settings.bOverride_AutoExposureBias = true;
	SpacePostProcess->Settings.AutoExposureBias = 0.0f;
	SpacePostProcess->Settings.bOverride_BloomIntensity = true;
	SpacePostProcess->Settings.BloomIntensity = 0.28f;
	SpacePostProcess->Settings.bOverride_BloomThreshold = true;
	SpacePostProcess->Settings.BloomThreshold = 1.0f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		SystemOriginMesh->SetStaticMesh(SphereMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SkyMesh(TEXT("/Game/Meshes/Sky/SM_MilkyWaySkySphere.SM_MilkyWaySkySphere"));
	if (SkyMesh.Succeeded())
	{
		SkySphereMesh->SetStaticMesh(SkyMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SkyMaterial(TEXT("/Game/Materials/Sky/M_MilkyWaySkybox.M_MilkyWaySkybox"));
	if (SkyMaterial.Succeeded())
	{
		SkySphereMesh->SetMaterial(0, SkyMaterial.Object);
	}
}

void ASystemSpacePresentationActor::BeginPlay()
{
	Super::BeginPlay();
	ScrubLegacyStarAnchorChildren();
}

void ASystemSpacePresentationActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshSpaceVisuals(DeltaSeconds);
}

void ASystemSpacePresentationActor::ConfigureForSystem(FName InSystemId)
{
	SystemId = InSystemId;
	Tags.AddUnique(SystemId);
	Tags.AddUnique(TEXT("system_presentation"));
	ScrubLegacyStarAnchorChildren();
	OnConfiguredForSystem(InSystemId);

#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("Presentation_%s"), *SystemId.ToString()));
#endif
}

void ASystemSpacePresentationActor::RefreshSpaceVisuals(float DeltaSeconds)
{
	FVector LogicalStarLocation = FVector::ZeroVector;
	FVector LogicalPlayerLocation = FVector::ZeroVector;
	FVector ActorPlayerLocation = FVector::ZeroVector;
	if (!ResolveStarAndPlayerLocations(LogicalStarLocation, LogicalPlayerLocation, ActorPlayerLocation))
	{
		return;
	}

	SetActorLocation(ActorPlayerLocation);
	if (SkySphereMesh)
	{
		SkySphereMesh->SetWorldLocation(ActorPlayerLocation);
	}

	LightingUpdateAccumulatorSeconds += DeltaSeconds;
	const FVector PreviousDirection = (LastLightingPlayerLocation - LastLightingStarLocation).GetSafeNormal();
	const FVector NewDirection = (LogicalPlayerLocation - LogicalStarLocation).GetSafeNormal();
	const float DeltaAngleDegrees = (!PreviousDirection.IsNearlyZero() && !NewDirection.IsNearlyZero())
		? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(PreviousDirection, NewDirection), -1.0, 1.0)))
		: 180.0f;
	const bool bMovedEnough = FVector::DistSquared(LogicalPlayerLocation, LastLightingPlayerLocation) >= FMath::Square(LightingMoveThresholdCm);
	const bool bAngleChangedEnough = DeltaAngleDegrees >= LightingAngleThresholdDegrees;
	const bool bTimeToRefresh = LightingUpdateAccumulatorSeconds >= LightingUpdateIntervalSeconds;

	if (bTimeToRefresh || bMovedEnough || bAngleChangedEnough)
	{
		UpdateStarLight(LogicalStarLocation, LogicalPlayerLocation, ActorPlayerLocation);
		LastLightingStarLocation = LogicalStarLocation;
		LastLightingPlayerLocation = LogicalPlayerLocation;
		LightingUpdateAccumulatorSeconds = 0.0f;
	}
}

bool ASystemSpacePresentationActor::ResolveStarAndPlayerLocations(FVector& OutLogicalStarLocation, FVector& OutLogicalPlayerLocation, FVector& OutActorPlayerLocation) const
{
	const UWorld* World = GetWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	const ASpaceFlightPawn* PlayerPawn = StarSystem ? Cast<ASpaceFlightPawn>(StarSystem->GetActivePlayerPawn()) : nullptr;
	if (!StarSystem || !PlayerPawn)
	{
		return false;
	}

	OutLogicalPlayerLocation = PlayerPawn->GetLogicalSystemPositionCm();
	OutActorPlayerLocation = PlayerPawn->GetActorLocation();

	FName StarBodyId = NAME_None;
	for (const FBodyDefinition& Body : StarSystem->GetActiveSystemDefinition().Bodies)
	{
		if (Body.BodyType == FName(TEXT("star")))
		{
			StarBodyId = Body.BodyId;
			break;
		}
	}

	if (StarBodyId.IsNone())
	{
		return false;
	}

	const FSimulationClockSnapshot ClockSnapshot = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(StarSystem->GetActiveSystemId(), 0.0);
	FFrameResolvedTransform StarTransform;
	if (UOrbitRouteFrameQueryService::ResolveEntityFrame(StarSystem->GetActiveSystemDefinition(), StarBodyId, ClockSnapshot, 0.0, StarTransform))
	{
		OutLogicalStarLocation = StarTransform.PositionCm;
		return true;
	}

	return false;
}

void ASystemSpacePresentationActor::UpdateStarLight(const FVector& LogicalStarLocation, const FVector& LogicalPlayerLocation, const FVector& ActorPlayerLocation)
{
	if (!StarDirectionalLight)
	{
		return;
	}

	const FVector LightDirection = (LogicalPlayerLocation - LogicalStarLocation).GetSafeNormal();
	if (LightDirection.IsNearlyZero())
	{
		return;
	}

	StarDirectionalLight->SetWorldLocation(ActorPlayerLocation);
	StarDirectionalLight->SetWorldRotation(LightDirection.Rotation());

	const double DistanceCm = FVector::Distance(LogicalPlayerLocation, LogicalStarLocation);
	const double InnerReferenceCm = 65000000.0;
	const double RelativeSolarFlux = FMath::Square(InnerReferenceCm / FMath::Max(DistanceCm, 1000000.0));
	const float Intensity = static_cast<float>(FMath::Clamp(RelativeSolarFlux * 12.0, 1.5, 18.0));
	StarDirectionalLight->SetIntensity(Intensity);
}

void ASystemSpacePresentationActor::ScrubLegacyStarAnchorChildren()
{
	TArray<UChildActorComponent*> ChildActorComponents;
	GetComponents(ChildActorComponents);
	for (UChildActorComponent* ChildActorComponent : ChildActorComponents)
	{
		if (!ChildActorComponent)
		{
			continue;
		}

		AActor* ChildActor = ChildActorComponent->GetChildActor();
		const bool bIsStarAnchor = Cast<ASectorStarAnchorActor>(ChildActor) != nullptr
			|| ChildActorComponent->GetName().Contains(TEXT("SectorStarAnchor"));
		if (!bIsStarAnchor)
		{
			continue;
		}

		if (ChildActor)
		{
			ChildActor->SetActorHiddenInGame(true);
			ChildActor->SetActorEnableCollision(false);
			ChildActor->Destroy();
		}
		ChildActorComponent->SetHiddenInGame(true);
		ChildActorComponent->SetVisibility(false, true);
		ChildActorComponent->DestroyComponent();
		UE_LOG(LogTemp, Display, TEXT("Removed legacy presentation-owned star anchor child from %s."), *GetName());
	}
}
