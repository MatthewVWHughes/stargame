#include "Space/SectorStarAnchorActor.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float BasicSphereRadiusCm = 50.0f;
	constexpr float BasicPlaneSizeCm = 100.0f;
	constexpr int32 MaxProminenceArcs = 12;

	float SampleStarFbm(FVector P)
	{
		float Value = 0.0f;
		float Amplitude = 0.5f;
		for (int32 Octave = 0; Octave < 5; ++Octave)
		{
			Value += (FMath::PerlinNoise3D(P) * 0.5f + 0.5f) * Amplitude;
			P = P * 2.03f + FVector(7.1f, 13.7f, 3.9f);
			Amplitude *= 0.5f;
		}
		return FMath::Clamp(Value, 0.0f, 1.0f);
	}

	float StarHash(float Value)
	{
		const float Raw = FMath::Sin(Value * 12.9898f) * 43758.5453f;
		return Raw - FMath::FloorToFloat(Raw);
	}

	float FiniteOrDefault(float Value, float DefaultValue)
	{
		return FMath::IsFinite(Value) ? Value : DefaultValue;
	}

	FVector SunspotDirectionFromSeed(float Seed)
	{
		const float Z = FMath::Lerp(-0.72f, 0.72f, StarHash(Seed + 11.17f));
		const float Azimuth = StarHash(Seed + 31.73f) * 2.0f * PI;
		const float Radius = FMath::Sqrt(FMath::Max(0.0f, 1.0f - Z * Z));
		return FVector(Radius * FMath::Cos(Azimuth), Radius * FMath::Sin(Azimuth), Z).GetSafeNormal();
	}

	FStargameSectorStarVisualPreset MakePreset(
		const FLinearColor& Core,
		const FLinearColor& Hot,
		const FLinearColor& Spot,
		const FLinearColor& Halo,
		const FLinearColor& Flare,
		const FLinearColor& Light,
		float TemperatureKelvin,
		float RelativeRadius,
		float RelativeLuminosity,
		float SurfaceEmission,
		float CoronaEmission,
		float HaloEmission,
		float FlareEmission,
		float LightIntensityLux,
		float Activity,
		float CoronaRadiusMultiplier,
		float HaloRadiusMultiplier,
		float EjectionRadiusMultiplier,
		float SurfaceFlowSpeed,
		float CoronaFlowSpeed)
	{
		FStargameSectorStarVisualPreset Preset;
		Preset.CoreColor = Core;
		Preset.HotColor = Hot;
		Preset.SpotColor = Spot;
		Preset.HaloColor = Halo;
		Preset.FlareColor = Flare;
		Preset.LightColor = Light;
		Preset.TemperatureKelvin = TemperatureKelvin;
		Preset.RelativeRadius = RelativeRadius;
		Preset.RelativeLuminosity = RelativeLuminosity;
		Preset.SurfaceEmission = SurfaceEmission;
		Preset.CoronaEmission = CoronaEmission;
		Preset.HaloEmission = HaloEmission;
		Preset.FlareEmission = FlareEmission;
		Preset.LightIntensityLux = LightIntensityLux;
		Preset.Activity = Activity;
		Preset.CoronaRadiusMultiplier = CoronaRadiusMultiplier;
		Preset.HaloRadiusMultiplier = HaloRadiusMultiplier;
		Preset.EjectionRadiusMultiplier = EjectionRadiusMultiplier;
		Preset.SurfaceFlowSpeed = SurfaceFlowSpeed;
		Preset.CoronaFlowSpeed = CoronaFlowSpeed;
		return Preset;
	}

	void ConfigureEffectMesh(UStaticMeshComponent* Component, bool bVisible = true)
	{
		if (!Component)
		{
			return;
		}

		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->SetReceivesDecals(false);
		Component->bAffectDistanceFieldLighting = false;
		Component->bDisallowNanite = true;
		Component->SetForceDisableNanite(true);
		Component->SetVisibility(bVisible);
		Component->SetHiddenInGame(!bVisible);
	}
}

