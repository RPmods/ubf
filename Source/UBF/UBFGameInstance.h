#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "UBFGameInstance.generated.h"

UCLASS()
class UBF_API UUBFGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UFUNCTION(BlueprintPure, Category="UBF|Session")
	FString GetSessionUserName() const { return SessionUserName; }

private:
	UPROPERTY(BlueprintReadOnly, Category="UBF|Session", meta=(AllowPrivateAccess="true"))
	FString SessionUserName;
};
