#include "Space/SystemicGameplayQueryService.h"
#include "Space/OrbitRouteFrameQueryService.h"

namespace
{
	template <typename T, typename PredicateType>
	T* FindMutable(TArray<T>& Items, PredicateType Predicate)
	{
		return Items.FindByPredicate(Predicate);
	}

	template <typename T, typename PredicateType>
	const T* FindConst(const TArray<T>& Items, PredicateType Predicate)
	{
		return Items.FindByPredicate(Predicate);
	}

	FName MakeId(const TCHAR* Prefix, FName SourceId)
	{
		return FName(*FString::Printf(TEXT("%s_%s"), Prefix, *SourceId.ToString()));
	}

	double GetItemMass(const FSystemicGameplayState& State, FName ItemId)
	{
		if (const FItemDefinition* Item = FindConst(State.Items, [ItemId](const FItemDefinition& Candidate)
		{
			return Candidate.ItemId == ItemId;
		}))
		{
			return Item->MassPerUnitKg;
		}
		return 0.0;
	}

	double GetItemVolume(const FSystemicGameplayState& State, FName ItemId)
	{
		if (const FItemDefinition* Item = FindConst(State.Items, [ItemId](const FItemDefinition& Candidate)
		{
			return Candidate.ItemId == ItemId;
		}))
		{
			return Item->VolumePerUnitM3;
		}
		return 0.0;
	}

	void GetContainerLoad(const FSystemicGameplayState& State, const FContainerState& Container, double& OutMassKg, double& OutVolumeM3)
	{
		OutMassKg = 0.0;
		OutVolumeM3 = 0.0;
		for (const FItemStackState& Stack : Container.Stacks)
		{
			OutMassKg += GetItemMass(State, Stack.ItemId) * Stack.Quantity;
			OutVolumeM3 += GetItemVolume(State, Stack.ItemId) * Stack.Quantity;
		}
	}

	bool ContainerHasStackId(const FContainerState& Container, FName StackId)
	{
		return Container.Stacks.ContainsByPredicate([StackId](const FItemStackState& Candidate)
		{
			return Candidate.StackId == StackId;
		});
	}

	FName MakeUniqueStackId(const FContainerState& Container, FName StackIdPrefix)
	{
		for (int32 CandidateIndex = Container.Stacks.Num() + 1; CandidateIndex < TNumericLimits<int32>::Max(); ++CandidateIndex)
		{
			const FName CandidateId = FName(*FString::Printf(TEXT("%s_%03d"), *StackIdPrefix.ToString(), CandidateIndex));
			if (!ContainerHasStackId(Container, CandidateId))
			{
				return CandidateId;
			}
		}

		return FName(*FString::Printf(TEXT("%s_overflow"), *StackIdPrefix.ToString()));
	}

	bool HasNamedGameplayTag(const FGameplayTagContainer& Tags, const TCHAR* Token)
	{
		return Tags.ToStringSimple().Contains(Token, ESearchCase::IgnoreCase);
	}

	bool MissionObjectivesComplete(const FSystemicGameplayState& State, const FMissionInstanceState& Mission)
	{
		if (Mission.ObjectiveStateIds.IsEmpty())
		{
			return false;
		}

		for (const FName ObjectiveStateId : Mission.ObjectiveStateIds)
		{
			const FObjectiveState* Objective = FindConst(State.ObjectiveStates, [ObjectiveStateId](const FObjectiveState& Candidate)
			{
				return Candidate.ObjectiveStateId == ObjectiveStateId;
			});
			if (!Objective || Objective->State != FName(TEXT("completed")))
			{
				return false;
			}
		}

		return true;
	}

	double ResolvePositiveCapacity(double AuthoredCapacity, double FallbackCapacity)
	{
		return AuthoredCapacity > UE_SMALL_NUMBER ? AuthoredCapacity : FallbackCapacity;
	}

	int64 CalculateServiceTotalCost(double Amount, int64 UnitPriceCredits)
	{
		return static_cast<int64>(FMath::RoundToDouble(Amount * static_cast<double>(UnitPriceCredits)));
	}

	void EnsureMissionDefinition(
		FSystemicGameplayState& State,
		FName MissionDefinitionId,
		const TCHAR* Title,
		const TCHAR* Summary,
		const TCHAR* OfferDialog,
		const TCHAR* InProgressDialog,
		const TCHAR* TurnInDialog)
	{
		if (State.MissionDefinitions.ContainsByPredicate([MissionDefinitionId](const FMissionDefinition& MissionDefinition)
		{
			return MissionDefinition.MissionDefinitionId == MissionDefinitionId;
		}))
		{
			return;
		}

		FMissionDefinition MissionDefinition;
		MissionDefinition.MissionDefinitionId = MissionDefinitionId;
		MissionDefinition.Title = FText::FromString(Title);
		MissionDefinition.Summary = FText::FromString(Summary);
		MissionDefinition.OfferDialog = FText::FromString(OfferDialog);
		MissionDefinition.AcceptedDialog = FText::FromString(TEXT("Contract accepted. I will keep the channel open."));
		MissionDefinition.InProgressDialog = FText::FromString(InProgressDialog);
		MissionDefinition.TurnInDialog = FText::FromString(TurnInDialog);
		State.MissionDefinitions.Add(MissionDefinition);
	}

	void EnsureHostileBoardingMissionFixture(FSystemicGameplayState& State)
	{
		EnsureMissionDefinition(
			State,
			TEXT("mission_m12_derelict_boarding"),
			TEXT("Derelict Boarding"),
			TEXT("Board the derelict outpost, clear the threat, and report back."),
			TEXT("We have a dead outpost on the lane and no response from inside. Dock, sweep it, then return."),
			TEXT("The outpost still needs clearing. Board the station and remove the hostile presence."),
			TEXT("That clears the report. The escrow is ready for release."));

		if (!State.CreditAccounts.ContainsByPredicate([](const FCreditAccountRecord& Account)
		{
			return Account.AccountId == FName(TEXT("account_wayfarer_mission_escrow"));
		}))
		{
			FCreditAccountRecord Account;
			Account.AccountId = TEXT("account_wayfarer_mission_escrow");
			Account.OwnerType = TEXT("mission_escrow");
			Account.OwnerId = TEXT("brink_watch");
			Account.AvailableBalance = 50000;
			State.CreditAccounts.Add(Account);
		}

		if (!State.MissionOffers.ContainsByPredicate([](const FMissionOfferRecord& Offer)
		{
			return Offer.OfferId == FName(TEXT("offer_m12_derelict_boarding_01"));
		}))
		{
			FMissionOfferRecord Offer;
			Offer.OfferId = TEXT("offer_m12_derelict_boarding_01");
			Offer.MissionDefinitionId = TEXT("mission_m12_derelict_boarding");
			Offer.SourceStationId = TEXT("brink_watch");
			Offer.SourceServiceEndpointId = TEXT("service_brink_watch_mission_board");
			Offer.IssuerFactionId = TEXT("frontier_local_authority");
			Offer.GiverNpcId = TEXT("npc_brink_watch_dispatch");
			Offer.RequiredStationTags = { TEXT("frontier_security") };
			State.MissionOffers.Add(Offer);
		}

		if (!State.MissionInstances.ContainsByPredicate([](const FMissionInstanceState& Mission)
		{
			return Mission.MissionInstanceId == FName(TEXT("mission_m12_derelict_boarding_01"));
		}))
		{
			FMissionInstanceState Mission;
			Mission.MissionInstanceId = TEXT("mission_m12_derelict_boarding_01");
			Mission.MissionDefinitionId = TEXT("mission_m12_derelict_boarding");
			Mission.OfferId = TEXT("offer_m12_derelict_boarding_01");
			Mission.OwnerId = TEXT("player");
			Mission.IssuerFactionId = TEXT("frontier_local_authority");
			Mission.CurrentState = TEXT("offered");
			Mission.ObjectiveStateIds = { TEXT("objective_m12_derelict_reach"), TEXT("objective_m12_derelict_clear") };
			Mission.RewardEscrowIds = { TEXT("escrow_m12_derelict_boarding_01") };
			Mission.IdempotencyKey = TEXT("idem_m12_mission_derelict_boarding");
			State.MissionInstances.Add(Mission);
		}

		if (!State.ObjectiveStates.ContainsByPredicate([](const FObjectiveState& Objective)
		{
			return Objective.ObjectiveStateId == FName(TEXT("objective_m12_derelict_reach"));
		}))
		{
			FObjectiveState Objective;
			Objective.ObjectiveStateId = TEXT("objective_m12_derelict_reach");
			Objective.MissionInstanceId = TEXT("mission_m12_derelict_boarding_01");
			Objective.ObjectiveType = TEXT("reach_station");
			Objective.TargetType = TEXT("station");
			Objective.TargetId = TEXT("derelict_outpost_01");
			Objective.JurisdictionId = TEXT("frontier_local_authority");
			Objective.State = TEXT("inactive");
			State.ObjectiveStates.Add(Objective);
		}

		if (!State.ObjectiveStates.ContainsByPredicate([](const FObjectiveState& Objective)
		{
			return Objective.ObjectiveStateId == FName(TEXT("objective_m12_derelict_clear"));
		}))
		{
			FObjectiveState Objective;
			Objective.ObjectiveStateId = TEXT("objective_m12_derelict_clear");
			Objective.MissionInstanceId = TEXT("mission_m12_derelict_boarding_01");
			Objective.ObjectiveType = TEXT("clear_station_hostiles");
			Objective.TargetType = TEXT("station");
			Objective.TargetId = TEXT("derelict_outpost_01");
			Objective.JurisdictionId = TEXT("frontier_local_authority");
			Objective.State = TEXT("inactive");
			State.ObjectiveStates.Add(Objective);
		}

		if (!State.EscrowHolds.ContainsByPredicate([](const FEscrowHoldRecord& Escrow)
		{
			return Escrow.EscrowId == FName(TEXT("escrow_m12_derelict_boarding_01"));
		}))
		{
			FEscrowHoldRecord Escrow;
			Escrow.EscrowId = TEXT("escrow_m12_derelict_boarding_01");
			Escrow.HoldingAccountId = TEXT("account_wayfarer_mission_escrow");
			Escrow.BeneficiaryAccountId = TEXT("account_player");
			Escrow.Amount = 500;
			Escrow.Reason = TEXT("m12_derelict_boarding_reward");
			Escrow.SourceEventId = TEXT("event_m12_derelict_boarding_seed");
			Escrow.IdempotencyKey = TEXT("idem_m12_escrow_derelict_boarding");
			State.EscrowHolds.Add(Escrow);
		}
	}

	void EnsureGodotMissionBreadthFixture(FSystemicGameplayState& State)
	{
		auto EnsureAccount = [&State](FName AccountId, FName OwnerType, FName OwnerId, int64 Balance)
		{
			if (!State.CreditAccounts.ContainsByPredicate([AccountId](const FCreditAccountRecord& Account)
			{
				return Account.AccountId == AccountId;
			}))
			{
				FCreditAccountRecord Account;
				Account.AccountId = AccountId;
				Account.OwnerType = OwnerType;
				Account.OwnerId = OwnerId;
				Account.AvailableBalance = Balance;
				State.CreditAccounts.Add(Account);
			}
		};
		EnsureAccount(TEXT("account_wayfarer_mission_escrow"), TEXT("mission_escrow"), TEXT("wayfarer_depot"), 50000);

		EnsureMissionDefinition(
			State,
			TEXT("mission_m12_frontier_feedstock"),
			TEXT("Frontier Feedstock"),
			TEXT("Move industrial feedstock from Brink Watch to Wayfarer Depot and return for payment."),
			TEXT("Wayfarer Depot needs feedstock moved across the lane. Deliver the manifest and come back for settlement."),
			TEXT("The feedstock run is still open. Get the cargo to Wayfarer Depot, then return to Brink Watch."),
			TEXT("Feedstock delivery is confirmed. I can close the contract."));
		EnsureMissionDefinition(
			State,
			TEXT("mission_m12_wayfarer_relief"),
			TEXT("Wayfarer Relief"),
			TEXT("Carry relief water from Wayfarer Depot back to Brink Watch."),
			TEXT("Brink Watch is short on relief water. Take this manifest back across the lane and return here."),
			TEXT("The relief run is active. Deliver the water to Brink Watch and return to Wayfarer Depot."),
			TEXT("Relief delivery is logged. The depot can release your payment."));
		EnsureMissionDefinition(
			State,
			TEXT("mission_m12_wayfarer_approach_sweep"),
			TEXT("Wayfarer Approach Sweep"),
			TEXT("Respond to hostile pressure near the trade lane and report back to Wayfarer Depot."),
			TEXT("We have raiders probing the approach. Push them off the lane and return to me."),
			TEXT("The approach sweep is still live. Resolve the hostile contact and come back to Wayfarer."),
			TEXT("The approach is clear enough for traffic. I can close this out."));
		EnsureMissionDefinition(
			State,
			TEXT("mission_m12_outpost_manifest"),
			TEXT("Outpost Manifest"),
			TEXT("Board the derelict outpost, recover its records, and return to Wayfarer Depot."),
			TEXT("The derelict outpost may still have useful records. Board it, secure the site, and bring back what you find."),
			TEXT("The manifest recovery is still open. Secure the derelict and return to Wayfarer Depot."),
			TEXT("The outpost records are back in our hands. Payment is ready."));

		if (!State.CommsEndpoints.ContainsByPredicate([](const FCommsEndpointDefinition& Endpoint)
		{
			return Endpoint.EndpointId == FName(TEXT("comms_wayfarer_depot"));
		}))
		{
			FCommsEndpointDefinition Comms;
			Comms.EndpointId = TEXT("comms_wayfarer_depot");
			Comms.OwnerType = TEXT("station");
			Comms.OwnerId = TEXT("wayfarer_depot");
			Comms.AvailableChannels = { TEXT("cockpit_comms"), TEXT("docked_menu") };
			Comms.AccessPolicyId = TEXT("frontier_open_access");
			State.CommsEndpoints.Add(Comms);
		}

		if (!State.StationServiceEndpoints.ContainsByPredicate([](const FStationServiceEndpointDefinition& Endpoint)
		{
			return Endpoint.ServiceEndpointId == FName(TEXT("service_wayfarer_depot_mission_board"));
		}))
		{
			FStationServiceEndpointDefinition Endpoint;
			Endpoint.ServiceEndpointId = TEXT("service_wayfarer_depot_mission_board");
			Endpoint.StationId = TEXT("wayfarer_depot");
			Endpoint.ServiceType = TEXT("mission_board");
			Endpoint.ProviderFactionId = TEXT("frontier_local_authority");
			Endpoint.CommsEndpointId = TEXT("comms_wayfarer_depot");
			Endpoint.CreditAccountId = TEXT("account_wayfarer_mission_escrow");
			Endpoint.AccessPolicyId = TEXT("frontier_open_access");
			Endpoint.PresentationModes = { TEXT("docked_menu"), TEXT("cockpit_comms"), TEXT("fps_counter") };
			State.StationServiceEndpoints.Add(Endpoint);
		}

		auto AddEscrow = [&State](FName EscrowId, int64 Amount, FName Reason)
		{
			if (State.EscrowHolds.ContainsByPredicate([EscrowId](const FEscrowHoldRecord& Escrow)
			{
				return Escrow.EscrowId == EscrowId;
			}))
			{
				return;
			}

			FEscrowHoldRecord Escrow;
			Escrow.EscrowId = EscrowId;
			Escrow.HoldingAccountId = TEXT("account_wayfarer_mission_escrow");
			Escrow.BeneficiaryAccountId = TEXT("account_player");
			Escrow.Amount = Amount;
			Escrow.Reason = Reason;
			Escrow.SourceEventId = MakeId(TEXT("event_seed"), EscrowId);
			Escrow.IdempotencyKey = MakeId(TEXT("idem_seed"), EscrowId);
			State.EscrowHolds.Add(Escrow);
		};

		auto AddOffer = [&State](FName OfferId, FName MissionDefinitionId, FName StationId, FName EndpointId, FName GiverNpcId, TArray<FName> RequiredTags)
		{
			if (State.MissionOffers.ContainsByPredicate([OfferId](const FMissionOfferRecord& Offer)
			{
				return Offer.OfferId == OfferId;
			}))
			{
				return;
			}

			FMissionOfferRecord Offer;
			Offer.OfferId = OfferId;
			Offer.MissionDefinitionId = MissionDefinitionId;
			Offer.SourceStationId = StationId;
			Offer.SourceServiceEndpointId = EndpointId;
			Offer.IssuerFactionId = TEXT("frontier_local_authority");
			Offer.GiverNpcId = GiverNpcId;
			Offer.RequiredStationTags = RequiredTags;
			State.MissionOffers.Add(Offer);
		};

		auto AddMission = [&State](FName MissionInstanceId, FName MissionDefinitionId, FName OfferId, TArray<FName> ObjectiveIds, FName EscrowId)
		{
			if (State.MissionInstances.ContainsByPredicate([MissionInstanceId](const FMissionInstanceState& Mission)
			{
				return Mission.MissionInstanceId == MissionInstanceId;
			}))
			{
				return;
			}

			FMissionInstanceState Mission;
			Mission.MissionInstanceId = MissionInstanceId;
			Mission.MissionDefinitionId = MissionDefinitionId;
			Mission.OfferId = OfferId;
			Mission.OwnerId = TEXT("player");
			Mission.IssuerFactionId = TEXT("frontier_local_authority");
			Mission.CurrentState = TEXT("offered");
			Mission.ObjectiveStateIds = ObjectiveIds;
			Mission.RewardEscrowIds = { EscrowId };
			Mission.IdempotencyKey = MakeId(TEXT("idem_seed_mission"), MissionInstanceId);
			State.MissionInstances.Add(Mission);
		};

		auto AddObjective = [&State](FName ObjectiveId, FName MissionInstanceId, FName ObjectiveType, FName TargetType, FName TargetId, FName ManifestId = NAME_None)
		{
			if (State.ObjectiveStates.ContainsByPredicate([ObjectiveId](const FObjectiveState& Objective)
			{
				return Objective.ObjectiveStateId == ObjectiveId;
			}))
			{
				return;
			}

			FObjectiveState Objective;
			Objective.ObjectiveStateId = ObjectiveId;
			Objective.MissionInstanceId = MissionInstanceId;
			Objective.ObjectiveType = ObjectiveType;
			Objective.TargetType = TargetType;
			Objective.TargetId = TargetId;
			Objective.RouteSegmentIds = { TEXT("m7_brink_watch_wayfarer_trade") };
			Objective.RequiredCargoManifestId = ManifestId;
			Objective.JurisdictionId = TEXT("frontier_local_authority");
			Objective.State = TEXT("inactive");
			State.ObjectiveStates.Add(Objective);
		};

		auto AddCargoManifest = [&State](FName CargoManifestId, FName ManifestType, FName SourceId, FName DestinationId, TArray<FCargoManifestLine> Lines)
		{
			if (State.CargoManifests.ContainsByPredicate([CargoManifestId](const FCargoManifestDefinition& Manifest)
			{
				return Manifest.CargoManifestId == CargoManifestId;
			}))
			{
				return;
			}

			FCargoManifestDefinition Manifest;
			Manifest.CargoManifestId = CargoManifestId;
			Manifest.ManifestType = ManifestType;
			Manifest.SourceId = SourceId;
			Manifest.DestinationId = DestinationId;
			Manifest.Lines = Lines;
			Manifest.State = TEXT("available");
			State.CargoManifests.Add(Manifest);
		};

		auto MakeManifestLine = [](FName LineId, FName ItemId, FName CommodityId, int32 Quantity)
		{
			FCargoManifestLine Line;
			Line.LineId = LineId;
			Line.ItemId = ItemId;
			Line.CommodityId = CommodityId;
			Line.Quantity = Quantity;
			return Line;
		};

		AddCargoManifest(TEXT("manifest_m12_wayfarer_security"), TEXT("delivery"), TEXT("brink_watch"), TEXT("wayfarer_depot"), {
			MakeManifestLine(TEXT("line_m12_wayfarer_security_parts"), TEXT("synthetic_parts"), TEXT("synthetic_parts"), 4)
		});
		AddCargoManifest(TEXT("manifest_m12_frontier_feedstock"), TEXT("delivery"), TEXT("brink_watch"), TEXT("wayfarer_depot"), {
			MakeManifestLine(TEXT("line_m12_feedstock_iron"), TEXT("iron"), TEXT("iron"), 16)
		});
		AddCargoManifest(TEXT("manifest_m12_wayfarer_relief"), TEXT("delivery"), TEXT("wayfarer_depot"), TEXT("brink_watch"), {
			MakeManifestLine(TEXT("line_m12_relief_water"), TEXT("water"), TEXT("water"), 12)
		});
		AddCargoManifest(TEXT("manifest_m12_outpost_records"), TEXT("quest_recovery"), TEXT("derelict_outpost_01"), TEXT("wayfarer_depot"), {});

		AddOffer(TEXT("offer_m12_frontier_feedstock_01"), TEXT("mission_m12_frontier_feedstock"), TEXT("brink_watch"), TEXT("service_brink_watch_mission_board"), TEXT("npc_brink_market_factor"), { TEXT("ore_trade") });
		AddMission(TEXT("mission_m12_frontier_feedstock_01"), TEXT("mission_m12_frontier_feedstock"), TEXT("offer_m12_frontier_feedstock_01"), { TEXT("objective_m12_feedstock_deliver"), TEXT("objective_m12_feedstock_return") }, TEXT("escrow_m12_frontier_feedstock_01"));
		AddObjective(TEXT("objective_m12_feedstock_deliver"), TEXT("mission_m12_frontier_feedstock_01"), TEXT("deliver_cargo"), TEXT("station"), TEXT("wayfarer_depot"), TEXT("manifest_m12_frontier_feedstock"));
		AddObjective(TEXT("objective_m12_feedstock_return"), TEXT("mission_m12_frontier_feedstock_01"), TEXT("return_to_giver"), TEXT("station"), TEXT("brink_watch"));
		AddEscrow(TEXT("escrow_m12_frontier_feedstock_01"), 700, TEXT("m12_frontier_feedstock_reward"));

		AddOffer(TEXT("offer_m12_wayfarer_relief_01"), TEXT("mission_m12_wayfarer_relief"), TEXT("wayfarer_depot"), TEXT("service_wayfarer_depot_mission_board"), TEXT("npc_wayfarer_depot_master"), { TEXT("route_delivery"), TEXT("trade_lane") });
		AddMission(TEXT("mission_m12_wayfarer_relief_01"), TEXT("mission_m12_wayfarer_relief"), TEXT("offer_m12_wayfarer_relief_01"), { TEXT("objective_m12_relief_deliver"), TEXT("objective_m12_relief_return") }, TEXT("escrow_m12_wayfarer_relief_01"));
		AddObjective(TEXT("objective_m12_relief_deliver"), TEXT("mission_m12_wayfarer_relief_01"), TEXT("deliver_cargo"), TEXT("station"), TEXT("brink_watch"), TEXT("manifest_m12_wayfarer_relief"));
		AddObjective(TEXT("objective_m12_relief_return"), TEXT("mission_m12_wayfarer_relief_01"), TEXT("return_to_giver"), TEXT("station"), TEXT("wayfarer_depot"));
		AddEscrow(TEXT("escrow_m12_wayfarer_relief_01"), 650, TEXT("m12_wayfarer_relief_reward"));

		AddOffer(TEXT("offer_m12_wayfarer_approach_sweep_01"), TEXT("mission_m12_wayfarer_approach_sweep"), TEXT("wayfarer_depot"), TEXT("service_wayfarer_depot_mission_board"), TEXT("npc_wayfarer_depot_master"), { TEXT("distress_response"), TEXT("trade_lane") });
		AddMission(TEXT("mission_m12_wayfarer_approach_sweep_01"), TEXT("mission_m12_wayfarer_approach_sweep"), TEXT("offer_m12_wayfarer_approach_sweep_01"), { TEXT("objective_m12_wayfarer_sweep"), TEXT("objective_m12_wayfarer_sweep_return") }, TEXT("escrow_m12_wayfarer_approach_sweep_01"));
		AddObjective(TEXT("objective_m12_wayfarer_sweep"), TEXT("mission_m12_wayfarer_approach_sweep_01"), TEXT("security_response"), TEXT("encounter"), TEXT("encounter_pirate_trade_lane_01"));
		AddObjective(TEXT("objective_m12_wayfarer_sweep_return"), TEXT("mission_m12_wayfarer_approach_sweep_01"), TEXT("return_to_giver"), TEXT("station"), TEXT("wayfarer_depot"));
		AddEscrow(TEXT("escrow_m12_wayfarer_approach_sweep_01"), 750, TEXT("m12_wayfarer_approach_sweep_reward"));

		AddOffer(TEXT("offer_m12_outpost_manifest_01"), TEXT("mission_m12_outpost_manifest"), TEXT("wayfarer_depot"), TEXT("service_wayfarer_depot_mission_board"), TEXT("npc_wayfarer_depot_master"), { TEXT("distress_response"), TEXT("trade_lane") });
		AddMission(TEXT("mission_m12_outpost_manifest_01"), TEXT("mission_m12_outpost_manifest"), TEXT("offer_m12_outpost_manifest_01"), { TEXT("objective_m12_manifest_board"), TEXT("objective_m12_manifest_secure"), TEXT("objective_m12_manifest_return") }, TEXT("escrow_m12_outpost_manifest_01"));
		AddObjective(TEXT("objective_m12_manifest_board"), TEXT("mission_m12_outpost_manifest_01"), TEXT("reach_station"), TEXT("station"), TEXT("derelict_outpost_01"));
		AddObjective(TEXT("objective_m12_manifest_secure"), TEXT("mission_m12_outpost_manifest_01"), TEXT("clear_station_hostiles"), TEXT("station"), TEXT("derelict_outpost_01"), TEXT("manifest_m12_outpost_records"));
		AddObjective(TEXT("objective_m12_manifest_return"), TEXT("mission_m12_outpost_manifest_01"), TEXT("return_to_giver"), TEXT("station"), TEXT("wayfarer_depot"));
		AddEscrow(TEXT("escrow_m12_outpost_manifest_01"), 900, TEXT("m12_outpost_manifest_reward"));
	}

	const FObjectiveState* FindFirstActiveMissionObjective(const FSystemicGameplayState& State, const FMissionInstanceState& Mission)
	{
		for (const FName ObjectiveStateId : Mission.ObjectiveStateIds)
		{
			const FObjectiveState* Objective = FindConst(State.ObjectiveStates, [ObjectiveStateId](const FObjectiveState& Candidate)
			{
				return Candidate.ObjectiveStateId == ObjectiveStateId;
			});
			if (Objective && Objective->State == FName(TEXT("active")))
			{
				return Objective;
			}
		}

		return nullptr;
	}

	FString MakeReadableMissionId(FName Id)
	{
		FString Text = Id.IsNone() ? FString(TEXT("Mission")) : Id.ToString();
		Text.RemoveFromStart(TEXT("mission_m12_"));
		Text.ReplaceInline(TEXT("_"), TEXT(" "));
		return Text;
	}

	FString DescribeObjective(const FObjectiveState& Objective)
	{
		const FString Target = Objective.TargetId.IsNone() ? FString(TEXT("target")) : Objective.TargetId.ToString();
		if (Objective.ObjectiveType == FName(TEXT("deliver_cargo")))
		{
			return FString::Printf(TEXT("Deliver cargo to %s."), *Target);
		}
		if (Objective.ObjectiveType == FName(TEXT("return_to_giver")))
		{
			return FString::Printf(TEXT("Return to %s."), *Target);
		}
		if (Objective.ObjectiveType == FName(TEXT("security_response")))
		{
			return FString::Printf(TEXT("Resolve the hostile encounter near %s."), *Target);
		}
		if (Objective.ObjectiveType == FName(TEXT("reach_station")))
		{
			return FString::Printf(TEXT("Reach %s."), *Target);
		}
		if (Objective.ObjectiveType == FName(TEXT("clear_station_hostiles")))
		{
			return FString::Printf(TEXT("Board and clear hostiles at %s."), *Target);
		}
		return FString::Printf(TEXT("%s at %s."), *Objective.ObjectiveType.ToString(), *Target);
	}

	FString DescribeCargoManifest(const FSystemicGameplayState& State, FName CargoManifestId)
	{
		const FCargoManifestDefinition* Manifest = FindConst(State.CargoManifests, [CargoManifestId](const FCargoManifestDefinition& Candidate)
		{
			return Candidate.CargoManifestId == CargoManifestId;
		});
		if (!Manifest)
		{
			return FString();
		}
		if (Manifest->ManifestType == FName(TEXT("quest_recovery")))
		{
			return FString::Printf(TEXT("Recover payload from %s and return it to %s."), *Manifest->SourceId.ToString(), *Manifest->DestinationId.ToString());
		}

		TArray<FString> Lines;
		for (const FCargoManifestLine& Line : Manifest->Lines)
		{
			FString ItemName = Line.ItemId.ToString();
			if (const FItemDefinition* Item = FindConst(State.Items, [&Line](const FItemDefinition& Candidate)
			{
				return Candidate.ItemId == Line.ItemId;
			}))
			{
				ItemName = Item->DisplayName.IsEmpty() ? Item->ItemId.ToString() : Item->DisplayName.ToString();
			}
			Lines.Add(FString::Printf(TEXT("%d %s"), Line.Quantity, *ItemName));
		}
		return Lines.IsEmpty()
			? FString::Printf(TEXT("Cargo manifest %s."), *CargoManifestId.ToString())
			: FString::Printf(TEXT("Cargo: %s."), *FString::Join(Lines, TEXT(", ")));
	}

	int64 ResolveMissionRewardCredits(const FSystemicGameplayState& State, const FMissionInstanceState& Mission)
	{
		int64 RewardCredits = 0;
		for (const FName EscrowId : Mission.RewardEscrowIds)
		{
			if (const FEscrowHoldRecord* Escrow = FindConst(State.EscrowHolds, [EscrowId](const FEscrowHoldRecord& Candidate)
			{
				return Candidate.EscrowId == EscrowId;
			}))
			{
				RewardCredits += Escrow->Amount;
			}
		}
		return RewardCredits;
	}

	void FillMissionPresentation(const FSystemicGameplayState& State, const FMissionInstanceState& Mission, FMissionContactInteractionView& OutInteraction)
	{
		const FMissionDefinition* MissionDefinition = FindConst(State.MissionDefinitions, [&Mission](const FMissionDefinition& Candidate)
		{
			return Candidate.MissionDefinitionId == Mission.MissionDefinitionId;
		});

		OutInteraction.MissionTitle = MissionDefinition && !MissionDefinition->Title.IsEmpty()
			? MissionDefinition->Title
			: FText::FromString(MakeReadableMissionId(Mission.MissionDefinitionId));
		OutInteraction.RewardCredits = ResolveMissionRewardCredits(State, Mission);
		OutInteraction.RewardText = OutInteraction.RewardCredits > 0
			? FString::Printf(TEXT("Reward: %lld credits."), static_cast<long long>(OutInteraction.RewardCredits))
			: FString(TEXT("Reward: none listed."));

		TArray<FString> ObjectiveLines;
		TArray<FString> ManifestLines;
		for (const FName ObjectiveStateId : Mission.ObjectiveStateIds)
		{
			const FObjectiveState* Objective = FindConst(State.ObjectiveStates, [ObjectiveStateId](const FObjectiveState& Candidate)
			{
				return Candidate.ObjectiveStateId == ObjectiveStateId;
			});
			if (!Objective)
			{
				continue;
			}
			ObjectiveLines.Add(DescribeObjective(*Objective));
			const FString ManifestLine = DescribeCargoManifest(State, Objective->RequiredCargoManifestId);
			if (!ManifestLine.IsEmpty())
			{
				ManifestLines.Add(ManifestLine);
			}
		}
		OutInteraction.ObjectiveSummaryText = FString::Join(ObjectiveLines, TEXT("\n"));
		OutInteraction.CargoManifestSummaryText = FString::Join(ManifestLines, TEXT("\n"));

		TArray<FString> DialogLines;
		if (OutInteraction.InteractionState == FName(TEXT("ready_to_turn_in")))
		{
			DialogLines.Add(MissionDefinition && !MissionDefinition->TurnInDialog.IsEmpty()
				? MissionDefinition->TurnInDialog.ToString()
				: FString(TEXT("Good work. The contract is ready to close.")));
			DialogLines.Add(OutInteraction.RewardText);
		}
		else if (OutInteraction.InteractionState == FName(TEXT("in_progress")))
		{
			DialogLines.Add(MissionDefinition && !MissionDefinition->InProgressDialog.IsEmpty()
				? MissionDefinition->InProgressDialog.ToString()
				: FString(TEXT("The contract is active.")));
			if (const FObjectiveState* Objective = FindFirstActiveMissionObjective(State, Mission))
			{
				DialogLines.Add(FString::Printf(TEXT("Objective: %s"), *DescribeObjective(*Objective)));
			}
		}
		else
		{
			DialogLines.Add(MissionDefinition && !MissionDefinition->OfferDialog.IsEmpty()
				? MissionDefinition->OfferDialog.ToString()
				: FString(TEXT("I have a contract available.")));
			if (MissionDefinition && !MissionDefinition->Summary.IsEmpty())
			{
				DialogLines.Add(MissionDefinition->Summary.ToString());
			}
			if (!OutInteraction.ObjectiveSummaryText.IsEmpty())
			{
				DialogLines.Add(FString::Printf(TEXT("Objective summary:\n%s"), *OutInteraction.ObjectiveSummaryText));
			}
			if (!OutInteraction.CargoManifestSummaryText.IsEmpty())
			{
				DialogLines.Add(OutInteraction.CargoManifestSummaryText);
			}
			DialogLines.Add(OutInteraction.RewardText);
		}
		OutInteraction.DialogText = FString::Join(DialogLines, TEXT("\n\n"));
	}

	void AddJournalEntry(FSystemicGameplayState& State, FName TransactionId, int32 Sequence, FName SideEffectType, FName SideEffectRefId, FName IdempotencyKey)
	{
		FGameplayTransactionJournalEntry Entry;
		Entry.JournalEntryId = FName(*FString::Printf(TEXT("%s_journal_%03d"), *TransactionId.ToString(), Sequence));
		Entry.TransactionId = TransactionId;
		Entry.Sequence = Sequence;
		Entry.Phase = TEXT("commit");
		Entry.SideEffectType = SideEffectType;
		Entry.SideEffectRefId = SideEffectRefId;
		Entry.State = TEXT("applied");
		Entry.IdempotencyKey = IdempotencyKey;
		State.TransactionJournal.Add(Entry);
	}

	FStationInteriorCombatProfileDefinition MakeGodotHostileBoardingCombatProfile()
	{
		FStationInteriorCombatProfileDefinition Profile;
		Profile.ProfileId = TEXT("profile_godot_hostile_boarding_basic");
		Profile.DisplayName = FText::FromString(TEXT("Godot Hostile Boarding Basic"));
		Profile.PlayerMaxHealth = 100.0;
		Profile.PlayerWeaponDamage = 20.0;
		Profile.PlayerWeaponRangeCm = 6000.0;
		Profile.PlayerWeaponCooldownSeconds = 0.35;
		Profile.HostileMaxHealth = 45.0;
		Profile.HostileFireDamage = 8.0;
		Profile.HostileDetectionRangeCm = 3500.0;
		Profile.HostileFireRangeCm = 2800.0;
		Profile.HostileFireCooldownSeconds = 1.3;
		Profile.HostileInaccuracyDegrees = FMath::RadiansToDegrees(0.04);
		Profile.HostileInitialFireDelaySeconds = 0.4;
		Profile.HostileCount = 3;
		return Profile;
	}

	void EnsureGodotHostileBoardingCombatProfile(FSystemicGameplayState& State)
	{
		const FName ProfileId(TEXT("profile_godot_hostile_boarding_basic"));
		if (!State.StationInteriorCombatProfiles.ContainsByPredicate([ProfileId](const FStationInteriorCombatProfileDefinition& Candidate)
		{
			return Candidate.ProfileId == ProfileId;
		}))
		{
			State.StationInteriorCombatProfiles.Add(MakeGodotHostileBoardingCombatProfile());
		}
	}