ASectorStarAnchorActor::ASectorStarAnchorActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	StarBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StarBody"));
	StarBody->SetupAttachment(SceneRoot);
	ConfigureEffectMesh(StarBody, false);

	CoronaShell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoronaShell"));
	CoronaShell->SetupAttachment(SceneRoot);
	ConfigureEffectMesh(CoronaShell, false);

	PhotosphereSurface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PhotosphereSurface"));
	PhotosphereSurface->SetupAttachment(SceneRoot);
	PhotosphereSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PhotosphereSurface->SetCastShadow(false);
	PhotosphereSurface->SetReceivesDecals(false);
	PhotosphereSurface->bAffectDistanceFieldLighting = false;
	PhotosphereSurface->bUseAsyncCooking = true;
	PhotosphereSurface->SetVisibility(true);
	PhotosphereSurface->SetHiddenInGame(false);

	CoronaSurface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CoronaSurface"));
	CoronaSurface->SetupAttachment(SceneRoot);
	CoronaSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoronaSurface->SetCastShadow(false);
	CoronaSurface->SetReceivesDecals(false);
	CoronaSurface->bAffectDistanceFieldLighting = false;
	CoronaSurface->bUseAsyncCooking = true;
	CoronaSurface->SetVisibility(false);
	CoronaSurface->SetHiddenInGame(true);

	ProminenceShell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProminenceShell"));
	ProminenceShell->SetupAttachment(SceneRoot);
	ConfigureEffectMesh(ProminenceShell, false);

	ProminenceArcs.Reserve(MaxProminenceArcs);
	for (int32 Index = 0; Index < MaxProminenceArcs; ++Index)
	{
		const FName ComponentName(*FString::Printf(TEXT("ProminenceArc%02d"), Index));
		UProceduralMeshComponent* Arc = CreateDefaultSubobject<UProceduralMeshComponent>(ComponentName);
		Arc->SetupAttachment(SceneRoot);
		Arc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Arc->SetCastShadow(false);
		Arc->SetReceivesDecals(false);
		Arc->bAffectDistanceFieldLighting = false;
		Arc->bUseAsyncCooking = true;
		Arc->SetVisibility(false);
		Arc->SetHiddenInGame(true);
		ProminenceArcs.Add(Arc);
	}

	StarHaloBillboard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StarHaloBillboard"));
	StarHaloBillboard->SetupAttachment(SceneRoot);
	ConfigureEffectMesh(StarHaloBillboard, false);

	auto CreateEjectionBillboard = [this](const TCHAR* Name, const FRotator& Rotation, const FVector& Location)
	{
		UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetupAttachment(SceneRoot);
		Component->SetRelativeRotation(Rotation);
		Component->SetRelativeLocation(Location);
		ConfigureEffectMesh(Component, false);
		return Component;
	};

	EjectionBillboardA = CreateEjectionBillboard(TEXT("EjectionBillboardA"), FRotator(0.0, 0.0, 18.0), FVector(0.0, 0.0, 0.0));
	EjectionBillboardB = CreateEjectionBillboard(TEXT("EjectionBillboardB"), FRotator(0.0, 72.0, -28.0), FVector(0.0, 0.0, 0.0));
	EjectionBillboardC = CreateEjectionBillboard(TEXT("EjectionBillboardC"), FRotator(0.0, 149.0, 41.0), FVector(0.0, 0.0, 0.0));
	EjectionBillboardD = CreateEjectionBillboard(TEXT("EjectionBillboardD"), FRotator(0.0, 241.0, -8.0), FVector(0.0, 0.0, 0.0));

	StarLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("StarLight"));
	StarLight->SetupAttachment(SceneRoot);
	StarLight->SetRelativeRotation(FRotator(-8.0, 180.0, 0.0));
	StarLight->Mobility = EComponentMobility::Movable;
	StarLight->SetIntensity(120000.0f);
	StarLight->SetLightColor(FLinearColor(1.0f, 0.95f, 0.80f));
	StarLight->SetLightSourceAngle(0.5357f);
	StarLight->SetAtmosphereSunLight(true);
	StarLight->SetAtmosphereSunLightIndex(0);

	ClassLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ClassLabel"));
	ClassLabel->SetupAttachment(SceneRoot);
	ClassLabel->SetRelativeLocation(FVector(0.0, 0.0, 3800.0));
	ClassLabel->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	ClassLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	ClassLabel->SetWorldSize(220.0f);
	ClassLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ClassLabel->SetTextRenderColor(FColor(230, 232, 238, 255));
	ClassLabel->SetVisibility(false);
	ClassLabel->SetHiddenInGame(true);

	PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
	PreviewCamera->SetupAttachment(SceneRoot);
	PreviewCamera->SetRelativeLocation(FVector(8500.0, 0.0, 1800.0));
	PreviewCamera->SetRelativeRotation(FRotator(-8.0, 180.0, 0.0));
	PreviewCamera->FieldOfView = 50.0f;
	PreviewCamera->PostProcessBlendWeight = 1.0f;
	PreviewCamera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	PreviewCamera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	PreviewCamera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PreviewCamera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	PreviewCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
	PreviewCamera->PostProcessSettings.AutoExposureBias = 0.0f;
	PreviewCamera->PostProcessSettings.bOverride_BloomIntensity = true;
	PreviewCamera->PostProcessSettings.BloomIntensity = 0.08f;
	PreviewCamera->PostProcessSettings.bOverride_BloomThreshold = true;
	PreviewCamera->PostProcessSettings.BloomThreshold = 4.0f;

	PreviewPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PreviewPostProcess"));
	PreviewPostProcess->SetupAttachment(SceneRoot);
	PreviewPostProcess->bUnbound = true;
	PreviewPostProcess->BlendWeight = 1.0f;
	PreviewPostProcess->Settings.bOverride_AutoExposureMethod = true;
	PreviewPostProcess->Settings.AutoExposureMethod = AEM_Manual;
	PreviewPostProcess->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PreviewPostProcess->Settings.AutoExposureApplyPhysicalCameraExposure = false;
	PreviewPostProcess->Settings.bOverride_AutoExposureBias = true;
	PreviewPostProcess->Settings.AutoExposureBias = 0.0f;
	PreviewPostProcess->Settings.bOverride_BloomIntensity = true;
	PreviewPostProcess->Settings.BloomIntensity = 0.05f;
	PreviewPostProcess->Settings.bOverride_BloomThreshold = true;
	PreviewPostProcess->Settings.BloomThreshold = 4.0f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SmoothSphereMesh(TEXT("/Game/Meshes/Space/SM_StarPhotosphereSmooth.SM_StarPhotosphereSmooth"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DisplacedSphereMesh(TEXT("/Game/Meshes/Space/SM_StarPhotosphereDisplaced.SM_StarPhotosphereDisplaced"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> EngineSphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> EnginePlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));

	UStaticMesh* SphereMesh = SmoothSphereMesh.Succeeded() ? SmoothSphereMesh.Object.Get() : EngineSphereMesh.Object.Get();
	UStaticMesh* DisplacedMesh = DisplacedSphereMesh.Succeeded() ? DisplacedSphereMesh.Object.Get() : SphereMesh;
	if (SphereMesh)
	{
		StarBody->SetStaticMesh(DisplacedMesh);
		CoronaShell->SetStaticMesh(SphereMesh);
	}
	ProminenceShell->SetStaticMesh(nullptr);
	if (EnginePlaneMesh.Succeeded())
	{
		StarHaloBillboard->SetStaticMesh(EnginePlaneMesh.Object);
		for (UStaticMeshComponent* Ejection : GetEjectionBillboards())
		{
			Ejection->SetStaticMesh(EnginePlaneMesh.Object);
		}
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SurfaceMaterialFinder(TEXT("/Game/Materials/Space/M_StarSurface.M_StarSurface"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CoronaMaterialFinder(TEXT("/Game/Materials/Space/M_StarCorona.M_StarCorona"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HaloMaterialFinder(TEXT("/Game/Materials/Space/M_StarHaloBillboard.M_StarHaloBillboard"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ProminenceMaterialFinder(TEXT("/Game/Materials/Space/M_StarProminence.M_StarProminence"));
	static ConstructorHelpers::FObjectFinder<UTexture> PhotosphereMaskFinder(TEXT("/Game/Textures/Space/Star/T_StarPhotosphereMasks.T_StarPhotosphereMasks"));
	if (UMaterialInterface* ProceduralSurfaceMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Space/M_StarPhotosphereProcedural.M_StarPhotosphereProcedural")))
	{
		StarSurfaceMaterial = ProceduralSurfaceMaterial;
		StarBody->SetMaterial(0, StarSurfaceMaterial);
		PhotosphereSurface->SetMaterial(0, StarSurfaceMaterial);
	}
	else if (UMaterialInterface* DynamicSurfaceMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Space/M_StarSurfaceDynamic.M_StarSurfaceDynamic")))
	{
		StarSurfaceMaterial = DynamicSurfaceMaterial;
		StarBody->SetMaterial(0, StarSurfaceMaterial);
		PhotosphereSurface->SetMaterial(0, StarSurfaceMaterial);
	}
	else if (SurfaceMaterialFinder.Succeeded())
	{
		StarSurfaceMaterial = SurfaceMaterialFinder.Object;
		StarBody->SetMaterial(0, StarSurfaceMaterial);
		PhotosphereSurface->SetMaterial(0, StarSurfaceMaterial);
	}
	if (PhotosphereMaskFinder.Succeeded())
	{
		StarPhotosphereMaskTexture = PhotosphereMaskFinder.Object;
	}
	if (UMaterialInterface* ProceduralCoronaMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Space/M_StarCoronaProcedural.M_StarCoronaProcedural")))
	{
		StarCoronaMaterial = ProceduralCoronaMaterial;
		CoronaShell->SetMaterial(0, StarCoronaMaterial);
		CoronaSurface->SetMaterial(0, StarCoronaMaterial);
	}
	else if (CoronaMaterialFinder.Succeeded())
	{
		StarCoronaMaterial = CoronaMaterialFinder.Object;
		CoronaShell->SetMaterial(0, StarCoronaMaterial);
		CoronaSurface->SetMaterial(0, StarCoronaMaterial);
	}
	if (HaloMaterialFinder.Succeeded())
	{
		StarHaloMaterial = HaloMaterialFinder.Object;
		StarHaloBillboard->SetMaterial(0, StarHaloMaterial);
	}
	if (ProminenceMaterialFinder.Succeeded())
	{
		StarProminenceMaterial = ProminenceMaterialFinder.Object;
		for (UProceduralMeshComponent* Arc : ProminenceArcs)
		{
			if (Arc)
			{
				Arc->SetMaterial(0, StarProminenceMaterial);
			}
		}
		for (UStaticMeshComponent* Ejection : GetEjectionBillboards())
		{
			if (Ejection)
			{
				Ejection->SetMaterial(0, StarProminenceMaterial);
			}
		}
	}

	StellarClassPresets.Add(EStargameStellarClass::O, MakePreset(
		FLinearColor(0.72f, 0.82f, 1.0f), FLinearColor(0.50f, 0.66f, 1.0f), FLinearColor(0.10f, 0.15f, 0.34f),
		FLinearColor(0.56f, 0.74f, 1.0f), FLinearColor(0.40f, 0.62f, 1.0f), FLinearColor(0.82f, 0.90f, 1.0f),
		30000.0f, 7.0f, 30000.0f, 18.0f, 3.2f, 5.0f, 14.0f, 220000.0f, 0.62f, 1.32f, 4.8f, 2.4f, 0.07f, 0.035f));
	StellarClassPresets.Add(EStargameStellarClass::B, MakePreset(
		FLinearColor(0.84f, 0.91f, 1.0f), FLinearColor(0.66f, 0.78f, 1.0f), FLinearColor(0.20f, 0.25f, 0.46f),
		FLinearColor(0.74f, 0.86f, 1.0f), FLinearColor(0.58f, 0.74f, 1.0f), FLinearColor(0.88f, 0.94f, 1.0f),
		15000.0f, 3.8f, 800.0f, 16.0f, 2.8f, 4.6f, 12.5f, 190000.0f, 0.58f, 1.34f, 4.5f, 2.25f, 0.08f, 0.04f));
	StellarClassPresets.Add(EStargameStellarClass::A, MakePreset(
		FLinearColor(0.98f, 0.98f, 1.0f), FLinearColor(0.86f, 0.90f, 1.0f), FLinearColor(0.36f, 0.36f, 0.48f),
		FLinearColor(0.88f, 0.94f, 1.0f), FLinearColor(0.74f, 0.82f, 1.0f), FLinearColor(0.96f, 0.98f, 1.0f),
		8500.0f, 1.9f, 35.0f, 13.0f, 2.4f, 4.0f, 10.0f, 165000.0f, 0.48f, 1.36f, 4.2f, 2.05f, 0.10f, 0.045f));
	StellarClassPresets.Add(EStargameStellarClass::F, MakePreset(
		FLinearColor(1.0f, 0.93f, 0.72f), FLinearColor(1.0f, 0.68f, 0.32f), FLinearColor(0.42f, 0.24f, 0.10f),
		FLinearColor(1.0f, 0.84f, 0.42f), FLinearColor(1.0f, 0.54f, 0.18f), FLinearColor(1.0f, 0.96f, 0.84f),
		6500.0f, 1.35f, 4.0f, 10.5f, 2.6f, 3.8f, 10.5f, 140000.0f, 0.52f, 1.38f, 4.2f, 2.1f, 0.115f, 0.05f));
	StellarClassPresets.Add(EStargameStellarClass::G, MakePreset(
		FLinearColor(0.95f, 0.46f, 0.10f), FLinearColor(1.0f, 0.88f, 0.30f), FLinearColor(0.11f, 0.025f, 0.005f),
		FLinearColor(0.95f, 0.34f, 0.06f), FLinearColor(1.0f, 0.82f, 0.28f), FLinearColor(1.0f, 0.92f, 0.72f),
		5778.0f, 1.0f, 1.0f, 6.9f, 1.8f, 0.0f, 10.5f, 120000.0f, 0.70f, 1.24f, 0.0f, 2.2f, 0.12f, 0.055f));
	StellarClassPresets.Add(EStargameStellarClass::K, MakePreset(
		FLinearColor(1.0f, 0.56f, 0.22f), FLinearColor(0.95f, 0.26f, 0.05f), FLinearColor(0.28f, 0.06f, 0.014f),
		FLinearColor(1.0f, 0.48f, 0.12f), FLinearColor(0.95f, 0.20f, 0.035f), FLinearColor(1.0f, 0.72f, 0.46f),
		4300.0f, 0.78f, 0.35f, 6.8f, 3.0f, 3.9f, 12.0f, 90000.0f, 0.68f, 1.42f, 4.15f, 2.25f, 0.13f, 0.06f));
	StellarClassPresets.Add(EStargameStellarClass::M, MakePreset(
		FLinearColor(0.95f, 0.18f, 0.07f), FLinearColor(0.62f, 0.075f, 0.03f), FLinearColor(0.16f, 0.018f, 0.007f),
		FLinearColor(0.84f, 0.13f, 0.045f), FLinearColor(0.82f, 0.10f, 0.022f), FLinearColor(0.95f, 0.34f, 0.18f),
		3200.0f, 0.45f, 0.04f, 5.8f, 3.4f, 3.6f, 13.0f, 52000.0f, 0.78f, 1.48f, 4.0f, 2.35f, 0.16f, 0.075f));
	StellarClassPresets.Add(EStargameStellarClass::L, MakePreset(
		FLinearColor(0.55f, 0.10f, 0.12f), FLinearColor(0.30f, 0.04f, 0.08f), FLinearColor(0.08f, 0.01f, 0.03f),
		FLinearColor(0.42f, 0.07f, 0.17f), FLinearColor(0.50f, 0.07f, 0.13f), FLinearColor(0.70f, 0.22f, 0.28f),
		2200.0f, 0.28f, 0.003f, 3.2f, 1.4f, 1.6f, 4.0f, 14000.0f, 0.25f, 1.28f, 3.0f, 1.55f, 0.07f, 0.025f));
	StellarClassPresets.Add(EStargameStellarClass::BlueGiant, MakePreset(
		FLinearColor(0.54f, 0.70f, 1.0f), FLinearColor(0.18f, 0.42f, 1.0f), FLinearColor(0.04f, 0.06f, 0.24f),
		FLinearColor(0.36f, 0.62f, 1.0f), FLinearColor(0.20f, 0.50f, 1.0f), FLinearColor(0.72f, 0.84f, 1.0f),
		26000.0f, 12.0f, 90000.0f, 22.0f, 3.8f, 6.2f, 18.0f, 260000.0f, 0.70f, 1.30f, 5.2f, 2.55f, 0.055f, 0.03f));
	StellarClassPresets.Add(EStargameStellarClass::RedGiant, MakePreset(
		FLinearColor(1.0f, 0.30f, 0.08f), FLinearColor(0.95f, 0.18f, 0.035f), FLinearColor(0.20f, 0.035f, 0.012f),
		FLinearColor(1.0f, 0.26f, 0.06f), FLinearColor(1.0f, 0.16f, 0.025f), FLinearColor(1.0f, 0.50f, 0.25f),
		3600.0f, 18.0f, 1800.0f, 8.5f, 4.8f, 5.5f, 18.0f, 110000.0f, 0.92f, 1.58f, 5.6f, 2.85f, 0.07f, 0.095f));
	StellarClassPresets.Add(EStargameStellarClass::WhiteDwarf, MakePreset(
		FLinearColor(0.92f, 0.96f, 1.0f), FLinearColor(0.74f, 0.84f, 1.0f), FLinearColor(0.24f, 0.30f, 0.42f),
		FLinearColor(0.70f, 0.84f, 1.0f), FLinearColor(0.62f, 0.78f, 1.0f), FLinearColor(0.95f, 0.98f, 1.0f),
		12000.0f, 0.18f, 0.012f, 20.0f, 1.3f, 2.8f, 4.0f, 85000.0f, 0.20f, 1.18f, 3.1f, 1.4f, 0.035f, 0.018f));
}

void ASectorStarAnchorActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyStellarClass(StellarClass);
}

void ASectorStarAnchorActor::BeginPlay()
{
	Super::BeginPlay();
	ScrubLegacyProminenceMeshes();
	ApplyStellarClass(StellarClass);
	SetupPreviewInput();
	AlignBillboardToView();
}

void ASectorStarAnchorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AnimateStarEffects(DeltaSeconds);
	AlignBillboardToView();
}

#if WITH_EDITOR
void ASectorStarAnchorActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, StellarClass) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, StellarClassPresets) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, StarSurfaceMaterial) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, StarPhotosphereMaskTexture) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, BasePhotosphereRadiusCm) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, bApplyClassRadiusScale) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, VisualEmissionScale) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, LightIntensityScale) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, HaloAuthoringScale) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, bShowCorona) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, bShowHaloBillboard) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, bShowProminences) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, bShowEjections) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, bShowClassLabel) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereLatitudeSegments) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereLongitudeSegments) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereRefreshInterval) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereYawDegreesPerSecond) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereRollDegreesPerSecond) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereBroadScale) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereMidScale) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereFineScale) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereMicroScale) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereMicroSparkStrength) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereCoolCellStrength) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereSunspotStrength) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereAnchoredSpotStrength) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereSunspotSeed) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereSunspotDriftDegreesPerSecond) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereDynamicSunspotCount) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereSunspotLifetimeSeconds) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereSunspotAngularSize) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereLimbDarkening) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, PhotosphereDisplacementCm) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, CoronaYawDegreesPerSecond) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, CoronaRollDegreesPerSecond) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, ProminenceArcCount) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, ProminenceArcHeight) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, ProminenceArcWidth) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, ProminenceArcSpanDegrees) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, ProminenceArcCurl) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ASectorStarAnchorActor, ProminenceSeed))
	{
		ApplyStellarClass(StellarClass);
	}
}
#endif

