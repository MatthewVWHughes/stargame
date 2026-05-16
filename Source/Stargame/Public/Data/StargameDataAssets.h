#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "Engine/DataAsset.h"
#include "StargameDataAssets.generated.h"

USTRUCT(BlueprintType)
struct STARGAME_API FStarCatalogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	FName SystemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	FName StellarClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	FName CatalogSource;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	FVector GalacticPositionLy = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	bool bAuthored = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	bool bDiscoverable = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	bool bInitiallyKnown = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	FPrimaryAssetId SystemDefinitionAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	int32 GenerationSeed = 0;
};

USTRUCT(BlueprintType)
struct STARGAME_API FRouteGraphEdge
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	FName SourceSystemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	FName DestinationSystemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	FName RouteType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	double DistanceLy = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	bool bArtificialGate = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	bool bInitiallyKnown = false;
};

UCLASS(BlueprintType)
class STARGAME_API UStarCatalogAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	FName CatalogId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	TArray<FStarCatalogEntry> Systems;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	TArray<FRouteGraphEdge> RouteEdges;
};

UCLASS(BlueprintType)
class STARGAME_API UStartProfileAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Profile")
	FStartProfileDefinition Definition;
};

UCLASS(BlueprintType)
class STARGAME_API UStarSystemDefinitionAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
	FStarSystemDefinition Definition;
};

UCLASS(BlueprintType)
class STARGAME_API UBodyVisualProfileAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	FName VisualProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	FName VisualKind;
};

UCLASS(BlueprintType)
class STARGAME_API UAtmosphereProfileAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	FName AtmosphereProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	FName AtmosphereKind;
};

UCLASS(BlueprintType)
class STARGAME_API UStationProfileAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	FName StationProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	FName StationRole;
};

UCLASS(BlueprintType)
class STARGAME_API UGateProfileAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	FName GateProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	FName GateKind;
};

UCLASS(BlueprintType)
class STARGAME_API UShipMovementProfileAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FShipMovementProfileDefinition Definition;
};

UCLASS(BlueprintType)
class STARGAME_API UShipDurabilityProfileAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FShipDurabilityProfileDefinition Definition;
};

UCLASS(BlueprintType)
class STARGAME_API UShipLoadoutProfileAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FShipLoadoutProfileDefinition Definition;
};

UCLASS(BlueprintType)
class STARGAME_API UShipResourceProfileAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FShipResourceProfileDefinition Definition;
};

UCLASS(BlueprintType)
class STARGAME_API UShipArchetypeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FShipArchetypeDefinition Definition;
};
