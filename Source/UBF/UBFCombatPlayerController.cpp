#include "UBFCombatPlayerController.h"

#include "UBFCombatGameMode.h"
#include "UBFCombatGameState.h"
#include "UBFCombatCharacter.h"
#include "UBFCombatHUD.h"
#include "InputCoreTypes.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

void AUBFCombatPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AUBFCombatPlayerController::HandleReadyPressed);
		InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed,
			this, &AUBFCombatPlayerController::HandleReadyPressed);
		InputComponent->BindKey(EKeys::Escape, IE_Pressed,
			this, &AUBFCombatPlayerController::ToggleCombatMenu);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed,
			this, &AUBFCombatPlayerController::HandleCombatMenuClick);
	}
}

bool AUBFCombatPlayerController::CanSurrender() const
{
	const AUBFCombatGameState* State = GetWorld() ? GetWorld()->GetGameState<AUBFCombatGameState>() : nullptr;
	return State && State->GetRoundNumber() >= 3 && State->GetRoundPhase() == EUBFMatchPhase::InRound;
}

void AUBFCombatPlayerController::ToggleCombatMenu()
{
	if (bCombatMenuOpen)
	{
		CloseCombatMenu();
		return;
	}
	bCombatMenuOpen = true;
	SetShowMouseCursor(true);
	SetInputMode(FInputModeGameAndUI());
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
}

void AUBFCombatPlayerController::CloseCombatMenu()
{
	bCombatMenuOpen = false;
	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
}

void AUBFCombatPlayerController::HandleCombatMenuClick()
{
	if (!bCombatMenuOpen)
	{
		return;
	}
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (GetMousePosition(MouseX, MouseY))
	{
		if (AUBFCombatHUD* CombatHUD = Cast<AUBFCombatHUD>(GetHUD()))
		{
			CombatHUD->HandleCombatMenuClick(FVector2D(MouseX, MouseY));
		}
	}
}

void AUBFCombatPlayerController::RequestSurrender()
{
	if (CanSurrender())
	{
		CloseCombatMenu();
		ServerRequestSurrender();
	}
}

void AUBFCombatPlayerController::RequestAbandonRoom()
{
	CloseCombatMenu();
	ServerRequestAbandonRoom();
}

void AUBFCombatPlayerController::HandleReadyPressed()
{
	const AUBFCombatGameState* MatchState = GetWorld()
		? GetWorld()->GetGameState<AUBFCombatGameState>() : nullptr;
	if (MatchState && MatchState->GetRoundPhase() == EUBFMatchPhase::LoadingSync && !bReadyForRound)
	{
		ServerMarkReadyForRound();
	}
}

void AUBFCombatPlayerController::ServerMarkReadyForRound_Implementation()
{
	const AUBFCombatGameState* MatchState = GetWorld()
		? GetWorld()->GetGameState<AUBFCombatGameState>() : nullptr;
	if (!MatchState || MatchState->GetRoundPhase() != EUBFMatchPhase::LoadingSync || bReadyForRound)
	{
		return;
	}

	bReadyForRound = true;
	ForceNetUpdate();
	if (AUBFCombatGameMode* MatchMode = GetWorld()->GetAuthGameMode<AUBFCombatGameMode>())
	{
		MatchMode->NotifyPlayerReadyChanged();
	}
}

void AUBFCombatPlayerController::ServerRequestSurrender_Implementation()
{
	if (AUBFCombatGameMode* MatchMode = GetWorld()
		? GetWorld()->GetAuthGameMode<AUBFCombatGameMode>() : nullptr)
	{
		MatchMode->HandleSurrender(this);
	}
}

void AUBFCombatPlayerController::ServerRequestAbandonRoom_Implementation()
{
	if (AUBFCombatGameMode* MatchMode = GetWorld()
		? GetWorld()->GetAuthGameMode<AUBFCombatGameMode>() : nullptr)
	{
		MatchMode->HandleAbandonRoom(this);
	}
	CloseCombatMenu();
}

void AUBFCombatPlayerController::ResetReadyStateForNextRound()
{
	if (HasAuthority())
	{
		bReadyForRound = false;
		ForceNetUpdate();
	}
}

void AUBFCombatPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUBFCombatPlayerController, bReadyForRound);
}