void ASectorStarAnchorActor::ApplyStellarClass(EStargameStellarClass InClass)
{
	StellarClass = InClass;
	const bool bAllowDynamicMaterials = !HasAnyFlags(RF_ClassDefaultObject)
		&& GetWorld()
		&& (GetWorld()->IsGameWorld() || GetWorld()->WorldType == EWorldType::PIE);

	SurfaceMID = nullptr;
	CoronaMID = nullptr;
	HaloMID = nullptr;
	ProminenceMID = nullptr;
	EjectionMIDs.Reset();

	if (StarSurfaceMaterial && StarBody)
	{
		StarBody->SetMaterial(0, StarSurfaceMaterial);
	}
	if (StarSurfaceMaterial && PhotosphereSurface)
	{
		PhotosphereSurface->SetMaterial(0, StarSurfaceMaterial);
		if (bAllowDynamicMaterials)
		{
			SurfaceMID = PhotosphereSurface->CreateDynamicMaterialInstance(0, StarSurfaceMaterial);
		}
	}
	if (StarCoronaMaterial && CoronaShell)
	{
		CoronaShell->SetMaterial(0, StarCoronaMaterial);
	}
	if (StarCoronaMaterial && CoronaSurface)
	{
		CoronaSurface->SetMaterial(0, StarCoronaMaterial);
		if (bAllowDynamicMaterials)
		{
			CoronaMID = CoronaSurface->CreateDynamicMaterialInstance(0, StarCoronaMaterial);
		}
	}
	if (StarHaloMaterial && StarHaloBillboard)
	{
		StarHaloBillboard->SetMaterial(0, StarHaloMaterial);
		if (bAllowDynamicMaterials)
		{
			HaloMID = StarHaloBillboard->CreateDynamicMaterialInstance(0, StarHaloMaterial);
		}
	}
	if (StarProminenceMaterial && ProminenceShell)
	{
		ProminenceShell->SetStaticMesh(nullptr);
		ProminenceShell->SetMaterial(0, nullptr);
		if (bAllowDynamicMaterials)
		{
			ProminenceMID = nullptr;
		}
	}
	if (bAllowDynamicMaterials)
	{
		RefreshEjectionMaterialInstances();
	}

	ApplyPresetToComponents(FindPreset(StellarClass));
}

void ASectorStarAnchorActor::SetStellarClassByIndex(int32 InClassIndex)
{
	const TArray<EStargameStellarClass> Classes = GetOrderedClasses();
	if (Classes.Num() == 0)
	{
		return;
	}
	const int32 WrappedIndex = (InClassIndex % Classes.Num() + Classes.Num()) % Classes.Num();
	ApplyStellarClass(Classes[WrappedIndex]);
}

