#include "UBFGoldenGolemPickup.h"

#include "UBFCombatCharacter.h"
#include "UBFCombatGameMode.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"

AUBFGoldenGolemPickup::AUBFGoldenGolemPickup()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = false;

	PickupTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("PickupTrigger"));
	SetRootComponent(PickupTrigger);
	PickupTrigger->InitSphereRadius(86.0f);
	PickupTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	PickupTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupTrigger->SetGenerateOverlapEvents(true);
	PickupTrigger->OnComponentBeginOverlap.AddDynamic(this, &AUBFGoldenGolemPickup::OnPickupOverlap);

	OrbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbMesh"));
	OrbMesh->SetupAttachment(PickupTrigger);
	OrbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OrbMesh->SetRelativeScale3D(FVector(0.56f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		OrbMesh->SetStaticMesh(SphereMesh.Object);
	}

	GoldLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GoldLight"));
	GoldLight->SetupAttachment(PickupTrigger);
	GoldLight->SetLightColor(FLinearColor(1.0f, 0.66f, 0.08f));
	GoldLight->SetIntensity(650.0f);
	GoldLight->SetAttenuationRadius(420.0f);
	GoldLight->SetCastShadows(false);
}

void AUBFGoldenGolemPickup::OnPickupOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!HasAuthority())
	{
		return;
	}
	AUBFCombatCharacter* Fighter = Cast<AUBFCombatCharacter>(OtherActor);
	AUBFCombatGameMode* MatchMode = GetWorld()
		? GetWorld()->GetAuthGameMode<AUBFCombatGameMode>() : nullptr;
	if (!Fighter || Fighter->IsMasterGolem() || Fighter->IsGoldenBearer()
		|| Fighter->GetCurrentHealth() <= 0.0f || !MatchMode || !MatchMode->IsGolemMode())
	{
		return;
	}
	MatchMode->NotifyGoldenBearerPickedUp(Fighter, this);
}
