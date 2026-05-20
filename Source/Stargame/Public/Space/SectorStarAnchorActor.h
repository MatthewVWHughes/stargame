#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SectorStarAnchorActor.generated.h"

class UCameraComponent;
class UDirectionalLightComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class UProceduralMeshComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UTexture;

UENUM(BlueprintType)
enum class EStargameStellarClass : uint8
{
	O UMETA(DisplayName = "O"),
	B UMETA(DisplayName = "B"),
	A UMETA(DisplayName = "A"),
	F UMETA(DisplayName = "F"),
	G UMETA(DisplayName = "G"),
	K UMETA(DisplayName = "K"),
	M UMETA(DisplayName = "M"),
	L UMETA(DisplayName = "L"),
	BlueGiant UMETA(DisplayName = "Blue Giant"),
	RedGiant UMETA(DisplayName = "Red Giant"),
	WhiteDwarf UMETA(DisplayName = "White Dwarf"),
};

USTRUCT(BlueprintType)
struct STARGAME_API FStargameSectorStarVisualPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star")
	FLinearColor CoreColor = FLinearColor(1.0f, 0.85f, 0.40f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star")
	FLinearColor HotColor = FLinearColor(1.0f, 0.50f, 0.10f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star")
	FLinearColor SpotColor = FLinearColor(0.45f, 0.14f, 0.03f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star")
	FLinearColor HaloColor = FLinearColor(1.0f, 0.78f, 0.30f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star")
	FLinearColor FlareColor = FLinearColor(1.0f, 0.38f, 0.08f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star")
	FLinearColor LightColor = FLinearColor(1.0f, 0.95f, 0.80f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "1000.0", Units = "K"))
	float TemperatureKelvin = 5778.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.01"))
	float RelativeRadius = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.01"))
	float RelativeLuminosity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.0"))
	float SurfaceEmission = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.0"))
	float CoronaEmission = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.0"))
	float HaloEmission = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.0"))
	float FlareEmission = 11.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.0", Units = "lux"))
	float LightIntensityLux = 120000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.0"))
	float Activity = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.1"))
	float CoronaRadiusMultiplier = 1.38f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.1"))
	float HaloRadiusMultiplier = 4.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.1"))
	float EjectionRadiusMultiplier = 2.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.0"))
	float SurfaceFlowSpeed = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.0"))
	float CoronaFlowSpeed = 0.05f;
};

UCLASS(Blueprintable)
class STARGAME_API ASectorStarAnchorActor : public AActor
{
	GENERATED_BODY()

public:
	ASectorStarAnchorActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintCallable, Category = "Stargame|Star")
	void ApplyStellarClass(EStargameStellarClass InClass);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Star")
	void SetStellarClassByIndex(int32 InClassIndex);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Star")
	void CycleNextStellarClass();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Star")
	void CyclePreviousStellarClass();

	UFUNCTION(BlueprintPure, Category = "Stargame|Star")
	EStargameStellarClass GetStellarClass() const { return StellarClass; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Star")
	FText GetStellarClassLabel() const;

protected:
	virtual void SetupPreviewInput();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StarBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CoronaShell;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> PhotosphereSurface;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> CoronaSurface;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StarHaloBillboard;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ProminenceShell;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UProceduralMeshComponent>> ProminenceArcs;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> EjectionBillboardA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> EjectionBillboardB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> EjectionBillboardC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> EjectionBillboardD;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDirectionalLightComponent> StarLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> ClassLabel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> PreviewCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPostProcessComponent> PreviewPostProcess;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star")
	EStargameStellarClass StellarClass = EStargameStellarClass::G;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star")
	TMap<EStargameStellarClass, FStargameSectorStarVisualPreset> StellarClassPresets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star")
	TObjectPtr<UMaterialInterface> StarSurfaceMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star")
	TObjectPtr<UTexture> StarPhotosphereMaskTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star")
	TObjectPtr<UMaterialInterface> StarCoronaMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star")
	TObjectPtr<UMaterialInterface> StarHaloMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star")
	TObjectPtr<UMaterialInterface> StarProminenceMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star", meta = (ClampMin = "1.0"))
	float BasePhotosphereRadiusCm = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star")
	bool bApplyClassRadiusScale = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star", meta = (ClampMin = "0.01", ClampMax = "4.0"))
	float VisualEmissionScale = 0.09f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star", meta = (ClampMin = "0.01", ClampMax = "4.0"))
	float LightIntensityScale = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float HaloAuthoringScale = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star")
	bool bShowCorona = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star")
	bool bShowHaloBillboard = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star")
	bool bShowProminences = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star")
	bool bShowEjections = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star")
	bool bShowClassLabel = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star")
	bool bUseAsAtmosphereSunLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Preview")
	bool bEnablePreviewControls = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Preview")
	bool bUsePreviewCamera = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "24", ClampMax = "384"))
	int32 PhotosphereLatitudeSegments = 192;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "48", ClampMax = "768"))
	int32 PhotosphereLongitudeSegments = 384;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "0.02", ClampMax = "2.0", Units = "s"))
	float PhotosphereRefreshInterval = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "-10.0", ClampMax = "10.0", Units = "deg/s"))
	float PhotosphereYawDegreesPerSecond = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "-10.0", ClampMax = "10.0", Units = "deg/s"))
	float PhotosphereRollDegreesPerSecond = 0.025f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "1.0", ClampMax = "16.0"))
	float PhotosphereBroadScale = 4.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "4.0", ClampMax = "64.0"))
	float PhotosphereMidScale = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "8.0", ClampMax = "192.0"))
	float PhotosphereFineScale = 72.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "16.0", ClampMax = "384.0"))
	float PhotosphereMicroScale = 156.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PhotosphereMicroSparkStrength = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PhotosphereCoolCellStrength = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PhotosphereSunspotStrength = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PhotosphereAnchoredSpotStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "0.0", ClampMax = "9999.0"))
	float PhotosphereSunspotSeed = 37.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "-5.0", ClampMax = "5.0", Units = "deg/s"))
	float PhotosphereSunspotDriftDegreesPerSecond = 0.018f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "0", ClampMax = "12"))
	int32 PhotosphereDynamicSunspotCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "1.0", ClampMax = "120.0", Units = "s"))
	float PhotosphereSunspotLifetimeSeconds = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "0.2", ClampMax = "8.0"))
	float PhotosphereSunspotAngularSize = 2.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PhotosphereLimbDarkening = 0.58f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Photosphere", meta = (ClampMin = "0.0", ClampMax = "120.0", Units = "cm"))
	float PhotosphereDisplacementCm = 54.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Corona", meta = (ClampMin = "-10.0", ClampMax = "10.0", Units = "deg/s"))
	float CoronaYawDegreesPerSecond = -0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Corona", meta = (ClampMin = "-10.0", ClampMax = "10.0", Units = "deg/s"))
	float CoronaRollDegreesPerSecond = 0.09f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Prominences", meta = (ClampMin = "0", ClampMax = "24"))
	int32 ProminenceArcCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Prominences", meta = (ClampMin = "0.01", ClampMax = "0.95"))
	float ProminenceArcHeight = 0.38f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Prominences", meta = (ClampMin = "0.001", ClampMax = "0.14"))
	float ProminenceArcWidth = 0.09f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Prominences", meta = (ClampMin = "2.0", ClampMax = "44.0", Units = "deg"))
	float ProminenceArcSpanDegrees = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Prominences", meta = (ClampMin = "0.0", ClampMax = "0.35"))
	float ProminenceArcCurl = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stargame|Star|Prominences", meta = (ClampMin = "0.0", ClampMax = "9999.0"))
	float ProminenceSeed = 11.0f;