void ASectorStarAnchorActor::CycleNextStellarClass()
{
	SetStellarClassByIndex(GetOrderedClasses().IndexOfByKey(StellarClass) + 1);
}

void ASectorStarAnchorActor::CyclePreviousStellarClass()
{
	SetStellarClassByIndex(GetOrderedClasses().IndexOfByKey(StellarClass) - 1);
}

FText ASectorStarAnchorActor::GetStellarClassLabel() const
{
	switch (StellarClass)
	{
	case EStargameStellarClass::O: return FText::FromString(TEXT("Class O - blue superhot"));
	case EStargameStellarClass::B: return FText::FromString(TEXT("Class B - blue-white"));
	case EStargameStellarClass::A: return FText::FromString(TEXT("Class A - white"));
	case EStargameStellarClass::F: return FText::FromString(TEXT("Class F - yellow-white"));
	case EStargameStellarClass::G: return FText::FromString(TEXT("Class G - yellow"));
	case EStargameStellarClass::K: return FText::FromString(TEXT("Class K - orange"));
	case EStargameStellarClass::M: return FText::FromString(TEXT("Class M - red dwarf"));
	case EStargameStellarClass::L: return FText::FromString(TEXT("Class L - brown dwarf"));
	case EStargameStellarClass::BlueGiant: return FText::FromString(TEXT("Blue giant"));
	case EStargameStellarClass::RedGiant: return FText::FromString(TEXT("Red giant"));
	case EStargameStellarClass::WhiteDwarf: return FText::FromString(TEXT("White dwarf"));
	default: return FText::FromString(TEXT("Class G - yellow"));
	}
}

void ASectorStarAnchorActor::SetupPreviewInput()
{
	if (bUsePreviewCamera)
	{
		APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (!PlayerController)
		{
			return;
		}
		PlayerController->SetViewTarget(this);
	}
}

void ASectorStarAnchorActor::ApplyPresetToComponents(const FStargameSectorStarVisualPreset& Preset)
{
	ScrubLegacyProminenceMeshes();

	const float PhotosphereRadiusCm = BasePhotosphereRadiusCm * (bApplyClassRadiusScale ? FMath::Max(0.05f, Preset.RelativeRadius) : 1.0f);
	const float PhotosphereScale = PhotosphereRadiusCm / BasicSphereRadiusCm;
	const float CoronaScale = (PhotosphereRadiusCm * Preset.CoronaRadiusMultiplier) / BasicSphereRadiusCm;
	const float ProminenceScale = (PhotosphereRadiusCm * Preset.EjectionRadiusMultiplier) / BasicSphereRadiusCm;
	const float HaloScale = (PhotosphereRadiusCm * Preset.HaloRadiusMultiplier * 2.0f * HaloAuthoringScale) / BasicPlaneSizeCm;
	const float EjectionScale = (PhotosphereRadiusCm * Preset.EjectionRadiusMultiplier * 2.0f * HaloAuthoringScale) / BasicPlaneSizeCm;
	const float HaloRadiusCm = PhotosphereRadiusCm * Preset.HaloRadiusMultiplier * HaloAuthoringScale;
	const float EmissionScale = FMath::Max(0.01f, VisualEmissionScale);
	CurrentPhotosphereRadiusCm = PhotosphereRadiusCm;
	CurrentVisualPreset = Preset;

	if (StarBody)
	{
		StarBody->SetVisibility(false);
		StarBody->SetHiddenInGame(true);
	}
	if (PhotosphereSurface)
	{
		PhotosphereSurface->SetRelativeScale3D(FVector::OneVector);
		PhotosphereSurface->SetVisibility(true);
		PhotosphereSurface->SetHiddenInGame(false);
		RebuildPhotosphereSurface(PhotosphereRadiusCm, Preset);
	}
	if (CoronaShell)
	{
		CoronaShell->SetRelativeScale3D(FVector(CoronaScale));
		CoronaShell->SetVisibility(false);
		CoronaShell->SetHiddenInGame(true);
	}
	if (CoronaSurface)
	{
		CoronaSurface->SetRelativeScale3D(FVector::OneVector);
		const bool bVisibleCorona = bShowCorona && StarCoronaMaterial != nullptr;
		CoronaSurface->SetVisibility(bVisibleCorona);
		CoronaSurface->SetHiddenInGame(!bVisibleCorona);
		RebuildCoronaSurface(PhotosphereRadiusCm * Preset.CoronaRadiusMultiplier);
	}
	if (ProminenceShell)
	{
		ProminenceShell->SetStaticMesh(nullptr);
		ProminenceShell->SetMaterial(0, nullptr);
		ProminenceBaseScale = FVector(ProminenceScale);
		ProminenceShell->SetRelativeScale3D(ProminenceBaseScale);
		ProminenceShell->SetVisibility(false);
		ProminenceShell->SetHiddenInGame(true);
	}
	if (StarHaloBillboard)
	{
		StarHaloBillboard->SetRelativeScale3D(FVector(HaloScale, HaloScale, 1.0f));
		const bool bVisibleHalo = bShowHaloBillboard && HaloMID != nullptr;
		StarHaloBillboard->SetVisibility(bVisibleHalo);
		StarHaloBillboard->SetHiddenInGame(!bVisibleHalo);
	}

	const TArray<UStaticMeshComponent*> Ejections = GetEjectionBillboards();
	EjectionBaseScales.SetNum(Ejections.Num());
	for (int32 Index = 0; Index < Ejections.Num(); ++Index)
	{
		if (!Ejections[Index])
		{
			continue;
		}

		const float SizeJitter = 1.0f + 0.16f * static_cast<float>(Index);
		EjectionBaseScales[Index] = FVector(EjectionScale * SizeJitter, EjectionScale * (0.62f + Preset.Activity * 0.42f), 1.0f);
		Ejections[Index]->SetRelativeScale3D(EjectionBaseScales[Index]);
		Ejections[Index]->SetVisibility(false);
		Ejections[Index]->SetHiddenInGame(true);
	}

	PositionPreviewCamera(PhotosphereRadiusCm, HaloRadiusCm);
	RebuildProminenceArcs(PhotosphereRadiusCm, Preset);

	if (SurfaceMID)
	{
		SetNamedColor(SurfaceMID, { TEXT("CoreColor"), TEXT("core_color") }, Preset.CoreColor);
		SetNamedColor(SurfaceMID, { TEXT("HotColor"), TEXT("hot_color") }, Preset.HotColor);
		SetNamedColor(SurfaceMID, { TEXT("SpotColor"), TEXT("SunspotColor"), TEXT("sunspot_color") }, Preset.SpotColor);
		SetNamedTexture(SurfaceMID, { TEXT("PhotosphereMasks"), TEXT("PhotosphereMask"), TEXT("photosphere_masks") }, StarPhotosphereMaskTexture);
		SetNamedScalar(SurfaceMID, { TEXT("EmissionStrength"), TEXT("emission_strength") }, Preset.SurfaceEmission * EmissionScale);
		SetNamedScalar(SurfaceMID, { TEXT("SurfaceBrightness"), TEXT("surface_brightness") }, 2.8f);
		SetNamedScalar(SurfaceMID, { TEXT("ObjectRadius"), TEXT("object_radius") }, PhotosphereRadiusCm);
		SetNamedScalar(SurfaceMID, { TEXT("TemperatureKelvin"), TEXT("temperature_kelvin") }, Preset.TemperatureKelvin);
		SetNamedScalar(SurfaceMID, { TEXT("FlowSpeed"), TEXT("flow_speed") }, Preset.SurfaceFlowSpeed);
		SetNamedScalar(SurfaceMID, { TEXT("Activity"), TEXT("activity") }, Preset.Activity);
	}
	if (CoronaMID)
	{
		SetNamedColor(CoronaMID, { TEXT("HaloColor"), TEXT("FlameColor"), TEXT("flame_color") }, Preset.HaloColor);
		SetNamedColor(CoronaMID, { TEXT("FlareColor"), TEXT("TipColor"), TEXT("tip_color") }, Preset.FlareColor);
		SetNamedScalar(CoronaMID, { TEXT("CoronaEmission"), TEXT("HaloEmission"), TEXT("emission_strength") }, Preset.CoronaEmission * EmissionScale);
		SetNamedScalar(CoronaMID, { TEXT("FlareEmission") }, Preset.FlareEmission * EmissionScale);
		SetNamedScalar(CoronaMID, { TEXT("Activity"), TEXT("activity") }, Preset.Activity);
		SetNamedScalar(CoronaMID, { TEXT("TurbulenceSpeed"), TEXT("NoiseSpeed"), TEXT("turbulence_speed"), TEXT("noise_speed") }, Preset.CoronaFlowSpeed);
	}
	if (HaloMID)
	{
		SetNamedColor(HaloMID, { TEXT("HaloColor"), TEXT("FlameColor"), TEXT("flame_color") }, Preset.HaloColor);
		SetNamedColor(HaloMID, { TEXT("FlareColor"), TEXT("HotColor"), TEXT("hot_color"), TEXT("tip_color") }, Preset.FlareColor);
		SetNamedScalar(HaloMID, { TEXT("HaloEmission"), TEXT("EmissionStrength"), TEXT("emission_strength") }, Preset.HaloEmission * EmissionScale);
		SetNamedScalar(HaloMID, { TEXT("FlareEmission") }, Preset.FlareEmission * EmissionScale);
		SetNamedScalar(HaloMID, { TEXT("Activity"), TEXT("activity") }, Preset.Activity);
	}
	if (ProminenceMID)
	{
		SetNamedColor(ProminenceMID, { TEXT("FlareColor"), TEXT("BaseColor"), TEXT("base_color"), TEXT("flame_color") }, Preset.FlareColor);
		SetNamedColor(ProminenceMID, { TEXT("TipColor"), TEXT("tip_color") }, Preset.HaloColor);
		SetNamedScalar(ProminenceMID, { TEXT("FlareEmission"), TEXT("EmissionStrength"), TEXT("emission_strength") }, Preset.FlareEmission * EmissionScale);
		SetNamedScalar(ProminenceMID, { TEXT("Activity"), TEXT("activity") }, Preset.Activity);
	}
	for (int32 Index = 0; Index < ProminenceArcs.Num(); ++Index)
	{
		if (!ProminenceArcs[Index] || !StarProminenceMaterial)
		{
			continue;
		}

		UMaterialInstanceDynamic* ArcMID = nullptr;
		if (GetWorld() && (GetWorld()->IsGameWorld() || GetWorld()->WorldType == EWorldType::PIE))
		{
			ArcMID = ProminenceArcs[Index]->CreateDynamicMaterialInstance(0, StarProminenceMaterial);
		}
		else
		{
			ProminenceArcs[Index]->SetMaterial(0, StarProminenceMaterial);
		}
		SetNamedColor(ArcMID, { TEXT("FlareColor"), TEXT("BaseColor"), TEXT("base_color"), TEXT("flame_color") }, Preset.FlareColor);
		SetNamedColor(ArcMID, { TEXT("TipColor"), TEXT("tip_color"), TEXT("HaloColor") }, Preset.HaloColor);
		SetNamedScalar(ArcMID, { TEXT("FlareEmission"), TEXT("EmissionStrength"), TEXT("emission_strength") }, Preset.FlareEmission * FMath::Max(0.35f, EmissionScale) * (0.90f + 0.08f * Index));
		SetNamedScalar(ArcMID, { TEXT("Activity"), TEXT("activity") }, Preset.Activity);
		SetNamedScalar(ArcMID, { TEXT("SeedOffset"), TEXT("seed_offset") }, static_cast<float>(Index) * 5.37f);
	}
	for (int32 Index = 0; Index < EjectionMIDs.Num(); ++Index)
	{
		UMaterialInstanceDynamic* MID = EjectionMIDs[Index];
		if (!MID)
		{
			continue;
		}

		SetNamedColor(MID, { TEXT("FlareColor"), TEXT("BaseColor"), TEXT("base_color"), TEXT("flame_color") }, Preset.FlareColor);
		SetNamedColor(MID, { TEXT("TipColor"), TEXT("tip_color"), TEXT("HaloColor") }, Preset.HaloColor);
		SetNamedScalar(MID, { TEXT("FlareEmission"), TEXT("EmissionStrength"), TEXT("emission_strength") }, Preset.FlareEmission * EmissionScale * (0.55f + 0.12f * Index));
		SetNamedScalar(MID, { TEXT("Activity"), TEXT("activity") }, Preset.Activity);
		SetNamedScalar(MID, { TEXT("SeedOffset"), TEXT("seed_offset") }, static_cast<float>(Index) * 11.73f);
	}
	if (StarLight)
	{
		StarLight->SetLightColor(Preset.LightColor);
		StarLight->SetIntensity(Preset.LightIntensityLux * FMath::Max(0.01f, LightIntensityScale));
		StarLight->SetAtmosphereSunLight(bUseAsAtmosphereSunLight);
		StarLight->SetLightSourceAngle(FMath::Clamp(0.5357f * FMath::Sqrt(FMath::Max(0.1f, Preset.RelativeRadius)), 0.08f, 4.0f));
	}
	if (ClassLabel)
	{
		ClassLabel->SetRelativeLocation(FVector(0.0, 0.0, PhotosphereRadiusCm * 1.9f));
		ClassLabel->SetText(GetStellarClassLabel());
		ClassLabel->SetTextRenderColor(Preset.HaloColor.ToFColor(true));
		ClassLabel->SetVisibility(bShowClassLabel);
		ClassLabel->SetHiddenInGame(!bShowClassLabel);
	}
}