	void EnsureGodotInventoryEquipmentFoundation(FSystemicGameplayState& State)
	{
		auto AddGodotItem = [&State](FName ItemId, const TCHAR* DisplayName, FName ItemType, int32 StackLimit, int64 BaseValue)
		{
			if (State.Items.ContainsByPredicate([ItemId](const FItemDefinition& Candidate)
			{
				return Candidate.ItemId == ItemId;
			}))
			{
				return;
			}

			FItemDefinition Item;
			Item.ItemId = ItemId;
			Item.DisplayName = FText::FromString(DisplayName);
			Item.ItemType = ItemType;
			Item.StackLimit = StackLimit;
			Item.MassPerUnitKg = ItemType == FName(TEXT("commodity")) ? 1.0 : 0.0;
			Item.VolumePerUnitM3 = ItemType == FName(TEXT("commodity")) ? 1.0 : 0.0;
			Item.BaseValue = BaseValue;
			State.Items.Add(Item);
		};
		auto AddGodotCommodity = [&State](FName CommodityId, const TCHAR* DisplayName, int64 BasePrice, FName Category)
		{
			if (!State.Commodities.ContainsByPredicate([CommodityId](const FCommodityDefinition& Candidate)
			{
				return Candidate.CommodityId == CommodityId;
			}))
			{
				FCommodityDefinition Commodity;
				Commodity.CommodityId = CommodityId;
				Commodity.DisplayName = FText::FromString(DisplayName);
				Commodity.BasePrice = BasePrice;
				Commodity.MassPerUnitKg = 1.0;
				Commodity.VolumePerUnitM3 = 1.0;
				Commodity.Category = Category;
				State.Commodities.Add(Commodity);
			}

			const FName BridgeId(*FString::Printf(TEXT("bridge_%s"), *CommodityId.ToString()));
			if (!State.CommodityItemBridges.ContainsByPredicate([BridgeId](const FCommodityItemBridge& Candidate)
			{
				return Candidate.CommodityItemBridgeId == BridgeId;
			}))
			{
				FCommodityItemBridge Bridge;
				Bridge.CommodityItemBridgeId = BridgeId;
				Bridge.CommodityId = CommodityId;
				Bridge.ItemId = CommodityId;
				State.CommodityItemBridges.Add(Bridge);
			}
		};
		AddGodotItem(TEXT("food"), TEXT("Food"), TEXT("commodity"), 999, 10);
		AddGodotItem(TEXT("water"), TEXT("Water"), TEXT("commodity"), 999, 6);
		AddGodotItem(TEXT("iron"), TEXT("Iron"), TEXT("commodity"), 999, 14);
		AddGodotItem(TEXT("synthetic_parts"), TEXT("Synthetic Parts"), TEXT("commodity"), 999, 40);
		AddGodotCommodity(TEXT("food"), TEXT("Food"), 10, TEXT("consumer"));
		AddGodotCommodity(TEXT("water"), TEXT("Water"), 6, TEXT("life_support"));
		AddGodotCommodity(TEXT("iron"), TEXT("Iron"), 14, TEXT("ore"));
		AddGodotCommodity(TEXT("synthetic_parts"), TEXT("Synthetic Parts"), 40, TEXT("industrial"));
		AddGodotItem(TEXT("pistol_basic"), TEXT("Basic Pistol"), TEXT("player_sidearm"), 1, 120);
		AddGodotItem(TEXT("rifle_basic"), TEXT("Basic Rifle"), TEXT("player_weapon"), 1, 240);
		AddGodotItem(TEXT("medkit"), TEXT("Medkit"), TEXT("consumable"), 10, 35);
		AddGodotItem(TEXT("pulse_laser_mk1"), TEXT("Pulse Laser Mk1"), TEXT("ship_weapon"), 1, 600);
		AddGodotItem(TEXT("hostile_pulse_laser_mk1"), TEXT("Hostile Pulse Laser Mk1"), TEXT("ship_weapon"), 1, 0);
		AddGodotItem(TEXT("shield_basic"), TEXT("Basic Shield"), TEXT("ship_shield"), 1, 500);
		AddGodotItem(TEXT("engine_basic"), TEXT("Basic Engine"), TEXT("ship_engine"), 1, 450);
		AddGodotItem(TEXT("thrusters_basic"), TEXT("Basic Thrusters"), TEXT("ship_thrusters"), 1, 450);
		AddGodotItem(TEXT("countermeasures_basic"), TEXT("Basic Countermeasures"), TEXT("ship_countermeasures"), 1, 180);
		EnsureGodotHostileBoardingCombatProfile(State);

		if (!State.ShipWeaponStats.ContainsByPredicate([](const FShipWeaponStatDefinition& Candidate)
		{
			return Candidate.WeaponStatId == FName(TEXT("weapon_stat_pulse_laser_mk1"));
		}))
		{
			FShipWeaponStatDefinition PulseLaserStats;
			PulseLaserStats.WeaponStatId = TEXT("weapon_stat_pulse_laser_mk1");
			PulseLaserStats.ItemId = TEXT("pulse_laser_mk1");
			PulseLaserStats.DamageType = TEXT("energy_weapon");
			PulseLaserStats.DamageAmount = 14.0;
			PulseLaserStats.CooldownSeconds = 0.18;
			PulseLaserStats.EnergyCost = 9.0;
			PulseLaserStats.EnergyRegenPerSecond = 30.0;
			PulseLaserStats.MinAlignment = 0.955;
			PulseLaserStats.RangeCm = 120000.0;
			PulseLaserStats.ProjectileSpeedCmPerSec = 76000.0;
			PulseLaserStats.ProjectileCollisionRadiusCm = 850.0;
			PulseLaserStats.PresentationType = TEXT("projectile_bolt");
			PulseLaserStats.DisplayName = FText::FromString(TEXT("Pulse Laser Mk1"));
			State.ShipWeaponStats.Add(PulseLaserStats);
		}
		if (!State.ShipWeaponStats.ContainsByPredicate([](const FShipWeaponStatDefinition& Candidate)
		{
			return Candidate.WeaponStatId == FName(TEXT("weapon_stat_hostile_pulse_laser"));
		}))
		{
			FShipWeaponStatDefinition HostilePulseLaserStats;
			HostilePulseLaserStats.WeaponStatId = TEXT("weapon_stat_hostile_pulse_laser");
			HostilePulseLaserStats.ItemId = TEXT("hostile_pulse_laser_mk1");
			HostilePulseLaserStats.DamageType = TEXT("energy_weapon");
			HostilePulseLaserStats.DamageAmount = 12.0;
			HostilePulseLaserStats.CooldownSeconds = 0.35;
			HostilePulseLaserStats.EnergyCost = 14.0;
			HostilePulseLaserStats.EnergyRegenPerSecond = 28.0;
			HostilePulseLaserStats.MinAlignment = 0.93;
			HostilePulseLaserStats.RangeCm = 90000.0;
			HostilePulseLaserStats.ProjectileSpeedCmPerSec = 70000.0;
			HostilePulseLaserStats.ProjectileCollisionRadiusCm = 850.0;
			HostilePulseLaserStats.PresentationType = TEXT("projectile_bolt");
			HostilePulseLaserStats.DisplayName = FText::FromString(TEXT("Hostile Pulse Laser Mk1"));
			State.ShipWeaponStats.Add(HostilePulseLaserStats);
		}

		if (!State.Containers.ContainsByPredicate([](const FContainerState& Candidate)
		{
			return Candidate.ContainerId == FName(TEXT("player_personal_inventory"));
		}))
		{
			FContainerState PlayerInventory;
			PlayerInventory.ContainerId = TEXT("player_personal_inventory");
			PlayerInventory.OwnerId = TEXT("player");
			PlayerInventory.LocationType = TEXT("player");
			PlayerInventory.LocationId = TEXT("player");
			PlayerInventory.CapacityMassKg = 120.0;
			PlayerInventory.CapacityVolumeM3 = 120.0;
			PlayerInventory.MaxSlotCount = 20;
			State.Containers.Add(PlayerInventory);
		}

		auto AddEquipmentSlot = [&State](FName SlotId, FName OwnerId, FName SlotType, TArray<FName> AcceptedTypes, FName ItemId)
		{
			if (State.EquipmentSlots.ContainsByPredicate([SlotId](const FEquipmentSlotState& Candidate)
			{
				return Candidate.SlotId == SlotId;
			}))
			{
				return;
			}

			FEquipmentSlotState Slot;
			Slot.SlotId = SlotId;
			Slot.OwnerId = OwnerId;
			Slot.SlotType = SlotType;
			Slot.AcceptedItemTypes = MoveTemp(AcceptedTypes);
			if (!ItemId.IsNone())
			{
				Slot.EquippedStack.StackId = FName(*FString::Printf(TEXT("equipped_%s"), *SlotId.ToString()));
				Slot.EquippedStack.ItemId = ItemId;
				Slot.EquippedStack.Quantity = 1;
			}
			State.EquipmentSlots.Add(Slot);
		};
		AddEquipmentSlot(TEXT("player_primary_weapon"), TEXT("player"), TEXT("player_primary_weapon"), { TEXT("player_weapon") }, NAME_None);
		AddEquipmentSlot(TEXT("player_secondary_sidearm"), TEXT("player"), TEXT("player_secondary_sidearm"), { TEXT("player_sidearm") }, TEXT("pistol_basic"));
		AddEquipmentSlot(TEXT("ship_weapon_hardpoint_1"), TEXT("player_ship"), TEXT("ship_weapon_hardpoint"), { TEXT("ship_weapon") }, TEXT("pulse_laser_mk1"));
		AddEquipmentSlot(TEXT("ship_weapon_hardpoint_2"), TEXT("player_ship"), TEXT("ship_weapon_hardpoint"), { TEXT("ship_weapon") }, NAME_None);
		AddEquipmentSlot(TEXT("ship_shield"), TEXT("player_ship"), TEXT("ship_shield"), { TEXT("ship_shield") }, TEXT("shield_basic"));
		AddEquipmentSlot(TEXT("ship_engine"), TEXT("player_ship"), TEXT("ship_engine"), { TEXT("ship_engine") }, TEXT("engine_basic"));
		AddEquipmentSlot(TEXT("ship_thrusters"), TEXT("player_ship"), TEXT("ship_thrusters"), { TEXT("ship_thrusters") }, TEXT("thrusters_basic"));
		AddEquipmentSlot(TEXT("ship_countermeasures"), TEXT("player_ship"), TEXT("ship_countermeasures"), { TEXT("ship_countermeasures") }, NAME_None);
	}

	void EnsureGodotFrontierMarketStock(FSystemicGameplayState& State)
	{
		FStationMarketState* Market = FindMutable(State.Markets, [](const FStationMarketState& Candidate)
		{
			return Candidate.MarketId == FName(TEXT("brink_watch"));
		});
		if (!Market)
		{
			return;
		}

		auto EnsureStock = [Market](FName CommodityId, int32 Stock, int32 MaxStock)
		{
			if (!Market->StockByCommodity.Contains(CommodityId))
			{
				Market->StockByCommodity.Add(CommodityId, Stock);
			}
			if (!Market->MaxStockByCommodity.Contains(CommodityId))
			{
				Market->MaxStockByCommodity.Add(CommodityId, MaxStock);
			}
		};
		EnsureStock(TEXT("commodity_ember_ore"), 100, 200);
		EnsureStock(TEXT("food"), 80, 120);
		EnsureStock(TEXT("water"), 45, 100);
		EnsureStock(TEXT("iron"), 160, 220);
		EnsureStock(TEXT("synthetic_parts"), 18, 60);

		FContainerState* MarketCargo = FindMutable(State.Containers, [](const FContainerState& Candidate)
		{
			return Candidate.ContainerId == FName(TEXT("brink_watch_market_inventory"));
		});
		if (!MarketCargo)
		{
			return;
		}

		auto EnsureStack = [MarketCargo](FName ItemId, int32 Quantity)
		{
			if (MarketCargo->Stacks.ContainsByPredicate([ItemId](const FItemStackState& Candidate)
			{
				return Candidate.ItemId == ItemId;
			}))
			{
				return;
			}
			FItemStackState Stack;
			Stack.StackId = FName(*FString::Printf(TEXT("stack_brink_%s_01"), *ItemId.ToString()));
			Stack.ItemId = ItemId;
			Stack.Quantity = Quantity;
			Stack.OwnerFactionId = TEXT("frontier_local_authority");
			MarketCargo->Stacks.Add(Stack);
		};
		EnsureStack(TEXT("food"), 80);
		EnsureStack(TEXT("water"), 45);
		EnsureStack(TEXT("iron"), 160);
		EnsureStack(TEXT("synthetic_parts"), 18);
	}

	bool IsRestrictedCargo(const FSystemicGameplayState& State, FName CommodityId, FName CommodityItemBridgeId)
	{
		const FCommodityDefinition* Commodity = FindConst(State.Commodities, [CommodityId](const FCommodityDefinition& Candidate)
		{
			return Candidate.CommodityId == CommodityId;
		});
		const FCommodityItemBridge* Bridge = FindConst(State.CommodityItemBridges, [CommodityId, CommodityItemBridgeId](const FCommodityItemBridge& Candidate)
		{
			return Candidate.CommodityItemBridgeId == CommodityItemBridgeId && Candidate.CommodityId == CommodityId;
		});
		const FItemDefinition* Item = Bridge ? FindConst(State.Items, [Bridge](const FItemDefinition& Candidate)
		{
			return Candidate.ItemId == Bridge->ItemId;
		}) : nullptr;

		const auto IsRestrictedName = [](FName Value)
		{
			return Value == FName(TEXT("contraband")) || Value == FName(TEXT("restricted")) || Value == FName(TEXT("illegal"));
		};
		const bool bRestrictedCommodity = Commodity && (IsRestrictedName(Commodity->Category) || HasNamedGameplayTag(Commodity->LegalityTags, TEXT("contraband")) || HasNamedGameplayTag(Commodity->LegalityTags, TEXT("restricted")) || HasNamedGameplayTag(Commodity->LegalityTags, TEXT("illegal")));
		const bool bRestrictedItem = Item && (IsRestrictedName(Item->ItemType) || HasNamedGameplayTag(Item->LegalityTags, TEXT("contraband")) || HasNamedGameplayTag(Item->LegalityTags, TEXT("restricted")) || HasNamedGameplayTag(Item->LegalityTags, TEXT("illegal")));
		return bRestrictedCommodity || bRestrictedItem;
	}

	bool HasRestrictedCargoClearance(const FSystemicGameplayState& State, const FMarketTransactionRequest& Request)
	{
		if (Request.LegalContextId.IsNone())
		{
			return false;
		}
		const FStationMarketState* Market = FindConst(State.Markets, [&Request](const FStationMarketState& Candidate)
		{
			return Candidate.MarketId == Request.MarketId;
		});
		const bool bMarketLegalPolicyResolves = State.StationServiceEndpoints.ContainsByPredicate([&Request, Market](const FStationServiceEndpointDefinition& Endpoint)
		{
			return Endpoint.ServiceType == FName(TEXT("market")) &&
				Endpoint.MarketId == Request.MarketId &&
				!Endpoint.LegalPolicyId.IsNone() &&
				Endpoint.LegalPolicyId == Request.LegalContextId &&
				(!Market || Endpoint.StationId == Market->StationId);
		});
		const bool bJurisdictionResolves = State.Jurisdictions.ContainsByPredicate([&Request](const FJurisdictionDefinition& Jurisdiction)
		{
			return Jurisdiction.JurisdictionId == Request.LegalContextId || Jurisdiction.LawProfileId == Request.LegalContextId;
		});

		const FString Context = Request.LegalContextId.ToString();
		const bool bExplicitException = Context.Contains(TEXT("permit"), ESearchCase::IgnoreCase) ||
			Context.Contains(TEXT("exception"), ESearchCase::IgnoreCase) ||
			Context.Contains(TEXT("clearance"), ESearchCase::IgnoreCase);
		// M12 has only the open legal policy fixture. Restricted cargo needs an explicit exception/permit context.
		return bExplicitException && !bMarketLegalPolicyResolves && !bJurisdictionResolves;
	}

	void AddTransactionIfMissing(FSystemicGameplayState& State, const FGameplayTransactionRecord& Transaction)
	{
		if (!State.Transactions.ContainsByPredicate([&Transaction](const FGameplayTransactionRecord& Candidate)
		{
			return Candidate.TransactionId == Transaction.TransactionId || Candidate.IdempotencyKey == Transaction.IdempotencyKey;
		}))
		{
			State.Transactions.Add(Transaction);
		}
	}
}

FSystemicGameplayState USystemicGameplayQueryService::MakeM9FixtureState(const FStarSystemDefinition& SystemDefinition)
{
	if (!SystemDefinition.SystemicGameplay.Factions.IsEmpty())
	{
		FSystemicGameplayState State = SystemDefinition.SystemicGameplay;
		EnsureGodotInventoryEquipmentFoundation(State);
		EnsureGodotFrontierMarketStock(State);
		return State;
	}

	FSystemicGameplayState State;

	FFactionDefinition Authority;
	Authority.FactionId = TEXT("frontier_local_authority");
	Authority.DisplayName = FText::FromString(TEXT("Frontier Local Authority"));
	Authority.FactionType = TEXT("police");
	Authority.DefaultLawProfileId = TEXT("frontier_law_basic");
	Authority.RelationshipGroupId = TEXT("frontier_lawful");
	Authority.bCanOwnStations = true;
	Authority.bCanOwnShips = true;
	Authority.bCanIssueMissions = true;
	State.Factions.Add(Authority);

	FFactionDefinition Traders;
	Traders.FactionId = TEXT("wayfarer_traders");
	Traders.DisplayName = FText::FromString(TEXT("Wayfarer Traders"));
	Traders.FactionType = TEXT("corporation");
	Traders.RelationshipGroupId = TEXT("frontier_civilian");
	Traders.bCanOwnShips = true;
	Traders.bCanIssueMissions = true;
	State.Factions.Add(Traders);

	FFactionDefinition Pirates;
	Pirates.FactionId = TEXT("ember_raiders");
	Pirates.DisplayName = FText::FromString(TEXT("Ember Raiders"));
	Pirates.FactionType = TEXT("pirate");
	Pirates.RelationshipGroupId = TEXT("frontier_hostile");
	Pirates.bCanOwnShips = true;
	State.Factions.Add(Pirates);

	FFactionRelationshipRecord PirateRelationship;
	PirateRelationship.SourceFactionId = Pirates.FactionId;
	PirateRelationship.TargetFactionId = Authority.FactionId;
	PirateRelationship.Standing = -0.8;
	PirateRelationship.HostilityState = TEXT("hostile");
	State.FactionRelationships.Add(PirateRelationship);

	FFactionOperationalState Operations;
	Operations.FactionId = Authority.FactionId;
	Operations.SystemId = SystemDefinition.SystemId;
	Operations.Influence = 0.65;
	Operations.SecurityPressure = 0.55;
	Operations.CriminalPressure = 0.25;
	Operations.PatrolAssetIds = { TEXT("patrol_frontier_local_01") };
	Operations.AvailablePatrolBudget = 1;
	Operations.AlertLevel = TEXT("watch");
	State.FactionOperations.Add(Operations);

	FJurisdictionDefinition Jurisdiction;
	Jurisdiction.JurisdictionId = TEXT("frontier_local_authority");
	Jurisdiction.AuthorityFactionId = Authority.FactionId;
	Jurisdiction.SystemId = SystemDefinition.SystemId;
	Jurisdiction.LawProfileId = TEXT("frontier_law_basic");
	Jurisdiction.Priority = 10;
	State.Jurisdictions.Add(Jurisdiction);

	FItemDefinition OreItem;
	OreItem.ItemId = TEXT("item_ember_ore");
	OreItem.DisplayName = FText::FromString(TEXT("Ember Ore"));
	OreItem.ItemType = TEXT("commodity");
	OreItem.StackLimit = 1000;
	OreItem.MassPerUnitKg = 10.0;
	OreItem.VolumePerUnitM3 = 1.0;
	OreItem.BaseValue = 100;
	State.Items.Add(OreItem);

	auto AddGodotItem = [&State](FName ItemId, const TCHAR* DisplayName, FName ItemType, int32 StackLimit, int64 BaseValue)
	{
		if (State.Items.ContainsByPredicate([ItemId](const FItemDefinition& Candidate)
		{
			return Candidate.ItemId == ItemId;
		}))
		{
			return;
		}

		FItemDefinition Item;
		Item.ItemId = ItemId;
		Item.DisplayName = FText::FromString(DisplayName);
		Item.ItemType = ItemType;
		Item.StackLimit = StackLimit;
		Item.MassPerUnitKg = ItemType == FName(TEXT("commodity")) ? 1.0 : 0.0;
		Item.VolumePerUnitM3 = ItemType == FName(TEXT("commodity")) ? 1.0 : 0.0;
		Item.BaseValue = BaseValue;
		State.Items.Add(Item);
	};
	AddGodotItem(TEXT("food"), TEXT("Food"), TEXT("commodity"), 999, 10);
	AddGodotItem(TEXT("water"), TEXT("Water"), TEXT("commodity"), 999, 6);
	AddGodotItem(TEXT("iron"), TEXT("Iron"), TEXT("commodity"), 999, 14);
	AddGodotItem(TEXT("synthetic_parts"), TEXT("Synthetic Parts"), TEXT("commodity"), 999, 40);
	AddGodotItem(TEXT("pistol_basic"), TEXT("Basic Pistol"), TEXT("player_sidearm"), 1, 120);
	AddGodotItem(TEXT("rifle_basic"), TEXT("Basic Rifle"), TEXT("player_weapon"), 1, 240);
	AddGodotItem(TEXT("medkit"), TEXT("Medkit"), TEXT("consumable"), 10, 35);
	AddGodotItem(TEXT("pulse_laser_mk1"), TEXT("Pulse Laser Mk1"), TEXT("ship_weapon"), 1, 600);
	AddGodotItem(TEXT("hostile_pulse_laser_mk1"), TEXT("Hostile Pulse Laser Mk1"), TEXT("ship_weapon"), 1, 0);
	AddGodotItem(TEXT("shield_basic"), TEXT("Basic Shield"), TEXT("ship_shield"), 1, 500);
	AddGodotItem(TEXT("engine_basic"), TEXT("Basic Engine"), TEXT("ship_engine"), 1, 450);
	AddGodotItem(TEXT("thrusters_basic"), TEXT("Basic Thrusters"), TEXT("ship_thrusters"), 1, 450);
	AddGodotItem(TEXT("countermeasures_basic"), TEXT("Basic Countermeasures"), TEXT("ship_countermeasures"), 1, 180);

	FShipWeaponStatDefinition PulseLaserStats;
	PulseLaserStats.WeaponStatId = TEXT("weapon_stat_pulse_laser_mk1");
	PulseLaserStats.ItemId = TEXT("pulse_laser_mk1");
	PulseLaserStats.DamageType = TEXT("energy_weapon");
	PulseLaserStats.DamageAmount = 14.0;
	PulseLaserStats.CooldownSeconds = 0.18;
	PulseLaserStats.EnergyCost = 9.0;
	PulseLaserStats.EnergyRegenPerSecond = 30.0;
	PulseLaserStats.MinAlignment = 0.955;
	PulseLaserStats.RangeCm = 120000.0;
	PulseLaserStats.ProjectileSpeedCmPerSec = 76000.0;
	PulseLaserStats.ProjectileCollisionRadiusCm = 850.0;
	PulseLaserStats.PresentationType = TEXT("projectile_bolt");
	PulseLaserStats.DisplayName = FText::FromString(TEXT("Pulse Laser Mk1"));
	State.ShipWeaponStats.Add(PulseLaserStats);

	FShipWeaponStatDefinition HostilePulseLaserStats;
	HostilePulseLaserStats.WeaponStatId = TEXT("weapon_stat_hostile_pulse_laser");
	HostilePulseLaserStats.ItemId = TEXT("hostile_pulse_laser_mk1");
	HostilePulseLaserStats.DamageType = TEXT("energy_weapon");
	HostilePulseLaserStats.DamageAmount = 12.0;
	HostilePulseLaserStats.CooldownSeconds = 0.35;
	HostilePulseLaserStats.EnergyCost = 14.0;
	HostilePulseLaserStats.EnergyRegenPerSecond = 28.0;
	HostilePulseLaserStats.MinAlignment = 0.93;
	HostilePulseLaserStats.RangeCm = 90000.0;
	HostilePulseLaserStats.ProjectileSpeedCmPerSec = 70000.0;
	HostilePulseLaserStats.ProjectileCollisionRadiusCm = 850.0;
	HostilePulseLaserStats.PresentationType = TEXT("projectile_bolt");
	HostilePulseLaserStats.DisplayName = FText::FromString(TEXT("Hostile Pulse Laser Mk1"));
	State.ShipWeaponStats.Add(HostilePulseLaserStats);
	State.StationInteriorCombatProfiles.Add(MakeGodotHostileBoardingCombatProfile());

	FCommodityDefinition OreCommodity;
	OreCommodity.CommodityId = TEXT("commodity_ember_ore");
	OreCommodity.DisplayName = OreItem.DisplayName;
	OreCommodity.BasePrice = 100;
	OreCommodity.MassPerUnitKg = OreItem.MassPerUnitKg;
	OreCommodity.VolumePerUnitM3 = OreItem.VolumePerUnitM3;
	OreCommodity.Category = TEXT("ore");
	State.Commodities.Add(OreCommodity);

	FCommodityItemBridge Bridge;
	Bridge.CommodityItemBridgeId = TEXT("bridge_ember_ore");
	Bridge.CommodityId = OreCommodity.CommodityId;
	Bridge.ItemId = OreItem.ItemId;
	State.CommodityItemBridges.Add(Bridge);

	FContainerState PlayerCargo;
	PlayerCargo.ContainerId = TEXT("player_ship_cargo");
	PlayerCargo.OwnerId = TEXT("player");
	PlayerCargo.LocationType = TEXT("ship");
	PlayerCargo.LocationId = TEXT("player_ship");
	PlayerCargo.CapacityMassKg = 10000.0;
	PlayerCargo.CapacityVolumeM3 = 1000.0;
	State.Containers.Add(PlayerCargo);

	FContainerState PlayerInventory;
	PlayerInventory.ContainerId = TEXT("player_personal_inventory");
	PlayerInventory.OwnerId = TEXT("player");
	PlayerInventory.LocationType = TEXT("player");
	PlayerInventory.LocationId = TEXT("player");
	PlayerInventory.CapacityMassKg = 120.0;
	PlayerInventory.CapacityVolumeM3 = 120.0;
	PlayerInventory.MaxSlotCount = 20;
	State.Containers.Add(PlayerInventory);

	auto AddEquipmentSlot = [&State](FName SlotId, FName OwnerId, FName SlotType, TArray<FName> AcceptedTypes, FName ItemId)
	{
		FEquipmentSlotState Slot;
		Slot.SlotId = SlotId;
		Slot.OwnerId = OwnerId;
		Slot.SlotType = SlotType;
		Slot.AcceptedItemTypes = MoveTemp(AcceptedTypes);
		if (!ItemId.IsNone())
		{
			Slot.EquippedStack.StackId = FName(*FString::Printf(TEXT("equipped_%s"), *SlotId.ToString()));
			Slot.EquippedStack.ItemId = ItemId;
			Slot.EquippedStack.Quantity = 1;
		}
		State.EquipmentSlots.Add(Slot);
	};
	AddEquipmentSlot(TEXT("player_primary_weapon"), TEXT("player"), TEXT("player_primary_weapon"), { TEXT("player_weapon") }, NAME_None);
	AddEquipmentSlot(TEXT("player_secondary_sidearm"), TEXT("player"), TEXT("player_secondary_sidearm"), { TEXT("player_sidearm") }, TEXT("pistol_basic"));
	AddEquipmentSlot(TEXT("ship_weapon_hardpoint_1"), TEXT("player_ship"), TEXT("ship_weapon_hardpoint"), { TEXT("ship_weapon") }, TEXT("pulse_laser_mk1"));
	AddEquipmentSlot(TEXT("ship_weapon_hardpoint_2"), TEXT("player_ship"), TEXT("ship_weapon_hardpoint"), { TEXT("ship_weapon") }, NAME_None);
	AddEquipmentSlot(TEXT("ship_shield"), TEXT("player_ship"), TEXT("ship_shield"), { TEXT("ship_shield") }, TEXT("shield_basic"));
	AddEquipmentSlot(TEXT("ship_engine"), TEXT("player_ship"), TEXT("ship_engine"), { TEXT("ship_engine") }, TEXT("engine_basic"));
	AddEquipmentSlot(TEXT("ship_thrusters"), TEXT("player_ship"), TEXT("ship_thrusters"), { TEXT("ship_thrusters") }, TEXT("thrusters_basic"));
	AddEquipmentSlot(TEXT("ship_countermeasures"), TEXT("player_ship"), TEXT("ship_countermeasures"), { TEXT("ship_countermeasures") }, NAME_None);

	FContainerState MarketCargo;
	MarketCargo.ContainerId = TEXT("brink_watch_market_inventory");
	MarketCargo.OwnerId = TEXT("brink_watch");
	MarketCargo.LocationType = TEXT("market");
	MarketCargo.LocationId = TEXT("brink_watch");
	MarketCargo.CapacityMassKg = 1000000.0;
	MarketCargo.CapacityVolumeM3 = 100000.0;
	FItemStackState MarketStack;
	MarketStack.StackId = TEXT("stack_brink_ore_01");
	MarketStack.ItemId = OreItem.ItemId;
	MarketStack.Quantity = 100;
	MarketCargo.Stacks.Add(MarketStack);
	State.Containers.Add(MarketCargo);

	FCreditAccountRecord PlayerAccount;
	PlayerAccount.AccountId = TEXT("account_player");
	PlayerAccount.OwnerType = TEXT("player");
	PlayerAccount.OwnerId = TEXT("player");
	PlayerAccount.AvailableBalance = 100000;
	State.CreditAccounts.Add(PlayerAccount);

	FCreditAccountRecord MarketAccount;
	MarketAccount.AccountId = TEXT("account_brink_watch_market");
	MarketAccount.OwnerType = TEXT("market");
	MarketAccount.OwnerId = TEXT("brink_watch");
	MarketAccount.AvailableBalance = 1000000;
	State.CreditAccounts.Add(MarketAccount);

	FStationMarketState Market;
	Market.MarketId = TEXT("brink_watch");
	Market.StationId = TEXT("brink_watch");
	Market.MarketProfileId = TEXT("frontier_basic_market");
	Market.StockByCommodity.Add(OreCommodity.CommodityId, 100);
	Market.MaxStockByCommodity.Add(OreCommodity.CommodityId, 200);
	State.Markets.Add(Market);
	EnsureGodotFrontierMarketStock(State);

	FCommsEndpointDefinition Comms;
	Comms.EndpointId = TEXT("comms_brink_watch");
	Comms.OwnerType = TEXT("station");
	Comms.OwnerId = TEXT("brink_watch");
	Comms.AvailableChannels = { TEXT("cockpit_comms"), TEXT("docked_menu") };
	Comms.AccessPolicyId = TEXT("frontier_open_access");
	State.CommsEndpoints.Add(Comms);

	FStationServiceEndpointDefinition MarketEndpoint;
	MarketEndpoint.ServiceEndpointId = TEXT("service_brink_watch_market");
	MarketEndpoint.StationId = TEXT("brink_watch");
	MarketEndpoint.ServiceType = TEXT("market");
	MarketEndpoint.ProviderFactionId = Authority.FactionId;
	MarketEndpoint.CommsEndpointId = Comms.EndpointId;
	MarketEndpoint.MarketId = Market.MarketId;
	MarketEndpoint.InventoryContainerId = MarketCargo.ContainerId;
	MarketEndpoint.CreditAccountId = MarketAccount.AccountId;
	MarketEndpoint.AccessPolicyId = TEXT("frontier_open_access");
	MarketEndpoint.LegalPolicyId = Jurisdiction.LawProfileId;
	MarketEndpoint.PresentationModes = { TEXT("docked_menu"), TEXT("cockpit_comms"), TEXT("fps_counter") };
	State.StationServiceEndpoints.Add(MarketEndpoint);

	FMessageDefinition Message;
	Message.MessageId = TEXT("msg_market_quote_01");
	Message.MessageType = TEXT("service");
	Message.SpeakerId = MarketEndpoint.ServiceEndpointId;
	Message.TextKey = TEXT("frontier.market.quote");
	State.MessageDefinitions.Add(Message);

	EnsureMissionDefinition(
		State,
		TEXT("mission_ore_delivery_basic"),
		TEXT("Ore Delivery"),
		TEXT("Deliver ore from Brink Watch to Wayfarer Depot."),
		TEXT("Wayfarer Depot needs ore. Take the shipment out and report back when it is delivered."),
		TEXT("The ore delivery is still active. Get the cargo to Wayfarer Depot."),
		TEXT("Ore delivery confirmed. Payment is ready."));

	FMissionOfferRecord Offer;
	Offer.OfferId = TEXT("offer_brink_ore_delivery_01");
	Offer.MissionDefinitionId = TEXT("mission_ore_delivery_basic");
	Offer.SourceStationId = TEXT("brink_watch");
	Offer.SourceServiceEndpointId = MarketEndpoint.ServiceEndpointId;
	Offer.IssuerFactionId = Authority.FactionId;
	Offer.GiverNpcId = TEXT("npc_brink_market_factor");
	Offer.RequiredStationTags = { TEXT("ore_trade") };
	State.MissionOffers.Add(Offer);

	FMissionInstanceState Mission;
	Mission.MissionInstanceId = TEXT("mission_instance_fixture_01");
	Mission.MissionDefinitionId = Offer.MissionDefinitionId;
	Mission.OfferId = Offer.OfferId;
	Mission.OwnerId = TEXT("player");
	Mission.IssuerFactionId = Authority.FactionId;
	Mission.CurrentState = TEXT("offered");
	Mission.IdempotencyKey = TEXT("m9_fixture_mission_instance");
	Mission.ObjectiveStateIds = { TEXT("objective_deliver_ore_01") };
	State.MissionInstances.Add(Mission);

	FObjectiveState Objective;
	Objective.ObjectiveStateId = TEXT("objective_deliver_ore_01");
	Objective.MissionInstanceId = Mission.MissionInstanceId;
	Objective.ObjectiveType = TEXT("deliver_cargo");
	Objective.TargetType = TEXT("station");
	Objective.TargetId = TEXT("wayfarer_depot");
	Objective.RouteSegmentIds = { TEXT("m7_brink_watch_wayfarer_trade") };
	Objective.JurisdictionId = Jurisdiction.JurisdictionId;
	Objective.State = TEXT("inactive");
	State.ObjectiveStates.Add(Objective);

	return State;
}

FSystemicGameplayState USystemicGameplayQueryService::MakeM10FixtureState(const FStarSystemDefinition& SystemDefinition)
{
	if (!SystemDefinition.SystemicGameplay.LogicalEncounters.IsEmpty())
	{
		return SystemDefinition.SystemicGameplay;
	}

	FSystemicGameplayState State = MakeM9FixtureState(SystemDefinition);
	const FName RouteId(TEXT("m7_brink_watch_wayfarer_trade"));
	const FName SourceEventId(TEXT("event_m10_pirate_trade_lane_01"));
	const FName DistressEventId(TEXT("event_distress_trade_lane_01"));
	const FName HazardId(TEXT("hazard_interdiction_trade_lane_01"));
	const FName EncounterId(TEXT("encounter_pirate_trade_lane_01"));
	const FName EngagementId(TEXT("engagement_pirate_trade_lane_01"));

	FSimulationEventRecord EncounterEvent;
	EncounterEvent.EventId = SourceEventId;
	EncounterEvent.SystemId = SystemDefinition.SystemId;
	EncounterEvent.EventType = TEXT("logical_pirate_interdiction");
	EncounterEvent.SourceType = TEXT("npc_group");
	EncounterEvent.SourceId = TEXT("pirate_raider_01");
	EncounterEvent.TargetType = TEXT("ship");
	EncounterEvent.TargetId = TEXT("trader_brink_01");
	EncounterEvent.ParticipantShipIds = { TEXT("trader_brink_01"), TEXT("pirate_raider_01"), TEXT("patrol_frontier_local_01") };
	EncounterEvent.ParticipantGroupIds = { TEXT("group_traders_m7_trade_lane"), TEXT("pirate_group_01"), TEXT("patrol_group_01") };
	EncounterEvent.PayloadRef = EncounterId;
	EncounterEvent.IdempotencyKey = TEXT("idem_m10_pirate_trade_lane_01");
	State.Events.Add(EncounterEvent);

	FRouteSecuritySnapshot Security;
	Security.SnapshotId = TEXT("security_trade_lane_window_01");
	Security.RouteSegmentId = RouteId;
	Security.WindowStartTimeSeconds = 0.0;
	Security.WindowEndTimeSeconds = 600.0;
	Security.JurisdictionId = TEXT("frontier_local_authority");
	Security.SecurityRating = 0.45;
	Security.PatrolCoverageScore = 0.4;
	Security.PirateRiskScore = 0.6;
	Security.RouteValue = 0.65;
	Security.SourceEventId = SourceEventId;
	Security.ProcessedWatermark = 1;
	State.RouteSecuritySnapshots.Add(Security);

	FPatrolReservationRecord Reservation;
	Reservation.ReservationId = TEXT("reservation_patrol_trade_lane_01");
	Reservation.PatrolAssetId = TEXT("patrol_frontier_local_01");
	Reservation.FactionId = TEXT("frontier_local_authority");
	Reservation.JurisdictionId = TEXT("frontier_local_authority");
	Reservation.RouteSegmentId = RouteId;
	Reservation.SourceEventId = SourceEventId;
	Reservation.ExpiresAtTimeSeconds = 600.0;
	Reservation.IdempotencyKey = TEXT("idem_m10_patrol_trade_lane_01");
	State.PatrolReservations.Add(Reservation);
	if (FFactionOperationalState* FixtureOperation = FindMutable(State.FactionOperations, [](const FFactionOperationalState& Candidate)
	{
		return Candidate.FactionId == FName(TEXT("frontier_local_authority"));
	}))
	{
		FixtureOperation->ReservedPatrolBudget = FMath::Max(FixtureOperation->ReservedPatrolBudget, 1);
	}

	FContainerState TraderCargo;
	TraderCargo.ContainerId = TEXT("trader_brink_cargo");
	TraderCargo.OwnerId = TEXT("trader_brink_01");
	TraderCargo.LocationType = TEXT("ship");
	TraderCargo.LocationId = TEXT("trader_brink_01");
	TraderCargo.CapacityMassKg = 500.0;
	TraderCargo.CapacityVolumeM3 = 20.0;
	FItemStackState TraderOre;
	TraderOre.StackId = TEXT("trader_brink_ore_stack");
	TraderOre.ItemId = TEXT("item_ember_ore");
	TraderOre.Quantity = 4;
	TraderOre.OwnerFactionId = TEXT("frontier_local_authority");
	TraderCargo.Stacks.Add(TraderOre);
	State.Containers.Add(TraderCargo);

	FContainerState PirateCargo;
	PirateCargo.ContainerId = TEXT("pirate_raider_cargo");
	PirateCargo.OwnerId = TEXT("pirate_raider_01");
	PirateCargo.LocationType = TEXT("ship");
	PirateCargo.LocationId = TEXT("pirate_raider_01");
	PirateCargo.CapacityMassKg = 500.0;
	PirateCargo.CapacityVolumeM3 = 20.0;
	State.Containers.Add(PirateCargo);

	FCreditAccountRecord TraderAccount;
	TraderAccount.AccountId = TEXT("account_trader_brink_01");
	TraderAccount.OwnerId = TEXT("trader_brink_01");
	TraderAccount.OwnerType = TEXT("ship");
	TraderAccount.AvailableBalance = 200;
	State.CreditAccounts.Add(TraderAccount);

	FCreditAccountRecord PirateAccount;
	PirateAccount.AccountId = TEXT("account_pirate_raider_01");
	PirateAccount.OwnerId = TEXT("pirate_raider_01");
	PirateAccount.OwnerType = TEXT("ship");
	PirateAccount.AvailableBalance = 0;
	State.CreditAccounts.Add(PirateAccount);

	FInterdictionHazardRecord Hazard;
	Hazard.HazardId = HazardId;
	Hazard.SourceEventId = SourceEventId;
	Hazard.RouteSegmentId = RouteId;
	Hazard.OwnerGroupId = TEXT("pirate_group_01");
	Hazard.TargetShipId = TEXT("trader_brink_01");
	Hazard.State = TEXT("pending");
	Hazard.ExpiresAtTimeSeconds = 600.0;
	Hazard.SaveLoadEventId = SourceEventId;
	Hazard.IdempotencyKey = TEXT("idem_m10_interdiction_trade_lane_01");
	const FSimulationClockSnapshot Clock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, 0.0);
	UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, RouteId, 0.55, Clock, 0.0, Hazard.RouteSample);
	State.InterdictionHazards.Add(Hazard);

	FDistressEventRecord Distress;
	Distress.DistressEventId = DistressEventId;
	Distress.SourceEventId = SourceEventId;
	Distress.SourceShipId = TEXT("trader_brink_01");
	Distress.ThreatId = TEXT("pirate_raider_01");
	Distress.LocationTarget.TargetId = RouteId;
	Distress.LocationTarget.TargetType = TEXT("route_sample");
	Distress.LocationTarget.RouteSegmentId = RouteId;
	Distress.RespondingPatrolReservationIds = { Reservation.ReservationId };
	Distress.MessageInstanceIds = { TEXT("message_distress_trade_lane_01") };
	Distress.IdempotencyKey = TEXT("idem_m10_distress_trade_lane_01");
	State.DistressEvents.Add(Distress);

	FMessageDefinition DistressMessage;
	DistressMessage.MessageId = TEXT("msg_distress_trade_lane_01");
	DistressMessage.MessageType = TEXT("distress");
	DistressMessage.SpeakerId = TEXT("service_brink_watch_market");
	DistressMessage.TextKey = TEXT("frontier.distress.trade_lane");
	State.MessageDefinitions.Add(DistressMessage);

	FMessageDefinition PirateDemandMessage;
	PirateDemandMessage.MessageId = TEXT("msg_pirate_demand_01");
	PirateDemandMessage.MessageType = TEXT("pirate_demand");
	PirateDemandMessage.SpeakerId = TEXT("service_brink_watch_market");
	PirateDemandMessage.TextKey = TEXT("frontier.pirate.demand");
	State.MessageDefinitions.Add(PirateDemandMessage);

	FMessageLogEntry DistressLog;
	DistressLog.MessageInstanceId = TEXT("message_distress_trade_lane_01");
	DistressLog.MessageId = DistressMessage.MessageId;
	DistressLog.SourceEndpointId = TEXT("service_brink_watch_market");
	DistressLog.TargetId = TEXT("patrol_frontier_local_01");
	DistressLog.DeliveryChannel = TEXT("cockpit_comms");
	DistressLog.SourceEventId = SourceEventId;
	State.MessageLog.Add(DistressLog);

	FLogicalContactTrack Contact;
	Contact.ContactId = TEXT("contact_pirate_trade_lane_01");
	Contact.SourceShipId = TEXT("trader_brink_01");
	Contact.TargetShipId = TEXT("pirate_raider_01");
	Contact.RouteSegmentId = RouteId;
	Contact.LastKnownTarget.TargetId = RouteId;
	Contact.LastKnownTarget.TargetType = TEXT("route_sample");
	Contact.LastKnownTarget.RouteSegmentId = RouteId;
	Contact.Confidence = 0.9;
	State.LogicalContactTracks.Add(Contact);

	FLogicalEncounterRecord Encounter;
	Encounter.EncounterId = EncounterId;
	Encounter.EncounterType = TEXT("pirate_interdiction");
	Encounter.SourceEventId = SourceEventId;
	Encounter.RouteSegmentId = RouteId;
	Encounter.ParticipantShipIds = EncounterEvent.ParticipantShipIds;
	Encounter.ParticipantGroupIds = EncounterEvent.ParticipantGroupIds;
	Encounter.InterdictionHazardId = HazardId;
	Encounter.DistressEventId = DistressEventId;
	Encounter.EngagementId = EngagementId;
	Encounter.ProcessedWatermark = 1;
	Encounter.IdempotencyKey = EncounterEvent.IdempotencyKey;
	State.LogicalEncounters.Add(Encounter);

	FLogicalActorPromotionRecord Promotion;
	Promotion.PromotionId = TEXT("promotion_attach_pirate_trade_lane_01");
	Promotion.EncounterId = EncounterId;
	Promotion.SourceEventId = SourceEventId;
	Promotion.ShipInstanceId = TEXT("pirate_raider_01");
	Promotion.RealizationToken = TEXT("m10_future_realization_token");
	Promotion.bCanResolveEncounter = false;
	Promotion.IdempotencyKey = TEXT("idem_m10_promotion_attach_pirate_trade_lane_01");
	State.ActorPromotionAttachments.Add(Promotion);

	return State;
}

