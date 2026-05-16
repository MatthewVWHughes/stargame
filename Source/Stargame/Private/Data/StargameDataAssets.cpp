#include "Data/StargameDataAssets.h"

const FPrimaryAssetType UStarCatalogAsset::AssetType(TEXT("StarCatalog"));
const FPrimaryAssetType UStartProfileAsset::AssetType(TEXT("StartProfile"));
const FPrimaryAssetType UStarSystemDefinitionAsset::AssetType(TEXT("StarSystemDefinition"));
const FPrimaryAssetType UBodyVisualProfileAsset::AssetType(TEXT("BodyVisualProfile"));
const FPrimaryAssetType UAtmosphereProfileAsset::AssetType(TEXT("AtmosphereProfile"));
const FPrimaryAssetType UStationProfileAsset::AssetType(TEXT("StationProfile"));
const FPrimaryAssetType UGateProfileAsset::AssetType(TEXT("GateProfile"));
const FPrimaryAssetType UShipMovementProfileAsset::AssetType(TEXT("ShipMovementProfile"));
const FPrimaryAssetType UShipDurabilityProfileAsset::AssetType(TEXT("ShipDurabilityProfile"));
const FPrimaryAssetType UShipLoadoutProfileAsset::AssetType(TEXT("ShipLoadoutProfile"));
const FPrimaryAssetType UShipResourceProfileAsset::AssetType(TEXT("ShipResourceProfile"));
const FPrimaryAssetType UShipArchetypeDataAsset::AssetType(TEXT("ShipArchetype"));

namespace
{
	FPrimaryAssetId MakeStargameAssetId(FPrimaryAssetType AssetType, FName GameplayId, const UObject* Object)
	{
		return FPrimaryAssetId(AssetType, GameplayId.IsNone() && Object ? FName(Object->GetFName()) : GameplayId);
	}
}

FPrimaryAssetId UStarCatalogAsset::GetPrimaryAssetId() const
{
	return MakeStargameAssetId(AssetType, CatalogId, this);
}

FPrimaryAssetId UStartProfileAsset::GetPrimaryAssetId() const
{
	return MakeStargameAssetId(AssetType, Definition.StartProfileId, this);
}

FPrimaryAssetId UStarSystemDefinitionAsset::GetPrimaryAssetId() const
{
	return MakeStargameAssetId(AssetType, Definition.SystemId, this);
}

FPrimaryAssetId UBodyVisualProfileAsset::GetPrimaryAssetId() const
{
	return MakeStargameAssetId(AssetType, VisualProfileId, this);
}

FPrimaryAssetId UAtmosphereProfileAsset::GetPrimaryAssetId() const
{
	return MakeStargameAssetId(AssetType, AtmosphereProfileId, this);
}

FPrimaryAssetId UStationProfileAsset::GetPrimaryAssetId() const
{
	return MakeStargameAssetId(AssetType, StationProfileId, this);
}

FPrimaryAssetId UGateProfileAsset::GetPrimaryAssetId() const
{
	return MakeStargameAssetId(AssetType, GateProfileId, this);
}

FPrimaryAssetId UShipMovementProfileAsset::GetPrimaryAssetId() const
{
	return MakeStargameAssetId(AssetType, Definition.MovementProfileId, this);
}

FPrimaryAssetId UShipDurabilityProfileAsset::GetPrimaryAssetId() const
{
	return MakeStargameAssetId(AssetType, Definition.DurabilityProfileId, this);
}

FPrimaryAssetId UShipLoadoutProfileAsset::GetPrimaryAssetId() const
{
	return MakeStargameAssetId(AssetType, Definition.LoadoutProfileId, this);
}

FPrimaryAssetId UShipResourceProfileAsset::GetPrimaryAssetId() const
{
	return MakeStargameAssetId(AssetType, Definition.ResourceProfileId, this);
}

FPrimaryAssetId UShipArchetypeDataAsset::GetPrimaryAssetId() const
{
	return MakeStargameAssetId(AssetType, Definition.ShipArchetypeId, this);
}