void ASectorStarAnchorActor::ScrubLegacyProminenceMeshes()
{
	TArray<UStaticMeshComponent*> StaticMeshComponents;
	GetComponents<UStaticMeshComponent>(StaticMeshComponents);
	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		if (!Component)
		{
			continue;
		}

		UStaticMesh* Mesh = Component->GetStaticMesh();
		const bool bIsLegacyProminence = Component == ProminenceShell || (Mesh && Mesh->GetName().Contains(TEXT("SM_StarProminenceBundle")));
		if (!bIsLegacyProminence)
		{
			continue;
		}

		Component->SetStaticMesh(nullptr);
		Component->SetMaterial(0, nullptr);
		Component->SetVisibility(false);
		Component->SetHiddenInGame(true);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
	}
}

void ASectorStarAnchorActor::AnimateStarEffects(float DeltaSeconds)
{
	StarVisualTime += DeltaSeconds;

	if (PhotosphereSurface && PhotosphereSurface->IsVisible())
	{
		PhotosphereSurface->AddLocalRotation(FRotator(0.0f, DeltaSeconds * PhotosphereYawDegreesPerSecond, DeltaSeconds * PhotosphereRollDegreesPerSecond));
#if WITH_EDITOR
		PhotosphereRefreshAccumulator += DeltaSeconds;
		const UWorld* World = GetWorld();
		const bool bEditorPreviewWorld = World && (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview);
		if (bEditorPreviewWorld && PhotosphereRefreshAccumulator >= FMath::Max(0.02f, PhotosphereRefreshInterval))
		{
			PhotosphereRefreshAccumulator = 0.0f;
			RebuildPhotosphereSurface(CurrentPhotosphereRadiusCm, CurrentVisualPreset);
		}
#endif
	}
	if (CoronaSurface && CoronaSurface->IsVisible())
	{
		CoronaSurface->AddLocalRotation(FRotator(0.0f, DeltaSeconds * CoronaYawDegreesPerSecond, DeltaSeconds * CoronaRollDegreesPerSecond));
	}
	if (ProminenceShell && ProminenceShell->IsVisible())
	{
		const float Pulse = 1.0f + 0.045f * FMath::Sin(StarVisualTime * 0.9f);
		ProminenceShell->SetRelativeScale3D(ProminenceBaseScale * Pulse);
		ProminenceShell->AddLocalRotation(FRotator(0.0f, DeltaSeconds * 0.35f, DeltaSeconds * -0.12f));
	}

	const TArray<UStaticMeshComponent*> Ejections = GetEjectionBillboards();
	for (int32 Index = 0; Index < Ejections.Num(); ++Index)
	{
		UStaticMeshComponent* Ejection = Ejections[Index];
		if (!Ejection || !Ejection->IsVisible() || !EjectionBaseScales.IsValidIndex(Index))
		{
			continue;
		}

		const float Phase = StarVisualTime * (0.65f + 0.11f * static_cast<float>(Index)) + static_cast<float>(Index) * 1.73f;
		const float WidthPulse = 1.0f + 0.10f * FMath::Sin(Phase);
		const float HeightPulse = 1.0f + 0.16f * FMath::Sin(Phase * 0.83f + 0.65f);
		const FVector BaseScale = EjectionBaseScales[Index];
		Ejection->SetRelativeScale3D(FVector(BaseScale.X * WidthPulse, BaseScale.Y * HeightPulse, BaseScale.Z));

		if (EjectionMIDs.IsValidIndex(Index))
		{
			SetNamedScalar(EjectionMIDs[Index], { TEXT("RuntimeSeconds"), TEXT("TimeOffset"), TEXT("runtime_seconds") }, StarVisualTime);
		}
	}

	SetNamedScalar(SurfaceMID, { TEXT("RuntimeSeconds"), TEXT("runtime_seconds") }, StarVisualTime);
	SetNamedScalar(CoronaMID, { TEXT("RuntimeSeconds"), TEXT("runtime_seconds") }, StarVisualTime);
	SetNamedScalar(ProminenceMID, { TEXT("RuntimeSeconds"), TEXT("runtime_seconds") }, StarVisualTime);
	for (UProceduralMeshComponent* Arc : ProminenceArcs)
	{
		if (Arc && Arc->IsVisible())
		{
			if (UMaterialInstanceDynamic* ArcMID = Cast<UMaterialInstanceDynamic>(Arc->GetMaterial(0)))
			{
				SetNamedScalar(ArcMID, { TEXT("RuntimeSeconds"), TEXT("runtime_seconds") }, StarVisualTime);
			}
		}
	}
}