FSystemicGameplayState USystemicGameplayQueryService::MakeM10_5FixtureState(const FStarSystemDefinition& SystemDefinition)
{
	if (!SystemDefinition.SystemicGameplay.ThreatRecords.IsEmpty() &&
		!SystemDefinition.SystemicGameplay.ShipDurabilityStates.IsEmpty() &&
		!SystemDefinition.SystemicGameplay.DamageEvents.IsEmpty())
	{
		return SystemDefinition.SystemicGameplay;
	}

	FSystemicGameplayState State = MakeM10FixtureState(SystemDefinition);
	const FName RouteId(TEXT("m7_brink_watch_wayfarer_trade"));
	const FName SourceEventId(TEXT("event_m10_pirate_trade_lane_01"));
	const FName ThreatId(TEXT("threat_m11_pirate_trade_lane_01"));

	FThreatRecord Threat;
	Threat.ThreatId = ThreatId;
	Threat.AttackerId = TEXT("pirate_raider_01");
	Threat.DefenderId = TEXT("trader_brink_01");
	Threat.LastKnownTarget.TargetId = RouteId;
	Threat.LastKnownTarget.TargetType = TEXT("route_sample");
	Threat.LastKnownTarget.RouteSegmentId = RouteId;
	Threat.LastKnownTarget.RouteProgress01 = 0.55;
	Threat.Severity = 0.7;
	Threat.Confidence = 0.9;
	Threat.ExpiresAtTimeSeconds = 600.0;
	Threat.SourceEventId = SourceEventId;
	Threat.IdempotencyKey = TEXT("idem_m10_5_threat_pirate_trade_lane_01");
	State.ThreatRecords.Add(Threat);

	auto AddDurability = [&State](FName ShipId)
	{
		FShipDurabilityState Durability;
		Durability.DurabilityId = MakeId(TEXT("durability"), ShipId);
		Durability.CombatantId = ShipId;
		Durability.Shield = 100.0;
		Durability.MaxShield = 100.0;
		Durability.Hull = 100.0;
		Durability.MaxHull = 100.0;
		Durability.State = TEXT("active");
		Durability.bCanSurrender = ShipId == FName(TEXT("trader_brink_01"));
		Durability.bCanEscape = ShipId == FName(TEXT("trader_brink_01"));
		Durability.IdempotencyKey = MakeId(TEXT("idem_durability"), ShipId);
		State.ShipDurabilityStates.Add(Durability);
	};
	AddDurability(TEXT("trader_brink_01"));
	AddDurability(TEXT("pirate_raider_01"));
	AddDurability(TEXT("patrol_frontier_local_01"));

	FDamageEventRecord Damage;
	Damage.DamageEventId = TEXT("damage_m10_5_pirate_warning_shot_01");
	Damage.SourceCombatantId = TEXT("pirate_raider_01");
	Damage.TargetCombatantId = TEXT("trader_brink_01");
	Damage.DamageType = TEXT("kinetic_warning");
	Damage.Amount = 12.0;
	Damage.AuthorityTimeSeconds = 60.0;
	Damage.ResultState = TEXT("applied");
	Damage.ThreatId = ThreatId;
	Damage.IdempotencyKey = TEXT("idem_m10_5_damage_warning_shot_01");
	State.DamageEvents.Add(Damage);

	FRealizedAISteeringIntent AttackIntent;
	AttackIntent.IntentId = TEXT("attack_intent_m10_5_pirate_trade_lane_01");
	AttackIntent.ShipInstanceId = TEXT("pirate_raider_01");
	AttackIntent.IntentType = TEXT("attack");
	AttackIntent.TargetShipId = TEXT("trader_brink_01");
	AttackIntent.TargetGroupId = TEXT("pirate_group_01");
	AttackIntent.RouteSegmentId = RouteId;
	AttackIntent.ThreatId = ThreatId;
	AttackIntent.TargetFrame.TargetId = RouteId;
	AttackIntent.TargetFrame.TargetType = TEXT("route_sample");
	AttackIntent.TargetFrame.RouteSegmentId = RouteId;
	AttackIntent.TargetFrame.RouteProgress01 = 0.55;
	AttackIntent.IdempotencyKey = TEXT("idem_m10_5_attack_intent_pirate_trade_lane_01");
	State.RealizedSteeringIntents.Add(AttackIntent);

	return State;
}

FSystemicGameplayState USystemicGameplayQueryService::MakeM11FixtureState(const FStarSystemDefinition& SystemDefinition)
{
	if (!SystemDefinition.SystemicGameplay.RealizedActorBudgetProfiles.IsEmpty())
	{
		return SystemDefinition.SystemicGameplay;
	}

	FSystemicGameplayState State = MakeM10_5FixtureState(SystemDefinition);
	const FName RouteId(TEXT("m7_brink_watch_wayfarer_trade"));
	const FName EncounterId(TEXT("encounter_pirate_trade_lane_01"));
	const FName SourceEventId(TEXT("event_m10_pirate_trade_lane_01"));
	const FName BudgetId(TEXT("actor_budget_m11_low"));
	const FName ThreatId(TEXT("threat_m11_pirate_trade_lane_01"));

	FRealizedActorBudgetProfile Budget;
	Budget.BudgetProfileId = BudgetId;
	Budget.MaxRealizedActors = 3;
	Budget.MaxPromotionsPerTick = 2;
	Budget.PriorityPolicyId = TEXT("m11_nearby_interdiction_priority");
	Budget.PromotionRadiusCm = 250000000.0;
	Budget.IdempotencyKey = TEXT("idem_m11_actor_budget_low");
	State.RealizedActorBudgetProfiles.Add(Budget);

	auto AddMapping = [&State, BudgetId, EncounterId, SourceEventId](FName MappingId, FName ShipId, FName GroupId, FName Token, int32 Priority)
	{
		FRealizedActorMappingRecord Mapping;
		Mapping.MappingId = MappingId;
		Mapping.ShipInstanceId = ShipId;
		Mapping.GroupId = GroupId;
		Mapping.EncounterId = EncounterId;
		Mapping.SourceEventId = SourceEventId;
		Mapping.RealizationToken = Token;
		Mapping.ActorBudgetProfileId = BudgetId;
		Mapping.PromotionPriority = Priority;
		Mapping.State = TEXT("eligible");
		Mapping.IdempotencyKey = MakeId(TEXT("idem_m11_mapping"), MappingId);
		State.RealizedActorMappings.Add(Mapping);
	};

	AddMapping(TEXT("mapping_m11_trader_brink_01"), TEXT("trader_brink_01"), TEXT("group_traders_m7_trade_lane"), TEXT("m11_actor_trader_brink_01"), 100);
	AddMapping(TEXT("mapping_m11_pirate_raider_01"), TEXT("pirate_raider_01"), TEXT("pirate_group_01"), TEXT("m11_actor_pirate_raider_01"), 90);
	AddMapping(TEXT("mapping_m11_patrol_frontier_local_01"), TEXT("patrol_frontier_local_01"), TEXT("patrol_group_01"), TEXT("m11_actor_patrol_frontier_local_01"), 80);
	AddMapping(TEXT("mapping_m11_trader_brink_02"), TEXT("trader_brink_02"), TEXT("group_traders_m7_trade_lane"), TEXT("m11_actor_trader_brink_02"), 40);
	AddMapping(TEXT("mapping_m11_trader_wayfarer_01"), TEXT("trader_wayfarer_01"), TEXT("group_traders_m7_trade_lane"), TEXT("m11_actor_trader_wayfarer_01"), 35);
	AddMapping(TEXT("mapping_m11_pirate_raider_02"), TEXT("pirate_raider_02"), TEXT("pirate_group_01"), TEXT("m11_actor_pirate_raider_02"), 30);
	AddMapping(TEXT("mapping_m11_patrol_frontier_local_02"), TEXT("patrol_frontier_local_02"), TEXT("patrol_group_01"), TEXT("m11_actor_patrol_frontier_local_02"), 25);

	const FShipTrafficInstance* Trader = FindConst(SystemDefinition.LogicalTraffic, [](const FShipTrafficInstance& Candidate)
	{
		return Candidate.ShipInstanceId == FName(TEXT("trader_brink_01"));
	});
	FRealizedAIDemotionSnapshot Snapshot;
	Snapshot.SnapshotId = TEXT("demotion_m11_trader_brink_01");
	Snapshot.ShipInstanceId = TEXT("trader_brink_01");
	Snapshot.GroupId = TEXT("group_traders_m7_trade_lane");
	Snapshot.EncounterId = EncounterId;
	Snapshot.RealizationToken = TEXT("m11_actor_trader_brink_01");
	if (Trader)
	{
		Snapshot.GoalState = Trader->CurrentGoal;
		Snapshot.TargetFrame = Trader->CurrentGoal.TargetFrame;
		Snapshot.VelocityFrame = Trader->VelocityFrame;
		Snapshot.LogicalVelocityCmPerSec = Trader->LogicalVelocityCmPerSec;
	}
	Snapshot.ThreatId = ThreatId;
	Snapshot.RecoveryPolicyId = TEXT("m11_route_recover_or_free_flight");
	Snapshot.IdempotencyKey = TEXT("idem_m11_demotion_trader_brink_01");
	State.RealizedDemotionSnapshots.Add(Snapshot);

	auto AddIntent = [&State, RouteId, ThreatId](FName IntentId, FName ShipId, FName Type, FName TargetShipId, FName TargetGroupId, double Progress, FName SlotId)
	{
		FRealizedAISteeringIntent Intent;
		Intent.IntentId = IntentId;
		Intent.ShipInstanceId = ShipId;
		Intent.IntentType = Type;
		Intent.TargetShipId = TargetShipId;
		Intent.TargetGroupId = TargetGroupId;
		Intent.RouteSegmentId = RouteId;
		Intent.FormationSlotId = SlotId;
		Intent.ThreatId = ThreatId;
		Intent.TargetFrame.TargetId = RouteId;
		Intent.TargetFrame.TargetType = TEXT("route_sample");
		Intent.TargetFrame.RouteSegmentId = RouteId;
		Intent.TargetFrame.RouteProgress01 = Progress;
		Intent.IdempotencyKey = MakeId(TEXT("idem_m11_steering"), IntentId);
		State.RealizedSteeringIntents.Add(Intent);
	};

	AddIntent(TEXT("steer_m11_trader_approach"), TEXT("trader_brink_01"), TEXT("approach"), TEXT(""), TEXT("group_traders_m7_trade_lane"), 0.52, TEXT("slot_lead_trade"));
	AddIntent(TEXT("steer_m11_patrol_formation"), TEXT("patrol_frontier_local_01"), TEXT("formation"), TEXT("patrol_frontier_local_02"), TEXT("patrol_group_01"), 0.32, TEXT("slot_patrol_lead"));
	AddIntent(TEXT("steer_m11_trader_flee"), TEXT("trader_brink_01"), TEXT("flee"), TEXT("pirate_raider_01"), TEXT("group_traders_m7_trade_lane"), 0.15, TEXT("slot_lead_trade"));
	AddIntent(TEXT("steer_m11_pirate_attack"), TEXT("pirate_raider_01"), TEXT("attack"), TEXT("trader_brink_01"), TEXT("pirate_group_01"), 0.55, TEXT("slot_pirate_lead"));

	if (!State.MessageDefinitions.ContainsByPredicate([](const FMessageDefinition& Message) { return Message.MessageId == FName(TEXT("msg_police_scan_01")); }))
	{
		FMessageDefinition ScanMessage;
		ScanMessage.MessageId = TEXT("msg_police_scan_01");
		ScanMessage.MessageType = TEXT("police_scan");
		ScanMessage.SpeakerId = TEXT("service_brink_watch_market");
		ScanMessage.TextKey = TEXT("frontier.police.scan");
		State.MessageDefinitions.Add(ScanMessage);
	}
	if (!State.MessageDefinitions.ContainsByPredicate([](const FMessageDefinition& Message) { return Message.MessageId == FName(TEXT("msg_flee_surrender_01")); }))
	{
		FMessageDefinition FleeMessage;
		FleeMessage.MessageId = TEXT("msg_flee_surrender_01");
		FleeMessage.MessageType = TEXT("flee_surrender");
		FleeMessage.SpeakerId = TEXT("service_brink_watch_market");
		FleeMessage.TextKey = TEXT("frontier.trader.surrender");
		State.MessageDefinitions.Add(FleeMessage);
	}

	auto AddHook = [&State, EncounterId](FName HookId, FName HookType, FName MessageId, FName SourceShipId, FName TargetShipId)
	{
		FRealizedAICommsHook Hook;
		Hook.HookId = HookId;
		Hook.HookType = HookType;
		Hook.MessageId = MessageId;
		Hook.SourceShipId = SourceShipId;
		Hook.TargetShipId = TargetShipId;
		Hook.EncounterId = EncounterId;
		Hook.IdempotencyKey = MakeId(TEXT("idem_m11_comms"), HookId);
		State.RealizedCommsHooks.Add(Hook);
	};
	AddHook(TEXT("hook_m11_scan"), TEXT("scan"), TEXT("msg_police_scan_01"), TEXT("patrol_frontier_local_01"), TEXT("trader_brink_01"));
	AddHook(TEXT("hook_m11_pirate_demand"), TEXT("pirate_demand"), TEXT("msg_pirate_demand_01"), TEXT("pirate_raider_01"), TEXT("trader_brink_01"));
	AddHook(TEXT("hook_m11_distress"), TEXT("distress"), TEXT("msg_distress_trade_lane_01"), TEXT("trader_brink_01"), TEXT("patrol_frontier_local_01"));
	AddHook(TEXT("hook_m11_flee_surrender"), TEXT("flee_surrender"), TEXT("msg_flee_surrender_01"), TEXT("trader_brink_01"), TEXT("pirate_raider_01"));

	return State;
}

FSystemicGameplayState USystemicGameplayQueryService::MakeM12FixtureState(const FStarSystemDefinition& SystemDefinition)
{
	auto ApplyM12EndpointUnitPrices = [](FSystemicGameplayState& State)
	{
		for (FStationServiceEndpointDefinition& Endpoint : State.StationServiceEndpoints)
		{
			if (Endpoint.ServiceEndpointId == FName(TEXT("service_brink_watch_repair")) || Endpoint.ServiceType == FName(TEXT("repair")))
			{
				Endpoint.UnitPriceCredits = 10;
			}
			else if (Endpoint.ServiceEndpointId == FName(TEXT("service_brink_watch_refuel")) || Endpoint.ServiceType == FName(TEXT("refuel")))
			{
				Endpoint.UnitPriceCredits = 2;
			}
			else if (Endpoint.ServiceEndpointId == FName(TEXT("service_brink_watch_rearm")) || Endpoint.ServiceType == FName(TEXT("rearm")))
			{
				Endpoint.UnitPriceCredits = 5;
			}
		}
	};

	if (!SystemDefinition.SystemicGameplay.ShipResourceStates.IsEmpty() ||
		!SystemDefinition.SystemicGameplay.StationServiceResults.IsEmpty() ||
		!SystemDefinition.SystemicGameplay.FollowUpOpportunities.IsEmpty())
	{
		FSystemicGameplayState State = SystemDefinition.SystemicGameplay;
		EnsureGodotInventoryEquipmentFoundation(State);
		EnsureHostileBoardingMissionFixture(State);
		EnsureGodotMissionBreadthFixture(State);
		ApplyM12EndpointUnitPrices(State);
		return State;
	}

	FSystemicGameplayState State = MakeM11FixtureState(SystemDefinition);
	EnsureGodotInventoryEquipmentFoundation(State);
	EnsureGodotFrontierMarketStock(State);

	auto EnsureAccount = [&State](FName AccountId, FName OwnerType, FName OwnerId, int64 Balance)
	{
		if (!State.CreditAccounts.ContainsByPredicate([AccountId](const FCreditAccountRecord& Account)
		{
			return Account.AccountId == AccountId;
		}))
		{
			FCreditAccountRecord Account;
			Account.AccountId = AccountId;
			Account.OwnerType = OwnerType;
			Account.OwnerId = OwnerId;
			Account.AvailableBalance = Balance;
			State.CreditAccounts.Add(Account);
		}
	};
	EnsureAccount(TEXT("account_brink_watch_services"), TEXT("service"), TEXT("brink_watch"), 250000);
	EnsureAccount(TEXT("account_wayfarer_mission_escrow"), TEXT("mission_escrow"), TEXT("wayfarer_depot"), 50000);

	auto EnsureEndpoint = [&State](FName EndpointId, FName ServiceType, FName AccountId, int64 UnitPriceCredits)
	{
		if (FStationServiceEndpointDefinition* ExistingEndpoint = State.StationServiceEndpoints.FindByPredicate([EndpointId](const FStationServiceEndpointDefinition& Endpoint)
		{
			return Endpoint.ServiceEndpointId == EndpointId;
		}))
		{
			ExistingEndpoint->UnitPriceCredits = UnitPriceCredits;
			return;
		}
		else
		{
			FStationServiceEndpointDefinition Endpoint;
			Endpoint.ServiceEndpointId = EndpointId;
			Endpoint.StationId = TEXT("brink_watch");
			Endpoint.ServiceType = ServiceType;
			Endpoint.ProviderFactionId = TEXT("frontier_local_authority");
			Endpoint.CommsEndpointId = TEXT("comms_brink_watch");
			Endpoint.MarketId = TEXT("brink_watch");
			Endpoint.InventoryContainerId = TEXT("brink_watch_market_inventory");
			Endpoint.CreditAccountId = AccountId;
			Endpoint.UnitPriceCredits = UnitPriceCredits;
			Endpoint.AccessPolicyId = TEXT("frontier_open_access");
			Endpoint.LegalPolicyId = TEXT("frontier_law_basic");
			Endpoint.PresentationModes = { TEXT("docked_menu"), TEXT("cockpit_comms") };
			State.StationServiceEndpoints.Add(Endpoint);
		}
	};
	EnsureEndpoint(TEXT("service_brink_watch_repair"), TEXT("repair"), TEXT("account_brink_watch_services"), 10);
	EnsureEndpoint(TEXT("service_brink_watch_refuel"), TEXT("refuel"), TEXT("account_brink_watch_services"), 2);
	EnsureEndpoint(TEXT("service_brink_watch_rearm"), TEXT("rearm"), TEXT("account_brink_watch_services"), 5);
	EnsureEndpoint(TEXT("service_brink_watch_mission_board"), TEXT("mission_board"), TEXT("account_wayfarer_mission_escrow"), 0);
	ApplyM12EndpointUnitPrices(State);

	if (!State.Items.ContainsByPredicate([](const FItemDefinition& Item) { return Item.ItemId == FName(TEXT("item_restricted_artifacts")); }))
	{
		FItemDefinition RestrictedItem;
		RestrictedItem.ItemId = TEXT("item_restricted_artifacts");
		RestrictedItem.DisplayName = FText::FromString(TEXT("Restricted Artifacts"));
		RestrictedItem.ItemType = TEXT("contraband");
		RestrictedItem.StackLimit = 25;
		RestrictedItem.MassPerUnitKg = 5.0;
		RestrictedItem.VolumePerUnitM3 = 0.5;
		RestrictedItem.BaseValue = 600;
		State.Items.Add(RestrictedItem);

		FCommodityDefinition RestrictedCommodity;
		RestrictedCommodity.CommodityId = TEXT("commodity_restricted_artifacts");
		RestrictedCommodity.DisplayName = RestrictedItem.DisplayName;
		RestrictedCommodity.BasePrice = 600;
		RestrictedCommodity.MassPerUnitKg = RestrictedItem.MassPerUnitKg;
		RestrictedCommodity.VolumePerUnitM3 = RestrictedItem.VolumePerUnitM3;
		RestrictedCommodity.Category = TEXT("contraband");
		State.Commodities.Add(RestrictedCommodity);

		FCommodityItemBridge RestrictedBridge;
		RestrictedBridge.CommodityItemBridgeId = TEXT("bridge_restricted_artifacts");
		RestrictedBridge.CommodityId = RestrictedCommodity.CommodityId;
		RestrictedBridge.ItemId = RestrictedItem.ItemId;
		State.CommodityItemBridges.Add(RestrictedBridge);

		if (FStationMarketState* Market = FindMutable(State.Markets, [](const FStationMarketState& Candidate)
		{
			return Candidate.MarketId == FName(TEXT("brink_watch"));
		}))
		{
			Market->StockByCommodity.Add(RestrictedCommodity.CommodityId, 1);
		}
		if (FContainerState* MarketCargo = FindMutable(State.Containers, [](const FContainerState& Candidate)
		{
			return Candidate.ContainerId == FName(TEXT("brink_watch_market_inventory"));
		}))
		{
			FItemStackState RestrictedStack;
			RestrictedStack.StackId = TEXT("stack_brink_restricted_artifacts_01");
			RestrictedStack.ItemId = RestrictedItem.ItemId;
			RestrictedStack.Quantity = 1;
			RestrictedStack.OwnerFactionId = TEXT("frontier_local_authority");
			MarketCargo->Stacks.Add(RestrictedStack);
		}
	}

	if (!State.ShipDurabilityStates.ContainsByPredicate([](const FShipDurabilityState& Durability)
	{
		return Durability.CombatantId == FName(TEXT("player_ship"));
	}))
	{
		FShipDurabilityState PlayerDurability;
		PlayerDurability.DurabilityId = TEXT("durability_player_ship");
		PlayerDurability.CombatantId = TEXT("player_ship");
		PlayerDurability.Shield = 65.0;
		PlayerDurability.MaxShield = 100.0;
		PlayerDurability.Hull = 70.0;
		PlayerDurability.MaxHull = 100.0;
		PlayerDurability.State = TEXT("damaged");
		PlayerDurability.IdempotencyKey = TEXT("idem_m12_durability_player_ship");
		State.ShipDurabilityStates.Add(PlayerDurability);
	}

	FShipResourceState Resources;
	Resources.ResourceStateId = TEXT("resources_player_ship");
	Resources.ShipId = TEXT("player_ship");
	Resources.Fuel = 35.0;
	Resources.MaxFuel = 100.0;
	Resources.Ammo = 12.0;
	Resources.MaxAmmo = 40.0;
	Resources.IdempotencyKey = TEXT("idem_m12_resources_player_ship");
	State.ShipResourceStates.Add(Resources);

	EnsureMissionDefinition(
		State,
		TEXT("mission_m12_route_security_cargo"),
		TEXT("Wayfarer Security Cargo"),
		TEXT("Deliver security cargo to Wayfarer Depot, respond to trouble on the lane, and return."),
		TEXT("Take this security cargo to Wayfarer Depot. If the lane flares up, handle it and come back."),
		TEXT("The Wayfarer security cargo contract is active. Finish the delivery and patrol response."),
		TEXT("Wayfarer confirmed the delivery and the patrol report. Escrow is ready."));

	FMissionOfferRecord M12Offer;
	M12Offer.OfferId = TEXT("offer_m12_wayfarer_security_01");
	M12Offer.MissionDefinitionId = TEXT("mission_m12_route_security_cargo");
	M12Offer.SourceStationId = TEXT("brink_watch");
	M12Offer.SourceServiceEndpointId = TEXT("service_brink_watch_mission_board");
	M12Offer.IssuerFactionId = TEXT("frontier_local_authority");
	M12Offer.GiverNpcId = TEXT("npc_brink_watch_dispatch");
	M12Offer.RequiredStationTags = { TEXT("convoy_defense"), TEXT("frontier_security") };
	State.MissionOffers.Add(M12Offer);

	FMissionInstanceState M12Mission;
	M12Mission.MissionInstanceId = TEXT("mission_m12_wayfarer_security_01");
	M12Mission.MissionDefinitionId = M12Offer.MissionDefinitionId;
	M12Mission.OfferId = M12Offer.OfferId;
	M12Mission.OwnerId = TEXT("player");
	M12Mission.IssuerFactionId = M12Offer.IssuerFactionId;
	M12Mission.CurrentState = TEXT("offered");
	M12Mission.ObjectiveStateIds = { TEXT("objective_m12_route_wayfarer"), TEXT("objective_m12_security_patrol") };
	M12Mission.RewardEscrowIds = { TEXT("escrow_m12_wayfarer_security_01") };
	M12Mission.IdempotencyKey = TEXT("idem_m12_mission_wayfarer_security");
	State.MissionInstances.Add(M12Mission);

	FObjectiveState RouteObjective;
	RouteObjective.ObjectiveStateId = TEXT("objective_m12_route_wayfarer");
	RouteObjective.MissionInstanceId = M12Mission.MissionInstanceId;
	RouteObjective.ObjectiveType = TEXT("deliver_cargo");
	RouteObjective.TargetType = TEXT("station");
	RouteObjective.TargetId = TEXT("wayfarer_depot");
	RouteObjective.RouteSegmentIds = { TEXT("m7_brink_watch_wayfarer_trade") };
	RouteObjective.RequiredCargoManifestId = TEXT("manifest_m12_wayfarer_security");
	RouteObjective.JurisdictionId = TEXT("frontier_local_authority");
	RouteObjective.State = TEXT("inactive");
	State.ObjectiveStates.Add(RouteObjective);

	FObjectiveState SecurityObjective;
	SecurityObjective.ObjectiveStateId = TEXT("objective_m12_security_patrol");
	SecurityObjective.MissionInstanceId = M12Mission.MissionInstanceId;
	SecurityObjective.ObjectiveType = TEXT("security_response");
	SecurityObjective.TargetType = TEXT("encounter");
	SecurityObjective.TargetId = TEXT("encounter_pirate_trade_lane_01");
	SecurityObjective.RouteSegmentIds = { TEXT("m7_brink_watch_wayfarer_trade") };
	SecurityObjective.JurisdictionId = TEXT("frontier_local_authority");
	SecurityObjective.State = TEXT("inactive");
	State.ObjectiveStates.Add(SecurityObjective);

	FEscrowHoldRecord Escrow;
	Escrow.EscrowId = TEXT("escrow_m12_wayfarer_security_01");
	Escrow.HoldingAccountId = TEXT("account_wayfarer_mission_escrow");
	Escrow.BeneficiaryAccountId = TEXT("account_player");
	Escrow.Amount = 750;
	Escrow.Reason = TEXT("m12_security_cargo_reward");
	Escrow.SourceEventId = TEXT("event_m12_mission_board_seed");
	Escrow.IdempotencyKey = TEXT("idem_m12_escrow_wayfarer_security");
	State.EscrowHolds.Add(Escrow);

	FFollowUpOpportunityRecord SeedOpportunity;
	SeedOpportunity.OpportunityId = TEXT("followup_m12_wayfarer_return_01");
	SeedOpportunity.OpportunityType = TEXT("route_cargo_return");
	SeedOpportunity.SourceEventId = TEXT("event_m12_mission_board_seed");
	SeedOpportunity.RouteSegmentId = TEXT("m7_brink_watch_wayfarer_trade");
	SeedOpportunity.ServiceEndpointId = TEXT("service_brink_watch_mission_board");
	SeedOpportunity.MissionOfferId = M12Offer.OfferId;
	SeedOpportunity.FactionId = M12Offer.IssuerFactionId;
	SeedOpportunity.State = TEXT("locked");
	SeedOpportunity.IdempotencyKey = TEXT("idem_m12_followup_wayfarer_return_seed");
	State.FollowUpOpportunities.Add(SeedOpportunity);

	FProgressionDebugLedgerEntry SeedTrace;
	SeedTrace.ProgressionEntryId = TEXT("progression_m12_seed_fixture");
	SeedTrace.EntryType = TEXT("fixture_seed");
	SeedTrace.SubjectId = TEXT("player");
	SeedTrace.FactionId = TEXT("frontier_local_authority");
	SeedTrace.SourceEventId = TEXT("event_m12_mission_board_seed");
	SeedTrace.MissionInstanceId = M12Mission.MissionInstanceId;
	SeedTrace.FollowUpOpportunityId = SeedOpportunity.OpportunityId;
	SeedTrace.DebugReason = TEXT("M12 fixture links mission board, route, cargo, service, faction, ledger, and follow-up opportunity contracts.");
	SeedTrace.IdempotencyKey = TEXT("idem_m12_seed_trace");
	State.ProgressionDebugLedger.Add(SeedTrace);

	EnsureHostileBoardingMissionFixture(State);
	EnsureGodotMissionBreadthFixture(State);

	return State;
}

