// Copyright Epic Games, Inc. All Rights Reserved.


#include "TitanWaterDetectionComponent.h"
#include "WaterBodyActor.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "TitanRaftLogging.h"

UTitanWaterDetectionComponent::UTitanWaterDetectionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTitanWaterDetectionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* ActorOwner = GetOwner())
	{
		MonitoringPrimitive = Cast<UPrimitiveComponent>(ActorOwner->GetRootComponent());

		if (MonitoringPrimitive)
		{
			MonitoringPrimitive->OnComponentBeginOverlap.AddDynamic(this, &UTitanWaterDetectionComponent::OnBeginOverlap);
			MonitoringPrimitive->OnComponentEndOverlap.AddDynamic(this, &UTitanWaterDetectionComponent::OnEndOverlap);
		}
	}
	
}

void UTitanWaterDetectionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (MonitoringPrimitive)
	{
		if (bUseWaterProbe)
		{
			// update the water probe
			UpdateWaterProbe();
		}

		if (bUseGroundProbe)
		{
			// update the ground probe
			UpdateGroundProbe();
		}
	}
}

bool UTitanWaterDetectionComponent::IsOverlappingWater() const
{
	return OverlappingWaterBodies.Num() > 0;
}

bool UTitanWaterDetectionComponent::IsAboveWater() const
{
	return bIsAboveWater;
}

bool UTitanWaterDetectionComponent::IsSubmerged() const
{
	return bIsSubmerged;
}

bool UTitanWaterDetectionComponent::IsOnGround() const
{
	// return false if we're overlapping any water bodies
	return bIsOnGround && OverlappingWaterBodies.Num() < 1;
}

bool UTitanWaterDetectionComponent::HasGroundTrace() const
{
	return bIsOnGround;
}

float UTitanWaterDetectionComponent::GetImmersionDepth() const
{
	return LastImmersionDepth;
}

float UTitanWaterDetectionComponent::GetWaterDepth() const
{
	return LastWaterDepth;
}

FVector UTitanWaterDetectionComponent::GetWaterSurfaceAtLocation(const FVector& QueryLocation) const
{
	EWaterBodyQueryFlags QueryFlags = EWaterBodyQueryFlags::ComputeLocation;

	float ClosestDist = 100000000000000.0f;
	FVector OutLoc = QueryLocation;

	for (AWaterBody* CurrentWaterBody : OverlappingWaterBodies)
	{
		FWaterBodyQueryResult QueryResult = CurrentWaterBody->GetWaterBodyComponent()->QueryWaterInfoClosestToWorldLocation(QueryLocation, QueryFlags);

		FVector SurfaceLocation = QueryResult.GetWaterPlaneLocation();

		float CurrentDist = (QueryLocation - SurfaceLocation).Size();

		if (CurrentDist < ClosestDist)
		{
			OutLoc = SurfaceLocation;
		}
	}

	return OutLoc;
}

void UTitanWaterDetectionComponent::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (AWaterBody* WaterOverlap = Cast<AWaterBody>(OtherActor))
	{
		uint32 PreviousOverlaps = OverlappingWaterBodies.Num();

		// add the overlapping water body
		OverlappingWaterBodies.AddUnique(WaterOverlap);

		// is this our first overlap?
		if (PreviousOverlaps < 1 && OverlappingWaterBodies.Num() > 0)
		{
			// send the water begin overlap event
			if (WaterBeginOverlapEvent != FGameplayTag::EmptyTag)
			{
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), WaterBeginOverlapEvent, FGameplayEventData());
			}
		}
	}
}

void UTitanWaterDetectionComponent::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (AWaterBody* WaterOverlap = Cast<AWaterBody>(OtherActor))
	{
		uint32 PreviousOverlaps = OverlappingWaterBodies.Num();

		OverlappingWaterBodies.Remove(WaterOverlap);

		// are we no longer overlapping water?
		if (PreviousOverlaps > 0 && OverlappingWaterBodies.Num() < 1)
		{
			// send the water end overlap event
			if (WaterEndOverlapEvent != FGameplayTag::EmptyTag)
			{
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), WaterEndOverlapEvent, FGameplayEventData());
			}
		}
	}
}