void ASectorStarAnchorActor::AlignBillboardToView()
{
	AlignBillboardToView(StarHaloBillboard);
}

void ASectorStarAnchorActor::AlignBillboardToView(UStaticMeshComponent* Billboard, const FRotator& LocalRollOffset)
{
	if (!Billboard)
	{
		return;
	}

	FVector ViewLocation = PreviewCamera ? PreviewCamera->GetComponentLocation() : FVector::ZeroVector;
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			if (PlayerController->PlayerCameraManager)
			{
				ViewLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
			}
		}
	}

	const FVector DirectionToView = (ViewLocation - Billboard->GetComponentLocation()).GetSafeNormal();
	if (!DirectionToView.IsNearlyZero())
	{
		Billboard->SetWorldRotation((FRotationMatrix::MakeFromZ(DirectionToView).Rotator() + LocalRollOffset).Quaternion());
	}
}

void ASectorStarAnchorActor::RefreshEjectionMaterialInstances()
{
	EjectionMIDs.Reset();

	for (UStaticMeshComponent* Ejection : GetEjectionBillboards())
	{
		if (!Ejection || !StarProminenceMaterial)
		{
			continue;
		}

		if (UMaterialInstanceDynamic* MID = Ejection->CreateDynamicMaterialInstance(0, StarProminenceMaterial))
		{
			EjectionMIDs.Add(MID);
		}
	}
}

void ASectorStarAnchorActor::RebuildPhotosphereSurface(float PhotosphereRadiusCm, const FStargameSectorStarVisualPreset& Preset)
{
	if (!PhotosphereSurface)
	{
		return;
	}

	const int32 LatitudeSegments = FMath::Clamp(PhotosphereLatitudeSegments, 24, 384);
	const int32 LongitudeSegments = FMath::Clamp(PhotosphereLongitudeSegments, 48, 768);
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<FColor> Colors;
	Vertices.Reserve((LatitudeSegments + 1) * (LongitudeSegments + 1));
	Normals.Reserve(Vertices.Max());
	UVs.Reserve(Vertices.Max());
	Tangents.Reserve(Vertices.Max());
	Colors.Reserve(Vertices.Max());
	Triangles.Reserve(LatitudeSegments * LongitudeSegments * 6);

	for (int32 Lat = 0; Lat <= LatitudeSegments; ++Lat)
	{
		const float V = static_cast<float>(Lat) / static_cast<float>(LatitudeSegments);
		const float Theta = V * PI;
		const float SinTheta = FMath::Sin(Theta);
		const float CosTheta = FMath::Cos(Theta);

		for (int32 Lon = 0; Lon <= LongitudeSegments; ++Lon)
		{
			const float U = static_cast<float>(Lon) / static_cast<float>(LongitudeSegments);
			const float Phi = U * 2.0f * PI;
			const FVector Normal(SinTheta * FMath::Cos(Phi), SinTheta * FMath::Sin(Phi), CosTheta);
			const FVector Tangent(-FMath::Sin(Phi), FMath::Cos(Phi), 0.0f);
			const float Time = StarVisualTime * FMath::Max(0.01f, Preset.SurfaceFlowSpeed);
			const float Fine = SampleStarFbm(Normal * FMath::Max(1.0f, PhotosphereFineScale) + FVector(Time * 2.1f, -Time * 1.3f, Time * 1.5f));
			const float Micro = SampleStarFbm(Normal * FMath::Max(1.0f, PhotosphereMicroScale) + FVector(-Time * 3.3f, Time * 1.9f, Time * 2.6f));
			const float Broad = SampleStarFbm(Normal * FMath::Max(1.0f, PhotosphereBroadScale) + FVector(-Time * 0.22f, Time * 0.31f, Time * 0.13f));
			const float Mid = SampleStarFbm(Normal * FMath::Max(1.0f, PhotosphereMidScale) + FVector(Time * 0.72f, Time * 0.39f, -Time * 0.51f));
			const float SpotNoise = SampleStarFbm(Normal * 5.6f + FVector(Time * 0.11f, -Time * 0.17f, Time * 0.07f));
			const float SpotDetail = SampleStarFbm(Normal * 13.0f + FVector(-Time * 0.21f, Time * 0.08f, 2.3f));
			const float SpotMask = FMath::SmoothStep(0.58f, 0.72f, SpotNoise) * FMath::SmoothStep(0.32f, 0.66f, SpotDetail) * FMath::Clamp(Preset.Activity * 1.35f, 0.0f, 1.0f);
			const float Filament = FMath::SmoothStep(0.44f, 0.80f, Fine) * FMath::SmoothStep(0.36f, 0.78f, Mid) * FMath::SmoothStep(0.30f, 0.74f, Broad);
			const float MicroSpark = FMath::SmoothStep(0.58f, 0.86f, Micro) * (1.0f - SpotMask);
			const float CoolCell = FMath::SmoothStep(0.035f, 0.26f, FMath::Abs(Fine - Mid));
			const int32 DynamicSpotCount = FMath::Clamp(PhotosphereDynamicSunspotCount, 0, 12);
			const float SpotLifetime = FMath::Max(1.0f, PhotosphereSunspotLifetimeSeconds);
			const float SpotSharpness = FMath::Clamp(PhotosphereSunspotAngularSize, 0.2f, 8.0f) * 0.014f;
			float DynamicSpotMask = 0.0f;
			for (int32 SpotIndex = 0; SpotIndex < DynamicSpotCount; ++SpotIndex)
			{
				const float SpotOffset = static_cast<float>(SpotIndex) * 0.6180339f;
				const float SpotCycleTime = StarVisualTime / SpotLifetime + SpotOffset;
				const float SpotAge = FMath::Frac(SpotCycleTime);
				const float SpotCycle = FMath::FloorToFloat(SpotCycleTime);
				const float SpotFade = FMath::Pow(FMath::Sin(SpotAge * PI), 1.45f);
				const float SpotSeed = PhotosphereSunspotSeed + static_cast<float>(SpotIndex) * 43.13f + SpotCycle * 97.31f;
				const FVector SpotAxis = SunspotDirectionFromSeed(SpotSeed + 5.7f);
				const FVector SpotDirection = SunspotDirectionFromSeed(SpotSeed)
					.RotateAngleAxis(SpotAge * PhotosphereSunspotDriftDegreesPerSecond * SpotLifetime, SpotAxis)
					.GetSafeNormal();
				const float Dot = static_cast<float>(FVector::DotProduct(Normal, SpotDirection));
				const float SpotBody = FMath::SmoothStep(1.0f - SpotSharpness * 1.8f, 1.0f - SpotSharpness * 0.22f, Dot);
				const float SpotInteriorNoise = SampleStarFbm(Normal * (22.0f + 6.0f * SpotIndex) + FVector(SpotSeed * 0.03f, SpotAge * 2.0f, SpotSeed * 0.07f));
				DynamicSpotMask = FMath::Max(DynamicSpotMask, SpotBody * SpotFade * FMath::Lerp(0.55f, 1.0f, SpotInteriorNoise));
			}
			const float AnchoredSpotMask = FMath::Clamp(DynamicSpotMask * PhotosphereAnchoredSpotStrength, 0.0f, 1.0f);
			const float Limb = FMath::Clamp(Normal.X * 0.5f + 0.5f, 0.0f, 1.0f);
			const float LimbTone = FMath::Lerp(1.0f - FMath::Clamp(PhotosphereLimbDarkening, 0.0f, 1.0f), 1.05f, FMath::Pow(Limb, 0.32f));
			FLinearColor Color = FMath::Lerp(Preset.CoreColor * 0.28f, Preset.CoreColor * 0.90f, FMath::SmoothStep(0.16f, 0.84f, Broad * 0.65f + Mid * 0.35f));
			Color = FMath::Lerp(Color, Preset.HotColor * 0.92f, Filament * 0.55f);
			Color = FMath::Lerp(Color, Preset.HotColor * 1.12f, MicroSpark * PhotosphereMicroSparkStrength);
			Color = FMath::Lerp(Color, Preset.CoreColor * 0.13f, CoolCell * PhotosphereCoolCellStrength * (1.0f - Filament));
			Color = FMath::Lerp(Color, Preset.SpotColor * 0.78f, FMath::Max(SpotMask * PhotosphereSunspotStrength, AnchoredSpotMask));
			Color *= LimbTone;
			const float DisplacementScale = PhotosphereDisplacementCm / 54.0f;
			const float Displacement = ((Broad - 0.5f) * 30.0f + (Mid - 0.5f) * 16.0f + (Fine - 0.5f) * 8.0f) * DisplacementScale;
			Vertices.Add(Normal * (PhotosphereRadiusCm + Displacement));
			Normals.Add(Normal);
			UVs.Add(FVector2D(U, V));
			Tangents.Add(FProcMeshTangent(Tangent, false));
			Colors.Add(Color.ToFColor(false));
		}
	}

	for (int32 Lat = 0; Lat < LatitudeSegments; ++Lat)
	{
		for (int32 Lon = 0; Lon < LongitudeSegments; ++Lon)
		{
			const int32 A = Lat * (LongitudeSegments + 1) + Lon;
			const int32 B = A + LongitudeSegments + 1;
			Triangles.Add(A);
			Triangles.Add(B);
			Triangles.Add(A + 1);
			Triangles.Add(A + 1);
			Triangles.Add(B);
			Triangles.Add(B + 1);
		}
	}

	PhotosphereSurface->ClearAllMeshSections();
	PhotosphereSurface->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
	if (StarSurfaceMaterial)
	{
		PhotosphereSurface->SetMaterial(0, StarSurfaceMaterial);
	}
}

