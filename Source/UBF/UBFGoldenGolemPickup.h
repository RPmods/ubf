#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UBFGoldenGolemPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;

UCLASS()
class UBF_API AUBFGoldenGolemPickup : public AActor
{
	GENERATED_BODY()

public:
	AUBFGoldenGolemPickup();

private:
	UFUNCTION()
	void OnPickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category="UBF|Golem Mode")
	TObjectPtr<USphereComponent> PickupTrigger;

	UPROPERTY(VisibleAnywhere, Category="UBF|Golem Mode")
	TObjectPtr<UStaticMeshComponent> OrbMesh;

	UPROPERTY(VisibleAnywhere, Category="UBF|Golem Mode")
	TObjectPtr<UPointLightComponent> GoldLight;
};