private:
	void ApplyPresetToComponents(const FStargameSectorStarVisualPreset& Preset);
	void AnimateStarEffects(float DeltaSeconds);
	void AlignBillboardToView();
	void AlignBillboardToView(UStaticMeshComponent* Billboard, const FRotator& LocalRollOffset = FRotator::ZeroRotator);
	void RefreshEjectionMaterialInstances();
	void ScrubLegacyProminenceMeshes();
	void RebuildPhotosphereSurface(float PhotosphereRadiusCm, const FStargameSectorStarVisualPreset& Preset);
	void RebuildCoronaSurface(float CoronaRadiusCm);
	void RebuildProminenceArcs(float PhotosphereRadiusCm, const FStargameSectorStarVisualPreset& Preset);
	void PositionPreviewCamera(float PhotosphereRadiusCm, float HaloRadiusCm);
	void SetNamedScalar(UMaterialInstanceDynamic* MID, const TArray<FName>& Names, float Value) const;
	void SetNamedColor(UMaterialInstanceDynamic* MID, const TArray<FName>& Names, const FLinearColor& Value) const;
	void SetNamedTexture(UMaterialInstanceDynamic* MID, const TArray<FName>& Names, UTexture* Value) const;
	FStargameSectorStarVisualPreset FindPreset(EStargameStellarClass InClass) const;
	TArray<UStaticMeshComponent*> GetEjectionBillboards() const;
	static TArray<EStargameStellarClass> GetOrderedClasses();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CoronaMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HaloMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ProminenceMID;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> EjectionMIDs;

	float StarVisualTime = 0.0f;
	float PhotosphereRefreshAccumulator = 0.0f;
	float CurrentPhotosphereRadiusCm = 2000.0f;
	FStargameSectorStarVisualPreset CurrentVisualPreset;
	FVector ProminenceBaseScale = FVector::OneVector;
	TArray<FVector> EjectionBaseScales;
};