void UTitanWaterDetectionComponent::UpdateWaterProbe()
{
	// assume we're not submerged or above water by default
	bIsSubmerged = false;
	bIsAboveWater = false;

	if (OverlappingWaterBodies.Num() <= 0)
	{
		TArray<AActor*> Overlaps;

		MonitoringPrimitive->GetOverlappingActors(Overlaps, AWaterBody::StaticClass());

		if (Overlaps.Num() > 0)
		{
			UE_LOG(LogTitanWaterDetection, Warning, TEXT("Detected Overlapping Water Bodies without a BeginOverlap event."));

			for (AActor* CurrentActor : Overlaps)
			{
				UE_LOG(LogTitanWaterDetection, Warning, TEXT("Actor: [%s]"), *CurrentActor->GetHumanReadableName());

				OverlappingWaterBodies.AddUnique(Cast<AWaterBody>(CurrentActor));
			}

			// send the water begin overlap event
			if (WaterBeginOverlapEvent != FGameplayTag::EmptyTag)
			{
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), WaterBeginOverlapEvent, FGameplayEventData());
			}
		}
	}

	// are we overlapping any water bodies?
	if (OverlappingWaterBodies.Num() > 0)
	{
		// query each overlapping body for the water depth and immersion depth at the actor's location
		float LowestImmersion = -100000.0f;
		float LowestDepth = -100000.0f;

		FVector QueryLocation = GetOwner()->GetActorLocation();
		EWaterBodyQueryFlags QueryFlags = EWaterBodyQueryFlags::ComputeImmersionDepth | EWaterBodyQueryFlags::ComputeDepth | EWaterBodyQueryFlags::IncludeWaves;

		for (AWaterBody* CurrentWaterBody : OverlappingWaterBodies)
		{
			FWaterBodyQueryResult QueryResult = CurrentWaterBody->GetWaterBodyComponent()->QueryWaterInfoClosestToWorldLocation(QueryLocation, QueryFlags);

			// get the immersion depth
			float CurrentImmersion = QueryResult.GetImmersionDepth();

			// is this lower than our last result?
			if (CurrentImmersion > LowestImmersion)
			{
				LowestImmersion = CurrentImmersion;
			}

			// get the water depth
			float CurrentDepth = QueryResult.GetWaterPlaneDepth();

			// is this lower than our last result?
			if (CurrentDepth > LowestDepth)
			{
				LowestDepth = CurrentDepth;
			}
		}

		// are we below the water surface?
		if (LowestImmersion > ImmersionDepthOffset)
		{
			bIsSubmerged = true;

			// were we below water surface last frame?
			if (LastImmersionDepth <= ImmersionDepthOffset)
			{
				// send the immersion event
				if (ImmersionEvent != FGameplayTag::EmptyTag)
				{
					UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), ImmersionEvent, FGameplayEventData());
				}
			}
		}

		// save the immersion depth
		LastImmersionDepth = LowestImmersion;
		LastWaterDepth = LowestDepth;

	// we may be hovering over water, so run the collision check
	} else {

		// set the query params
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(GetOwner());

		FCollisionResponseParams ResponseParams;
		MonitoringPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

		TArray<FHitResult> HitResults;

		const FVector Start = MonitoringPrimitive->GetComponentLocation();
		const FVector End = Start + FVector::DownVector * WaterProbeLength;

		// look for overlaps with water bodies underneath us
		GetWorld()->SweepMultiByChannel(HitResults, Start, End, FQuat::Identity, WaterProbeChannel, FCollisionShape::MakeSphere(WaterProbeRadius), QueryParams, ResponseParams);

		for (const FHitResult& CurrentHit : HitResults)
		{
			// have we overlapped a water body?
			if (AWaterBody* WaterBody = Cast<AWaterBody>(CurrentHit.GetActor()))
			{
				// query the water body at the location to get the depth values
				FVector QueryLocation = CurrentHit.Location;
				EWaterBodyQueryFlags QueryFlags = EWaterBodyQueryFlags::ComputeImmersionDepth | EWaterBodyQueryFlags::ComputeDepth | EWaterBodyQueryFlags::IncludeWaves;

				FWaterBodyQueryResult QueryResult = WaterBody->GetWaterBodyComponent()->QueryWaterInfoClosestToWorldLocation(QueryLocation, QueryFlags);

				LastImmersionDepth = QueryResult.GetImmersionDepth();
				LastWaterDepth = QueryResult.GetWaterSurfaceDepth();

				// set the flag and stop
				bIsAboveWater = true;
				break;
			}
		}
	}
}

void UTitanWaterDetectionComponent::UpdateGroundProbe()
{
	// set the query params
	FCollisionQueryParams QueryParams;

	FCollisionResponseParams ResponseParams;
	MonitoringPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.bReturnPhysicalMaterial = true;

	const ECollisionChannel GroundProbeChannel = MonitoringPrimitive->GetCollisionObjectType();

	const FVector Start = MonitoringPrimitive->GetComponentLocation();
	const FVector End = Start + FVector::DownVector * GroundProbeLength;

	bool bWasOnGround = bIsOnGround;

	// run a downwards sweep 
	bIsOnGround = GetWorld()->SweepSingleByChannel(LastGroundHit, Start, End, FQuat::Identity, GroundProbeChannel, FCollisionShape::MakeSphere(GroundProbeRadius), QueryParams, ResponseParams);
	
	//FVector TraceEnd = bIsOnGround ? HitResult.ImpactPoint : End;
	//DrawDebugLine(GetWorld(), Start, TraceEnd, bIsOnGround ? FColor::Green : FColor::Red, false, 0.0f, 20, 3.0f);

	// did the sweep hit something?
	if (bIsOnGround)
	{
		if (!bWasOnGround)
		{
			// send the ground contact event
			if (GroundContactEvent != FGameplayTag::EmptyTag)
			{
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), GroundContactEvent, FGameplayEventData());
			}
		}
	}

}

bool UTitanWaterDetectionComponent::GetGroundSurfaceType(TEnumAsByte<EPhysicalSurface>& SurfaceType) const
{
	if (LastGroundHit.PhysMaterial.IsValid())
	{
		SurfaceType = LastGroundHit.PhysMaterial->SurfaceType;
		return true;
	}

	return false;
}

const FVector& UTitanWaterDetectionComponent::GetGroundImpactPoint() const
{
	return LastGroundHit.ImpactPoint;
}

const FVector& UTitanWaterDetectionComponent::GetGroundNormal() const
{
	return LastGroundHit.ImpactNormal;
}