void ASectorStarAnchorActor::RebuildCoronaSurface(float CoronaRadiusCm)
{
	if (!CoronaSurface)
	{
		return;
	}

	constexpr int32 LatitudeSegments = 64;
	constexpr int32 LongitudeSegments = 128;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<FColor> Colors;
	Vertices.Reserve((LatitudeSegments + 1) * (LongitudeSegments + 1));
	Normals.Reserve(Vertices.Max());
	UVs.Reserve(Vertices.Max());
	Tangents.Reserve(Vertices.Max());
	Colors.Reserve(Vertices.Max());
	Triangles.Reserve(LatitudeSegments * LongitudeSegments * 6);

	for (int32 Lat = 0; Lat <= LatitudeSegments; ++Lat)
	{
		const float V = static_cast<float>(Lat) / static_cast<float>(LatitudeSegments);
		const float Theta = V * PI;
		const float SinTheta = FMath::Sin(Theta);
		const float CosTheta = FMath::Cos(Theta);

		for (int32 Lon = 0; Lon <= LongitudeSegments; ++Lon)
		{
			const float U = static_cast<float>(Lon) / static_cast<float>(LongitudeSegments);
			const float Phi = U * 2.0f * PI;
			const FVector Normal(SinTheta * FMath::Cos(Phi), SinTheta * FMath::Sin(Phi), CosTheta);
			const FVector Tangent(-FMath::Sin(Phi), FMath::Cos(Phi), 0.0f);
			Vertices.Add(Normal * CoronaRadiusCm);
			Normals.Add(Normal);
			UVs.Add(FVector2D(U, V));
			Tangents.Add(FProcMeshTangent(Tangent, false));
			Colors.Add(FColor::White);
		}
	}

	for (int32 Lat = 0; Lat < LatitudeSegments; ++Lat)
	{
		for (int32 Lon = 0; Lon < LongitudeSegments; ++Lon)
		{
			const int32 A = Lat * (LongitudeSegments + 1) + Lon;
			const int32 B = A + LongitudeSegments + 1;
			Triangles.Add(A);
			Triangles.Add(B);
			Triangles.Add(A + 1);
			Triangles.Add(A + 1);
			Triangles.Add(B);
			Triangles.Add(B + 1);
		}
	}

	CoronaSurface->ClearAllMeshSections();
	CoronaSurface->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
	if (StarCoronaMaterial)
	{
		CoronaSurface->SetMaterial(0, StarCoronaMaterial);
	}
}