bool USystemicGameplayQueryService::ValidateSystemicGameplayState(const FStarSystemDefinition& SystemDefinition, const FSystemicGameplayState& State, FString& OutFailureReason)
{
	auto Fail = [&OutFailureReason](const FString& Reason)
	{
		OutFailureReason = Reason;
		return false;
	};

	if (State.Factions.IsEmpty() || State.Jurisdictions.IsEmpty() || State.Items.IsEmpty() || State.Containers.IsEmpty() || State.Markets.IsEmpty())
	{
		return Fail(TEXT("Systemic state is missing required M9 factions, jurisdictions, inventory, or market records."));
	}

	TSet<FName> FactionIds;
	for (const FFactionDefinition& Faction : State.Factions)
	{
		if (Faction.FactionId.IsNone() || FactionIds.Contains(Faction.FactionId))
		{
			return Fail(FString::Printf(TEXT("Systemic state has an empty or duplicate faction ID '%s'."), *Faction.FactionId.ToString()));
		}
		FactionIds.Add(Faction.FactionId);
	}

	TSet<FName> JurisdictionIds;
	for (const FJurisdictionDefinition& Jurisdiction : State.Jurisdictions)
	{
		if (Jurisdiction.JurisdictionId.IsNone() || JurisdictionIds.Contains(Jurisdiction.JurisdictionId))
		{
			return Fail(FString::Printf(TEXT("Systemic state has an empty or duplicate jurisdiction ID '%s'."), *Jurisdiction.JurisdictionId.ToString()));
		}
		if (!Jurisdiction.SystemId.IsNone() && Jurisdiction.SystemId != SystemDefinition.SystemId)
		{
			return Fail(FString::Printf(TEXT("Jurisdiction '%s' belongs to system '%s', expected '%s'."),
				*Jurisdiction.JurisdictionId.ToString(),
				*Jurisdiction.SystemId.ToString(),
				*SystemDefinition.SystemId.ToString()));
		}
		if (!FactionIds.Contains(Jurisdiction.AuthorityFactionId))
		{
			return Fail(FString::Printf(TEXT("Jurisdiction '%s' authority faction does not resolve."), *Jurisdiction.JurisdictionId.ToString()));
		}
		JurisdictionIds.Add(Jurisdiction.JurisdictionId);
	}
	TSet<FName> LawProfileIds;
	for (const FJurisdictionDefinition& Jurisdiction : State.Jurisdictions)
	{
		if (Jurisdiction.LawProfileId.IsNone())
		{
			return Fail(FString::Printf(TEXT("Jurisdiction '%s' is missing a law profile ID."), *Jurisdiction.JurisdictionId.ToString()));
		}
		LawProfileIds.Add(Jurisdiction.LawProfileId);
	}
	TSet<FName> RouteIds;
	for (const FTrafficRouteSegmentDefinition& Route : SystemDefinition.TrafficRoutes)
	{
		RouteIds.Add(Route.RouteSegmentId);
	}
	TSet<FName> StationIds;
	TMap<FName, const FStationDefinition*> StationsById;
	for (const FStationDefinition& Station : SystemDefinition.Stations)
	{
		StationIds.Add(Station.StationId);
		StationsById.Add(Station.StationId, &Station);
	}
	for (const FFactionRelationshipRecord& Relationship : State.FactionRelationships)
	{
		if (!FactionIds.Contains(Relationship.SourceFactionId) || !FactionIds.Contains(Relationship.TargetFactionId))
		{
			return Fail(FString::Printf(TEXT("Faction relationship '%s -> %s' does not resolve both factions."),
				*Relationship.SourceFactionId.ToString(),
				*Relationship.TargetFactionId.ToString()));
		}
	}
	for (const FFactionOperationalState& Operation : State.FactionOperations)
	{
		if (!FactionIds.Contains(Operation.FactionId))
		{
			return Fail(FString::Printf(TEXT("Faction operation '%s' does not resolve its faction."), *Operation.FactionId.ToString()));
		}
		if (!Operation.SystemId.IsNone() && Operation.SystemId != SystemDefinition.SystemId)
		{
			return Fail(FString::Printf(TEXT("Faction operation '%s' belongs to system '%s', expected '%s'."),
				*Operation.FactionId.ToString(),
				*Operation.SystemId.ToString(),
				*SystemDefinition.SystemId.ToString()));
		}
	}

	TSet<FName> ItemIds;
	for (const FItemDefinition& Item : State.Items)
	{
		if (Item.ItemId.IsNone() || ItemIds.Contains(Item.ItemId))
		{
			return Fail(FString::Printf(TEXT("Systemic state has an empty or duplicate item ID '%s'."), *Item.ItemId.ToString()));
		}
		ItemIds.Add(Item.ItemId);
	}

	TSet<FName> ContainerIds;
	TSet<FName> StackIds;
	for (const FContainerState& Container : State.Containers)
	{
		if (Container.ContainerId.IsNone() || ContainerIds.Contains(Container.ContainerId))
		{
			return Fail(FString::Printf(TEXT("Systemic state has an empty or duplicate container ID '%s'."), *Container.ContainerId.ToString()));
		}
		if (Container.OwnerId.IsNone())
		{
			return Fail(FString::Printf(TEXT("Container '%s' is missing an owner ID."), *Container.ContainerId.ToString()));
		}
		if (Container.CapacityMassKg <= 0.0 || Container.CapacityVolumeM3 <= 0.0)
		{
			return Fail(FString::Printf(TEXT("Container '%s' has invalid cargo capacity."), *Container.ContainerId.ToString()));
		}
		for (const FItemStackState& Stack : Container.Stacks)
		{
			const FItemDefinition* StackItem = FindConst(State.Items, [&Stack](const FItemDefinition& Candidate)
			{
				return Candidate.ItemId == Stack.ItemId;
			});
			if (Stack.StackId.IsNone() || StackIds.Contains(Stack.StackId) || !StackItem || Stack.Quantity <= 0 ||
				StackItem->StackLimit <= 0 || Stack.Quantity > StackItem->StackLimit)
			{
				return Fail(FString::Printf(TEXT("Container '%s' has an invalid cargo stack '%s'."), *Container.ContainerId.ToString(), *Stack.StackId.ToString()));
			}
			if (!Stack.OwnerFactionId.IsNone() && !FactionIds.Contains(Stack.OwnerFactionId))
			{
				return Fail(FString::Printf(TEXT("Cargo stack '%s' owner faction does not resolve."), *Stack.StackId.ToString()));
			}
			StackIds.Add(Stack.StackId);
		}
		ContainerIds.Add(Container.ContainerId);
	}

	TSet<FName> EquipmentSlotIds;
	for (const FEquipmentSlotState& Slot : State.EquipmentSlots)
	{
		if (Slot.SlotId.IsNone() || EquipmentSlotIds.Contains(Slot.SlotId) || Slot.OwnerId.IsNone() || Slot.AcceptedItemTypes.IsEmpty())
		{
			return Fail(FString::Printf(TEXT("Equipment slot '%s' is incomplete or duplicated."), *Slot.SlotId.ToString()));
		}
		if (!Slot.EquippedStack.ItemId.IsNone())
		{
			const FItemDefinition* EquippedItem = FindConst(State.Items, [&Slot](const FItemDefinition& Candidate)
			{
				return Candidate.ItemId == Slot.EquippedStack.ItemId;
			});
			if (!EquippedItem || Slot.EquippedStack.StackId.IsNone() || Slot.EquippedStack.Quantity <= 0 ||
				EquippedItem->StackLimit <= 0 || Slot.EquippedStack.Quantity > EquippedItem->StackLimit ||
				!Slot.AcceptedItemTypes.Contains(EquippedItem->ItemType))
			{
				return Fail(FString::Printf(TEXT("Equipment slot '%s' has an invalid equipped item '%s'."),
					*Slot.SlotId.ToString(),
					*Slot.EquippedStack.ItemId.ToString()));
			}
		}
		EquipmentSlotIds.Add(Slot.SlotId);
	}

	TSet<FName> CommodityIds;
	for (const FCommodityDefinition& Commodity : State.Commodities)
	{
		if (Commodity.CommodityId.IsNone() || CommodityIds.Contains(Commodity.CommodityId))
		{
			return Fail(FString::Printf(TEXT("Systemic state has an empty or duplicate commodity ID '%s'."), *Commodity.CommodityId.ToString()));
		}
		CommodityIds.Add(Commodity.CommodityId);
	}

	TSet<FName> CargoManifestIds;
	for (const FCargoManifestDefinition& Manifest : State.CargoManifests)
	{
		if (Manifest.CargoManifestId.IsNone() || CargoManifestIds.Contains(Manifest.CargoManifestId) || Manifest.ManifestType.IsNone() || Manifest.State.IsNone())
		{
			return Fail(FString::Printf(TEXT("Cargo manifest '%s' has invalid durable IDs or state."), *Manifest.CargoManifestId.ToString()));
		}
		if (Manifest.ManifestType == FName(TEXT("delivery")) && Manifest.Lines.IsEmpty())
		{
			return Fail(FString::Printf(TEXT("Delivery cargo manifest '%s' has no cargo lines."), *Manifest.CargoManifestId.ToString()));
		}
		TSet<FName> LineIds;
		for (const FCargoManifestLine& Line : Manifest.Lines)
		{
			if (Line.LineId.IsNone() || LineIds.Contains(Line.LineId) || Line.Quantity <= 0 ||
				(Line.ItemId.IsNone() && Line.CommodityId.IsNone()) ||
				(!Line.ItemId.IsNone() && !ItemIds.Contains(Line.ItemId)) ||
				(!Line.CommodityId.IsNone() && !CommodityIds.Contains(Line.CommodityId)))
			{
				return Fail(FString::Printf(TEXT("Cargo manifest '%s' has invalid line '%s'."), *Manifest.CargoManifestId.ToString(), *Line.LineId.ToString()));
			}
			LineIds.Add(Line.LineId);
		}
		CargoManifestIds.Add(Manifest.CargoManifestId);
	}

	TSet<FName> BridgedCommodityIds;
	for (const FCommodityItemBridge& Bridge : State.CommodityItemBridges)
	{
		if (!CommodityIds.Contains(Bridge.CommodityId) || !ItemIds.Contains(Bridge.ItemId) || BridgedCommodityIds.Contains(Bridge.CommodityId))
		{
			return Fail(FString::Printf(TEXT("Commodity bridge '%s' is invalid or duplicated."), *Bridge.CommodityItemBridgeId.ToString()));
		}
		BridgedCommodityIds.Add(Bridge.CommodityId);
	}

	TSet<FName> MarketIds;
	for (const FStationMarketState& Market : State.Markets)
	{
		if (Market.MarketId.IsNone() || MarketIds.Contains(Market.MarketId))
		{
			return Fail(FString::Printf(TEXT("Systemic state has an empty or duplicate market ID '%s'."), *Market.MarketId.ToString()));
		}
		if (Market.MarketId != Market.StationId)
		{
			return Fail(FString::Printf(TEXT("Market '%s' violates the M9 MarketId = StationId identity rule."), *Market.MarketId.ToString()));
		}
		for (const TPair<FName, int32>& Stock : Market.StockByCommodity)
		{
			if (!CommodityIds.Contains(Stock.Key) || !BridgedCommodityIds.Contains(Stock.Key) || Stock.Value < 0)
			{
				return Fail(FString::Printf(TEXT("Market '%s' has invalid stock for commodity '%s'."), *Market.MarketId.ToString(), *Stock.Key.ToString()));
			}
		}
		MarketIds.Add(Market.MarketId);
	}

	TSet<FName> AccountIds;
	for (const FCreditAccountRecord& Account : State.CreditAccounts)
	{
		if (Account.AccountId.IsNone() || AccountIds.Contains(Account.AccountId) || Account.OwnerId.IsNone() || Account.AvailableBalance < 0 || Account.HeldBalance < 0)
		{
			return Fail(FString::Printf(TEXT("Credit account '%s' is invalid."), *Account.AccountId.ToString()));
		}
		AccountIds.Add(Account.AccountId);
	}

	TSet<FName> CommsEndpointIds;
	for (const FCommsEndpointDefinition& Comms : State.CommsEndpoints)
	{
		if (Comms.EndpointId.IsNone() || CommsEndpointIds.Contains(Comms.EndpointId) || Comms.OwnerId.IsNone())
		{
			return Fail(FString::Printf(TEXT("Comms endpoint '%s' is invalid."), *Comms.EndpointId.ToString()));
		}
		CommsEndpointIds.Add(Comms.EndpointId);
	}

	TSet<FName> ServiceEndpointIds;
	for (const FStationServiceEndpointDefinition& Endpoint : State.StationServiceEndpoints)
	{
		if (Endpoint.ServiceEndpointId.IsNone() || ServiceEndpointIds.Contains(Endpoint.ServiceEndpointId))
		{
			return Fail(FString::Printf(TEXT("Station service endpoint '%s' is empty or duplicated."), *Endpoint.ServiceEndpointId.ToString()));
		}
		if ((!Endpoint.StationId.IsNone() && !StationIds.Contains(Endpoint.StationId)) ||
			!FactionIds.Contains(Endpoint.ProviderFactionId) ||
			(!Endpoint.CommsEndpointId.IsNone() && !CommsEndpointIds.Contains(Endpoint.CommsEndpointId)) ||
			(!Endpoint.MarketId.IsNone() && !MarketIds.Contains(Endpoint.MarketId)) ||
			(!Endpoint.InventoryContainerId.IsNone() && !ContainerIds.Contains(Endpoint.InventoryContainerId)) ||
			(!Endpoint.CreditAccountId.IsNone() && !AccountIds.Contains(Endpoint.CreditAccountId)) ||
			(!Endpoint.LegalPolicyId.IsNone() && !LawProfileIds.Contains(Endpoint.LegalPolicyId)))
		{
			return Fail(FString::Printf(TEXT("Station service endpoint '%s' has unresolved systemic references."), *Endpoint.ServiceEndpointId.ToString()));
		}
		ServiceEndpointIds.Add(Endpoint.ServiceEndpointId);
	}

	TSet<FName> MessageIds;
	for (const FMessageDefinition& Message : State.MessageDefinitions)
	{
		if (Message.MessageId.IsNone() || MessageIds.Contains(Message.MessageId) || !ServiceEndpointIds.Contains(Message.SpeakerId))
		{
			return Fail(FString::Printf(TEXT("Message definition '%s' is invalid or has an unresolved speaker."), *Message.MessageId.ToString()));
		}
		MessageIds.Add(Message.MessageId);
	}
	for (const FMessageLogEntry& MessageLog : State.MessageLog)
	{
		if (MessageLog.MessageInstanceId.IsNone() || !MessageIds.Contains(MessageLog.MessageId) || !ServiceEndpointIds.Contains(MessageLog.SourceEndpointId))
		{
			return Fail(FString::Printf(TEXT("Message log entry '%s' has unresolved message or endpoint references."), *MessageLog.MessageInstanceId.ToString()));
		}
	}

	TSet<FName> MissionDefinitionIds;
	for (const FMissionDefinition& MissionDefinition : State.MissionDefinitions)
	{
		if (MissionDefinition.MissionDefinitionId.IsNone() ||
			MissionDefinitionIds.Contains(MissionDefinition.MissionDefinitionId) ||
			MissionDefinition.Title.IsEmpty())
		{
			return Fail(FString::Printf(TEXT("Mission definition '%s' has invalid durable IDs or presentation fields."), *MissionDefinition.MissionDefinitionId.ToString()));
		}
		MissionDefinitionIds.Add(MissionDefinition.MissionDefinitionId);
	}

	for (const FCreditLedgerEntry& Ledger : State.CreditLedger)
	{
		if (Ledger.LedgerEntryId.IsNone() || !AccountIds.Contains(Ledger.DebitAccountId) || !AccountIds.Contains(Ledger.CreditAccountId) || Ledger.Amount <= 0 || Ledger.IdempotencyKey.IsNone())
		{
			return Fail(FString::Printf(TEXT("Credit ledger entry '%s' is invalid."), *Ledger.LedgerEntryId.ToString()));
		}
	}

	TSet<FName> TransactionIds;
	TSet<FName> TransactionIdempotencyKeys;
	for (const FGameplayTransactionRecord& Transaction : State.Transactions)
	{
		if (Transaction.TransactionId.IsNone() || Transaction.IdempotencyKey.IsNone() ||
			TransactionIds.Contains(Transaction.TransactionId) || TransactionIdempotencyKeys.Contains(Transaction.IdempotencyKey))
		{
			return Fail(FString::Printf(TEXT("Gameplay transaction '%s' has invalid transaction or idempotency IDs."), *Transaction.TransactionId.ToString()));
		}
		TransactionIds.Add(Transaction.TransactionId);
		TransactionIdempotencyKeys.Add(Transaction.IdempotencyKey);
	}

	for (const FGameplayTransactionJournalEntry& Entry : State.TransactionJournal)
	{
		if (Entry.JournalEntryId.IsNone() || !TransactionIds.Contains(Entry.TransactionId) || Entry.IdempotencyKey.IsNone())
		{
			return Fail(FString::Printf(TEXT("Transaction journal entry '%s' is invalid."), *Entry.JournalEntryId.ToString()));
		}
	}

	TSet<FName> CargoTransferIdempotencyKeys;
	for (const FCargoTransferResult& Result : State.CargoTransferResults)
	{
		if (Result.TransferId.IsNone() || Result.RequestId.IsNone() || Result.IdempotencyKey.IsNone() || CargoTransferIdempotencyKeys.Contains(Result.IdempotencyKey))
		{
			return Fail(FString::Printf(TEXT("Cargo transfer result '%s' has invalid durable IDs."), *Result.TransferId.ToString()));
		}
		CargoTransferIdempotencyKeys.Add(Result.IdempotencyKey);
	}

	TSet<FName> MarketTransactionIdempotencyKeys;
	for (const FMarketTransactionResult& Result : State.MarketTransactionResults)
	{
		if (Result.TransactionId.IsNone() || Result.IdempotencyKey.IsNone() || MarketTransactionIdempotencyKeys.Contains(Result.IdempotencyKey))
		{
			return Fail(FString::Printf(TEXT("Market transaction result '%s' has invalid durable IDs."), *Result.TransactionId.ToString()));
		}
		MarketTransactionIdempotencyKeys.Add(Result.IdempotencyKey);
	}

	TSet<FName> MissionOfferIds;
	for (const FMissionOfferRecord& Offer : State.MissionOffers)
	{
		const FStationDefinition* SourceStation = StationsById.FindRef(Offer.SourceStationId);
		if (Offer.OfferId.IsNone() || MissionOfferIds.Contains(Offer.OfferId) ||
			!MissionDefinitionIds.Contains(Offer.MissionDefinitionId) ||
			!SourceStation ||
			!ServiceEndpointIds.Contains(Offer.SourceServiceEndpointId) ||
			!FactionIds.Contains(Offer.IssuerFactionId))
		{
			return Fail(FString::Printf(TEXT("Mission offer '%s' has unresolved definition, station, service, or faction references."), *Offer.OfferId.ToString()));
		}
		if (!Offer.GiverNpcId.IsNone() && !SourceStation->QuestGiverNpcIds.Contains(Offer.GiverNpcId))
		{
			return Fail(FString::Printf(TEXT("Mission offer '%s' references giver '%s' that is not authored on station '%s'."),
				*Offer.OfferId.ToString(),
				*Offer.GiverNpcId.ToString(),
				*Offer.SourceStationId.ToString()));
		}
		for (const FName RequiredTag : Offer.RequiredStationTags)
		{
			if (RequiredTag.IsNone() || !SourceStation->MissionTags.Contains(RequiredTag))
			{
				return Fail(FString::Printf(TEXT("Mission offer '%s' requires missing station tag '%s' on station '%s'."),
					*Offer.OfferId.ToString(),
					*RequiredTag.ToString(),
					*Offer.SourceStationId.ToString()));
			}
		}
		MissionOfferIds.Add(Offer.OfferId);
	}

	TSet<FName> MissionInstanceIds;
	TSet<FName> ObjectiveIdsDeclaredByMissions;
	for (const FMissionInstanceState& Mission : State.MissionInstances)
	{
		if (Mission.MissionInstanceId.IsNone() || MissionInstanceIds.Contains(Mission.MissionInstanceId) ||
			!MissionDefinitionIds.Contains(Mission.MissionDefinitionId) ||
			!MissionOfferIds.Contains(Mission.OfferId) ||
			!FactionIds.Contains(Mission.IssuerFactionId) ||
			Mission.IdempotencyKey.IsNone())
		{
			return Fail(FString::Printf(TEXT("Mission instance '%s' has unresolved definition, offer, faction, or durable IDs."), *Mission.MissionInstanceId.ToString()));
		}
		for (const FName ObjectiveStateId : Mission.ObjectiveStateIds)
		{
			ObjectiveIdsDeclaredByMissions.Add(ObjectiveStateId);
		}
		MissionInstanceIds.Add(Mission.MissionInstanceId);
	}

	TSet<FName> ObjectiveStateIds;
	for (const FObjectiveState& Objective : State.ObjectiveStates)
	{
		if (Objective.ObjectiveStateId.IsNone() || ObjectiveStateIds.Contains(Objective.ObjectiveStateId) ||
			!MissionInstanceIds.Contains(Objective.MissionInstanceId) ||
			!JurisdictionIds.Contains(Objective.JurisdictionId) ||
			!ObjectiveIdsDeclaredByMissions.Contains(Objective.ObjectiveStateId))
		{
			return Fail(FString::Printf(TEXT("Objective state '%s' has unresolved mission or jurisdiction references."), *Objective.ObjectiveStateId.ToString()));
		}
		for (const FName RouteId : Objective.RouteSegmentIds)
		{
			if (!RouteIds.Contains(RouteId))
			{
				return Fail(FString::Printf(TEXT("Objective state '%s' references missing route '%s'."), *Objective.ObjectiveStateId.ToString(), *RouteId.ToString()));
			}
		}
		if (!Objective.RequiredCargoManifestId.IsNone() && !CargoManifestIds.Contains(Objective.RequiredCargoManifestId))
		{
			return Fail(FString::Printf(TEXT("Objective state '%s' references missing cargo manifest '%s'."),
				*Objective.ObjectiveStateId.ToString(),
				*Objective.RequiredCargoManifestId.ToString()));
		}
		ObjectiveStateIds.Add(Objective.ObjectiveStateId);
	}

	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ValidateLogicalEncounterState(const FStarSystemDefinition& SystemDefinition, const FSystemicGameplayState& State, FString& OutFailureReason)
{
	auto Fail = [&OutFailureReason](const FString& Reason)
	{
		OutFailureReason = Reason;
		return false;
	};

	FString BaseFailure;
	if (!ValidateSystemicGameplayState(SystemDefinition, State, BaseFailure))
	{
		return Fail(BaseFailure);
	}
	if (State.LogicalContactTracks.IsEmpty() || State.RouteSecuritySnapshots.IsEmpty() || State.PatrolReservations.IsEmpty() ||
		State.InterdictionHazards.IsEmpty() || State.DistressEvents.IsEmpty() || State.LogicalEncounters.IsEmpty() ||
		State.ActorPromotionAttachments.IsEmpty())
	{
		return Fail(TEXT("M10 logical encounter state is missing required contacts, security snapshots, patrol reservations, hazards, distress records, encounters, or promotion attachment metadata."));
	}

	TSet<FName> RouteIds;
	for (const FTrafficRouteSegmentDefinition& Route : SystemDefinition.TrafficRoutes)
	{
		RouteIds.Add(Route.RouteSegmentId);
	}
	TSet<FName> ShipIds;
	for (const FShipTrafficInstance& Ship : SystemDefinition.LogicalTraffic)
	{
		ShipIds.Add(Ship.ShipInstanceId);
	}
	TSet<FName> GroupIds;
	for (const FShipGroupState& Group : SystemDefinition.ShipGroups)
	{
		GroupIds.Add(Group.GroupId);
	}
	TSet<FName> FactionIds;
	for (const FFactionDefinition& Faction : State.Factions)
	{
		FactionIds.Add(Faction.FactionId);
	}
	TSet<FName> JurisdictionIds;
	for (const FJurisdictionDefinition& Jurisdiction : State.Jurisdictions)
	{
		JurisdictionIds.Add(Jurisdiction.JurisdictionId);
	}
	TSet<FName> EventIds;
	for (const FSimulationEventRecord& Event : State.Events)
	{
		if (Event.EventId.IsNone() || EventIds.Contains(Event.EventId) || Event.IdempotencyKey.IsNone())
		{
			return Fail(FString::Printf(TEXT("M10 event '%s' has an empty, duplicate, or non-idempotent event ID."), *Event.EventId.ToString()));
		}
		EventIds.Add(Event.EventId);
	}
	TSet<FName> MessageInstanceIds;
	for (const FMessageLogEntry& Message : State.MessageLog)
	{
		MessageInstanceIds.Add(Message.MessageInstanceId);
	}
	TSet<FName> CargoTransferIds;
	for (const FCargoTransferResult& Cargo : State.CargoTransferResults)
	{
		CargoTransferIds.Add(Cargo.TransferId);
	}
	TSet<FName> GameplayTransactionIds;
	for (const FGameplayTransactionRecord& Transaction : State.Transactions)
	{
		GameplayTransactionIds.Add(Transaction.TransactionId);
	}
	TSet<FName> CreditLedgerIds;
	for (const FCreditLedgerEntry& Ledger : State.CreditLedger)
	{
		CreditLedgerIds.Add(Ledger.LedgerEntryId);
	}
	TSet<FName> MarketTransactionIds;
	for (const FMarketTransactionResult& Market : State.MarketTransactionResults)
	{
		MarketTransactionIds.Add(Market.TransactionId);
	}
	TSet<FName> OffenseIds;
	for (const FOffenseEvent& Offense : State.Offenses)
	{
		OffenseIds.Add(Offense.OffenseId);
	}
	TSet<FName> EvidenceIds;
	for (const FEvidenceRecord& Evidence : State.EvidenceRecords)
	{
		EvidenceIds.Add(Evidence.EvidenceId);
	}
	TSet<FName> CriminalRecordIds;
	for (const FCriminalRecord& Criminal : State.CriminalRecords)
	{
		CriminalRecordIds.Add(Criminal.CriminalRecordId);
	}

	for (const FLogicalContactTrack& Contact : State.LogicalContactTracks)
	{
		if (Contact.ContactId.IsNone() || !ShipIds.Contains(Contact.SourceShipId) || !ShipIds.Contains(Contact.TargetShipId) ||
			!RouteIds.Contains(Contact.RouteSegmentId) || Contact.LastKnownTarget.RouteSegmentId != Contact.RouteSegmentId ||
			Contact.Confidence < 0.0 || Contact.Confidence > 1.0)
		{
			return Fail(FString::Printf(TEXT("M10 contact '%s' has unresolved ships, route sample, or confidence."), *Contact.ContactId.ToString()));
		}
	}

	for (const FRouteSecuritySnapshot& Security : State.RouteSecuritySnapshots)
	{
		if (Security.SnapshotId.IsNone() || !RouteIds.Contains(Security.RouteSegmentId) || !JurisdictionIds.Contains(Security.JurisdictionId) ||
			!EventIds.Contains(Security.SourceEventId) || Security.WindowEndTimeSeconds <= Security.WindowStartTimeSeconds ||
			Security.ProcessedWatermark <= 0 || Security.SecurityRating < 0.0 || Security.SecurityRating > 1.0 ||
			Security.PatrolCoverageScore < 0.0 || Security.PatrolCoverageScore > 1.0 ||
			Security.PirateRiskScore < 0.0 || Security.PirateRiskScore > 1.0 || Security.RouteValue < 0.0)
		{
			return Fail(FString::Printf(TEXT("M10 route security snapshot '%s' is incomplete or has unresolved references."), *Security.SnapshotId.ToString()));
		}
	}

	TSet<FName> PatrolReservationIds;
	TSet<FName> PatrolIdempotencyKeys;
	for (const FPatrolReservationRecord& Reservation : State.PatrolReservations)
	{
		const FFactionOperationalState* Operation = FindConst(State.FactionOperations, [&Reservation](const FFactionOperationalState& Candidate)
		{
			return Candidate.FactionId == Reservation.FactionId && Candidate.PatrolAssetIds.Contains(Reservation.PatrolAssetId);
		});
		if (Reservation.ReservationId.IsNone() || PatrolReservationIds.Contains(Reservation.ReservationId) ||
			Reservation.IdempotencyKey.IsNone() || PatrolIdempotencyKeys.Contains(Reservation.IdempotencyKey) ||
			!FactionIds.Contains(Reservation.FactionId) || !JurisdictionIds.Contains(Reservation.JurisdictionId) ||
			!RouteIds.Contains(Reservation.RouteSegmentId) || !EventIds.Contains(Reservation.SourceEventId) ||
			!Operation || Reservation.ExpiresAtTimeSeconds <= 0.0 || Reservation.State.IsNone())
		{
			return Fail(FString::Printf(TEXT("M10 patrol reservation '%s' does not resolve to a patrol asset, jurisdiction, route, event, expiry, and idempotency key."), *Reservation.ReservationId.ToString()));
		}
		PatrolReservationIds.Add(Reservation.ReservationId);
		PatrolIdempotencyKeys.Add(Reservation.IdempotencyKey);
	}

	TSet<FName> HazardIds;
	TSet<FName> HazardIdempotencyKeys;
	for (const FInterdictionHazardRecord& Hazard : State.InterdictionHazards)
	{
		if (Hazard.HazardId.IsNone() || HazardIds.Contains(Hazard.HazardId) ||
			Hazard.IdempotencyKey.IsNone() || HazardIdempotencyKeys.Contains(Hazard.IdempotencyKey) ||
			!EventIds.Contains(Hazard.SourceEventId) || !RouteIds.Contains(Hazard.RouteSegmentId) ||
			!GroupIds.Contains(Hazard.OwnerGroupId) || !ShipIds.Contains(Hazard.TargetShipId) ||
			Hazard.RouteSample.RouteSegmentId != Hazard.RouteSegmentId || Hazard.ExpiresAtTimeSeconds <= 0.0 ||
			!EventIds.Contains(Hazard.SaveLoadEventId) || Hazard.State.IsNone())
		{
			return Fail(FString::Printf(TEXT("M10 interdiction hazard '%s' must reference a route sample, owner group, target ship, expiry, save event, and idempotency key."), *Hazard.HazardId.ToString()));
		}
		HazardIds.Add(Hazard.HazardId);
		HazardIdempotencyKeys.Add(Hazard.IdempotencyKey);
	}

	TSet<FName> DistressIds;
	TSet<FName> DistressIdempotencyKeys;
	for (const FDistressEventRecord& Distress : State.DistressEvents)
	{
		if (Distress.DistressEventId.IsNone() || DistressIds.Contains(Distress.DistressEventId) ||
			Distress.IdempotencyKey.IsNone() || DistressIdempotencyKeys.Contains(Distress.IdempotencyKey) ||
			!EventIds.Contains(Distress.SourceEventId) || !ShipIds.Contains(Distress.SourceShipId) ||
			!ShipIds.Contains(Distress.ThreatId) || !RouteIds.Contains(Distress.LocationTarget.RouteSegmentId) || Distress.State.IsNone())
		{
			return Fail(FString::Printf(TEXT("M10 distress record '%s' has unresolved source, threat, route location, state, or idempotency references."), *Distress.DistressEventId.ToString()));
		}
		for (const FName ReservationId : Distress.RespondingPatrolReservationIds)
		{
			if (!PatrolReservationIds.Contains(ReservationId))
			{
				return Fail(FString::Printf(TEXT("M10 distress record '%s' references missing patrol reservation '%s'."), *Distress.DistressEventId.ToString(), *ReservationId.ToString()));
			}
		}
		for (const FName MessageInstanceId : Distress.MessageInstanceIds)
		{
			if (!MessageInstanceIds.Contains(MessageInstanceId))
			{
				return Fail(FString::Printf(TEXT("M10 distress record '%s' references missing message '%s'."), *Distress.DistressEventId.ToString(), *MessageInstanceId.ToString()));
			}
		}
		DistressIds.Add(Distress.DistressEventId);
		DistressIdempotencyKeys.Add(Distress.IdempotencyKey);
	}

	TSet<FName> EncounterIds;
	TSet<FName> EngagementIds;
	TSet<FName> EncounterIdempotencyKeys;
	for (const FAbstractEngagementRecord& Engagement : State.AbstractEngagements)
	{
		EngagementIds.Add(Engagement.EngagementId);
	}
	for (const FLogicalEncounterRecord& Encounter : State.LogicalEncounters)
	{
		if (Encounter.EncounterId.IsNone() || EncounterIds.Contains(Encounter.EncounterId) ||
			Encounter.IdempotencyKey.IsNone() || EncounterIdempotencyKeys.Contains(Encounter.IdempotencyKey) ||
			!EventIds.Contains(Encounter.SourceEventId) || !RouteIds.Contains(Encounter.RouteSegmentId) ||
			!HazardIds.Contains(Encounter.InterdictionHazardId) || !DistressIds.Contains(Encounter.DistressEventId) ||
			Encounter.ProcessedWatermark <= 0 || Encounter.bRequiresActor || Encounter.State.IsNone())
		{
			return Fail(FString::Printf(TEXT("M10 logical encounter '%s' is incomplete, actor-bound, duplicate, or has unresolved event/route/hazard/distress references."), *Encounter.EncounterId.ToString()));
		}
		for (const FName ShipId : Encounter.ParticipantShipIds)
		{
			if (!ShipIds.Contains(ShipId))
			{
				return Fail(FString::Printf(TEXT("M10 logical encounter '%s' references missing ship '%s'."), *Encounter.EncounterId.ToString(), *ShipId.ToString()));
			}
		}
		for (const FName GroupId : Encounter.ParticipantGroupIds)
		{
			if (!GroupIds.Contains(GroupId))
			{
				return Fail(FString::Printf(TEXT("M10 logical encounter '%s' references missing group '%s'."), *Encounter.EncounterId.ToString(), *GroupId.ToString()));
			}
		}
		if (Encounter.State == FName(TEXT("resolved")) && !EngagementIds.Contains(Encounter.EngagementId))
		{
			return Fail(FString::Printf(TEXT("M10 resolved encounter '%s' is missing its abstract engagement output."), *Encounter.EncounterId.ToString()));
		}
		EncounterIds.Add(Encounter.EncounterId);
		EncounterIdempotencyKeys.Add(Encounter.IdempotencyKey);
	}

	TSet<FName> EngagementIdempotencyKeys;
	for (const FAbstractEngagementRecord& Engagement : State.AbstractEngagements)
	{
		if (Engagement.EngagementId.IsNone() || Engagement.IdempotencyKey.IsNone() || EngagementIdempotencyKeys.Contains(Engagement.IdempotencyKey) ||
			!EncounterIds.Contains(Engagement.EncounterId) || !EventIds.Contains(Engagement.SourceEventId) ||
			!ShipIds.Contains(Engagement.AttackerId) || !ShipIds.Contains(Engagement.DefenderId) || Engagement.OutcomeType.IsNone() || Engagement.bRequiresActor)
		{
			return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' is incomplete, actor-bound, duplicate, or has unresolved encounter/event/ship references."), *Engagement.EngagementId.ToString()));
		}
		for (const FName CargoId : Engagement.CargoTransferResultIds)
		{
			if (!CargoTransferIds.Contains(CargoId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing cargo result '%s'."), *Engagement.EngagementId.ToString(), *CargoId.ToString()));
			}
		}
		for (const FName MarketId : Engagement.MarketTransactionResultIds)
		{
			if (!MarketTransactionIds.Contains(MarketId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing market result '%s'."), *Engagement.EngagementId.ToString(), *MarketId.ToString()));
			}
		}
		for (const FName TransactionId : Engagement.GameplayTransactionIds)
		{
			if (!GameplayTransactionIds.Contains(TransactionId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing gameplay transaction '%s'."), *Engagement.EngagementId.ToString(), *TransactionId.ToString()));
			}
		}
		for (const FName LedgerId : Engagement.CreditLedgerEntryIds)
		{
			if (!CreditLedgerIds.Contains(LedgerId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing credit ledger entry '%s'."), *Engagement.EngagementId.ToString(), *LedgerId.ToString()));
			}
		}
		for (const FName OffenseId : Engagement.OffenseIds)
		{
			if (!OffenseIds.Contains(OffenseId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing offense '%s'."), *Engagement.EngagementId.ToString(), *OffenseId.ToString()));
			}
		}
		for (const FName EvidenceId : Engagement.EvidenceIds)
		{
			if (!EvidenceIds.Contains(EvidenceId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing evidence '%s'."), *Engagement.EngagementId.ToString(), *EvidenceId.ToString()));
			}
		}
		for (const FName CriminalId : Engagement.CriminalRecordIds)
		{
			if (!CriminalRecordIds.Contains(CriminalId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing criminal record '%s'."), *Engagement.EngagementId.ToString(), *CriminalId.ToString()));
			}
		}
		for (const FName MessageId : Engagement.MessageInstanceIds)
		{
			if (!MessageInstanceIds.Contains(MessageId))
			{
				return Fail(FString::Printf(TEXT("M10 abstract engagement '%s' references missing message '%s'."), *Engagement.EngagementId.ToString(), *MessageId.ToString()));
			}
		}
		EngagementIdempotencyKeys.Add(Engagement.IdempotencyKey);
	}

	TSet<FName> PromotionIds;
	TSet<FName> PromotionIdempotencyKeys;
	for (const FLogicalActorPromotionRecord& Promotion : State.ActorPromotionAttachments)
	{
		if (Promotion.PromotionId.IsNone() || PromotionIds.Contains(Promotion.PromotionId) ||
			Promotion.IdempotencyKey.IsNone() || PromotionIdempotencyKeys.Contains(Promotion.IdempotencyKey) ||
			!EncounterIds.Contains(Promotion.EncounterId) || !EventIds.Contains(Promotion.SourceEventId) ||
			!ShipIds.Contains(Promotion.ShipInstanceId) || Promotion.RealizationToken.IsNone() || Promotion.bCanResolveEncounter)
		{
			return Fail(FString::Printf(TEXT("M10 actor promotion '%s' must attach metadata to an existing logical encounter/event/ship and must not be allowed to resolve the encounter."), *Promotion.PromotionId.ToString()));
		}
		PromotionIds.Add(Promotion.PromotionId);
		PromotionIdempotencyKeys.Add(Promotion.IdempotencyKey);
	}

	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ValidateCombatDamageThreatState(const FStarSystemDefinition& SystemDefinition, const FSystemicGameplayState& State, FString& OutFailureReason)
{
	if (!ValidateLogicalEncounterState(SystemDefinition, State, OutFailureReason))
	{
		return false;
	}

	auto Fail = [&OutFailureReason](const FString& Reason)
	{
		OutFailureReason = Reason;
		return false;
	};

	TSet<FName> ShipIds;
	TSet<FName> GroupIds;
	TSet<FName> RouteIds;
	TSet<FName> CombatantIds;
	CombatantIds.Add(TEXT("player_ship"));
	for (const FShipTrafficInstance& Ship : SystemDefinition.LogicalTraffic)
	{
		ShipIds.Add(Ship.ShipInstanceId);
		CombatantIds.Add(Ship.ShipInstanceId);
	}
	for (const FShipGroupState& Group : SystemDefinition.ShipGroups)
	{
		GroupIds.Add(Group.GroupId);
	}
	for (const FTrafficRouteSegmentDefinition& Route : SystemDefinition.TrafficRoutes)
	{
		RouteIds.Add(Route.RouteSegmentId);
	}
	for (const FRealizedActorMappingRecord& Mapping : State.RealizedActorMappings)
	{
		if (!Mapping.ShipInstanceId.IsNone())
		{
			CombatantIds.Add(Mapping.ShipInstanceId);
		}
	}

	TSet<FName> ThreatIds;
	TSet<FName> ThreatIdempotencyKeys;
	for (const FThreatRecord& Threat : State.ThreatRecords)
	{
		if (Threat.ThreatId.IsNone() || ThreatIds.Contains(Threat.ThreatId) ||
			Threat.IdempotencyKey.IsNone() || ThreatIdempotencyKeys.Contains(Threat.IdempotencyKey) ||
			!CombatantIds.Contains(Threat.AttackerId) || !CombatantIds.Contains(Threat.DefenderId) ||
			Threat.LastKnownTarget.TargetType != FName(TEXT("route_sample")) ||
			!RouteIds.Contains(Threat.LastKnownTarget.RouteSegmentId) ||
			Threat.Severity <= 0.0 || Threat.Confidence <= 0.0 || Threat.ExpiresAtTimeSeconds <= 0.0)
		{
			return Fail(FString::Printf(TEXT("M10.5 threat '%s' must use stable combatant IDs, route-frame target, severity, confidence, expiry, and unique idempotency."), *Threat.ThreatId.ToString()));
		}
		ThreatIds.Add(Threat.ThreatId);
		ThreatIdempotencyKeys.Add(Threat.IdempotencyKey);
	}
	if (ThreatIds.IsEmpty())
	{
		return Fail(TEXT("M10.5 requires at least one threat record."));
	}

	TSet<FName> DurabilityIds;
	TSet<FName> DurabilityIdempotencyKeys;
	TSet<FName> DurabilityCombatants;
	for (const FShipDurabilityState& Durability : State.ShipDurabilityStates)
	{
		if (Durability.DurabilityId.IsNone() || DurabilityIds.Contains(Durability.DurabilityId) ||
			Durability.IdempotencyKey.IsNone() || DurabilityIdempotencyKeys.Contains(Durability.IdempotencyKey) ||
			!CombatantIds.Contains(Durability.CombatantId) || Durability.MaxShield <= 0.0 || Durability.MaxHull <= 0.0 ||
			Durability.Shield < 0.0 || Durability.Shield - Durability.MaxShield > UE_SMALL_NUMBER ||
			Durability.Hull < 0.0 || Durability.Hull - Durability.MaxHull > UE_SMALL_NUMBER)
		{
			return Fail(FString::Printf(TEXT("M10.5 durability '%s' must resolve to a player, logical NPC, or realized combatant with authored shield/hull capacity."), *Durability.DurabilityId.ToString()));
		}
		DurabilityIds.Add(Durability.DurabilityId);
		DurabilityIdempotencyKeys.Add(Durability.IdempotencyKey);
		DurabilityCombatants.Add(Durability.CombatantId);
	}
	if (DurabilityCombatants.IsEmpty())
	{
		return Fail(TEXT("M10.5 requires durability state for damageable combatants."));
	}

	TSet<FName> DamageIds;
	TSet<FName> DamageIdempotencyKeys;
	for (const FDamageEventRecord& Damage : State.DamageEvents)
	{
		if (Damage.DamageEventId.IsNone() || DamageIds.Contains(Damage.DamageEventId) ||
			Damage.IdempotencyKey.IsNone() || DamageIdempotencyKeys.Contains(Damage.IdempotencyKey) ||
			!DurabilityCombatants.Contains(Damage.SourceCombatantId) ||
			!DurabilityCombatants.Contains(Damage.TargetCombatantId) ||
			Damage.DamageType.IsNone() || Damage.Amount <= 0.0 || Damage.AuthorityTimeSeconds < 0.0 ||
			!ThreatIds.Contains(Damage.ThreatId))
		{
			return Fail(FString::Printf(TEXT("M10.5 damage event '%s' must include stable IDs, source/target durability, damage type, amount, authority time, threat, and unique idempotency."), *Damage.DamageEventId.ToString()));
		}
		DamageIds.Add(Damage.DamageEventId);
		DamageIdempotencyKeys.Add(Damage.IdempotencyKey);
	}
	if (DamageIds.IsEmpty())
	{
		return Fail(TEXT("M10.5 requires at least one damage event record."));
	}

	TSet<FName> IntentIds;
	TSet<FName> IntentIdempotencyKeys;
	bool bHasAttackIntent = false;
	for (const FRealizedAISteeringIntent& Intent : State.RealizedSteeringIntents)
	{
		if (Intent.IntentId.IsNone() || IntentIds.Contains(Intent.IntentId) ||
			Intent.IdempotencyKey.IsNone() || IntentIdempotencyKeys.Contains(Intent.IdempotencyKey))
		{
			return Fail(FString::Printf(TEXT("M10.5 attack intent '%s' must have unique intent and idempotency IDs."), *Intent.IntentId.ToString()));
		}
		IntentIds.Add(Intent.IntentId);
		IntentIdempotencyKeys.Add(Intent.IdempotencyKey);

		if (Intent.IntentType != FName(TEXT("attack")))
		{
			continue;
		}
		if (!CombatantIds.Contains(Intent.ShipInstanceId) ||
			(!Intent.TargetShipId.IsNone() && !CombatantIds.Contains(Intent.TargetShipId)) ||
			(!Intent.TargetGroupId.IsNone() && !GroupIds.Contains(Intent.TargetGroupId)) ||
			!ThreatIds.Contains(Intent.ThreatId) ||
			Intent.TargetFrame.TargetType != FName(TEXT("route_sample")) ||
			!RouteIds.Contains(Intent.TargetFrame.RouteSegmentId))
		{
			return Fail(FString::Printf(TEXT("M10.5 attack intent '%s' must bind combatant IDs, threat, and a route-frame target without static world-space coupling."), *Intent.IntentId.ToString()));
		}
		bHasAttackIntent = true;
	}
	if (!bHasAttackIntent)
	{
		return Fail(TEXT("M10.5 requires an abstract attack intent consumable by logical and realized combat."));
	}

	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ValidateRealizedAISliceState(const FStarSystemDefinition& SystemDefinition, const FSystemicGameplayState& State, FString& OutFailureReason)
{
	if (!ValidateLogicalEncounterState(SystemDefinition, State, OutFailureReason))
	{
		return false;
	}

	auto Fail = [&OutFailureReason](const FString& Reason)
	{
		OutFailureReason = Reason;
		return false;
	};

	TSet<FName> ShipIds;
	TSet<FName> TraderIds;
	TSet<FName> GroupIds;
	TSet<FName> RouteIds;
	for (const FShipTrafficInstance& Ship : SystemDefinition.LogicalTraffic)
	{
		ShipIds.Add(Ship.ShipInstanceId);
		if (Ship.CurrentGoal.GoalKind == EShipGoalKind::TradeRoute)
		{
			TraderIds.Add(Ship.ShipInstanceId);
		}
	}
	for (const FShipGroupState& Group : SystemDefinition.ShipGroups)
	{
		GroupIds.Add(Group.GroupId);
	}
	for (const FTrafficRouteSegmentDefinition& Route : SystemDefinition.TrafficRoutes)
	{
		RouteIds.Add(Route.RouteSegmentId);
	}

	if (!ShipIds.Contains(TEXT("trader_brink_01")) ||
		!ShipIds.Contains(TEXT("trader_brink_02")) ||
		!ShipIds.Contains(TEXT("trader_wayfarer_01")) ||
		!GroupIds.Contains(TEXT("group_traders_m7_trade_lane")) ||
		!ShipIds.Contains(TEXT("patrol_frontier_local_01")) ||
		!ShipIds.Contains(TEXT("patrol_frontier_local_02")) ||
		!ShipIds.Contains(TEXT("pirate_raider_01")) ||
		!ShipIds.Contains(TEXT("pirate_raider_02")))
	{
		return Fail(TEXT("M11 fixture is missing required trader, patrol, or pirate logical ships/groups."));
	}

	const FTrafficRouteSegmentDefinition* Route = FindConst(SystemDefinition.TrafficRoutes, [](const FTrafficRouteSegmentDefinition& Candidate)
	{
		return Candidate.RouteSegmentId == FName(TEXT("m7_brink_watch_wayfarer_trade"));
	});
	if (!Route || Route->RouteValue <= 0.0 || Route->AvoidanceAnchorIds.IsEmpty() || Route->ExclusionZoneIds.IsEmpty() ||
		Route->SourceAnchorId.IsNone() || Route->DestinationAnchorId.IsNone())
	{
		return Fail(TEXT("M11 fixture requires the valuable moving trade route to resolve moving endpoints and gravity-lockout metadata."));
	}

	const bool bHasFastStation = SystemDefinition.Stations.ContainsByPredicate([](const FStationDefinition& Station)
	{
		return Station.StationId == FName(TEXT("brink_watch")) && Station.Orbit.ParentId == FName(TEXT("brink")) && Station.Orbit.PeriodSeconds > 0.0 && Station.Orbit.PeriodSeconds <= 1800.0;
	});
	const bool bHasNestedMoonEndpoint = SystemDefinition.Stations.ContainsByPredicate([](const FStationDefinition& Station)
	{
		return Station.StationId == FName(TEXT("wayfarer_depot")) && Station.Orbit.ParentId == FName(TEXT("brink_minor")) && Station.Orbit.PeriodSeconds > 0.0;
	});
	if (!bHasFastStation || !bHasNestedMoonEndpoint)
	{
		return Fail(TEXT("M11 fixture requires a fast-orbiting station endpoint and a nested-moon endpoint."));
	}

	const FRealizedActorBudgetProfile* Budget = FindConst(State.RealizedActorBudgetProfiles, [](const FRealizedActorBudgetProfile& Candidate)
	{
		return Candidate.BudgetProfileId == FName(TEXT("actor_budget_m11_low"));
	});
	TSet<FName> BudgetIds;
	TSet<FName> BudgetIdempotencyKeys;
	for (const FRealizedActorBudgetProfile& BudgetProfile : State.RealizedActorBudgetProfiles)
	{
		if (BudgetProfile.BudgetProfileId.IsNone() || BudgetIds.Contains(BudgetProfile.BudgetProfileId) ||
			BudgetProfile.IdempotencyKey.IsNone() || BudgetIdempotencyKeys.Contains(BudgetProfile.IdempotencyKey) ||
			BudgetProfile.MaxRealizedActors <= 0 || BudgetProfile.MaxPromotionsPerTick <= 0 || BudgetProfile.PromotionRadiusCm <= 0.0)
		{
			return Fail(FString::Printf(TEXT("M11 actor budget profile '%s' must be unique, idempotent, capped, and proximity-aware."), *BudgetProfile.BudgetProfileId.ToString()));
		}
		BudgetIds.Add(BudgetProfile.BudgetProfileId);
		BudgetIdempotencyKeys.Add(BudgetProfile.IdempotencyKey);
	}
	if (!Budget || Budget->MaxRealizedActors >= State.RealizedActorMappings.Num())
	{
		return Fail(TEXT("M11 fixture requires a low actor budget profile that can prove blocked promotions."));
	}

	TSet<FName> EncounterIds;
	for (const FLogicalEncounterRecord& Encounter : State.LogicalEncounters)
	{
		EncounterIds.Add(Encounter.EncounterId);
	}
	TSet<FName> EventIds;
	for (const FSimulationEventRecord& Event : State.Events)
	{
		EventIds.Add(Event.EventId);
	}
	TSet<FName> MessageIds;
	for (const FMessageDefinition& Message : State.MessageDefinitions)
	{
		MessageIds.Add(Message.MessageId);
	}
	const TSet<FName> RequiredMessages = {
		TEXT("msg_distress_trade_lane_01"),
		TEXT("msg_pirate_demand_01"),
		TEXT("msg_police_scan_01"),
		TEXT("msg_flee_surrender_01")
	};
	for (const FName MessageId : RequiredMessages)
	{
		if (!MessageIds.Contains(MessageId))
		{
			return Fail(FString::Printf(TEXT("M11 comms hook message '%s' is missing."), *MessageId.ToString()));
		}
	}

	TSet<FName> MappingIds;
	TSet<FName> MappingTokens;
	TSet<FName> MappingIdempotencyKeys;
	TSet<FName> MappingShipIds;
	for (const FRealizedActorMappingRecord& Mapping : State.RealizedActorMappings)
	{
		if (Mapping.MappingId.IsNone() || MappingIds.Contains(Mapping.MappingId) ||
			!ShipIds.Contains(Mapping.ShipInstanceId) || !GroupIds.Contains(Mapping.GroupId) ||
			!EncounterIds.Contains(Mapping.EncounterId) || !EventIds.Contains(Mapping.SourceEventId) ||
			Mapping.RealizationToken.IsNone() || MappingTokens.Contains(Mapping.RealizationToken) ||
			Mapping.ActorBudgetProfileId != Budget->BudgetProfileId || Mapping.IdempotencyKey.IsNone() ||
			MappingIdempotencyKeys.Contains(Mapping.IdempotencyKey))
		{
			return Fail(FString::Printf(TEXT("M11 actor mapping '%s' has unresolved ship/group/encounter/event/budget data."), *Mapping.MappingId.ToString()));
		}
		MappingIds.Add(Mapping.MappingId);
		MappingTokens.Add(Mapping.RealizationToken);
		MappingIdempotencyKeys.Add(Mapping.IdempotencyKey);
		MappingShipIds.Add(Mapping.ShipInstanceId);
	}
	for (const FName RequiredShip : { FName(TEXT("trader_brink_01")), FName(TEXT("trader_brink_02")), FName(TEXT("trader_wayfarer_01")), FName(TEXT("pirate_raider_01")), FName(TEXT("patrol_frontier_local_01")) })
	{
		if (!MappingShipIds.Contains(RequiredShip))
		{
			return Fail(FString::Printf(TEXT("M11 required promotion mapping for '%s' is missing."), *RequiredShip.ToString()));
		}
	}
	for (const FName RequiredTradeShip : { FName(TEXT("trader_brink_01")), FName(TEXT("trader_brink_02")), FName(TEXT("trader_wayfarer_01")) })
	{
		const FShipTrafficInstance* TradeShip = FindConst(SystemDefinition.LogicalTraffic, [RequiredTradeShip](const FShipTrafficInstance& Candidate)
		{
			return Candidate.ShipInstanceId == RequiredTradeShip;
		});
		if (!TradeShip || TradeShip->GroupId != FName(TEXT("group_traders_m7_trade_lane")) ||
			TradeShip->CurrentGoal.GoalKind != EShipGoalKind::TradeRoute ||
			!RouteIds.Contains(TradeShip->CurrentGoal.RouteSegmentId) ||
			TradeShip->CurrentGoal.TargetFrame.TargetType != FName(TEXT("route_sample")))
		{
			return Fail(FString::Printf(TEXT("M11 trader '%s' must keep canonical group and route-frame goal state."), *RequiredTradeShip.ToString()));
		}
	}

	const bool bHasTraderCargo = State.Containers.ContainsByPredicate([](const FContainerState& Container)
	{
		return Container.ContainerId == FName(TEXT("trader_brink_cargo")) &&
			Container.OwnerId == FName(TEXT("trader_brink_01")) &&
			Container.LocationType == FName(TEXT("ship")) &&
			Container.Stacks.ContainsByPredicate([](const FItemStackState& Stack)
			{
				return Stack.StackId == FName(TEXT("trader_brink_ore_stack")) &&
					Stack.OwnerFactionId == FName(TEXT("frontier_local_authority")) &&
					Stack.Quantity > 0;
			});
	});
	const bool bHasPirateCargo = State.Containers.ContainsByPredicate([](const FContainerState& Container)
	{
		return Container.ContainerId == FName(TEXT("pirate_raider_cargo")) &&
			Container.OwnerId == FName(TEXT("pirate_raider_01")) &&
			Container.LocationType == FName(TEXT("ship"));
	});
	const bool bHasTraderAccount = State.CreditAccounts.ContainsByPredicate([](const FCreditAccountRecord& Account)
	{
		return Account.AccountId == FName(TEXT("account_trader_brink_01")) && Account.OwnerId == FName(TEXT("trader_brink_01")) && Account.AvailableBalance > 0;
	});
	const bool bHasPirateFaction = State.FactionRelationships.ContainsByPredicate([](const FFactionRelationshipRecord& Relationship)
	{
		return Relationship.SourceFactionId == FName(TEXT("ember_raiders")) && Relationship.TargetFactionId == FName(TEXT("frontier_local_authority"));
	});
	if (!bHasTraderCargo || !bHasPirateCargo || !bHasTraderAccount || !bHasPirateFaction)
	{
		return Fail(TEXT("M11 fixture must preserve cargo summaries, trader credit state, and pirate faction hostility from the M9/M10 contracts."));
	}

	TSet<FName> ThreatIds;
	TSet<FName> ThreatIdempotencyKeys;
	for (const FThreatRecord& Threat : State.ThreatRecords)
	{
		if (Threat.ThreatId.IsNone() || ThreatIds.Contains(Threat.ThreatId) ||
			!ShipIds.Contains(Threat.AttackerId) || !ShipIds.Contains(Threat.DefenderId) ||
			Threat.LastKnownTarget.TargetType != FName(TEXT("route_sample")) ||
			!RouteIds.Contains(Threat.LastKnownTarget.RouteSegmentId) ||
			Threat.Severity <= 0.0 || Threat.Confidence <= 0.0 || Threat.ExpiresAtTimeSeconds <= 0.0 ||
			Threat.IdempotencyKey.IsNone() || ThreatIdempotencyKeys.Contains(Threat.IdempotencyKey))
		{
			return Fail(FString::Printf(TEXT("M11 threat '%s' must use stable combatant IDs and a moving route-frame target."), *Threat.ThreatId.ToString()));
		}
		ThreatIds.Add(Threat.ThreatId);
		ThreatIdempotencyKeys.Add(Threat.IdempotencyKey);
	}
	TSet<FName> IntentIds;
	TSet<FName> IntentIdempotencyKeys;
	for (const FRealizedAISteeringIntent& Intent : State.RealizedSteeringIntents)
	{
		if (Intent.IntentId.IsNone() || IntentIds.Contains(Intent.IntentId) ||
			Intent.IdempotencyKey.IsNone() || IntentIdempotencyKeys.Contains(Intent.IdempotencyKey) ||
			!ShipIds.Contains(Intent.ShipInstanceId) ||
			(!Intent.TargetShipId.IsNone() && !ShipIds.Contains(Intent.TargetShipId)) ||
			(!Intent.TargetGroupId.IsNone() && !GroupIds.Contains(Intent.TargetGroupId)) ||
			!RouteIds.Contains(Intent.RouteSegmentId) ||
			Intent.TargetFrame.TargetType != FName(TEXT("route_sample")) ||
			Intent.TargetFrame.RouteSegmentId != Intent.RouteSegmentId ||
			(!Intent.ThreatId.IsNone() && !ThreatIds.Contains(Intent.ThreatId)))
		{
			return Fail(FString::Printf(TEXT("M11 steering intent '%s' must use route-frame targets, not static world-space targets."), *Intent.IntentId.ToString()));
		}
		IntentIds.Add(Intent.IntentId);
		IntentIdempotencyKeys.Add(Intent.IdempotencyKey);
	}
	TSet<FName> RequiredIntentTypes = { TEXT("approach"), TEXT("formation"), TEXT("flee"), TEXT("attack") };
	for (const FRealizedAISteeringIntent& Intent : State.RealizedSteeringIntents)
	{
		RequiredIntentTypes.Remove(Intent.IntentType);
	}
	if (!RequiredIntentTypes.IsEmpty())
	{
		return Fail(TEXT("M11 requires approach, formation, flee, and attack steering intents."));
	}

	TSet<FName> HookTypes = { TEXT("scan"), TEXT("pirate_demand"), TEXT("distress"), TEXT("flee_surrender") };
	TSet<FName> HookIds;
	TSet<FName> HookIdempotencyKeys;
	for (const FRealizedAICommsHook& Hook : State.RealizedCommsHooks)
	{
		if (Hook.HookId.IsNone() || HookIds.Contains(Hook.HookId) ||
			Hook.IdempotencyKey.IsNone() || HookIdempotencyKeys.Contains(Hook.IdempotencyKey) ||
			!HookTypes.Contains(Hook.HookType) ||
			!MessageIds.Contains(Hook.MessageId) ||
			(!Hook.SourceShipId.IsNone() && !ShipIds.Contains(Hook.SourceShipId)) ||
			(!Hook.TargetShipId.IsNone() && !ShipIds.Contains(Hook.TargetShipId)) ||
			!EncounterIds.Contains(Hook.EncounterId))
		{
			return Fail(FString::Printf(TEXT("M11 comms hook '%s' has unresolved ship/message/encounter refs."), *Hook.HookId.ToString()));
		}
		HookIds.Add(Hook.HookId);
		HookIdempotencyKeys.Add(Hook.IdempotencyKey);
		HookTypes.Remove(Hook.HookType);
	}
	if (!HookTypes.IsEmpty())
	{
		return Fail(TEXT("M11 requires scan, pirate demand, distress, and flee/surrender comms hooks."));
	}

	TSet<FName> SnapshotIds;
	TSet<FName> SnapshotIdempotencyKeys;
	for (const FRealizedAIDemotionSnapshot& Snapshot : State.RealizedDemotionSnapshots)
	{
		if (Snapshot.SnapshotId.IsNone() || SnapshotIds.Contains(Snapshot.SnapshotId) ||
			Snapshot.IdempotencyKey.IsNone() || SnapshotIdempotencyKeys.Contains(Snapshot.IdempotencyKey) ||
			!ShipIds.Contains(Snapshot.ShipInstanceId) || !GroupIds.Contains(Snapshot.GroupId) ||
			!EncounterIds.Contains(Snapshot.EncounterId) || Snapshot.RealizationToken.IsNone() ||
			Snapshot.GoalState.GoalId.IsNone() || Snapshot.TargetFrame.TargetType != FName(TEXT("route_sample")) ||
			!RouteIds.Contains(Snapshot.TargetFrame.RouteSegmentId) ||
			(!Snapshot.ThreatId.IsNone() && !ThreatIds.Contains(Snapshot.ThreatId)) ||
			Snapshot.RecoveryPolicyId.IsNone())
		{
			return Fail(FString::Printf(TEXT("M11 demotion snapshot '%s' cannot restore goal/group/threat/velocity-frame state."), *Snapshot.SnapshotId.ToString()));
		}
		SnapshotIds.Add(Snapshot.SnapshotId);
		SnapshotIdempotencyKeys.Add(Snapshot.IdempotencyKey);
	}

	TSet<FName> DurabilityIds;
	TSet<FName> DurabilityIdempotencyKeys;
	TSet<FName> DurabilityCombatants;
	for (const FShipDurabilityState& Durability : State.ShipDurabilityStates)
	{
		const bool bKnownCombatant = ShipIds.Contains(Durability.CombatantId) || Durability.CombatantId == FName(TEXT("player_ship"));
		if (Durability.DurabilityId.IsNone() || DurabilityIds.Contains(Durability.DurabilityId) ||
			Durability.IdempotencyKey.IsNone() || DurabilityIdempotencyKeys.Contains(Durability.IdempotencyKey) ||
			!bKnownCombatant || Durability.MaxShield <= 0.0 || Durability.MaxHull <= 0.0 ||
			Durability.Shield < 0.0 || Durability.Shield - Durability.MaxShield > UE_SMALL_NUMBER ||
			Durability.Hull < 0.0 || Durability.Hull - Durability.MaxHull > UE_SMALL_NUMBER)
		{
			return Fail(FString::Printf(TEXT("M11 durability '%s' must resolve to a logical or realized combatant with authored capacity."), *Durability.DurabilityId.ToString()));
		}
		DurabilityIds.Add(Durability.DurabilityId);
		DurabilityIdempotencyKeys.Add(Durability.IdempotencyKey);
		DurabilityCombatants.Add(Durability.CombatantId);
	}
	TSet<FName> DamageIds;
	TSet<FName> DamageIdempotencyKeys;
	for (const FDamageEventRecord& Damage : State.DamageEvents)
	{
		if (Damage.DamageEventId.IsNone() || DamageIds.Contains(Damage.DamageEventId) ||
			Damage.IdempotencyKey.IsNone() || DamageIdempotencyKeys.Contains(Damage.IdempotencyKey) ||
			!DurabilityCombatants.Contains(Damage.SourceCombatantId) ||
			!DurabilityCombatants.Contains(Damage.TargetCombatantId) || Damage.DamageType.IsNone() ||
			Damage.Amount <= 0.0 || Damage.AuthorityTimeSeconds < 0.0 ||
			!ThreatIds.Contains(Damage.ThreatId))
		{
			return Fail(FString::Printf(TEXT("M11 damage event '%s' must use the shared combat/threat contract."), *Damage.DamageEventId.ToString()));
		}
		DamageIds.Add(Damage.DamageEventId);
		DamageIdempotencyKeys.Add(Damage.IdempotencyKey);
	}

	const FInterdictionHazardRecord* Hazard = FindConst(State.InterdictionHazards, [](const FInterdictionHazardRecord& Candidate)
	{
		return Candidate.HazardId == FName(TEXT("hazard_interdiction_trade_lane_01"));
	});
	if (!Hazard || Hazard->State != FName(TEXT("pending")) || Hazard->SaveLoadEventId != FName(TEXT("event_m10_pirate_trade_lane_01")))
	{
		return Fail(TEXT("M11 pending interdiction must keep the same hazard and event IDs across save/load."));
	}

	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ValidateM12GameplayState(const FStarSystemDefinition& SystemDefinition, const FSystemicGameplayState& State, FString& OutFailureReason)
{
	if (!ValidateRealizedAISliceState(SystemDefinition, State, OutFailureReason))
	{
		return false;
	}

	auto Fail = [&OutFailureReason](const FString& Reason)
	{
		OutFailureReason = Reason;
		return false;
	};

	TSet<FName> RouteIds;
	for (const FTrafficRouteSegmentDefinition& Route : SystemDefinition.TrafficRoutes)
	{
		RouteIds.Add(Route.RouteSegmentId);
	}
	TSet<FName> StationIds;
	TMap<FName, const FStationDefinition*> StationsById;
	for (const FStationDefinition& Station : SystemDefinition.Stations)
	{
		StationIds.Add(Station.StationId);
		StationsById.Add(Station.StationId, &Station);
	}
	TSet<FName> FactionIds;
	for (const FFactionDefinition& Faction : State.Factions)
	{
		FactionIds.Add(Faction.FactionId);
	}
	TSet<FName> AccountIds;
	for (const FCreditAccountRecord& Account : State.CreditAccounts)
	{
		AccountIds.Add(Account.AccountId);
	}
	TSet<FName> CargoManifestIds;
	for (const FCargoManifestDefinition& Manifest : State.CargoManifests)
	{
		if (Manifest.CargoManifestId.IsNone() || CargoManifestIds.Contains(Manifest.CargoManifestId) || Manifest.ManifestType.IsNone() || Manifest.State.IsNone())
		{
			return Fail(FString::Printf(TEXT("M12 cargo manifest '%s' has invalid durable IDs or state."), *Manifest.CargoManifestId.ToString()));
		}
		CargoManifestIds.Add(Manifest.CargoManifestId);
	}
	TSet<FName> MissionDefinitionIds;
	for (const FMissionDefinition& MissionDefinition : State.MissionDefinitions)
	{
		if (MissionDefinition.MissionDefinitionId.IsNone() ||
			MissionDefinitionIds.Contains(MissionDefinition.MissionDefinitionId) ||
			MissionDefinition.Title.IsEmpty() ||
			MissionDefinition.OfferDialog.IsEmpty() ||
			MissionDefinition.InProgressDialog.IsEmpty() ||
			MissionDefinition.TurnInDialog.IsEmpty())
		{
			return Fail(FString::Printf(TEXT("M12 mission definition '%s' must provide stable presentation text."), *MissionDefinition.MissionDefinitionId.ToString()));
		}
		MissionDefinitionIds.Add(MissionDefinition.MissionDefinitionId);
	}
	TSet<FName> LedgerIds;
	for (const FCreditLedgerEntry& Ledger : State.CreditLedger)
	{
		LedgerIds.Add(Ledger.LedgerEntryId);
	}
	TSet<FName> TransactionIds;
	for (const FGameplayTransactionRecord& Transaction : State.Transactions)
	{
		TransactionIds.Add(Transaction.TransactionId);
	}
	TMap<FName, TSet<int32>> JournalSequencesByTransaction;
	TMap<FName, TSet<FName>> JournalSideEffectsByTransaction;
	for (const FGameplayTransactionJournalEntry& Entry : State.TransactionJournal)
	{
		if (!TransactionIds.Contains(Entry.TransactionId) || Entry.SideEffectType.IsNone())
		{
			return Fail(FString::Printf(TEXT("M12 journal entry '%s' has unresolved transaction or side effect type."), *Entry.JournalEntryId.ToString()));
		}
		TSet<int32>& Sequences = JournalSequencesByTransaction.FindOrAdd(Entry.TransactionId);
		if (Sequences.Contains(Entry.Sequence))
		{
			return Fail(FString::Printf(TEXT("M12 transaction '%s' has duplicate journal sequence %d."), *Entry.TransactionId.ToString(), Entry.Sequence));
		}
		Sequences.Add(Entry.Sequence);
		JournalSideEffectsByTransaction.FindOrAdd(Entry.TransactionId).Add(Entry.SideEffectType);
	}
	TSet<FName> ServiceEndpointIds;
	TSet<FName> ServiceTypes;
	for (const FStationServiceEndpointDefinition& Endpoint : State.StationServiceEndpoints)
	{
		ServiceEndpointIds.Add(Endpoint.ServiceEndpointId);
		ServiceTypes.Add(Endpoint.ServiceType);
		if (!Endpoint.StationId.IsNone() && !StationIds.Contains(Endpoint.StationId))
		{
			return Fail(FString::Printf(TEXT("M12 service endpoint '%s' references a missing station."), *Endpoint.ServiceEndpointId.ToString()));
		}
		if (!Endpoint.CreditAccountId.IsNone() && !AccountIds.Contains(Endpoint.CreditAccountId))
		{
			return Fail(FString::Printf(TEXT("M12 service endpoint '%s' references a missing account."), *Endpoint.ServiceEndpointId.ToString()));
		}
	}
	for (const FName RequiredService : { FName(TEXT("market")), FName(TEXT("repair")), FName(TEXT("refuel")), FName(TEXT("rearm")), FName(TEXT("mission_board")) })
	{
		if (!ServiceTypes.Contains(RequiredService))
		{
			return Fail(FString::Printf(TEXT("M12 requires a '%s' station service endpoint."), *RequiredService.ToString()));
		}
	}

	TSet<FName> OfferIds;
	for (const FMissionOfferRecord& Offer : State.MissionOffers)
	{
		const FStationDefinition* SourceStation = StationsById.FindRef(Offer.SourceStationId);
		if (!MissionDefinitionIds.Contains(Offer.MissionDefinitionId) || !SourceStation || !ServiceEndpointIds.Contains(Offer.SourceServiceEndpointId) || !FactionIds.Contains(Offer.IssuerFactionId))
		{
			return Fail(FString::Printf(TEXT("M12 mission offer '%s' does not resolve definition, station, service, and faction references."), *Offer.OfferId.ToString()));
		}
		if (Offer.GiverNpcId.IsNone() || !SourceStation->QuestGiverNpcIds.Contains(Offer.GiverNpcId))
		{
			return Fail(FString::Printf(TEXT("M12 mission offer '%s' must resolve a station-authored giver NPC."), *Offer.OfferId.ToString()));
		}
		for (const FName RequiredTag : Offer.RequiredStationTags)
		{
			if (RequiredTag.IsNone() || !SourceStation->MissionTags.Contains(RequiredTag))
			{
				return Fail(FString::Printf(TEXT("M12 mission offer '%s' requires missing station tag '%s'."), *Offer.OfferId.ToString(), *RequiredTag.ToString()));
			}
		}
		OfferIds.Add(Offer.OfferId);
	}
	TSet<FName> MissionIds;
	for (const FMissionInstanceState& Mission : State.MissionInstances)
	{
		if (!MissionDefinitionIds.Contains(Mission.MissionDefinitionId) || !OfferIds.Contains(Mission.OfferId) || !FactionIds.Contains(Mission.IssuerFactionId) || Mission.IdempotencyKey.IsNone())
		{
			return Fail(FString::Printf(TEXT("M12 mission '%s' has unresolved definition, offer, faction, or idempotency references."), *Mission.MissionInstanceId.ToString()));
		}
		MissionIds.Add(Mission.MissionInstanceId);
	}
	for (const FObjectiveState& Objective : State.ObjectiveStates)
	{
		if (!MissionIds.Contains(Objective.MissionInstanceId) || Objective.TargetId.IsNone() || Objective.State.IsNone())
		{
			return Fail(FString::Printf(TEXT("M12 objective '%s' has unresolved mission, target, or lifecycle state."), *Objective.ObjectiveStateId.ToString()));
		}
		for (const FName RouteId : Objective.RouteSegmentIds)
		{
			if (!RouteIds.Contains(RouteId))
			{
				return Fail(FString::Printf(TEXT("M12 objective '%s' references a missing route."), *Objective.ObjectiveStateId.ToString()));
			}
		}
		if (!Objective.RequiredCargoManifestId.IsNone() && !CargoManifestIds.Contains(Objective.RequiredCargoManifestId))
		{
			return Fail(FString::Printf(TEXT("M12 objective '%s' references a missing cargo manifest '%s'."),
				*Objective.ObjectiveStateId.ToString(),
				*Objective.RequiredCargoManifestId.ToString()));
		}
	}

	TSet<FName> ResourceIds;
	TSet<FName> ResourceIdempotencyKeys;
	for (const FShipResourceState& Resource : State.ShipResourceStates)
	{
		if (Resource.ResourceStateId.IsNone() || ResourceIds.Contains(Resource.ResourceStateId) ||
			Resource.ShipId.IsNone() || Resource.MaxFuel <= 0.0 || Resource.MaxAmmo < 0.0 ||
			Resource.Fuel < 0.0 || Resource.Fuel > Resource.MaxFuel ||
			Resource.Ammo < 0.0 || Resource.Ammo > Resource.MaxAmmo ||
			Resource.IdempotencyKey.IsNone() || ResourceIdempotencyKeys.Contains(Resource.IdempotencyKey))
		{
			return Fail(FString::Printf(TEXT("M12 ship resources '%s' must be bounded, stable, and idempotent."), *Resource.ResourceStateId.ToString()));
		}
		ResourceIds.Add(Resource.ResourceStateId);
		ResourceIdempotencyKeys.Add(Resource.IdempotencyKey);
	}

	TSet<FName> ServiceResultIds;
	TSet<FName> ServiceResultIdempotencyKeys;
	for (const FStationServiceResultRecord& Result : State.StationServiceResults)
	{
		if (Result.ResultId.IsNone() || ServiceResultIds.Contains(Result.ResultId) ||
			Result.RequestId.IsNone() || !ServiceEndpointIds.Contains(Result.ServiceEndpointId) ||
			Result.ServiceType.IsNone() || Result.IdempotencyKey.IsNone() ||
			ServiceResultIdempotencyKeys.Contains(Result.IdempotencyKey) ||
			(!Result.LedgerEntryId.IsNone() && !LedgerIds.Contains(Result.LedgerEntryId)))
		{
			return Fail(FString::Printf(TEXT("M12 service result '%s' has unresolved endpoint, ledger, or idempotency references."), *Result.ResultId.ToString()));
		}
		ServiceResultIds.Add(Result.ResultId);
		ServiceResultIdempotencyKeys.Add(Result.IdempotencyKey);
	}

	TSet<FName> ReputationDeltaIds;
	TSet<FName> ReputationIdempotencyKeys;
	for (const FReputationDeltaRecord& Delta : State.ReputationDeltas)
	{
		if (Delta.ReputationDeltaId.IsNone() || ReputationDeltaIds.Contains(Delta.ReputationDeltaId) ||
			Delta.SubjectId.IsNone() || !FactionIds.Contains(Delta.FactionId) ||
			Delta.IdempotencyKey.IsNone() || ReputationIdempotencyKeys.Contains(Delta.IdempotencyKey))
		{
			return Fail(FString::Printf(TEXT("M12 reputation delta '%s' has unresolved subject, faction, or idempotency references."), *Delta.ReputationDeltaId.ToString()));
		}
		ReputationDeltaIds.Add(Delta.ReputationDeltaId);
		ReputationIdempotencyKeys.Add(Delta.IdempotencyKey);
	}

	TSet<FName> OpportunityIds;
	TSet<FName> OpportunityIdempotencyKeys;
	for (const FFollowUpOpportunityRecord& Opportunity : State.FollowUpOpportunities)
	{
		if (Opportunity.OpportunityId.IsNone() || OpportunityIds.Contains(Opportunity.OpportunityId) ||
			Opportunity.OpportunityType.IsNone() || !RouteIds.Contains(Opportunity.RouteSegmentId) ||
			!ServiceEndpointIds.Contains(Opportunity.ServiceEndpointId) ||
			!OfferIds.Contains(Opportunity.MissionOfferId) || !FactionIds.Contains(Opportunity.FactionId) ||
			Opportunity.IdempotencyKey.IsNone() || OpportunityIdempotencyKeys.Contains(Opportunity.IdempotencyKey))
		{
			return Fail(FString::Printf(TEXT("M12 follow-up opportunity '%s' has unresolved route/service/offer/faction references."), *Opportunity.OpportunityId.ToString()));
		}
		OpportunityIds.Add(Opportunity.OpportunityId);
		OpportunityIdempotencyKeys.Add(Opportunity.IdempotencyKey);
	}

	TSet<FName> MessageInstanceIds;
	for (const FMessageLogEntry& Message : State.MessageLog)
	{
		MessageInstanceIds.Add(Message.MessageInstanceId);
	}
	TSet<FName> ArbitrationIds;
	TSet<FName> ArbitrationIdempotencyKeys;
	for (const FMessageArbitrationResultRecord& Arbitration : State.MessageArbitrationResults)
	{
		if (Arbitration.ArbitrationId.IsNone() || ArbitrationIds.Contains(Arbitration.ArbitrationId) ||
			Arbitration.IdempotencyKey.IsNone() || ArbitrationIdempotencyKeys.Contains(Arbitration.IdempotencyKey) ||
			!MessageInstanceIds.Contains(Arbitration.SelectedMessageInstanceId))
		{
			return Fail(FString::Printf(TEXT("M12 message arbitration '%s' has unresolved selected message or idempotency references."), *Arbitration.ArbitrationId.ToString()));
		}
		ArbitrationIds.Add(Arbitration.ArbitrationId);
		ArbitrationIdempotencyKeys.Add(Arbitration.IdempotencyKey);
	}

	TSet<FName> DebugIds;
	TSet<FName> DebugIdempotencyKeys;
	for (const FProgressionDebugLedgerEntry& Entry : State.ProgressionDebugLedger)
	{
		if (Entry.ProgressionEntryId.IsNone() || DebugIds.Contains(Entry.ProgressionEntryId) ||
			Entry.EntryType.IsNone() || Entry.SubjectId.IsNone() || Entry.IdempotencyKey.IsNone() ||
			DebugIdempotencyKeys.Contains(Entry.IdempotencyKey) ||
			(!Entry.SourceTransactionId.IsNone() && !TransactionIds.Contains(Entry.SourceTransactionId)) ||
			(!Entry.FactionId.IsNone() && !FactionIds.Contains(Entry.FactionId)) ||
			(!Entry.CreditLedgerEntryId.IsNone() && !LedgerIds.Contains(Entry.CreditLedgerEntryId)) ||
			(!Entry.ReputationDeltaId.IsNone() && !ReputationDeltaIds.Contains(Entry.ReputationDeltaId)) ||
			(!Entry.MissionInstanceId.IsNone() && !MissionIds.Contains(Entry.MissionInstanceId)) ||
			(!Entry.ServiceResultId.IsNone() && !ServiceResultIds.Contains(Entry.ServiceResultId)) ||
			(!Entry.FollowUpOpportunityId.IsNone() && !OpportunityIds.Contains(Entry.FollowUpOpportunityId)) ||
			(!Entry.MessageArbitrationId.IsNone() && !ArbitrationIds.Contains(Entry.MessageArbitrationId)))
		{
			return Fail(FString::Printf(TEXT("M12 progression trace entry '%s' has unresolved side-effect references."), *Entry.ProgressionEntryId.ToString()));
		}
		DebugIds.Add(Entry.ProgressionEntryId);
		DebugIdempotencyKeys.Add(Entry.IdempotencyKey);

		if (Entry.EntryType == FName(TEXT("mission_complete")))
		{
			const TSet<FName>* SideEffects = JournalSideEffectsByTransaction.Find(Entry.SourceTransactionId);
			if (!SideEffects || !SideEffects->Contains(FName(TEXT("ledger"))) || !SideEffects->Contains(FName(TEXT("escrow"))) ||
				!SideEffects->Contains(FName(TEXT("mission_state"))) || !SideEffects->Contains(FName(TEXT("reputation_delta"))) ||
				!SideEffects->Contains(FName(TEXT("generated_followup"))) || !SideEffects->Contains(FName(TEXT("message_arbitration"))) ||
				!SideEffects->Contains(FName(TEXT("debug"))))
			{
				return Fail(FString::Printf(TEXT("M12 mission completion '%s' must journal ledger, escrow, mission, reputation, follow-up, arbitration, and debug side effects."), *Entry.ProgressionEntryId.ToString()));
			}
		}
		else if (Entry.EntryType == FName(TEXT("mission_fail")))
		{
			const TSet<FName>* SideEffects = JournalSideEffectsByTransaction.Find(Entry.SourceTransactionId);
			if (!SideEffects || !SideEffects->Contains(FName(TEXT("mission_state"))) ||
				!SideEffects->Contains(FName(TEXT("reputation_delta"))) || !SideEffects->Contains(FName(TEXT("debug"))))
			{
				return Fail(FString::Printf(TEXT("M12 mission failure '%s' must journal mission, reputation, and debug side effects."), *Entry.ProgressionEntryId.ToString()));
			}
		}
		else if (Entry.EntryType == FName(TEXT("station_service")))
		{
			const TSet<FName>* SideEffects = JournalSideEffectsByTransaction.Find(Entry.SourceTransactionId);
			if (!SideEffects || !SideEffects->Contains(FName(TEXT("ledger"))) || !SideEffects->Contains(FName(TEXT("service_resource"))))
			{
				return Fail(FString::Printf(TEXT("M12 station service '%s' must journal ledger and service resource side effects."), *Entry.ProgressionEntryId.ToString()));
			}
		}
	}

	if (State.FollowUpOpportunities.IsEmpty() || State.ProgressionDebugLedger.IsEmpty())
	{
		return Fail(TEXT("M12 requires authored/generated follow-up opportunity records and debug progression ledger records."));
	}

	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::SelectRealizedPromotionCandidates(
	const FStarSystemDefinition& SystemDefinition,
	const FSystemicGameplayState& State,
	FName ActorBudgetProfileId,
	const FMovingFrameTarget& ObserverTarget,
	const FSimulationClockSnapshot& ClockSnapshot,
	double SimulationTimeSeconds,
	TArray<FRealizedActorMappingRecord>& OutPromotions,
	TArray<FName>& OutBlockedShipIds,
	FString& OutFailureReason)
{
	OutPromotions.Reset();
	OutBlockedShipIds.Reset();

	const FRealizedActorBudgetProfile* Budget = FindConst(State.RealizedActorBudgetProfiles, [ActorBudgetProfileId](const FRealizedActorBudgetProfile& Candidate)
	{
		return Candidate.BudgetProfileId == ActorBudgetProfileId;
	});
	if (!Budget || Budget->MaxRealizedActors <= 0)
	{
		OutFailureReason = TEXT("Actor budget profile does not resolve.");
		return false;
	}
	if (ObserverTarget.TargetType != FName(TEXT("route_sample")) || ObserverTarget.RouteSegmentId.IsNone())
	{
		OutFailureReason = TEXT("Promotion selection requires a moving observer route target.");
		return false;
	}

	FRouteSample ObserverSample;
	if (!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, ObserverTarget.RouteSegmentId, ObserverTarget.RouteProgress01, ClockSnapshot, SimulationTimeSeconds, ObserverSample))
	{
		OutFailureReason = TEXT("Promotion observer route target does not resolve.");
		return false;
	}

	TArray<FRealizedActorMappingRecord> Candidates;
	for (const FRealizedActorMappingRecord& Mapping : State.RealizedActorMappings)
	{
		if (Mapping.ActorBudgetProfileId != ActorBudgetProfileId || Mapping.State != FName(TEXT("eligible")))
		{
			continue;
		}
		const FShipTrafficInstance* Ship = FindConst(SystemDefinition.LogicalTraffic, [&Mapping](const FShipTrafficInstance& Candidate)
		{
			return Candidate.ShipInstanceId == Mapping.ShipInstanceId && Candidate.TrafficTier == ELogicalTrafficTier::Tier2Logical && Candidate.RealizationToken.IsNone();
		});
		if (!Ship || Ship->CurrentGoal.RouteSegmentId.IsNone())
		{
			continue;
		}
		FRouteSample CandidateSample;
		if (!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, Ship->CurrentGoal.RouteSegmentId, Ship->CurrentGoal.RouteProgress01, ClockSnapshot, SimulationTimeSeconds, CandidateSample))
		{
			continue;
		}
		const double DistanceCm = FVector::Distance(CandidateSample.ResolvedTransform.PositionCm, ObserverSample.ResolvedTransform.PositionCm);
		if (Budget->PromotionRadiusCm > 0.0 && DistanceCm > Budget->PromotionRadiusCm)
		{
			OutBlockedShipIds.Add(Mapping.ShipInstanceId);
			continue;
		}
		FRealizedActorMappingRecord Candidate = Mapping;
		Candidate.LastObserverDistanceCm = DistanceCm;
		Candidates.Add(Candidate);
	}
	Candidates.Sort([](const FRealizedActorMappingRecord& Left, const FRealizedActorMappingRecord& Right)
	{
		if (Left.PromotionPriority == Right.PromotionPriority)
		{
			return Left.MappingId.LexicalLess(Right.MappingId);
		}
		return Left.PromotionPriority > Right.PromotionPriority;
	});

	const int32 PromotionLimit = FMath::Max(0, FMath::Min(Budget->MaxRealizedActors, Budget->MaxPromotionsPerTick));
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		if (Index < PromotionLimit)
		{
			OutPromotions.Add(Candidates[Index]);
		}
		else
		{
			OutBlockedShipIds.AddUnique(Candidates[Index].ShipInstanceId);
		}
	}

	OutFailureReason = OutBlockedShipIds.IsEmpty()
		? TEXT("M11 promotions fit actor budget.")
		: TEXT("M11 actor budget blocked lower-priority promotions.");
	return true;
}

bool USystemicGameplayQueryService::ApplyDamageEventOnce(
	FSystemicGameplayState& InOutState,
	const FDamageEventRecord& DamageEvent,
	FDamageEventRecord& OutResult,
	FString& OutFailureReason)
{
	OutResult = DamageEvent;
	if (DamageEvent.DamageEventId.IsNone() || DamageEvent.IdempotencyKey.IsNone() || DamageEvent.SourceCombatantId.IsNone() ||
		DamageEvent.TargetCombatantId.IsNone() || DamageEvent.DamageType.IsNone() || DamageEvent.Amount <= 0.0)
	{
		OutFailureReason = TEXT("Damage event requires stable IDs, damage type, idempotency key, source, target, and positive amount.");
		OutResult.ResultState = TEXT("rejected");
		return false;
	}

	if (const FDamageEventRecord* Existing = FindConst(InOutState.DamageEvents, [&DamageEvent](const FDamageEventRecord& Candidate)
	{
		return Candidate.DamageEventId == DamageEvent.DamageEventId || Candidate.IdempotencyKey == DamageEvent.IdempotencyKey;
	}))
	{
		OutResult = *Existing;
		OutFailureReason.Reset();
		return Existing->ResultState == FName(TEXT("applied"));
	}

	const FThreatRecord* Threat = FindConst(InOutState.ThreatRecords, [&DamageEvent](const FThreatRecord& Candidate)
	{
		return Candidate.ThreatId == DamageEvent.ThreatId &&
			Candidate.AttackerId == DamageEvent.SourceCombatantId &&
			Candidate.DefenderId == DamageEvent.TargetCombatantId;
	});
	if (!Threat)
	{
		OutFailureReason = TEXT("Damage event must resolve a matching threat record.");
		OutResult.ResultState = TEXT("rejected");
		return false;
	}

	FShipDurabilityState* TargetDurability = FindMutable(InOutState.ShipDurabilityStates, [&DamageEvent](const FShipDurabilityState& Candidate)
	{
		return Candidate.CombatantId == DamageEvent.TargetCombatantId;
	});
	if (!TargetDurability)
	{
		OutFailureReason = TEXT("Damage target durability state does not resolve.");
		OutResult.ResultState = TEXT("rejected");
		return false;
	}

	if (DamageEvent.DamageType == FName(TEXT("surrender")))
	{
		if (!TargetDurability->bCanSurrender)
		{
			OutFailureReason = TEXT("Target cannot surrender under the current durability policy.");
			OutResult.ResultState = TEXT("rejected");
			return false;
		}
		TargetDurability->State = TEXT("surrendered");
	}
	else if (DamageEvent.DamageType == FName(TEXT("escape")))
	{
		if (!TargetDurability->bCanEscape)
		{
			OutFailureReason = TEXT("Target cannot escape under the current durability policy.");
			OutResult.ResultState = TEXT("rejected");
			return false;
		}
		TargetDurability->State = TEXT("escaped");
	}
	else
	{
		double RemainingDamage = DamageEvent.Amount;
		const double ShieldDamage = FMath::Min(TargetDurability->Shield, RemainingDamage);
		TargetDurability->Shield -= ShieldDamage;
		RemainingDamage -= ShieldDamage;
		TargetDurability->Hull = FMath::Max(0.0, TargetDurability->Hull - RemainingDamage);
		if (TargetDurability->Hull <= 0.0)
		{
			TargetDurability->State = TEXT("destroyed");
		}
		else if (TargetDurability->Hull <= ResolvePositiveCapacity(TargetDurability->MaxHull, 100.0) * 0.15)
		{
			TargetDurability->State = TEXT("disabled");
		}
		else
		{
			TargetDurability->State = TEXT("damaged");
		}
	}

	TargetDurability->LastDamageEventId = DamageEvent.DamageEventId;
	OutResult.ResultState = TEXT("applied");
	InOutState.DamageEvents.Add(OutResult);
	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ApplySimulationEventOnce(FSystemicGameplayState& InOutState, const FSimulationEventRecord& Event, double AppliedTimeSeconds, FSimulationEventResultRecord& OutResult)
{
	if (Event.EventId.IsNone() || Event.IdempotencyKey.IsNone())
	{
		OutResult = FSimulationEventResultRecord();
		OutResult.EventId = Event.EventId;
		OutResult.OutcomeType = TEXT("rejected");
		OutResult.DebugReason = TEXT("EventId and IdempotencyKey are required.");
		return false;
	}

	if (const FSimulationEventResultRecord* Existing = FindConst(InOutState.EventResults, [&Event](const FSimulationEventResultRecord& Candidate)
	{
		return Candidate.EventId == Event.EventId || Candidate.IdempotencyKey == Event.IdempotencyKey;
	}))
	{
		OutResult = *Existing;
		OutResult.OutcomeType = TEXT("duplicate");
		return true;
	}

	if (!FindConst(InOutState.Events, [&Event](const FSimulationEventRecord& Candidate)
	{
		return Candidate.EventId == Event.EventId;
	}))
	{
		InOutState.Events.Add(Event);
	}
	OutResult.EventId = Event.EventId;
	OutResult.ResultId = MakeId(TEXT("result"), Event.EventId);
	OutResult.AppliedTimeSeconds = AppliedTimeSeconds;
	OutResult.OutcomeType = TEXT("accepted");
	OutResult.DebugReason = TEXT("Canonical event accepted once.");
	OutResult.IdempotencyKey = Event.IdempotencyKey;
	InOutState.EventResults.Add(OutResult);
	return true;
}

bool USystemicGameplayQueryService::RecordOffense(FSystemicGameplayState& InOutState, const FOffenseEvent& Offense, FEvidenceRecord& OutEvidence, FCriminalRecord& OutCriminalRecord, FString& OutFailureReason)
{
	const FJurisdictionDefinition* Jurisdiction = FindConst(InOutState.Jurisdictions, [&Offense](const FJurisdictionDefinition& Candidate)
	{
		return Candidate.JurisdictionId == Offense.JurisdictionId;
	});
	if (!Jurisdiction)
	{
		OutFailureReason = TEXT("Offense jurisdiction does not resolve.");
		return false;
	}
	if (!FindConst(InOutState.Factions, [Jurisdiction](const FFactionDefinition& Candidate)
	{
		return Candidate.FactionId == Jurisdiction->AuthorityFactionId;
	}))
	{
		OutFailureReason = TEXT("Offense authority faction does not resolve.");
		return false;
	}

	if (!FindConst(InOutState.Offenses, [&Offense](const FOffenseEvent& Candidate) { return Candidate.OffenseId == Offense.OffenseId; }))
	{
		InOutState.Offenses.Add(Offense);
	}

	OutEvidence.EvidenceId = MakeId(TEXT("evidence"), Offense.OffenseId);
	OutEvidence.OffenseId = Offense.OffenseId;
	OutEvidence.WitnessType = TEXT("systemic_sensor");
	OutEvidence.WitnessId = Offense.JurisdictionId;
	OutEvidence.SourceEventId = Offense.SourceEventId;
	OutEvidence.EvidenceState = TEXT("reported");
	if (!FindConst(InOutState.EvidenceRecords, [&OutEvidence](const FEvidenceRecord& Candidate) { return Candidate.EvidenceId == OutEvidence.EvidenceId; }))
	{
		InOutState.EvidenceRecords.Add(OutEvidence);
	}

	const FName CriminalRecordId = FName(*FString::Printf(TEXT("criminal_%s_%s"), *Offense.JurisdictionId.ToString(), *Offense.OffenderId.ToString()));
	FCriminalRecord* CriminalRecord = FindMutable(InOutState.CriminalRecords, [CriminalRecordId](const FCriminalRecord& Candidate)
	{
		return Candidate.CriminalRecordId == CriminalRecordId;
	});
	if (!CriminalRecord)
	{
		FCriminalRecord NewRecord;
		NewRecord.CriminalRecordId = CriminalRecordId;
		NewRecord.SubjectType = Offense.OffenderType;
		NewRecord.SubjectId = Offense.OffenderId;
		NewRecord.JurisdictionId = Offense.JurisdictionId;
		InOutState.CriminalRecords.Add(NewRecord);
		CriminalRecord = &InOutState.CriminalRecords.Last();
	}
	if (!CriminalRecord->OffenseIds.Contains(Offense.OffenseId))
	{
		CriminalRecord->OffenseIds.Add(Offense.OffenseId);
		CriminalRecord->WantedLevel += 1;
	}
	OutCriminalRecord = *CriminalRecord;
	return true;
}

bool USystemicGameplayQueryService::TransferCargo(FSystemicGameplayState& InOutState, const FCargoTransferRequest& Request, FCargoTransferResult& OutResult)
{
	OutResult = FCargoTransferResult();
	OutResult.TransferId = MakeId(TEXT("transfer"), Request.RequestId);
	OutResult.RequestId = Request.RequestId;
	OutResult.SourceEventId = Request.SourceEventId;
	OutResult.IdempotencyKey = Request.IdempotencyKey;

	if (Request.RequestId.IsNone() || Request.IdempotencyKey.IsNone() || Request.Quantity <= 0)
	{
		OutResult.RejectedReason = TEXT("RequestId, IdempotencyKey, and positive quantity are required.");
		return false;
	}
	if (const FCargoTransferResult* Existing = FindConst(InOutState.CargoTransferResults, [&Request](const FCargoTransferResult& Candidate)
	{
		return Candidate.RequestId == Request.RequestId || Candidate.IdempotencyKey == Request.IdempotencyKey;
	}))
	{
		OutResult = *Existing;
		return Existing->Result == ESystemicActionResult::Accepted;
	}
	if (Request.SourceContainerId == Request.DestinationContainerId)
	{
		OutResult.RejectedReason = TEXT("Source and destination containers must be distinct.");
		return false;
	}

	FContainerState* Source = FindMutable(InOutState.Containers, [&Request](const FContainerState& Candidate) { return Candidate.ContainerId == Request.SourceContainerId; });
	FContainerState* Destination = FindMutable(InOutState.Containers, [&Request](const FContainerState& Candidate) { return Candidate.ContainerId == Request.DestinationContainerId; });
	if (!Source || !Destination)
	{
		OutResult.RejectedReason = TEXT("Source or destination container does not resolve.");
		return false;
	}
	if (Source->OwnerId.IsNone() || Destination->OwnerId.IsNone())
	{
		OutResult.RejectedReason = TEXT("Source and destination containers must have durable owner IDs.");
		return false;
	}
	if ((!Request.ExpectedSourceOwnerId.IsNone() && Source->OwnerId != Request.ExpectedSourceOwnerId) ||
		(!Request.ExpectedDestinationOwnerId.IsNone() && Destination->OwnerId != Request.ExpectedDestinationOwnerId))
	{
		OutResult.RejectedReason = TEXT("Cargo transfer owner validation failed.");
		return false;
	}

	FItemStackState* SourceStack = nullptr;
	if (!Request.StackIds.IsEmpty())
	{
		const FName RequestedStackId = Request.StackIds[0];
		SourceStack = Source->Stacks.FindByPredicate([RequestedStackId](const FItemStackState& Candidate) { return Candidate.StackId == RequestedStackId; });
	}
	else if (!Request.ItemId.IsNone())
	{
		SourceStack = Source->Stacks.FindByPredicate([&Request](const FItemStackState& Candidate) { return Candidate.ItemId == Request.ItemId; });
	}
	if (!SourceStack || SourceStack->Quantity < Request.Quantity)
	{
		OutResult.RejectedReason = TEXT("Requested cargo stack or quantity is unavailable.");
		return false;
	}
	const FItemDefinition* SourceItem = FindConst(InOutState.Items, [SourceStack](const FItemDefinition& Candidate)
	{
		return Candidate.ItemId == SourceStack->ItemId;
	});
	if (!SourceItem)
	{
		OutResult.RejectedReason = TEXT("Requested cargo item does not resolve.");
		return false;
	}
	if (SourceItem->StackLimit <= 0)
	{
		OutResult.RejectedReason = TEXT("Requested cargo item has an invalid stack limit.");
		return false;
	}
	if (!Request.LegalContextId.IsNone() && !FindConst(InOutState.Jurisdictions, [&Request](const FJurisdictionDefinition& Candidate)
	{
		return Candidate.LawProfileId == Request.LegalContextId;
	}))
	{
		OutResult.RejectedReason = TEXT("Cargo transfer legal context does not resolve.");
		return false;
	}
	if (HasNamedGameplayTag(SourceStack->Flags, TEXT("mission_locked")) && Request.Reason != FName(TEXT("mission")))
	{
		OutResult.RejectedReason = TEXT("Mission-locked cargo cannot be transferred outside a mission transaction.");
		return false;
	}
	if ((HasNamedGameplayTag(SourceStack->Flags, TEXT("stolen")) ||
		HasNamedGameplayTag(SourceStack->Flags, TEXT("contraband")) ||
		!SourceItem->LegalityTags.IsEmpty()) &&
		Request.LegalContextId.IsNone())
	{
		OutResult.RejectedReason = TEXT("Restricted cargo requires an explicit legal context.");
		return false;
	}

	const double AddedMass = GetItemMass(InOutState, SourceStack->ItemId) * Request.Quantity;
	const double AddedVolume = GetItemVolume(InOutState, SourceStack->ItemId) * Request.Quantity;
	double CurrentMass = 0.0;
	double CurrentVolume = 0.0;
	GetContainerLoad(InOutState, *Destination, CurrentMass, CurrentVolume);
	if (CurrentMass + AddedMass > Destination->CapacityMassKg || CurrentVolume + AddedVolume > Destination->CapacityVolumeM3)
	{
		OutResult.RejectedReason = TEXT("Destination container capacity would be exceeded.");
		return false;
	}
	const int32 NeededNewStacks = FMath::DivideAndRoundUp(Request.Quantity, SourceItem->StackLimit);
	if (Destination->MaxSlotCount > 0 && Destination->Stacks.Num() + NeededNewStacks > Destination->MaxSlotCount)
	{
		OutResult.RejectedReason = TEXT("Destination container slot count would be exceeded.");
		return false;
	}

	const FName SourceStackId = SourceStack->StackId;
	const FName SourceItemId = SourceStack->ItemId;
	const FGameplayTagContainer SourceFlags = SourceStack->Flags;
	const FName SourceOwnerFactionId = SourceStack->OwnerFactionId;
	SourceStack->Quantity -= Request.Quantity;
	if (SourceStack->Quantity <= 0)
	{
		Source->Stacks.RemoveAll([SourceStackId](const FItemStackState& Candidate)
		{
			return Candidate.StackId == SourceStackId;
		});
	}

	int32 RemainingQuantity = Request.Quantity;
	while (RemainingQuantity > 0)
	{
		FItemStackState NewStack;
		NewStack.StackId = MakeUniqueStackId(*Destination, FName(*FString::Printf(TEXT("%s_%s"), *Destination->ContainerId.ToString(), *SourceItemId.ToString())));
		NewStack.ItemId = SourceItemId;
		NewStack.Quantity = FMath::Min(SourceItem->StackLimit, RemainingQuantity);
		NewStack.Flags = SourceFlags;
		NewStack.OwnerFactionId = SourceOwnerFactionId;
		Destination->Stacks.Add(NewStack);
		OutResult.CreatedStackIds.Add(NewStack.StackId);
		RemainingQuantity -= NewStack.Quantity;
	}

	OutResult.Result = ESystemicActionResult::Accepted;
	OutResult.MovedStackIds.Add(SourceStackId);
	InOutState.CargoTransferResults.Add(OutResult);
	return true;
}

bool USystemicGameplayQueryService::AddItemToContainerStacking(
	FSystemicGameplayState& InOutState,
	FName ContainerId,
	FName ItemId,
	int32 Quantity,
	FName StackIdPrefix,
	FString& OutFailureReason)
{
	if (ContainerId.IsNone() || ItemId.IsNone() || Quantity <= 0 || StackIdPrefix.IsNone())
	{
		OutFailureReason = TEXT("Inventory add requires container, item, positive quantity, and stack ID prefix.");
		return false;
	}

	FContainerState* Container = FindMutable(InOutState.Containers, [ContainerId](const FContainerState& Candidate)
	{
		return Candidate.ContainerId == ContainerId;
	});
	const FItemDefinition* Item = FindConst(InOutState.Items, [ItemId](const FItemDefinition& Candidate)
	{
		return Candidate.ItemId == ItemId;
	});
	if (!Container || !Item || Item->StackLimit <= 0)
	{
		OutFailureReason = TEXT("Inventory add container or item does not resolve.");
		return false;
	}

	double CurrentMass = 0.0;
	double CurrentVolume = 0.0;
	GetContainerLoad(InOutState, *Container, CurrentMass, CurrentVolume);
	if (CurrentMass + Item->MassPerUnitKg * Quantity > Container->CapacityMassKg ||
		CurrentVolume + Item->VolumePerUnitM3 * Quantity > Container->CapacityVolumeM3)
	{
		OutFailureReason = TEXT("Inventory add would exceed container capacity.");
		return false;
	}

	int32 Remaining = Quantity;
	for (FItemStackState& Stack : Container->Stacks)
	{
		if (Stack.ItemId != ItemId || Stack.Quantity >= Item->StackLimit)
		{
			continue;
		}

		const int32 ToAdd = FMath::Min(Item->StackLimit - Stack.Quantity, Remaining);
		Stack.Quantity += ToAdd;
		Remaining -= ToAdd;
		if (Remaining <= 0)
		{
			OutFailureReason.Reset();
			return true;
		}
	}

	const int32 NeededNewStacks = FMath::DivideAndRoundUp(Remaining, Item->StackLimit);
	if (Container->MaxSlotCount > 0 && Container->Stacks.Num() + NeededNewStacks > Container->MaxSlotCount)
	{
		OutFailureReason = TEXT("Inventory add would exceed container slot count.");
		return false;
	}

	while (Remaining > 0)
	{
		FItemStackState NewStack;
		NewStack.StackId = MakeUniqueStackId(*Container, StackIdPrefix);
		NewStack.ItemId = ItemId;
		NewStack.Quantity = FMath::Min(Item->StackLimit, Remaining);
		Container->Stacks.Add(NewStack);
		Remaining -= NewStack.Quantity;
	}

	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::EquipItemFromContainer(
	FSystemicGameplayState& InOutState,
	FName ContainerId,
	FName StackId,
	FName SlotId,
	FString& OutFailureReason)
{
	if (ContainerId.IsNone() || StackId.IsNone() || SlotId.IsNone())
	{
		OutFailureReason = TEXT("Equip requires container, stack, and slot IDs.");
		return false;
	}

	FContainerState* Container = FindMutable(InOutState.Containers, [ContainerId](const FContainerState& Candidate)
	{
		return Candidate.ContainerId == ContainerId;
	});
	FEquipmentSlotState* Slot = FindMutable(InOutState.EquipmentSlots, [SlotId](const FEquipmentSlotState& Candidate)
	{
		return Candidate.SlotId == SlotId;
	});
	if (!Container || !Slot)
	{
		OutFailureReason = TEXT("Equip container or slot does not resolve.");
		return false;
	}

	const int32 StackIndex = Container->Stacks.IndexOfByPredicate([StackId](const FItemStackState& Candidate)
	{
		return Candidate.StackId == StackId;
	});
	if (StackIndex == INDEX_NONE || Container->Stacks[StackIndex].Quantity <= 0)
	{
		OutFailureReason = TEXT("Equip stack does not resolve.");
		return false;
	}

	const FItemStackState SourceStack = Container->Stacks[StackIndex];
	const FItemDefinition* Item = FindConst(InOutState.Items, [&SourceStack](const FItemDefinition& Candidate)
	{
		return Candidate.ItemId == SourceStack.ItemId;
	});
	if (!Item || !Slot->AcceptedItemTypes.Contains(Item->ItemType))
	{
		OutFailureReason = TEXT("Item cannot be equipped in the requested slot.");
		return false;
	}

	const FItemStackState PreviousEquippedStack = Slot->EquippedStack;
	if (!PreviousEquippedStack.ItemId.IsNone() && PreviousEquippedStack.Quantity > 0)
	{
		FSystemicGameplayState CapacityCheckState = InOutState;
		FContainerState* CapacityCheckContainer = FindMutable(CapacityCheckState.Containers, [ContainerId](const FContainerState& Candidate)
		{
			return Candidate.ContainerId == ContainerId;
		});
		if (!CapacityCheckContainer)
		{
			OutFailureReason = TEXT("Equip capacity check container does not resolve.");
			return false;
		}
		const int32 CapacityCheckStackIndex = CapacityCheckContainer->Stacks.IndexOfByPredicate([StackId](const FItemStackState& Candidate)
		{
			return Candidate.StackId == StackId;
		});
		if (CapacityCheckStackIndex == INDEX_NONE)
		{
			OutFailureReason = TEXT("Equip capacity check stack does not resolve.");
			return false;
		}
		CapacityCheckContainer->Stacks[CapacityCheckStackIndex].Quantity -= 1;
		if (CapacityCheckContainer->Stacks[CapacityCheckStackIndex].Quantity <= 0)
		{
			CapacityCheckContainer->Stacks.RemoveAt(CapacityCheckStackIndex);
		}
		FString CapacityFailureReason;
		if (!AddItemToContainerStacking(
			CapacityCheckState,
			ContainerId,
			PreviousEquippedStack.ItemId,
			PreviousEquippedStack.Quantity,
			TEXT("returned_equipped"),
			CapacityFailureReason))
		{
			OutFailureReason = FString::Printf(TEXT("Equipping would fail to return previous equipment: %s"), *CapacityFailureReason);
			return false;
		}
	}

	Slot->EquippedStack.StackId = FName(*FString::Printf(TEXT("equipped_%s"), *SlotId.ToString()));
	Slot->EquippedStack.ItemId = SourceStack.ItemId;
	Slot->EquippedStack.Quantity = 1;
	Container->Stacks[StackIndex].Quantity -= 1;
	if (Container->Stacks[StackIndex].Quantity <= 0)
	{
		Container->Stacks.RemoveAt(StackIndex);
	}
	if (!PreviousEquippedStack.ItemId.IsNone() && PreviousEquippedStack.Quantity > 0)
	{
		if (!AddItemToContainerStacking(
			InOutState,
			ContainerId,
			PreviousEquippedStack.ItemId,
			PreviousEquippedStack.Quantity,
			TEXT("returned_equipped"),
			OutFailureReason))
		{
			return false;
		}
	}

	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::UnequipItemToContainer(
	FSystemicGameplayState& InOutState,
	FName SlotId,
	FName ContainerId,
	FName StackIdPrefix,
	FString& OutFailureReason)
{
	if (SlotId.IsNone() || ContainerId.IsNone() || StackIdPrefix.IsNone())
	{
		OutFailureReason = TEXT("Unequip requires slot, container, and stack ID prefix.");
		return false;
	}

	FEquipmentSlotState* Slot = FindMutable(InOutState.EquipmentSlots, [SlotId](const FEquipmentSlotState& Candidate)
	{
		return Candidate.SlotId == SlotId;
	});
	if (!Slot)
	{
		OutFailureReason = TEXT("Unequip slot does not resolve.");
		return false;
	}
	if (Slot->EquippedStack.ItemId.IsNone() || Slot->EquippedStack.Quantity <= 0)
	{
		OutFailureReason = TEXT("Unequip slot is already empty.");
		return false;
	}

	FSystemicGameplayState CapacityCheckState = InOutState;
	FString CapacityFailureReason;
	if (!AddItemToContainerStacking(
		CapacityCheckState,
		ContainerId,
		Slot->EquippedStack.ItemId,
		Slot->EquippedStack.Quantity,
		StackIdPrefix,
		CapacityFailureReason))
	{
		OutFailureReason = FString::Printf(TEXT("Unequip would fail to store equipment: %s"), *CapacityFailureReason);
		return false;
	}

	const FItemStackState EquippedStack = Slot->EquippedStack;
	if (!AddItemToContainerStacking(
		InOutState,
		ContainerId,
		EquippedStack.ItemId,
		EquippedStack.Quantity,
		StackIdPrefix,
		OutFailureReason))
	{
		return false;
	}

	Slot->EquippedStack = FItemStackState();
	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ExecuteMarketTransaction(FSystemicGameplayState& InOutState, const FMarketTransactionRequest& Request, FMarketTransactionResult& OutResult)
{
	OutResult = FMarketTransactionResult();
	OutResult.TransactionId = Request.TransactionId;
	OutResult.SourceEventId = Request.SourceEventId;
	OutResult.IdempotencyKey = Request.IdempotencyKey;
	if (Request.TransactionId.IsNone() || Request.IdempotencyKey.IsNone() || Request.Quantity <= 0 || Request.QuotedUnitPrice <= 0)
	{
		OutResult.RejectionReason = TEXT("TransactionId, IdempotencyKey, positive quantity, and positive quoted unit price are required.");
		return false;
	}
	if (const FMarketTransactionResult* Existing = FindConst(InOutState.MarketTransactionResults, [&Request](const FMarketTransactionResult& Candidate)
	{
		return Candidate.TransactionId == Request.TransactionId || Candidate.IdempotencyKey == Request.IdempotencyKey;
	}))
	{
		OutResult = *Existing;
		return Existing->Result == ESystemicActionResult::Accepted;
	}

	FStationMarketState* Market = FindMutable(InOutState.Markets, [&Request](const FStationMarketState& Candidate) { return Candidate.MarketId == Request.MarketId; });
	const FCommodityItemBridge* Bridge = FindConst(InOutState.CommodityItemBridges, [&Request](const FCommodityItemBridge& Candidate)
	{
		return Candidate.CommodityItemBridgeId == Request.CommodityItemBridgeId && Candidate.CommodityId == Request.CommodityId;
	});
	FCreditAccountRecord* Debit = FindMutable(InOutState.CreditAccounts, [&Request](const FCreditAccountRecord& Candidate) { return Candidate.AccountId == Request.DebitAccountId; });
	FCreditAccountRecord* Credit = FindMutable(InOutState.CreditAccounts, [&Request](const FCreditAccountRecord& Candidate) { return Candidate.AccountId == Request.CreditAccountId; });
	if (!Market || !Bridge || !Debit || !Credit)
	{
		OutResult.RejectionReason = TEXT("Market, commodity bridge, debit account, or credit account does not resolve.");
		return false;
	}

	const bool bPlayerSellingToMarket = Request.SellerId == FName(TEXT("player"));
	const int32* Stock = Market->StockByCommodity.Find(Request.CommodityId);
	if (!bPlayerSellingToMarket && (!Stock || *Stock < Request.Quantity))
	{
		OutResult.RejectionReason = TEXT("Market stock is unavailable.");
		return false;
	}
	const int64 TotalPrice = Request.QuotedUnitPrice * Request.Quantity;
	if (Debit->AvailableBalance < TotalPrice)
	{
		OutResult.RejectionReason = TEXT("Debit account has insufficient credits.");
		return false;
	}

	FCargoTransferRequest CargoRequest;
	CargoRequest.RequestId = MakeId(TEXT("cargo"), Request.TransactionId);
	CargoRequest.SourceContainerId = Request.SourceContainerId;
	CargoRequest.DestinationContainerId = Request.DestinationContainerId;
	CargoRequest.ExpectedSourceOwnerId = Request.SellerId;
	CargoRequest.ExpectedDestinationOwnerId = Request.BuyerId;
	CargoRequest.ItemId = Bridge->ItemId;
	CargoRequest.Quantity = Request.Quantity;
	CargoRequest.Reason = TEXT("trade");
	CargoRequest.SourceEventId = Request.SourceEventId;
	CargoRequest.LegalContextId = Request.LegalContextId;
	CargoRequest.IdempotencyKey = MakeId(TEXT("cargo"), Request.IdempotencyKey);
	FCargoTransferResult CargoResult;
	if (!TransferCargo(InOutState, CargoRequest, CargoResult))
	{
		OutResult.RejectionReason = CargoResult.RejectedReason;
		return false;
	}

	int32& MutableStock = Market->StockByCommodity.FindOrAdd(Request.CommodityId);
	MutableStock += bPlayerSellingToMarket ? Request.Quantity : -Request.Quantity;
	Debit->AvailableBalance -= TotalPrice;
	Credit->AvailableBalance += TotalPrice;

	FCreditLedgerEntry Ledger;
	Ledger.LedgerEntryId = MakeId(TEXT("ledger"), Request.TransactionId);
	Ledger.DebitAccountId = Debit->AccountId;
	Ledger.CreditAccountId = Credit->AccountId;
	Ledger.Amount = TotalPrice;
	Ledger.Reason = bPlayerSellingToMarket ? FName(TEXT("market_sell")) : FName(TEXT("market_buy"));
	Ledger.SourceEventId = Request.SourceEventId;
	Ledger.SourceTransactionId = Request.TransactionId;
	Ledger.IdempotencyKey = MakeId(TEXT("ledger"), Request.IdempotencyKey);
	Ledger.State = TEXT("applied");
	InOutState.CreditLedger.Add(Ledger);
	Debit->LastLedgerEntryId = Ledger.LedgerEntryId;
	Credit->LastLedgerEntryId = Ledger.LedgerEntryId;

	FGameplayTransactionRecord Transaction;
	Transaction.TransactionId = Request.TransactionId;
	Transaction.TransactionType = TEXT("market_trade");
	Transaction.SourceEventId = Request.SourceEventId;
	Transaction.InitiatorType = TEXT("player");
	Transaction.InitiatorId = bPlayerSellingToMarket ? Request.SellerId : Request.BuyerId;
	Transaction.TargetType = TEXT("market");
	Transaction.TargetId = Request.MarketId;
	Transaction.State = TEXT("committed");
	Transaction.IdempotencyKey = Request.IdempotencyKey;
	Transaction.DebugReason = TEXT("Market stock, cargo, and ledger committed under one transaction.");
	InOutState.Transactions.Add(Transaction);
	AddJournalEntry(InOutState, Request.TransactionId, 1, TEXT("cargo_transfer"), CargoResult.TransferId, Request.IdempotencyKey);
	AddJournalEntry(InOutState, Request.TransactionId, 2, TEXT("ledger"), Ledger.LedgerEntryId, Request.IdempotencyKey);
	AddJournalEntry(InOutState, Request.TransactionId, 3, TEXT("market_stock"), Request.MarketId, Request.IdempotencyKey);

	OutResult.Result = ESystemicActionResult::Accepted;
	OutResult.AppliedQuantity = Request.Quantity;
	OutResult.UnitPrice = Request.QuotedUnitPrice;
	OutResult.StockDelta = bPlayerSellingToMarket ? Request.Quantity : -Request.Quantity;
	OutResult.DisplayCreditDelta = bPlayerSellingToMarket ? TotalPrice : -TotalPrice;
	OutResult.LedgerEntryIds.Add(Ledger.LedgerEntryId);
	OutResult.CargoTransferResultId = CargoResult.TransferId;
	InOutState.MarketTransactionResults.Add(OutResult);
	Market->LastTransactionIds.Add(Request.TransactionId);
	return true;
}

bool USystemicGameplayQueryService::ExecuteProgressionMarketTransactionOnce(
	FSystemicGameplayState& InOutState,
	const FMarketTransactionRequest& Request,
	FMarketTransactionResult& OutResult,
	FString& OutFailureReason)
{
	OutResult = FMarketTransactionResult();
	OutResult.TransactionId = Request.TransactionId;
	OutResult.SourceEventId = Request.SourceEventId;
	OutResult.IdempotencyKey = Request.IdempotencyKey;
	if (const FMarketTransactionResult* Existing = FindConst(InOutState.MarketTransactionResults, [&Request](const FMarketTransactionResult& Candidate)
	{
		return Candidate.TransactionId == Request.TransactionId || Candidate.IdempotencyKey == Request.IdempotencyKey;
	}))
	{
		OutResult = *Existing;
		OutFailureReason = Existing->RejectionReason;
		return Existing->Result == ESystemicActionResult::Accepted;
	}

	if (IsRestrictedCargo(InOutState, Request.CommodityId, Request.CommodityItemBridgeId) &&
		!HasRestrictedCargoClearance(InOutState, Request))
	{
		OutResult.Result = ESystemicActionResult::Rejected;
		OutResult.RejectionReason = TEXT("Restricted cargo requires station legal/access clearance before market mutation.");
		OutFailureReason = OutResult.RejectionReason;
		InOutState.MarketTransactionResults.Add(OutResult);
		return false;
	}

	const bool bApplied = ExecuteMarketTransaction(InOutState, Request, OutResult);
	if (!bApplied)
	{
		OutFailureReason = OutResult.RejectionReason;
		return false;
	}

	FProgressionDebugLedgerEntry Entry;
	Entry.ProgressionEntryId = MakeId(TEXT("progression_market"), Request.TransactionId);
	Entry.EntryType = TEXT("market_transaction");
	Entry.SubjectId = Request.BuyerId;
	Entry.SourceEventId = Request.SourceEventId;
	Entry.SourceTransactionId = Request.TransactionId;
	if (!OutResult.LedgerEntryIds.IsEmpty())
	{
		Entry.CreditLedgerEntryId = OutResult.LedgerEntryIds[0];
	}
	Entry.DebugReason = TEXT("M12 market transaction linked cargo, legal clearance, stock, account, ledger, and debug progression records.");
	Entry.IdempotencyKey = MakeId(TEXT("idem_m12_progression_market"), Request.TransactionId);
	if (!InOutState.ProgressionDebugLedger.ContainsByPredicate([&Entry](const FProgressionDebugLedgerEntry& Candidate)
	{
		return Candidate.ProgressionEntryId == Entry.ProgressionEntryId || Candidate.IdempotencyKey == Entry.IdempotencyKey;
	}))
	{
		InOutState.ProgressionDebugLedger.Add(Entry);
	}
	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ExecuteStationServiceRequestOnce(
	FSystemicGameplayState& InOutState,
	const FStationServiceRequest& Request,
	FStationServiceResultRecord& OutResult,
	FString& OutFailureReason)
{
	OutResult = FStationServiceResultRecord();
	OutResult.ResultId = MakeId(TEXT("service_result"), Request.RequestId);
	OutResult.RequestId = Request.RequestId;
	OutResult.ServiceEndpointId = Request.ServiceEndpointId;
	OutResult.ServiceType = Request.ServiceType;
	OutResult.TargetShipId = Request.TargetShipId;
	OutResult.SourceEventId = Request.SourceEventId;
	OutResult.IdempotencyKey = Request.IdempotencyKey;

	if (Request.RequestId.IsNone() || Request.IdempotencyKey.IsNone() || Request.ServiceEndpointId.IsNone() ||
		Request.ServiceType.IsNone() || Request.TargetShipId.IsNone() || Request.Amount <= 0.0 || Request.TotalCost <= 0)
	{
		OutResult.RejectionReason = TEXT("Station service request requires stable IDs, service type, target ship, positive amount, positive cost, and idempotency.");
		OutFailureReason = OutResult.RejectionReason;
		return false;
	}
	if (const FStationServiceResultRecord* Existing = FindConst(InOutState.StationServiceResults, [&Request, &OutResult](const FStationServiceResultRecord& Candidate)
	{
		return Candidate.RequestId == Request.RequestId || Candidate.IdempotencyKey == Request.IdempotencyKey || Candidate.ResultId == OutResult.ResultId;
	}))
	{
		OutResult = *Existing;
		OutFailureReason.Reset();
		return Existing->Result == ESystemicActionResult::Accepted;
	}

	const FStationServiceEndpointDefinition* Endpoint = FindConst(InOutState.StationServiceEndpoints, [&Request](const FStationServiceEndpointDefinition& Candidate)
	{
		return Candidate.ServiceEndpointId == Request.ServiceEndpointId && Candidate.ServiceType == Request.ServiceType;
	});
	FCreditAccountRecord* Debit = FindMutable(InOutState.CreditAccounts, [&Request](const FCreditAccountRecord& Candidate)
	{
		return Candidate.AccountId == Request.DebitAccountId;
	});
	FCreditAccountRecord* Credit = FindMutable(InOutState.CreditAccounts, [&Request](const FCreditAccountRecord& Candidate)
	{
		return Candidate.AccountId == Request.CreditAccountId;
	});
	if (!Endpoint || !Debit || !Credit || Debit->AvailableBalance < Request.TotalCost)
	{
		OutResult.RejectionReason = TEXT("Station service endpoint or accounts do not resolve, or the debit account has insufficient credits.");
		OutFailureReason = OutResult.RejectionReason;
		return false;
	}
	if (Endpoint->UnitPriceCredits <= 0)
	{
		OutResult.RejectionReason = TEXT("Station service endpoint requires a positive unit price.");
		OutFailureReason = OutResult.RejectionReason;
		return false;
	}
	const int64 ExpectedTotalCost = CalculateServiceTotalCost(Request.Amount, Endpoint->UnitPriceCredits);
	if (Request.TotalCost != ExpectedTotalCost)
	{
		OutResult.RejectionReason = FString::Printf(TEXT("Station service quoted total %lld does not match endpoint unit price quote %lld."), Request.TotalCost, ExpectedTotalCost);
		OutFailureReason = OutResult.RejectionReason;
		return false;
	}

	FShipDurabilityState* RepairDurability = nullptr;
	FShipResourceState* ResourceState = nullptr;
	if (Request.ServiceType == FName(TEXT("repair")))
	{
		RepairDurability = FindMutable(InOutState.ShipDurabilityStates, [&Request](const FShipDurabilityState& Candidate)
		{
			return Candidate.CombatantId == Request.TargetShipId;
		});
		if (!RepairDurability)
		{
			OutResult.RejectionReason = TEXT("Repair target durability state does not resolve.");
			OutFailureReason = OutResult.RejectionReason;
			return false;
		}
	}
	else if (Request.ServiceType == FName(TEXT("refuel")) || Request.ServiceType == FName(TEXT("rearm")))
	{
		ResourceState = FindMutable(InOutState.ShipResourceStates, [&Request](const FShipResourceState& Candidate)
		{
			return Candidate.ShipId == Request.TargetShipId;
		});
		if (!ResourceState)
		{
			OutResult.RejectionReason = TEXT("Service target resource state does not resolve.");
			OutFailureReason = OutResult.RejectionReason;
			return false;
		}
	}
	else
	{
		OutResult.RejectionReason = TEXT("Unsupported M12 station service type.");
		OutFailureReason = OutResult.RejectionReason;
		return false;
	}

	double AppliedServiceAmount = 0.0;
	double RepairedHull = 0.0;
	double RepairedShield = 0.0;
	if (Request.ServiceType == FName(TEXT("repair")))
	{
		const double MaxHull = ResolvePositiveCapacity(RepairDurability->MaxHull, 100.0);
		const double MaxShield = ResolvePositiveCapacity(RepairDurability->MaxShield, 100.0);
		RepairedHull = FMath::Min(MaxHull, RepairDurability->Hull + Request.Amount);
		RepairedShield = FMath::Min(MaxShield, RepairDurability->Shield + Request.Amount);
		AppliedServiceAmount = FMath::Max(RepairedHull - RepairDurability->Hull, RepairedShield - RepairDurability->Shield);
	}
	else if (Request.ServiceType == FName(TEXT("refuel")))
	{
		if (ResourceState->MaxFuel <= 0.0)
		{
			OutResult.RejectionReason = TEXT("Refuel target has invalid fuel capacity.");
			OutFailureReason = OutResult.RejectionReason;
			return false;
		}
		AppliedServiceAmount = FMath::Min(ResourceState->MaxFuel, ResourceState->Fuel + Request.Amount) - ResourceState->Fuel;
	}
	else if (Request.ServiceType == FName(TEXT("rearm")))
	{
		if (ResourceState->MaxAmmo <= 0.0)
		{
			OutResult.RejectionReason = TEXT("Rearm target has invalid ammo capacity.");
			OutFailureReason = OutResult.RejectionReason;
			return false;
		}
		AppliedServiceAmount = FMath::Min(ResourceState->MaxAmmo, ResourceState->Ammo + Request.Amount) - ResourceState->Ammo;
	}
	if (AppliedServiceAmount <= UE_SMALL_NUMBER)
	{
		OutResult.RejectionReason = TEXT("Station service would not change the target ship state.");
		OutFailureReason = OutResult.RejectionReason;
		return false;
	}
	if (AppliedServiceAmount + UE_SMALL_NUMBER < Request.Amount)
	{
		OutResult.RejectionReason = TEXT("Station service request exceeds the target ship's remaining service capacity.");
		OutFailureReason = OutResult.RejectionReason;
		return false;
	}

	FName LedgerId = MakeId(TEXT("ledger"), Request.RequestId);
	Debit->AvailableBalance -= Request.TotalCost;
	Credit->AvailableBalance += Request.TotalCost;
	FCreditLedgerEntry Ledger;
	Ledger.LedgerEntryId = LedgerId;
	Ledger.DebitAccountId = Debit->AccountId;
	Ledger.CreditAccountId = Credit->AccountId;
	Ledger.Amount = Request.TotalCost;
	Ledger.Reason = Request.ServiceType;
	Ledger.SourceEventId = Request.SourceEventId;
	Ledger.SourceTransactionId = Request.RequestId;
	Ledger.IdempotencyKey = MakeId(TEXT("ledger"), Request.IdempotencyKey);
	Ledger.State = TEXT("applied");
	InOutState.CreditLedger.Add(Ledger);
	Debit->LastLedgerEntryId = Ledger.LedgerEntryId;
	Credit->LastLedgerEntryId = Ledger.LedgerEntryId;

	if (Request.ServiceType == FName(TEXT("repair")))
	{
		RepairDurability->Hull = RepairedHull;
		RepairDurability->Shield = RepairedShield;
		RepairDurability->State = TEXT("active");
	}
	else if (Request.ServiceType == FName(TEXT("refuel")) || Request.ServiceType == FName(TEXT("rearm")))
	{
		if (Request.ServiceType == FName(TEXT("refuel")))
		{
			ResourceState->Fuel += AppliedServiceAmount;
		}
		else
		{
			ResourceState->Ammo += AppliedServiceAmount;
		}
		ResourceState->LastServiceResultId = OutResult.ResultId;
	}

	FGameplayTransactionRecord Transaction;
	Transaction.TransactionId = Request.RequestId;
	Transaction.TransactionType = TEXT("station_service");
	Transaction.SourceEventId = Request.SourceEventId;
	Transaction.InitiatorType = TEXT("player");
	Transaction.InitiatorId = Debit->OwnerId;
	Transaction.TargetType = TEXT("service");
	Transaction.TargetId = Request.ServiceEndpointId;
	Transaction.State = TEXT("committed");
	Transaction.IdempotencyKey = Request.IdempotencyKey;
	Transaction.DebugReason = TEXT("M12 station service committed ledger and ship resource side effects.");
	InOutState.Transactions.Add(Transaction);
	AddJournalEntry(InOutState, Request.RequestId, 1, TEXT("ledger"), Ledger.LedgerEntryId, Request.IdempotencyKey);
	AddJournalEntry(InOutState, Request.RequestId, 2, TEXT("service_resource"), OutResult.ResultId, Request.IdempotencyKey);

	OutResult.Result = ESystemicActionResult::Accepted;
	OutResult.AppliedAmount = AppliedServiceAmount;
	OutResult.UnitPriceCredits = Endpoint->UnitPriceCredits;
	OutResult.AppliedCost = Request.TotalCost;
	OutResult.LedgerEntryId = Ledger.LedgerEntryId;
	InOutState.StationServiceResults.Add(OutResult);

	FProgressionDebugLedgerEntry Entry;
	Entry.ProgressionEntryId = MakeId(TEXT("progression_service"), Request.RequestId);
	Entry.EntryType = TEXT("station_service");
	Entry.SubjectId = Debit->OwnerId;
	Entry.FactionId = Endpoint->ProviderFactionId;
	Entry.SourceEventId = Request.SourceEventId;
	Entry.SourceTransactionId = Request.RequestId;
	Entry.CreditLedgerEntryId = Ledger.LedgerEntryId;
	Entry.ServiceResultId = OutResult.ResultId;
	Entry.DebugReason = TEXT("Service transaction linked repair/refuel/rearm availability, account ledger, and ship resource state.");
	Entry.IdempotencyKey = MakeId(TEXT("idem_m12_progression_service"), Request.RequestId);
	InOutState.ProgressionDebugLedger.Add(Entry);

	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::AcceptMissionOfferOnce(
	FSystemicGameplayState& InOutState,
	FName OfferId,
	FName PlayerId,
	FName IdempotencyKey,
	FMissionInstanceState& OutMission,
	FString& OutFailureReason)
{
	OutMission = FMissionInstanceState();
	if (OfferId.IsNone() || PlayerId.IsNone() || IdempotencyKey.IsNone())
	{
		OutFailureReason = TEXT("Mission accept requires offer, player, and idempotency IDs.");
		return false;
	}
	FMissionOfferRecord* Offer = FindMutable(InOutState.MissionOffers, [OfferId](const FMissionOfferRecord& Candidate)
	{
		return Candidate.OfferId == OfferId;
	});
	FMissionInstanceState* Mission = FindMutable(InOutState.MissionInstances, [OfferId](const FMissionInstanceState& Candidate)
	{
		return Candidate.OfferId == OfferId;
	});
	if (!Offer || !Mission)
	{
		OutFailureReason = TEXT("Mission offer or mission instance does not resolve.");
		return false;
	}
	if (Mission->CurrentState == FName(TEXT("active")) || Offer->State == FName(TEXT("accepted")))
	{
		OutMission = *Mission;
		OutFailureReason.Reset();
		return true;
	}
	Offer->State = TEXT("accepted");
	Mission->CurrentState = TEXT("active");
	Mission->OwnerId = PlayerId;
	Mission->IdempotencyKey = IdempotencyKey;
	for (FObjectiveState& Objective : InOutState.ObjectiveStates)
	{
		if (Objective.MissionInstanceId == Mission->MissionInstanceId)
		{
			Objective.State = TEXT("active");
		}
	}

	const FName TransactionId = MakeId(TEXT("tx_accept"), Mission->MissionInstanceId);
	FGameplayTransactionRecord Transaction;
	Transaction.TransactionId = TransactionId;
	Transaction.TransactionType = TEXT("mission_accept");
	Transaction.InitiatorType = TEXT("player");
	Transaction.InitiatorId = PlayerId;
	Transaction.TargetType = TEXT("mission");
	Transaction.TargetId = Mission->MissionInstanceId;
	Transaction.State = TEXT("committed");
	Transaction.IdempotencyKey = IdempotencyKey;
	Transaction.DebugReason = TEXT("Mission accept committed offer, mission, objective, and progression trace state.");
	AddTransactionIfMissing(InOutState, Transaction);
	AddJournalEntry(InOutState, TransactionId, 1, TEXT("mission_state"), Mission->MissionInstanceId, IdempotencyKey);

	FProgressionDebugLedgerEntry Entry;
	Entry.ProgressionEntryId = MakeId(TEXT("progression_accept"), Mission->MissionInstanceId);
	Entry.EntryType = TEXT("mission_accept");
	Entry.SubjectId = PlayerId;
	Entry.FactionId = Mission->IssuerFactionId;
	Entry.MissionInstanceId = Mission->MissionInstanceId;
	Entry.SourceTransactionId = TransactionId;
	Entry.DebugReason = TEXT("Mission board offer accepted and objective states activated.");
	Entry.IdempotencyKey = IdempotencyKey;
	if (!InOutState.ProgressionDebugLedger.ContainsByPredicate([&Entry](const FProgressionDebugLedgerEntry& Candidate)
	{
		return Candidate.ProgressionEntryId == Entry.ProgressionEntryId || Candidate.IdempotencyKey == Entry.IdempotencyKey;
	}))
	{
		InOutState.ProgressionDebugLedger.Add(Entry);
	}
	AddJournalEntry(InOutState, TransactionId, 2, TEXT("debug"), Entry.ProgressionEntryId, IdempotencyKey);

	OutMission = *Mission;
	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ResolveMissionContactInteraction(
	const FStarSystemDefinition& SystemDefinition,
	const FSystemicGameplayState& State,
	FName StationId,
	FName GiverNpcId,
	FMissionContactInteractionView& OutInteraction)
{
	OutInteraction = FMissionContactInteractionView();
	OutInteraction.StationId = StationId;
	OutInteraction.GiverNpcId = GiverNpcId;

	if (StationId.IsNone() || GiverNpcId.IsNone())
	{
		OutInteraction.DebugReason = TEXT("Mission contact lookup requires station and giver NPC IDs.");
		return false;
	}

	const FStationDefinition* Station = FindConst(SystemDefinition.Stations, [StationId](const FStationDefinition& Candidate)
	{
		return Candidate.StationId == StationId;
	});
	if (!Station)
	{
		OutInteraction.DebugReason = TEXT("Mission contact station does not resolve.");
		return false;
	}
	if (!Station->QuestGiverNpcIds.Contains(GiverNpcId))
	{
		OutInteraction.DebugReason = TEXT("Mission contact NPC is not authored on this station.");
		return false;
	}

	for (const FMissionOfferRecord& Offer : State.MissionOffers)
	{
		if (Offer.SourceStationId != StationId || Offer.GiverNpcId != GiverNpcId)
		{
			continue;
		}

		bool bStationTagsSatisfied = true;
		for (const FName RequiredTag : Offer.RequiredStationTags)
		{
			if (RequiredTag.IsNone() || !Station->MissionTags.Contains(RequiredTag))
			{
				bStationTagsSatisfied = false;
				break;
			}
		}
		if (!bStationTagsSatisfied)
		{
			continue;
		}

		const FMissionInstanceState* Mission = FindConst(State.MissionInstances, [&Offer](const FMissionInstanceState& Candidate)
		{
			return Candidate.OfferId == Offer.OfferId;
		});
		if (!Mission || Mission->CurrentState == FName(TEXT("completed")) || Mission->CurrentState == FName(TEXT("failed")))
		{
			continue;
		}
		if (Mission->CurrentState != FName(TEXT("active")))
		{
			const bool bHasActiveMissionForContact = State.MissionOffers.ContainsByPredicate([&State, StationId, GiverNpcId, Station](const FMissionOfferRecord& OtherOffer)
			{
				if (OtherOffer.SourceStationId != StationId || OtherOffer.GiverNpcId != GiverNpcId)
				{
					return false;
				}
				for (const FName RequiredTag : OtherOffer.RequiredStationTags)
				{
					if (RequiredTag.IsNone() || !Station->MissionTags.Contains(RequiredTag))
					{
						return false;
					}
				}
				const FMissionInstanceState* OtherMission = FindConst(State.MissionInstances, [&OtherOffer](const FMissionInstanceState& Candidate)
				{
					return Candidate.OfferId == OtherOffer.OfferId;
				});
				return OtherMission && OtherMission->CurrentState == FName(TEXT("active"));
			});
			if (bHasActiveMissionForContact)
			{
				continue;
			}
		}

		OutInteraction.bHasMission = true;
		OutInteraction.OfferId = Offer.OfferId;
		OutInteraction.MissionDefinitionId = Offer.MissionDefinitionId;
		OutInteraction.MissionInstanceId = Mission->MissionInstanceId;
		OutInteraction.IssuerFactionId = Offer.IssuerFactionId;
		OutInteraction.RegionName = Station->RegionName;
		OutInteraction.RequiredStationTags = Offer.RequiredStationTags;

		if (const FFactionDefinition* Faction = FindConst(State.Factions, [&Offer](const FFactionDefinition& Candidate)
		{
			return Candidate.FactionId == Offer.IssuerFactionId;
		}))
		{
			OutInteraction.IssuerFactionDisplayName = Faction->DisplayName;
		}

		TArray<FString> ContextLines;
		if (!OutInteraction.RegionName.IsNone())
		{
			ContextLines.Add(FString::Printf(TEXT("Region: %s"), *OutInteraction.RegionName.ToString()));
		}
		if (!OutInteraction.IssuerFactionId.IsNone())
		{
			const FString Issuer = OutInteraction.IssuerFactionDisplayName.IsEmpty()
				? OutInteraction.IssuerFactionId.ToString()
				: OutInteraction.IssuerFactionDisplayName.ToString();
			ContextLines.Add(FString::Printf(TEXT("Issuer: %s"), *Issuer));
		}
		OutInteraction.ContextText = FString::Join(ContextLines, TEXT("\n"));

		if (MissionObjectivesComplete(State, *Mission))
		{
			OutInteraction.InteractionState = TEXT("ready_to_turn_in");
			OutInteraction.AvailableCommand = TEXT("turn_in");
			FillMissionPresentation(State, *Mission, OutInteraction);
			OutInteraction.DebugReason = TEXT("Mission objectives are complete; contact can turn in the mission.");
			return true;
		}

		if (Mission->CurrentState == FName(TEXT("active")))
		{
			if (const FObjectiveState* Objective = FindFirstActiveMissionObjective(State, *Mission))
			{
				OutInteraction.ActiveObjectiveStateId = Objective->ObjectiveStateId;
				OutInteraction.ActiveObjectiveType = Objective->ObjectiveType;
				OutInteraction.ActiveObjectiveTargetId = Objective->TargetId;
			}
			OutInteraction.InteractionState = TEXT("in_progress");
			OutInteraction.AvailableCommand = TEXT("continue");
			FillMissionPresentation(State, *Mission, OutInteraction);
			OutInteraction.DebugReason = TEXT("Mission is active; contact returns current objective context.");
			return true;
		}

		OutInteraction.InteractionState = TEXT("offer");
		OutInteraction.AvailableCommand = TEXT("accept");
		FillMissionPresentation(State, *Mission, OutInteraction);
		OutInteraction.DebugReason = TEXT("Station-authored contact has an available mission offer.");
		return true;
	}

	OutInteraction.DebugReason = TEXT("No eligible mission offer for this station contact.");
	return false;
}

bool USystemicGameplayQueryService::CompleteMissionOnce(
	FSystemicGameplayState& InOutState,
	FName MissionInstanceId,
	FName SourceEventId,
	FName IdempotencyKey,
	FProgressionDebugLedgerEntry& OutCompletionEntry,
	FString& OutFailureReason)
{
	OutCompletionEntry = FProgressionDebugLedgerEntry();
	if (MissionInstanceId.IsNone() || SourceEventId.IsNone() || IdempotencyKey.IsNone())
	{
		OutFailureReason = TEXT("Mission completion requires mission, source event, and idempotency IDs.");
		return false;
	}
	if (const FProgressionDebugLedgerEntry* Existing = FindConst(InOutState.ProgressionDebugLedger, [MissionInstanceId, IdempotencyKey](const FProgressionDebugLedgerEntry& Candidate)
	{
		return Candidate.MissionInstanceId == MissionInstanceId &&
			Candidate.EntryType == FName(TEXT("mission_complete")) &&
			Candidate.IdempotencyKey == IdempotencyKey;
	}))
	{
		OutCompletionEntry = *Existing;
		OutFailureReason.Reset();
		return true;
	}
	FMissionInstanceState* Mission = FindMutable(InOutState.MissionInstances, [MissionInstanceId](const FMissionInstanceState& Candidate)
	{
		return Candidate.MissionInstanceId == MissionInstanceId;
	});
	if (!Mission || Mission->CurrentState != FName(TEXT("active")))
	{
		OutFailureReason = TEXT("Mission instance does not resolve or is not completable.");
		return false;
	}
	if (!MissionObjectivesComplete(InOutState, *Mission))
	{
		OutFailureReason = TEXT("Mission objectives are not complete.");
		return false;
	}

	FCreditAccountRecord* PlayerAccount = FindMutable(InOutState.CreditAccounts, [](const FCreditAccountRecord& Candidate)
	{
		return Candidate.AccountId == FName(TEXT("account_player"));
	});
	FCreditAccountRecord* EscrowAccount = FindMutable(InOutState.CreditAccounts, [](const FCreditAccountRecord& Candidate)
	{
		return Candidate.AccountId == FName(TEXT("account_wayfarer_mission_escrow"));
	});
	FEscrowHoldRecord* Escrow = Mission->RewardEscrowIds.IsEmpty() ? nullptr : FindMutable(InOutState.EscrowHolds, [Mission](const FEscrowHoldRecord& Candidate)
	{
		return Candidate.EscrowId == Mission->RewardEscrowIds[0];
	});
	if (!PlayerAccount || !EscrowAccount || !Escrow || Escrow->Amount <= 0 || EscrowAccount->AvailableBalance < Escrow->Amount)
	{
		OutFailureReason = TEXT("Mission reward escrow or accounts do not resolve.");
		return false;
	}

	const FName TransactionId = MakeId(TEXT("tx_complete"), MissionInstanceId);
	const FName LedgerId = MakeId(TEXT("ledger_complete"), MissionInstanceId);
	EscrowAccount->AvailableBalance -= Escrow->Amount;
	PlayerAccount->AvailableBalance += Escrow->Amount;
	FCreditLedgerEntry Ledger;
	Ledger.LedgerEntryId = LedgerId;
	Ledger.DebitAccountId = EscrowAccount->AccountId;
	Ledger.CreditAccountId = PlayerAccount->AccountId;
	Ledger.Amount = Escrow->Amount;
	Ledger.Reason = TEXT("mission_reward");
	Ledger.SourceEventId = SourceEventId;
	Ledger.SourceTransactionId = TransactionId;
	Ledger.IdempotencyKey = MakeId(TEXT("ledger"), IdempotencyKey);
	Ledger.State = TEXT("applied");
	InOutState.CreditLedger.Add(Ledger);
	Escrow->State = TEXT("released");
	PlayerAccount->LastLedgerEntryId = Ledger.LedgerEntryId;
	EscrowAccount->LastLedgerEntryId = Ledger.LedgerEntryId;

	FGameplayTransactionRecord Transaction;
	Transaction.TransactionId = TransactionId;
	Transaction.TransactionType = TEXT("mission_turn_in");
	Transaction.SourceEventId = SourceEventId;
	Transaction.InitiatorType = TEXT("player");
	Transaction.InitiatorId = Mission->OwnerId;
	Transaction.TargetType = TEXT("mission");
	Transaction.TargetId = MissionInstanceId;
	Transaction.State = TEXT("committed");
	Transaction.IdempotencyKey = IdempotencyKey;
	Transaction.DebugReason = TEXT("Mission completion committed objectives, reward ledger, reputation, follow-up, and message arbitration.");
	InOutState.Transactions.Add(Transaction);
	AddJournalEntry(InOutState, TransactionId, 1, TEXT("ledger"), Ledger.LedgerEntryId, IdempotencyKey);

	Mission->CurrentState = TEXT("completed");
	for (FObjectiveState& Objective : InOutState.ObjectiveStates)
	{
		if (Objective.MissionInstanceId == MissionInstanceId)
		{
			Objective.State = TEXT("completed");
		}
	}

	FReputationDeltaRecord Delta;
	Delta.ReputationDeltaId = MakeId(TEXT("rep_complete"), MissionInstanceId);
	Delta.SubjectId = Mission->OwnerId;
	Delta.FactionId = Mission->IssuerFactionId;
	Delta.Delta = 0.05;
	Delta.ResultStanding = 0.05;
	Delta.Reason = TEXT("mission_completion");
	Delta.SourceEventId = SourceEventId;
	Delta.SourceTransactionId = TransactionId;
	Delta.IdempotencyKey = MakeId(TEXT("idem_m12_rep"), MissionInstanceId);
	InOutState.ReputationDeltas.Add(Delta);

	FFollowUpOpportunityRecord* SeedOpportunity = FindMutable(InOutState.FollowUpOpportunities, [](const FFollowUpOpportunityRecord& Candidate)
	{
		return Candidate.OpportunityId == FName(TEXT("followup_m12_wayfarer_return_01"));
	});
	if (SeedOpportunity)
	{
		SeedOpportunity->State = TEXT("available");
		SeedOpportunity->SourceResultId = Ledger.LedgerEntryId;
	}

	FMessageArbitrationResultRecord Arbitration;
	Arbitration.ArbitrationId = MakeId(TEXT("arb_complete"), MissionInstanceId);
	Arbitration.SourceEventId = SourceEventId;
	Arbitration.SelectedMessageInstanceId = TEXT("message_distress_trade_lane_01");
	Arbitration.State = TEXT("selected");
	Arbitration.IdempotencyKey = MakeId(TEXT("idem_m12_arbitration"), MissionInstanceId);
	InOutState.MessageArbitrationResults.Add(Arbitration);

	OutCompletionEntry.ProgressionEntryId = MakeId(TEXT("progression_complete"), MissionInstanceId);
	OutCompletionEntry.EntryType = TEXT("mission_complete");
	OutCompletionEntry.SubjectId = Mission->OwnerId;
	OutCompletionEntry.FactionId = Mission->IssuerFactionId;
	OutCompletionEntry.SourceEventId = SourceEventId;
	OutCompletionEntry.SourceTransactionId = TransactionId;
	OutCompletionEntry.CreditLedgerEntryId = Ledger.LedgerEntryId;
	OutCompletionEntry.ReputationDeltaId = Delta.ReputationDeltaId;
	OutCompletionEntry.MissionInstanceId = MissionInstanceId;
	OutCompletionEntry.FollowUpOpportunityId = SeedOpportunity ? SeedOpportunity->OpportunityId : FName();
	OutCompletionEntry.MessageArbitrationId = Arbitration.ArbitrationId;
	OutCompletionEntry.DebugReason = TEXT("M12 mission completion links reward, legal/faction standing, follow-up route, message arbitration, and idempotent ledger output.");
	OutCompletionEntry.IdempotencyKey = IdempotencyKey;
	InOutState.ProgressionDebugLedger.Add(OutCompletionEntry);
	AddJournalEntry(InOutState, TransactionId, 2, TEXT("escrow"), Escrow->EscrowId, IdempotencyKey);
	AddJournalEntry(InOutState, TransactionId, 3, TEXT("mission_state"), MissionInstanceId, IdempotencyKey);
	AddJournalEntry(InOutState, TransactionId, 4, TEXT("reputation_delta"), Delta.ReputationDeltaId, IdempotencyKey);
	if (SeedOpportunity)
	{
		AddJournalEntry(InOutState, TransactionId, 5, TEXT("generated_followup"), SeedOpportunity->OpportunityId, IdempotencyKey);
	}
	AddJournalEntry(InOutState, TransactionId, 6, TEXT("message_arbitration"), Arbitration.ArbitrationId, IdempotencyKey);
	AddJournalEntry(InOutState, TransactionId, 7, TEXT("debug"), OutCompletionEntry.ProgressionEntryId, IdempotencyKey);

	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::FailMissionOnce(
	FSystemicGameplayState& InOutState,
	FName MissionInstanceId,
	FName SourceEventId,
	FName IdempotencyKey,
	FProgressionDebugLedgerEntry& OutFailureEntry,
	FString& OutFailureReason)
{
	OutFailureEntry = FProgressionDebugLedgerEntry();
	if (MissionInstanceId.IsNone() || SourceEventId.IsNone() || IdempotencyKey.IsNone())
	{
		OutFailureReason = TEXT("Mission failure requires mission, source event, and idempotency IDs.");
		return false;
	}
	if (const FProgressionDebugLedgerEntry* Existing = FindConst(InOutState.ProgressionDebugLedger, [MissionInstanceId, IdempotencyKey](const FProgressionDebugLedgerEntry& Candidate)
	{
		return Candidate.MissionInstanceId == MissionInstanceId &&
			Candidate.EntryType == FName(TEXT("mission_fail")) &&
			Candidate.IdempotencyKey == IdempotencyKey;
	}))
	{
		OutFailureEntry = *Existing;
		OutFailureReason.Reset();
		return true;
	}

	FMissionInstanceState* Mission = FindMutable(InOutState.MissionInstances, [MissionInstanceId](const FMissionInstanceState& Candidate)
	{
		return Candidate.MissionInstanceId == MissionInstanceId;
	});
	if (!Mission || Mission->CurrentState != FName(TEXT("active")))
	{
		OutFailureReason = TEXT("Mission instance does not resolve or is not active.");
		return false;
	}

	const FName TransactionId = MakeId(TEXT("tx_fail"), MissionInstanceId);
	Mission->CurrentState = TEXT("failed");
	for (FObjectiveState& Objective : InOutState.ObjectiveStates)
	{
		if (Objective.MissionInstanceId == MissionInstanceId && Objective.State != FName(TEXT("completed")))
		{
			Objective.State = TEXT("failed");
		}
	}

	FGameplayTransactionRecord Transaction;
	Transaction.TransactionId = TransactionId;
	Transaction.TransactionType = TEXT("mission_turn_in");
	Transaction.SourceEventId = SourceEventId;
	Transaction.InitiatorType = TEXT("player");
	Transaction.InitiatorId = Mission->OwnerId;
	Transaction.TargetType = TEXT("mission");
	Transaction.TargetId = MissionInstanceId;
	Transaction.State = TEXT("committed");
	Transaction.IdempotencyKey = IdempotencyKey;
	Transaction.DebugReason = TEXT("Mission failure committed mission state and reputation penalty without releasing escrow.");
	AddTransactionIfMissing(InOutState, Transaction);

	FReputationDeltaRecord Delta;
	Delta.ReputationDeltaId = MakeId(TEXT("rep_fail"), MissionInstanceId);
	Delta.SubjectId = Mission->OwnerId;
	Delta.FactionId = Mission->IssuerFactionId;
	Delta.Delta = -0.02;
	Delta.ResultStanding = -0.02;
	Delta.Reason = TEXT("mission_failure");
	Delta.SourceEventId = SourceEventId;
	Delta.SourceTransactionId = TransactionId;
	Delta.IdempotencyKey = MakeId(TEXT("idem_m12_rep_fail"), MissionInstanceId);
	InOutState.ReputationDeltas.Add(Delta);

	OutFailureEntry.ProgressionEntryId = MakeId(TEXT("progression_fail"), MissionInstanceId);
	OutFailureEntry.EntryType = TEXT("mission_fail");
	OutFailureEntry.SubjectId = Mission->OwnerId;
	OutFailureEntry.FactionId = Mission->IssuerFactionId;
	OutFailureEntry.SourceEventId = SourceEventId;
	OutFailureEntry.SourceTransactionId = TransactionId;
	OutFailureEntry.ReputationDeltaId = Delta.ReputationDeltaId;
	OutFailureEntry.MissionInstanceId = MissionInstanceId;
	OutFailureEntry.DebugReason = TEXT("M12 mission failure links failed objective state and reputation penalty by stable IDs.");
	OutFailureEntry.IdempotencyKey = IdempotencyKey;
	InOutState.ProgressionDebugLedger.Add(OutFailureEntry);

	AddJournalEntry(InOutState, TransactionId, 1, TEXT("mission_state"), MissionInstanceId, IdempotencyKey);
	AddJournalEntry(InOutState, TransactionId, 2, TEXT("reputation_delta"), Delta.ReputationDeltaId, IdempotencyKey);
	AddJournalEntry(InOutState, TransactionId, 3, TEXT("debug"), OutFailureEntry.ProgressionEntryId, IdempotencyKey);

	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ApplyEncounterProgressionOutcomeOnce(
	FSystemicGameplayState& InOutState,
	FName EncounterId,
	FName IdempotencyKey,
	FProgressionDebugLedgerEntry& OutProgressionEntry,
	FString& OutFailureReason)
{
	OutProgressionEntry = FProgressionDebugLedgerEntry();
	if (EncounterId.IsNone() || IdempotencyKey.IsNone())
	{
		OutFailureReason = TEXT("Encounter progression requires encounter and idempotency IDs.");
		return false;
	}
	if (const FProgressionDebugLedgerEntry* Existing = FindConst(InOutState.ProgressionDebugLedger, [EncounterId, IdempotencyKey](const FProgressionDebugLedgerEntry& Candidate)
	{
		return Candidate.EntryType == FName(TEXT("encounter_outcome")) &&
			Candidate.SourceTransactionId == EncounterId &&
			Candidate.IdempotencyKey == IdempotencyKey;
	}))
	{
		OutProgressionEntry = *Existing;
		OutFailureReason.Reset();
		return true;
	}
	const FLogicalEncounterRecord* Encounter = FindConst(InOutState.LogicalEncounters, [EncounterId](const FLogicalEncounterRecord& Candidate)
	{
		return Candidate.EncounterId == EncounterId;
	});
	if (!Encounter || Encounter->State != FName(TEXT("resolved")))
	{
		OutFailureReason = TEXT("Encounter must be resolved before M12 progression outcome can be applied.");
		return false;
	}

	FReputationDeltaRecord Delta;
	Delta.ReputationDeltaId = MakeId(TEXT("rep_encounter"), EncounterId);
	Delta.SubjectId = TEXT("player");
	Delta.FactionId = TEXT("frontier_local_authority");
	Delta.Delta = 0.02;
	Delta.ResultStanding = 0.02;
	Delta.Reason = TEXT("distress_patrol_outcome");
	Delta.SourceEventId = Encounter->SourceEventId;
	Delta.SourceTransactionId = EncounterId;
	Delta.IdempotencyKey = MakeId(TEXT("idem_m12_rep_encounter"), EncounterId);
	InOutState.ReputationDeltas.Add(Delta);

	OutProgressionEntry.ProgressionEntryId = MakeId(TEXT("progression_encounter"), EncounterId);
	OutProgressionEntry.EntryType = TEXT("encounter_outcome");
	OutProgressionEntry.SubjectId = TEXT("player");
	OutProgressionEntry.FactionId = Delta.FactionId;
	OutProgressionEntry.SourceEventId = Encounter->SourceEventId;
	OutProgressionEntry.SourceTransactionId = EncounterId;
	OutProgressionEntry.ReputationDeltaId = Delta.ReputationDeltaId;
	OutProgressionEntry.DebugReason = TEXT("M12 pirate distress and patrol outcome updates faction reputation and debug progression trace after actor-free encounter resolution.");
	OutProgressionEntry.IdempotencyKey = IdempotencyKey;
	InOutState.ProgressionDebugLedger.Add(OutProgressionEntry);
	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ResolveStationServiceEndpoint(const FSystemicGameplayState& State, FName ServiceEndpointId, FStationServiceEndpointDefinition& OutEndpoint, FString& OutDebugReason)
{
	const FStationServiceEndpointDefinition* Endpoint = FindConst(State.StationServiceEndpoints, [ServiceEndpointId](const FStationServiceEndpointDefinition& Candidate)
	{
		return Candidate.ServiceEndpointId == ServiceEndpointId;
	});
	if (!Endpoint)
	{
		OutDebugReason = TEXT("Service endpoint does not resolve.");
		return false;
	}
	if (!FindConst(State.Factions, [Endpoint](const FFactionDefinition& Candidate) { return Candidate.FactionId == Endpoint->ProviderFactionId; }) ||
		(!Endpoint->MarketId.IsNone() && !FindConst(State.Markets, [Endpoint](const FStationMarketState& Candidate) { return Candidate.MarketId == Endpoint->MarketId; })) ||
		(!Endpoint->CreditAccountId.IsNone() && !FindConst(State.CreditAccounts, [Endpoint](const FCreditAccountRecord& Candidate) { return Candidate.AccountId == Endpoint->CreditAccountId; })) ||
		(!Endpoint->CommsEndpointId.IsNone() && !FindConst(State.CommsEndpoints, [Endpoint](const FCommsEndpointDefinition& Candidate) { return Candidate.EndpointId == Endpoint->CommsEndpointId; })))
	{
		OutDebugReason = TEXT("Endpoint faction, market, credit account, or comms endpoint reference is invalid.");
		return false;
	}

	OutEndpoint = *Endpoint;
	OutDebugReason = FString::Printf(TEXT("Endpoint=%s Station=%s Market=%s Faction=%s LegalPolicy=%s Comms=%s"),
		*Endpoint->ServiceEndpointId.ToString(),
		*Endpoint->StationId.ToString(),
		*Endpoint->MarketId.ToString(),
		*Endpoint->ProviderFactionId.ToString(),
		*Endpoint->LegalPolicyId.ToString(),
		*Endpoint->CommsEndpointId.ToString());
	return true;
}

bool USystemicGameplayQueryService::BuildSystemicDecisionInputSnapshot(const FStarSystemDefinition& SystemDefinition, const FSystemicGameplayState& State, FName RouteSegmentId, FSystemicDecisionInputSnapshot& OutSnapshot)
{
	OutSnapshot = FSystemicDecisionInputSnapshot();
	OutSnapshot.RouteSegmentId = RouteSegmentId;
	const FTrafficRouteSegmentDefinition* Route = SystemDefinition.TrafficRoutes.FindByPredicate([RouteSegmentId](const FTrafficRouteSegmentDefinition& Candidate)
	{
		return Candidate.RouteSegmentId == RouteSegmentId;
	});
	if (!Route)
	{
		OutSnapshot.DebugReason = TEXT("Route segment does not resolve.");
		return false;
	}
	const FJurisdictionDefinition* Jurisdiction = FindConst(State.Jurisdictions, [Route](const FJurisdictionDefinition& Candidate)
	{
		return Candidate.JurisdictionId == Route->JurisdictionId;
	});
	const FStationMarketState* Market = State.Markets.IsEmpty() ? nullptr : &State.Markets[0];
	const FContainerState* Cargo = FindConst(State.Containers, [](const FContainerState& Candidate) { return Candidate.ContainerId == FName(TEXT("player_ship_cargo")); });
	if (!Jurisdiction || !Market || !Cargo)
	{
		OutSnapshot.DebugReason = TEXT("Decision inputs are missing jurisdiction, market, or cargo state.");
		return false;
	}

	OutSnapshot.JurisdictionId = Jurisdiction->JurisdictionId;
	OutSnapshot.AuthorityFactionId = Jurisdiction->AuthorityFactionId;
	OutSnapshot.MarketId = Market->MarketId;
	OutSnapshot.CargoContainerId = Cargo->ContainerId;
	OutSnapshot.LegalContextId = Jurisdiction->LawProfileId;
	OutSnapshot.bValid = true;
	OutSnapshot.DebugReason = TEXT("Systemic inputs resolved by stable IDs; encounter resolution is deferred to M10.");
	return true;
}

bool USystemicGameplayQueryService::ReservePatrolForRoute(
	const FStarSystemDefinition& SystemDefinition,
	FSystemicGameplayState& InOutState,
	FName RouteSegmentId,
	FName SourceEventId,
	double ExpiresAtTimeSeconds,
	FPatrolReservationRecord& OutReservation,
	FString& OutFailureReason)
{
	OutReservation = FPatrolReservationRecord();
	const FTrafficRouteSegmentDefinition* Route = FindConst(SystemDefinition.TrafficRoutes, [RouteSegmentId](const FTrafficRouteSegmentDefinition& Candidate)
	{
		return Candidate.RouteSegmentId == RouteSegmentId;
	});
	if (!Route || !Route->bSupportsPatrolCoverage)
	{
		OutFailureReason = TEXT("Route does not resolve or does not support patrol coverage.");
		return false;
	}
	const FJurisdictionDefinition* Jurisdiction = FindConst(InOutState.Jurisdictions, [Route](const FJurisdictionDefinition& Candidate)
	{
		return Candidate.JurisdictionId == Route->JurisdictionId;
	});
	if (!Jurisdiction)
	{
		OutFailureReason = TEXT("Route jurisdiction does not resolve.");
		return false;
	}

	const FName IdempotencyKey = FName(*FString::Printf(TEXT("idem_m10_patrol_%s_%s"), *RouteSegmentId.ToString(), *SourceEventId.ToString()));
	if (const FPatrolReservationRecord* Existing = FindConst(InOutState.PatrolReservations, [IdempotencyKey](const FPatrolReservationRecord& Candidate)
	{
		return Candidate.IdempotencyKey == IdempotencyKey;
	}))
	{
		OutReservation = *Existing;
		OutFailureReason.Reset();
		return true;
	}

	FFactionOperationalState* Operation = FindMutable(InOutState.FactionOperations, [Jurisdiction](const FFactionOperationalState& Candidate)
	{
		return Candidate.FactionId == Jurisdiction->AuthorityFactionId && Candidate.AvailablePatrolBudget > Candidate.ReservedPatrolBudget && !Candidate.PatrolAssetIds.IsEmpty();
	});
	if (!Operation)
	{
		OutFailureReason = TEXT("No patrol operation has available patrol budget.");
		return false;
	}

	FName SelectedPatrolAssetId;
	for (const FName PatrolAssetId : Operation->PatrolAssetIds)
	{
		const bool bAlreadyActive = InOutState.PatrolReservations.ContainsByPredicate([PatrolAssetId](const FPatrolReservationRecord& Candidate)
		{
			return Candidate.PatrolAssetId == PatrolAssetId && Candidate.State != FName(TEXT("released")) && Candidate.ExpiresAtTimeSeconds > 0.0;
		});
		if (!bAlreadyActive)
		{
			SelectedPatrolAssetId = PatrolAssetId;
			break;
		}
	}
	if (SelectedPatrolAssetId.IsNone())
	{
		OutFailureReason = TEXT("All patrol assets are already reserved.");
		return false;
	}

	OutReservation.ReservationId = FName(*FString::Printf(TEXT("reservation_%s_%s"), *SelectedPatrolAssetId.ToString(), *SourceEventId.ToString()));
	OutReservation.PatrolAssetId = SelectedPatrolAssetId;
	OutReservation.FactionId = Operation->FactionId;
	OutReservation.JurisdictionId = Jurisdiction->JurisdictionId;
	OutReservation.RouteSegmentId = RouteSegmentId;
	OutReservation.SourceEventId = SourceEventId;
	OutReservation.State = TEXT("reserved");
	OutReservation.ExpiresAtTimeSeconds = ExpiresAtTimeSeconds;
	OutReservation.IdempotencyKey = IdempotencyKey;
	InOutState.PatrolReservations.Add(OutReservation);
	Operation->ReservedPatrolBudget += 1;
	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ReservePatrolForBestScoredRoute(
	const FStarSystemDefinition& SystemDefinition,
	FSystemicGameplayState& InOutState,
	FName SourceEventId,
	double ExpiresAtTimeSeconds,
	const FSimulationClockSnapshot& ClockSnapshot,
	double SimulationTimeSeconds,
	FPatrolReservationRecord& OutReservation,
	FString& OutFailureReason)
{
	OutReservation = FPatrolReservationRecord();
	TArray<FRouteEncounterScoreRecord> Scores;
	if (!ScorePatrolAndAmbushRoutes(SystemDefinition, InOutState, ClockSnapshot, SimulationTimeSeconds, Scores, OutFailureReason))
	{
		return false;
	}

	Scores.Sort([](const FRouteEncounterScoreRecord& Left, const FRouteEncounterScoreRecord& Right)
	{
		if (!FMath::IsNearlyEqual(Left.PatrolScore, Right.PatrolScore))
		{
			return Left.PatrolScore > Right.PatrolScore;
		}
		return Left.RouteSegmentId.LexicalLess(Right.RouteSegmentId);
	});

	for (const FRouteEncounterScoreRecord& Score : Scores)
	{
		if (Score.PatrolScore <= 0.0)
		{
			continue;
		}

		if (!ReservePatrolForRoute(SystemDefinition, InOutState, Score.RouteSegmentId, SourceEventId, ExpiresAtTimeSeconds, OutReservation, OutFailureReason))
		{
			continue;
		}

		OutReservation.RouteSample = Score.RouteSample;
		OutReservation.DynamicPatrolScore = Score.PatrolScore;
		OutReservation.SelectionDebugReason = Score.DebugReason;
		if (FPatrolReservationRecord* StoredReservation = FindMutable(InOutState.PatrolReservations, [&OutReservation](const FPatrolReservationRecord& Candidate)
		{
			return Candidate.ReservationId == OutReservation.ReservationId;
		}))
		{
			StoredReservation->RouteSample = OutReservation.RouteSample;
			StoredReservation->DynamicPatrolScore = OutReservation.DynamicPatrolScore;
			StoredReservation->SelectionDebugReason = OutReservation.SelectionDebugReason;
		}
		OutFailureReason.Reset();
		return true;
	}

	OutFailureReason = TEXT("No scored patrol route could reserve an available patrol asset.");
	return false;
}

bool USystemicGameplayQueryService::ScorePatrolAndAmbushRoutes(
	const FStarSystemDefinition& SystemDefinition,
	const FSystemicGameplayState& State,
	const FSimulationClockSnapshot& ClockSnapshot,
	double SimulationTimeSeconds,
	TArray<FRouteEncounterScoreRecord>& OutScores,
	FString& OutFailureReason)
{
	OutScores.Reset();
	for (const FTrafficRouteSegmentDefinition& Route : SystemDefinition.TrafficRoutes)
	{
		if (!Route.bSupportsPatrolCoverage && !Route.bSupportsPirateAmbush)
		{
			continue;
		}

		const FRouteSecuritySnapshot* Security = FindConst(State.RouteSecuritySnapshots, [&Route](const FRouteSecuritySnapshot& Candidate)
		{
			return Candidate.RouteSegmentId == Route.RouteSegmentId;
		});
		if (!Security)
		{
			continue;
		}

		const double RoutePhase = FMath::Abs(static_cast<double>(GetTypeHash(Route.RouteSegmentId) % 997)) / 997.0;
		const double SampleProgress = FMath::Fmod(0.25 + RoutePhase + SimulationTimeSeconds / 900.0, 0.5) + 0.25;
		FRouteSample RouteSample;
		if (!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, Route.RouteSegmentId, SampleProgress, ClockSnapshot, SimulationTimeSeconds, RouteSample))
		{
			continue;
		}

		double CriminalPressure = 0.0;
		for (const FCriminalRecord& CriminalRecord : State.CriminalRecords)
		{
			if (CriminalRecord.JurisdictionId == Security->JurisdictionId)
			{
				CriminalPressure += static_cast<double>(CriminalRecord.WantedLevel) * 0.18;
			}
		}
		for (const FOffenseEvent& Offense : State.Offenses)
		{
			if (Offense.JurisdictionId == Security->JurisdictionId && SimulationTimeSeconds - Offense.OccurredTimeSeconds <= 900.0)
			{
				CriminalPressure += 0.08;
			}
		}
		CriminalPressure = FMath::Clamp(CriminalPressure, 0.0, 1.0);

		double MissionPressure = 0.0;
		for (const FObjectiveState& Objective : State.ObjectiveStates)
		{
			if ((Objective.State == FName(TEXT("active")) || Objective.State == FName(TEXT("ready_to_turn_in"))) &&
				(Objective.RouteSegmentIds.Contains(Route.RouteSegmentId) || Objective.TargetId == Route.RouteSegmentId))
			{
				MissionPressure = 1.0;
				break;
			}
		}

		const double SecurityPressure = FMath::Clamp(1.0 - FMath::Max(Security->SecurityRating, Route.SecurityRating), 0.0, 1.0);
		const double RouteValuePressure = FMath::Clamp(FMath::Max(Security->RouteValue, Route.RouteValue) / 1000.0, 0.0, 1.0);
		const double TimePressure = 0.5 + 0.5 * FMath::Sin(SimulationTimeSeconds / 90.0 + RoutePhase * 2.0 * UE_DOUBLE_PI);

		FRouteEncounterScoreRecord Score;
		Score.RouteSegmentId = Route.RouteSegmentId;
		Score.JurisdictionId = Security->JurisdictionId;
		Score.SimulationTimeSeconds = SimulationTimeSeconds;
		Score.SecurityPressure = SecurityPressure;
		Score.CriminalPressure = CriminalPressure;
		Score.MissionPressure = MissionPressure;
		Score.RouteValuePressure = RouteValuePressure;
		Score.TimePressure = TimePressure;
		Score.RouteSample = RouteSample;
		Score.PatrolScore = Route.bSupportsPatrolCoverage
			? FMath::Clamp(SecurityPressure * 0.25 + Security->PirateRiskScore * 0.25 + CriminalPressure * 0.25 + MissionPressure * 0.15 + TimePressure * 0.10 - Security->PatrolCoverageScore * 0.15, 0.0, 1.0)
			: 0.0;
		Score.PirateAmbushScore = Route.bSupportsPirateAmbush
			? FMath::Clamp(Security->PirateRiskScore * 0.35 + RouteValuePressure * 0.20 + CriminalPressure * 0.10 + MissionPressure * 0.15 + TimePressure * 0.15 - Security->PatrolCoverageScore * 0.20, 0.0, 1.0)
			: 0.0;
		Score.DebugReason = FString::Printf(
			TEXT("Route=%s SecurityPressure=%.2f CriminalPressure=%.2f MissionPressure=%.2f RouteValue=%.2f TimePressure=%.2f Patrol=%.2f Ambush=%.2f"),
			*Score.RouteSegmentId.ToString(),
			Score.SecurityPressure,
			Score.CriminalPressure,
			Score.MissionPressure,
			Score.RouteValuePressure,
			Score.TimePressure,
			Score.PatrolScore,
			Score.PirateAmbushScore);
		OutScores.Add(Score);
	}

	OutScores.Sort([](const FRouteEncounterScoreRecord& Left, const FRouteEncounterScoreRecord& Right)
	{
		if (!FMath::IsNearlyEqual(Left.PirateAmbushScore, Right.PirateAmbushScore))
		{
			return Left.PirateAmbushScore > Right.PirateAmbushScore;
		}
		return Left.RouteSegmentId.LexicalLess(Right.RouteSegmentId);
	});
	if (OutScores.IsEmpty())
	{
		OutFailureReason = TEXT("No patrol or ambush route scores could be computed.");
		return false;
	}

	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::SelectPirateInterdictionHazard(
	const FStarSystemDefinition& SystemDefinition,
	const FSystemicGameplayState& State,
	FName RouteSegmentId,
	const FSimulationClockSnapshot& ClockSnapshot,
	double SimulationTimeSeconds,
	FInterdictionHazardRecord& OutHazard,
	FString& OutFailureReason)
{
	OutHazard = FInterdictionHazardRecord();
	const FTrafficRouteSegmentDefinition* Route = FindConst(SystemDefinition.TrafficRoutes, [RouteSegmentId](const FTrafficRouteSegmentDefinition& Candidate)
	{
		return Candidate.RouteSegmentId == RouteSegmentId;
	});
	if (!Route || !Route->bSupportsPirateAmbush)
	{
		OutFailureReason = TEXT("Route does not resolve or does not support pirate ambush.");
		return false;
	}

	const FShipGroupState* PirateGroup = FindConst(SystemDefinition.ShipGroups, [](const FShipGroupState& Candidate)
	{
		return Candidate.GroupRole == FName(TEXT("pirate"));
	});
	const FShipTrafficInstance* TargetShip = FindConst(SystemDefinition.LogicalTraffic, [](const FShipTrafficInstance& Candidate)
	{
		return Candidate.ShipInstanceId == FName(TEXT("trader_brink_01"));
	});
	const FRouteSecuritySnapshot* Security = FindConst(State.RouteSecuritySnapshots, [RouteSegmentId](const FRouteSecuritySnapshot& Candidate)
	{
		return Candidate.RouteSegmentId == RouteSegmentId;
	});
	TArray<FRouteEncounterScoreRecord> Scores;
	if (!ScorePatrolAndAmbushRoutes(SystemDefinition, State, ClockSnapshot, SimulationTimeSeconds, Scores, OutFailureReason))
	{
		return false;
	}
	const FRouteEncounterScoreRecord* RouteScore = FindConst(Scores, [RouteSegmentId](const FRouteEncounterScoreRecord& Candidate)
	{
		return Candidate.RouteSegmentId == RouteSegmentId && Candidate.PirateAmbushScore > 0.0;
	});
	FSystemicDecisionInputSnapshot Snapshot;
	const bool bInputsResolve = BuildSystemicDecisionInputSnapshot(SystemDefinition, State, RouteSegmentId, Snapshot) && Snapshot.bValid;
	const bool bHostilePirateFaction = State.FactionRelationships.ContainsByPredicate([](const FFactionRelationshipRecord& Candidate)
	{
		return Candidate.SourceFactionId == FName(TEXT("ember_raiders")) &&
			Candidate.TargetFactionId == FName(TEXT("frontier_local_authority")) &&
			Candidate.HostilityState == FName(TEXT("hostile"));
	});
	const bool bCargoTargetHasValue = State.Containers.ContainsByPredicate([](const FContainerState& Candidate)
	{
		return Candidate.ContainerId == FName(TEXT("trader_brink_cargo")) && !Candidate.Stacks.IsEmpty();
	});
	const bool bMarketHasRouteCommodity = State.Markets.ContainsByPredicate([](const FStationMarketState& Candidate)
	{
		const int32* Stock = Candidate.StockByCommodity.Find(FName(TEXT("commodity_ember_ore")));
		return Stock && *Stock > 0;
	});
	if (!PirateGroup || !TargetShip || !Security || !RouteScore || !bInputsResolve || !bHostilePirateFaction || !bCargoTargetHasValue || !bMarketHasRouteCommodity ||
		Security->JurisdictionId != Snapshot.JurisdictionId || Route->RouteValue <= 0.0)
	{
		OutFailureReason = TEXT("Pirate ambush policy requires M9 decision inputs, hostile faction/legal context, valued cargo/market data, and positive dynamic ambush pressure.");
		return false;
	}

	OutHazard.HazardId = FName(*FString::Printf(TEXT("hazard_%s_%s"), *PirateGroup->GroupId.ToString(), *RouteSegmentId.ToString()));
	OutHazard.SourceEventId = Security->SourceEventId;
	OutHazard.RouteSegmentId = RouteSegmentId;
	OutHazard.OwnerGroupId = PirateGroup->GroupId;
	OutHazard.TargetShipId = TargetShip->ShipInstanceId;
	OutHazard.State = TEXT("pending");
	OutHazard.ExpiresAtTimeSeconds = SimulationTimeSeconds + 600.0;
	OutHazard.SaveLoadEventId = Security->SourceEventId;
	OutHazard.IdempotencyKey = FName(*FString::Printf(TEXT("idem_m10_hazard_%s_%s"), *PirateGroup->GroupId.ToString(), *RouteSegmentId.ToString()));
	OutHazard.RouteSample = RouteScore->RouteSample;
	OutHazard.DynamicPirateAmbushScore = RouteScore->PirateAmbushScore;
	OutHazard.SelectionDebugReason = RouteScore->DebugReason;

	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ChooseFleeDestination(
	const FStarSystemDefinition& SystemDefinition,
	const FSystemicGameplayState& State,
	FName ThreatRouteSegmentId,
	FMovingFrameTarget& OutDestination,
	FString& OutFailureReason)
{
	OutDestination = FMovingFrameTarget();
	const FTrafficRouteSegmentDefinition* Route = FindConst(SystemDefinition.TrafficRoutes, [ThreatRouteSegmentId](const FTrafficRouteSegmentDefinition& Candidate)
	{
		return Candidate.RouteSegmentId == ThreatRouteSegmentId;
	});
	if (!Route)
	{
		OutFailureReason = TEXT("Threat route does not resolve.");
		return false;
	}
	const bool bRouteHasSecurity = State.RouteSecuritySnapshots.ContainsByPredicate([ThreatRouteSegmentId](const FRouteSecuritySnapshot& Candidate)
	{
		return Candidate.RouteSegmentId == ThreatRouteSegmentId && Candidate.SecurityRating > 0.0;
	});
	if (!bRouteHasSecurity)
	{
		OutFailureReason = TEXT("Flee policy needs a route security snapshot.");
		return false;
	}

	OutDestination.TargetId = Route->DestinationAnchorId;
	OutDestination.TargetType = TEXT("station");
	OutDestination.AnchorId = Route->DestinationAnchorId;
	OutDestination.CoordinateFrame.FrameType = TEXT("station_relative");
	OutDestination.CoordinateFrame.AnchorId = Route->DestinationAnchorId;
	OutDestination.RouteSegmentId = ThreatRouteSegmentId;
	OutDestination.RouteProgress01 = 1.0;
	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::SelectLogicalFightPolicy(
	const FStarSystemDefinition& SystemDefinition,
	const FSystemicGameplayState& State,
	FName EncounterId,
	FName& OutPolicyId,
	FString& OutFailureReason)
{
	OutPolicyId = NAME_None;
	const FLogicalEncounterRecord* Encounter = FindConst(State.LogicalEncounters, [EncounterId](const FLogicalEncounterRecord& Candidate)
	{
		return Candidate.EncounterId == EncounterId;
	});
	if (!Encounter || Encounter->bRequiresActor)
	{
		OutFailureReason = TEXT("Fight policy requires an actor-free logical encounter.");
		return false;
	}
	FSystemicDecisionInputSnapshot Snapshot;
	const bool bInputsResolve = BuildSystemicDecisionInputSnapshot(SystemDefinition, State, Encounter->RouteSegmentId, Snapshot) && Snapshot.bValid;
	const bool bHazardResolves = State.InterdictionHazards.ContainsByPredicate([Encounter](const FInterdictionHazardRecord& Candidate)
	{
		return Candidate.HazardId == Encounter->InterdictionHazardId && Candidate.State == FName(TEXT("pending"));
	});
	const bool bDistressResolves = State.DistressEvents.ContainsByPredicate([Encounter](const FDistressEventRecord& Candidate)
	{
		return Candidate.DistressEventId == Encounter->DistressEventId && !Candidate.MessageInstanceIds.IsEmpty();
	});
	if (!bInputsResolve || !bHazardResolves || !bDistressResolves)
	{
		OutFailureReason = TEXT("Fight policy requires M9 route/legal inputs plus pending hazard and distress records.");
		return false;
	}

	OutPolicyId = TEXT("m10_abstract_fight_distress_patrol_response");
	OutFailureReason.Reset();
	return true;
}

bool USystemicGameplayQueryService::ResolveLogicalEncounterOnce(
	FSystemicGameplayState& InOutState,
	FName EncounterId,
	double AppliedTimeSeconds,
	FLogicalEncounterResolutionResult& OutResult,
	FString& OutFailureReason)
{
	return ResolveLogicalEncounterOnceWithOutcome(InOutState, EncounterId, AppliedTimeSeconds, NAME_None, OutResult, OutFailureReason);
}

bool USystemicGameplayQueryService::ResolveLogicalEncounterOnceWithOutcome(
	FSystemicGameplayState& InOutState,
	FName EncounterId,
	double AppliedTimeSeconds,
	FName RuntimeOutcomeType,
	FLogicalEncounterResolutionResult& OutResult,
	FString& OutFailureReason)
{
	OutResult = FLogicalEncounterResolutionResult();
	OutResult.EncounterId = EncounterId;

	FLogicalEncounterRecord* Encounter = FindMutable(InOutState.LogicalEncounters, [EncounterId](const FLogicalEncounterRecord& Candidate)
	{
		return Candidate.EncounterId == EncounterId;
	});
	if (!Encounter)
	{
		OutFailureReason = TEXT("Logical encounter does not resolve.");
		OutResult.OutcomeType = TEXT("rejected");
		return false;
	}
	const FAbstractEngagementRecord* ExistingEngagement = FindConst(InOutState.AbstractEngagements, [Encounter](const FAbstractEngagementRecord& Candidate)
	{
		return Candidate.EncounterId == Encounter->EncounterId || Candidate.EngagementId == Encounter->EngagementId;
	});
	if (Encounter->State == FName(TEXT("resolved")) || ExistingEngagement)
	{
		OutResult.ResultEventId = Encounter->SourceEventId;
		OutResult.OutcomeType = TEXT("duplicate");
		OutResult.bDuplicate = true;
		OutResult.DebugReason = TEXT("Logical encounter was already resolved; no side effects were replayed.");
		OutFailureReason.Reset();
		return true;
	}

	const FSimulationEventRecord* Event = FindConst(InOutState.Events, [Encounter](const FSimulationEventRecord& Candidate)
	{
		return Candidate.EventId == Encounter->SourceEventId;
	});
	if (!Event)
	{
		OutFailureReason = TEXT("Logical encounter source event does not resolve.");
		OutResult.OutcomeType = TEXT("rejected");
		return false;
	}

	FSimulationEventResultRecord EventResult;
	if (!ApplySimulationEventOnce(InOutState, *Event, AppliedTimeSeconds, EventResult))
	{
		OutFailureReason = EventResult.DebugReason;
		OutResult.OutcomeType = TEXT("rejected");
		return false;
	}

	const FRouteSecuritySnapshot* Security = FindConst(InOutState.RouteSecuritySnapshots, [Encounter](const FRouteSecuritySnapshot& Candidate)
	{
		return Candidate.RouteSegmentId == Encounter->RouteSegmentId;
	});
	const FJurisdictionDefinition* Jurisdiction = FindConst(InOutState.Jurisdictions, [Security](const FJurisdictionDefinition& Candidate)
	{
		return Security && Candidate.JurisdictionId == Security->JurisdictionId;
	});
	FOffenseEvent Offense;
	Offense.OffenseId = MakeId(TEXT("offense"), Encounter->EncounterId);
	Offense.OffenderType = TEXT("ship");
	Offense.OffenderId = TEXT("pirate_raider_01");
	Offense.VictimType = TEXT("ship");
	Offense.VictimId = TEXT("trader_brink_01");
	Offense.JurisdictionId = Security ? Security->JurisdictionId : FName(TEXT("frontier_local_authority"));
	Offense.OffenseType = TEXT("pirate_attack");
	Offense.SourceEventId = Encounter->SourceEventId;
	Offense.OccurredTimeSeconds = AppliedTimeSeconds;

	FEvidenceRecord Evidence;
	FCriminalRecord CriminalRecord;
	if (!RecordOffense(InOutState, Offense, Evidence, CriminalRecord, OutFailureReason))
	{
		OutResult.OutcomeType = TEXT("rejected");
		return false;
	}

	const FName ResolutionOutcomeType = RuntimeOutcomeType.IsNone() ? FName(TEXT("distress_patrol_response")) : RuntimeOutcomeType;
	const bool bApplyPirateLootAndRansom = ResolutionOutcomeType != FName(TEXT("escape"));
	TArray<FName> CargoTransferResultIds;
	TArray<FName> GameplayTransactionIds;
	TArray<FName> CreditLedgerEntryIds;
	if (bApplyPirateLootAndRansom)
	{
		FCargoTransferRequest CargoRequest;
		CargoRequest.RequestId = MakeId(TEXT("cargo_loot"), Encounter->EncounterId);
		CargoRequest.SourceContainerId = TEXT("trader_brink_cargo");
		CargoRequest.DestinationContainerId = TEXT("pirate_raider_cargo");
		CargoRequest.ExpectedSourceOwnerId = TEXT("trader_brink_01");
		CargoRequest.ExpectedDestinationOwnerId = TEXT("pirate_raider_01");
		CargoRequest.ItemId = TEXT("item_ember_ore");
		CargoRequest.Quantity = 1;
		CargoRequest.Reason = TEXT("pirate_loot");
		CargoRequest.SourceEventId = Encounter->SourceEventId;
		CargoRequest.LegalContextId = Jurisdiction ? Jurisdiction->LawProfileId : FName();
		CargoRequest.IdempotencyKey = MakeId(TEXT("idem_m10_cargo_loot"), Encounter->EncounterId);
		FCargoTransferResult CargoResult;
		if (!TransferCargo(InOutState, CargoRequest, CargoResult))
		{
			OutFailureReason = CargoResult.RejectedReason;
			OutResult.OutcomeType = TEXT("rejected");
			return false;
		}
		CargoTransferResultIds.Add(CargoResult.TransferId);

		FCreditAccountRecord* TraderAccount = FindMutable(InOutState.CreditAccounts, [](const FCreditAccountRecord& Candidate)
		{
			return Candidate.AccountId == FName(TEXT("account_trader_brink_01"));
		});
		FCreditAccountRecord* PirateAccount = FindMutable(InOutState.CreditAccounts, [](const FCreditAccountRecord& Candidate)
		{
			return Candidate.AccountId == FName(TEXT("account_pirate_raider_01"));
		});
		if (!TraderAccount || !PirateAccount || TraderAccount->AvailableBalance < 25)
		{
			OutFailureReason = TEXT("Pirate encounter economy accounts do not resolve or cannot pay the abstract ransom.");
			OutResult.OutcomeType = TEXT("rejected");
			return false;
		}

		const FName EconomyTransactionId = MakeId(TEXT("tx_pirate_economy"), Encounter->EncounterId);
		const FName LedgerId = MakeId(TEXT("ledger_pirate_economy"), Encounter->EncounterId);
		const FName EconomyIdempotencyKey = MakeId(TEXT("idem_m10_economy"), Encounter->EncounterId);
		TraderAccount->AvailableBalance -= 25;
		PirateAccount->AvailableBalance += 25;
		FCreditLedgerEntry Ledger;
		Ledger.LedgerEntryId = LedgerId;
		Ledger.DebitAccountId = TraderAccount->AccountId;
		Ledger.CreditAccountId = PirateAccount->AccountId;
		Ledger.Amount = 25;
		Ledger.Reason = TEXT("pirate_ransom_abstract");
		Ledger.SourceEventId = Encounter->SourceEventId;
		Ledger.SourceTransactionId = EconomyTransactionId;
		Ledger.IdempotencyKey = MakeId(TEXT("ledger"), EconomyIdempotencyKey);
		Ledger.State = TEXT("applied");
		InOutState.CreditLedger.Add(Ledger);
		TraderAccount->LastLedgerEntryId = Ledger.LedgerEntryId;
		PirateAccount->LastLedgerEntryId = Ledger.LedgerEntryId;

		FGameplayTransactionRecord EconomyTransaction;
		EconomyTransaction.TransactionId = EconomyTransactionId;
		EconomyTransaction.TransactionType = TEXT("pirate_abstract_economy");
		EconomyTransaction.SourceEventId = Encounter->SourceEventId;
		EconomyTransaction.InitiatorType = TEXT("ship");
		EconomyTransaction.InitiatorId = TEXT("pirate_raider_01");
		EconomyTransaction.TargetType = TEXT("ship");
		EconomyTransaction.TargetId = TEXT("trader_brink_01");
		EconomyTransaction.State = TEXT("committed");
		EconomyTransaction.IdempotencyKey = EconomyIdempotencyKey;
		EconomyTransaction.DebugReason = TEXT("Pirate encounter cargo and ransom economy side effects committed without actors.");
		InOutState.Transactions.Add(EconomyTransaction);
		GameplayTransactionIds.Add(EconomyTransactionId);
		CreditLedgerEntryIds.Add(Ledger.LedgerEntryId);
		AddJournalEntry(InOutState, EconomyTransactionId, 1, TEXT("cargo_transfer"), CargoResult.TransferId, EconomyIdempotencyKey);
		AddJournalEntry(InOutState, EconomyTransactionId, 2, TEXT("ledger"), Ledger.LedgerEntryId, EconomyIdempotencyKey);
	}

	FAbstractEngagementRecord Engagement;
	Engagement.EngagementId = Encounter->EngagementId.IsNone() ? MakeId(TEXT("engagement"), Encounter->EncounterId) : Encounter->EngagementId;
	Engagement.SourceEventId = Encounter->SourceEventId;
	Engagement.EncounterId = Encounter->EncounterId;
	Engagement.AttackerId = Offense.OffenderId;
	Engagement.DefenderId = Offense.VictimId;
	Engagement.OutcomeType = ResolutionOutcomeType;
	Engagement.CargoTransferResultIds = CargoTransferResultIds;
	Engagement.GameplayTransactionIds = GameplayTransactionIds;
	Engagement.CreditLedgerEntryIds = CreditLedgerEntryIds;
	Engagement.OffenseIds = { Offense.OffenseId };
	Engagement.EvidenceIds = { Evidence.EvidenceId };
	Engagement.CriminalRecordIds = { CriminalRecord.CriminalRecordId };
	if (const FDistressEventRecord* Distress = FindConst(InOutState.DistressEvents, [Encounter](const FDistressEventRecord& Candidate)
	{
		return Candidate.DistressEventId == Encounter->DistressEventId;
	}))
	{
		Engagement.MessageInstanceIds = Distress->MessageInstanceIds;
	}
	Engagement.IdempotencyKey = MakeId(TEXT("idem_m10_engagement"), Encounter->EncounterId);
	InOutState.AbstractEngagements.Add(Engagement);

	Encounter->State = TEXT("resolved");
	Encounter->EngagementId = Engagement.EngagementId;
	Encounter->ProcessedWatermark += 1;
	if (FInterdictionHazardRecord* Hazard = FindMutable(InOutState.InterdictionHazards, [Encounter](const FInterdictionHazardRecord& Candidate)
	{
		return Candidate.HazardId == Encounter->InterdictionHazardId;
	}))
	{
		Hazard->State = TEXT("resolved");
	}
	if (FDistressEventRecord* Distress = FindMutable(InOutState.DistressEvents, [Encounter](const FDistressEventRecord& Candidate)
	{
		return Candidate.DistressEventId == Encounter->DistressEventId;
	}))
	{
		Distress->State = TEXT("responded");
		for (const FName ReservationId : Distress->RespondingPatrolReservationIds)
		{
			if (FPatrolReservationRecord* Reservation = FindMutable(InOutState.PatrolReservations, [ReservationId](const FPatrolReservationRecord& Candidate)
			{
				return Candidate.ReservationId == ReservationId;
			}))
			{
				Reservation->State = TEXT("consumed");
			}
		}
	}

	OutResult.ResultEventId = Encounter->SourceEventId;
	OutResult.OutcomeType = EventResult.OutcomeType == FName(TEXT("duplicate")) ? FName(TEXT("duplicate")) : FName(TEXT("resolved"));
	OutResult.bDuplicate = EventResult.OutcomeType == FName(TEXT("duplicate"));
	OutResult.DebugReason = bApplyPirateLootAndRansom
		? TEXT("Logical pirate encounter resolved through M9 event, legal, evidence, message, cargo, economy, and abstract engagement contracts without actors.")
		: TEXT("Logical pirate encounter resolved as runtime escape; legal, evidence, distress, and abstract engagement contracts were recorded without loot or ransom side effects.");
	OutFailureReason.Reset();
	return true;
}

FString USystemicGameplayQueryService::BuildSystemicDebugSummary(const FSystemicGameplayState& State)
{
	return FString::Printf(
		TEXT("Events=%d Results=%d Transactions=%d Journal=%d Factions=%d Jurisdictions=%d Offenses=%d Evidence=%d CriminalRecords=%d Containers=%d Markets=%d Ledger=%d Services=%d Messages=%d Missions=%d ServiceResults=%d Reputation=%d FollowUps=%d Progression=%d"),
		State.Events.Num(),
		State.EventResults.Num(),
		State.Transactions.Num(),
		State.TransactionJournal.Num(),
		State.Factions.Num(),
		State.Jurisdictions.Num(),
		State.Offenses.Num(),
		State.EvidenceRecords.Num(),
		State.CriminalRecords.Num(),
		State.Containers.Num(),
		State.Markets.Num(),
		State.CreditLedger.Num(),
		State.StationServiceEndpoints.Num(),
		State.MessageLog.Num(),
		State.MissionInstances.Num(),
		State.StationServiceResults.Num(),
		State.ReputationDeltas.Num(),
		State.FollowUpOpportunities.Num(),
		State.ProgressionDebugLedger.Num());
}

FString USystemicGameplayQueryService::BuildProgressionDebugTrace(const FSystemicGameplayState& State)
{
	TArray<FString> Lines;
	for (const FProgressionDebugLedgerEntry& Entry : State.ProgressionDebugLedger)
	{
		Lines.Add(FString::Printf(TEXT("%s:%s Subject=%s Faction=%s Mission=%s Ledger=%s Reputation=%s Service=%s FollowUp=%s Arbitration=%s Reason=%s"),
			*Entry.ProgressionEntryId.ToString(),
			*Entry.EntryType.ToString(),
			*Entry.SubjectId.ToString(),
			*Entry.FactionId.ToString(),
			*Entry.MissionInstanceId.ToString(),
			*Entry.CreditLedgerEntryId.ToString(),
			*Entry.ReputationDeltaId.ToString(),
			*Entry.ServiceResultId.ToString(),
			*Entry.FollowUpOpportunityId.ToString(),
			*Entry.MessageArbitrationId.ToString(),
			*Entry.DebugReason));
	}
	return FString::Join(Lines, TEXT("\n"));
}
