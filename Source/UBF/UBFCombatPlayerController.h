#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UBFCombatPlayerController.generated.h"

UCLASS()
class UBF_API AUBFCombatPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	bool IsReadyForRound() const { return bReadyForRound; }
	void ResetReadyStateForNextRound();
	bool IsCombatMenuOpen() const { return bCombatMenuOpen; }
	bool CanSurrender() const;
	void CloseCombatMenu();
	void RequestSurrender();
	void RequestAbandonRoom();

private:
	void HandleReadyPressed();
	void ToggleCombatMenu();
	void HandleCombatMenuClick();

	UFUNCTION(Server, Reliable)
	void ServerMarkReadyForRound();

	UFUNCTION(Server, Reliable)
	void ServerRequestSurrender();

	UFUNCTION(Server, Reliable)
	void ServerRequestAbandonRoom();

	UPROPERTY(Replicated)
	bool bReadyForRound = false;
	bool bCombatMenuOpen = false;
};