void ASectorStarAnchorActor::RebuildProminenceArcs(float PhotosphereRadiusCm, const FStargameSectorStarVisualPreset& Preset)
{
	TArray<UProceduralMeshComponent*> ArcComponents;
	ArcComponents.Reserve(MaxProminenceArcs);
	bool bNeedsDiscoveredArcs = ProminenceArcs.Num() == 0;
	for (UProceduralMeshComponent* Arc : ProminenceArcs)
	{
		if (!Arc)
		{
			bNeedsDiscoveredArcs = true;
			continue;
		}
		ArcComponents.Add(Arc);
	}
	if (bNeedsDiscoveredArcs || ArcComponents.Num() < MaxProminenceArcs)
	{
		TArray<UProceduralMeshComponent*> DiscoveredComponents;
		GetComponents<UProceduralMeshComponent>(DiscoveredComponents);
		DiscoveredComponents.RemoveAll([](const UProceduralMeshComponent* Component)
		{
			return !Component || !Component->GetName().StartsWith(TEXT("ProminenceArc"));
		});
		DiscoveredComponents.Sort([](const UProceduralMeshComponent& Left, const UProceduralMeshComponent& Right)
		{
			return Left.GetName() < Right.GetName();
		});
		if (DiscoveredComponents.Num() > 0)
		{
			ArcComponents = MoveTemp(DiscoveredComponents);
			ProminenceArcs.Reset(ArcComponents.Num());
			for (UProceduralMeshComponent* Arc : ArcComponents)
			{
				ProminenceArcs.Add(Arc);
			}
		}
	}

	const bool bVisible = bShowProminences && StarProminenceMaterial != nullptr && Preset.Activity > 0.05f;
	const int32 VisibleCount = bVisible ? FMath::Clamp(ProminenceArcCount, 0, ArcComponents.Num()) : 0;
	constexpr int32 SegmentCount = 34;
	const float SafeRadiusCm = FMath::Max(1.0f, FiniteOrDefault(PhotosphereRadiusCm, BasePhotosphereRadiusCm));
	const float SafeHeight = FMath::Clamp(FiniteOrDefault(ProminenceArcHeight, 0.38f), 0.01f, 0.95f);
	const float SafeWidth = FMath::Clamp(FiniteOrDefault(ProminenceArcWidth, 0.09f), 0.001f, 0.14f);
	const float SafeSpanDegrees = FMath::Clamp(FiniteOrDefault(ProminenceArcSpanDegrees, 34.0f), 2.0f, 44.0f);
	const float SafeCurl = FMath::Clamp(FiniteOrDefault(ProminenceArcCurl, 0.10f), 0.0f, 0.35f);
	const float SafeSeed = FiniteOrDefault(ProminenceSeed, 11.0f);
	const float SafeActivity = FMath::Clamp(FiniteOrDefault(Preset.Activity, 0.55f), 0.0f, 1.0f);
	const float SafeTime = FiniteOrDefault(StarVisualTime, 0.0f);
	FVector CameraVector = PreviewCamera ? PreviewCamera->GetRelativeLocation() : FVector::ZeroVector;
	if (CameraVector.IsNearlyZero())
	{
		CameraVector = FVector(1.0f, 0.0f, 0.22f);
	}
	const FVector CameraOut = CameraVector.GetSafeNormal();
	const FVector LimbVertical = (FVector::UpVector - CameraOut * FVector::DotProduct(FVector::UpVector, CameraOut)).GetSafeNormal();
	const FVector LimbHorizontal = FVector::CrossProduct(LimbVertical, CameraOut).GetSafeNormal();

	for (int32 ArcIndex = 0; ArcIndex < ArcComponents.Num(); ++ArcIndex)
	{
		UProceduralMeshComponent* Arc = ArcComponents[ArcIndex];
		if (!Arc)
		{
			continue;
		}

		Arc->ClearAllMeshSections();
		const bool bThisVisible = ArcIndex < VisibleCount;
		Arc->SetVisibility(bThisVisible);
		Arc->SetHiddenInGame(!bThisVisible);
		if (!bThisVisible)
		{
			continue;
		}

		const float Seed = SafeSeed + static_cast<float>(ArcIndex) * 41.731f;
		static constexpr float AnchorAnglesDegrees[] = { -42.0f, 38.0f, 128.0f, -132.0f, 6.0f, 82.0f, -92.0f, 165.0f };
		const float AnchorAngle = FMath::DegreesToRadians(AnchorAnglesDegrees[ArcIndex % UE_ARRAY_COUNT(AnchorAnglesDegrees)] + (StarHash(Seed + 7.0f) - 0.5f) * 14.0f);
		const FVector Anchor = (LimbHorizontal * FMath::Cos(AnchorAngle) + LimbVertical * FMath::Sin(AnchorAngle)).GetSafeNormal();
		FVector LimbTangent = (-LimbHorizontal * FMath::Sin(AnchorAngle) + LimbVertical * FMath::Cos(AnchorAngle)).GetSafeNormal();
		if (FVector::DotProduct(LimbTangent, FVector::UpVector) < -0.15f)
		{
			LimbTangent *= -1.0f;
		}
		const FVector CurlDirection = FVector::CrossProduct(Anchor, LimbTangent).GetSafeNormal();

		const float SpanRadians = FMath::DegreesToRadians(SafeSpanDegrees * (0.86f + 0.30f * StarHash(Seed + 19.0f)));
		const float Height = SafeHeight * (0.78f + 0.58f * StarHash(Seed + 31.0f)) * FMath::Lerp(0.86f, 1.36f, SafeActivity);
		const float WidthCm = SafeRadiusCm * SafeWidth * (0.70f + 0.65f * StarHash(Seed + 53.0f));
		const float CurlAmount = SafeCurl * (0.55f + 0.90f * StarHash(Seed + 71.0f));
		const float ActivityFade = FMath::Clamp(SafeActivity * 1.35f, 0.0f, 1.0f);

		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FProcMeshTangent> Tangents;
		TArray<FColor> Colors;
		Vertices.Reserve((SegmentCount + 1) * 2);
		Normals.Reserve((SegmentCount + 1) * 2);
		UVs.Reserve((SegmentCount + 1) * 2);
		Tangents.Reserve((SegmentCount + 1) * 2);
		Colors.Reserve((SegmentCount + 1) * 2);
		Triangles.Reserve(SegmentCount * 6);

		for (int32 Segment = 0; Segment <= SegmentCount; ++Segment)
		{
			const float Alpha = static_cast<float>(Segment) / static_cast<float>(SegmentCount);
			const float ArcPosition = (Alpha - 0.5f) * SpanRadians;
			const float Lift = FMath::Pow(FMath::Max(0.0f, FMath::Sin(Alpha * PI)), 0.78f);
			const float BaseNoise = FMath::Sin(Alpha * 13.7f + Seed * 0.11f + SafeTime * 0.22f);
			const float FineNoise = FMath::Sin(Alpha * 31.0f + Seed * 0.37f - SafeTime * 0.17f);
			const FVector SurfaceDirection = (Anchor + LimbTangent * ArcPosition + CurlDirection * CurlAmount * Lift * BaseNoise).GetSafeNormal();
			const float RootTether = FMath::SmoothStep(0.0f, 0.16f, Alpha) * FMath::SmoothStep(0.0f, 0.16f, 1.0f - Alpha);
			const float LiftHeight = Height * Lift * (0.88f + 0.12f * FineNoise);
			const FVector CurlOffset = CurlDirection * SafeRadiusCm * CurlAmount * 0.42f * FMath::Sin(Alpha * 2.0f * PI + Seed * 0.13f) * RootTether;
			const FVector Center = Anchor * SafeRadiusCm * (1.04f + LiftHeight)
				+ LimbTangent * SafeRadiusCm * ArcPosition
				+ CameraOut * SafeRadiusCm * 0.035f
				+ CurlOffset;

			FVector RibbonSide = FVector::CrossProduct(CameraOut, LimbTangent).GetSafeNormal();
			if (RibbonSide.IsNearlyZero())
			{
				RibbonSide = FVector::CrossProduct(CameraOut, SurfaceDirection).GetSafeNormal();
			}
			const FVector Normal = SurfaceDirection;
			const float TipTaper = FMath::Lerp(0.82f, 0.34f, FMath::Abs(Alpha - 0.5f) * 2.0f);
			const float Width = WidthCm * TipTaper * (0.72f + Lift * 0.74f) * (0.93f + 0.10f * FineNoise);
			const FVector Left = Center - RibbonSide * Width;
			const FVector Right = Center + RibbonSide * Width;
			const FLinearColor VertexColor = FMath::Lerp(Preset.FlareColor, Preset.HaloColor, Lift * 0.85f);
			const float VertexAlpha = ActivityFade * RootTether * FMath::Lerp(0.70f, 1.0f, Lift);

			Vertices.Add(Left);
			Vertices.Add(Right);
			Normals.Add(Normal);
			Normals.Add(Normal);
			UVs.Add(FVector2D(Alpha, 0.0f));
			UVs.Add(FVector2D(Alpha, 1.0f));
			Tangents.Add(FProcMeshTangent(LimbTangent, false));
			Tangents.Add(FProcMeshTangent(LimbTangent, false));
			Colors.Add(FLinearColor(VertexColor.R, VertexColor.G, VertexColor.B, VertexAlpha).ToFColor(false));
			Colors.Add(FLinearColor(VertexColor.R, VertexColor.G, VertexColor.B, VertexAlpha).ToFColor(false));
		}

		bool bHasInvalidVertex = false;
		for (const FVector& Vertex : Vertices)
		{
			if (Vertex.ContainsNaN())
			{
				bHasInvalidVertex = true;
				break;
			}
		}
		if (bHasInvalidVertex)
		{
			Arc->ClearAllMeshSections();
			Arc->SetVisibility(false);
			Arc->SetHiddenInGame(true);
			continue;
		}

		for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
		{
			const int32 Base = Segment * 2;
			Triangles.Add(Base);
			Triangles.Add(Base + 2);
			Triangles.Add(Base + 1);
			Triangles.Add(Base + 1);
			Triangles.Add(Base + 2);
			Triangles.Add(Base + 3);
		}

		Arc->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
		UMaterialInstanceDynamic* ArcMID = Arc->CreateDynamicMaterialInstance(0, StarProminenceMaterial);
		SetNamedColor(ArcMID, { TEXT("FlareColor"), TEXT("BaseColor"), TEXT("base_color"), TEXT("flame_color") }, Preset.FlareColor);
		SetNamedColor(ArcMID, { TEXT("TipColor"), TEXT("tip_color"), TEXT("HaloColor") }, Preset.HaloColor);
		SetNamedScalar(ArcMID, { TEXT("FlareEmission"), TEXT("EmissionStrength"), TEXT("emission_strength") }, Preset.FlareEmission * FMath::Max(0.65f, VisualEmissionScale) * (1.05f + 0.12f * ArcIndex));
		SetNamedScalar(ArcMID, { TEXT("Activity"), TEXT("activity") }, Preset.Activity);
		SetNamedScalar(ArcMID, { TEXT("SeedOffset"), TEXT("seed_offset") }, static_cast<float>(ArcIndex) * 5.37f);
		Arc->SetTranslucentSortPriority(10 + ArcIndex);
		Arc->MarkRenderStateDirty();
	}
}

void ASectorStarAnchorActor::PositionPreviewCamera(float PhotosphereRadiusCm, float HaloRadiusCm)
{
	if (!PreviewCamera)
	{
		return;
	}

	const float FramingRadiusCm = FMath::Max(PhotosphereRadiusCm * 1.35f, HaloRadiusCm);
	const float CameraDistanceCm = FMath::Clamp(FramingRadiusCm * 2.85f, 9000.0f, 750000.0f);
	const float CameraHeightCm = FMath::Clamp(PhotosphereRadiusCm * 0.55f, 1400.0f, CameraDistanceCm * 0.28f);
	const FVector CameraLocation(CameraDistanceCm, 0.0f, CameraHeightCm);

	PreviewCamera->SetRelativeLocation(CameraLocation);
	const FVector DirectionToStar = (-CameraLocation).GetSafeNormal();
	if (!DirectionToStar.IsNearlyZero())
	{
		PreviewCamera->SetRelativeRotation(FRotationMatrix::MakeFromX(DirectionToStar).Rotator());
	}
}

void ASectorStarAnchorActor::SetNamedScalar(UMaterialInstanceDynamic* MID, const TArray<FName>& Names, float Value) const
{
	if (!MID)
	{
		return;
	}

	for (const FName& Name : Names)
	{
		MID->SetScalarParameterValue(Name, Value);
	}
}

void ASectorStarAnchorActor::SetNamedColor(UMaterialInstanceDynamic* MID, const TArray<FName>& Names, const FLinearColor& Value) const
{
	if (!MID)
	{
		return;
	}

	for (const FName& Name : Names)
	{
		MID->SetVectorParameterValue(Name, Value);
	}
}

void ASectorStarAnchorActor::SetNamedTexture(UMaterialInstanceDynamic* MID, const TArray<FName>& Names, UTexture* Value) const
{
	if (!MID || !Value)
	{
		return;
	}

	for (const FName& Name : Names)
	{
		MID->SetTextureParameterValue(Name, Value);
	}
}

FStargameSectorStarVisualPreset ASectorStarAnchorActor::FindPreset(EStargameStellarClass InClass) const
{
	if (const FStargameSectorStarVisualPreset* Preset = StellarClassPresets.Find(InClass))
	{
		return *Preset;
	}
	if (const FStargameSectorStarVisualPreset* Preset = StellarClassPresets.Find(EStargameStellarClass::G))
	{
		return *Preset;
	}
	return FStargameSectorStarVisualPreset();
}

TArray<UStaticMeshComponent*> ASectorStarAnchorActor::GetEjectionBillboards() const
{
	return {
		EjectionBillboardA,
		EjectionBillboardB,
		EjectionBillboardC,
		EjectionBillboardD,
	};
}

TArray<EStargameStellarClass> ASectorStarAnchorActor::GetOrderedClasses()
{
	return {
		EStargameStellarClass::O,
		EStargameStellarClass::B,
		EStargameStellarClass::A,
		EStargameStellarClass::F,
		EStargameStellarClass::G,
		EStargameStellarClass::K,
		EStargameStellarClass::M,
		EStargameStellarClass::L,
		EStargameStellarClass::BlueGiant,
		EStargameStellarClass::RedGiant,
		EStargameStellarClass::WhiteDwarf,
	};
}
