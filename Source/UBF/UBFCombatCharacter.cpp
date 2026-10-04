#include "UBFCombatCharacter.h"
#include "UBFCombatGameMode.h"
#include "UBFCharacterDefinition.h"
#include "UBFPlayerData.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace UBFCombat
{
	const TCHAR* MeshPath = TEXT("/Game/Characters/AnimationLibraries/UAL1/UAL1_Standard.UAL1_Standard");
	const TCHAR* IdlePath = TEXT("/Game/Characters/AnimationLibraries/UAL1/UAL1_StandardIdle_Loop.UAL1_StandardIdle_Loop");
	const TCHAR* WalkPath = TEXT("/Game/Characters/AnimationLibraries/UAL1/UAL1_StandardWalk_Loop.UAL1_StandardWalk_Loop");
	const TCHAR* AttackPath = TEXT("/Game/Characters/AnimationLibraries/UAL1/UAL1_StandardPunch_Jab.UAL1_StandardPunch_Jab");
	const TCHAR* CrossAttackPath = TEXT("/Game/Characters/AnimationLibraries/UAL1/UAL1_StandardPunch_Cross.UAL1_StandardPunch_Cross");
	const TCHAR* HookAttackPath = TEXT("/Game/Characters/AnimationLibraries/UAL2/UAL2_StandardMelee_Hook.UAL2_StandardMelee_Hook");
	const TCHAR* ChargedAttackPath = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack.MM_ChargedAttack");
	const TCHAR* HitPath = TEXT("/Game/Characters/AnimationLibraries/UAL1/UAL1_StandardHit_Chest.UAL1_StandardHit_Chest");
}

AUBFCombatCharacter::AUBFCombatCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCapsuleComponent()->InitCapsuleSize(38.0f, 94.0f);
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = 520.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1800.0f;
	GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 420.0f;
	CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 62.0f));
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.0f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> FighterMesh(UBFCombat::MeshPath);
	if (FighterMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(FighterMesh.Object);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -92.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(UBFCombat::IdlePath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(UBFCombat::WalkPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack(UBFCombat::AttackPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> CrossAttack(UBFCombat::CrossAttackPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> HookAttack(UBFCombat::HookAttackPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> ChargedAttack(UBFCombat::ChargedAttackPath);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Hit(UBFCombat::HitPath);
	if (Idle.Succeeded()) IdleAnimation = Idle.Object;
	if (Walk.Succeeded()) WalkAnimation = Walk.Object;
	if (Attack.Succeeded()) AttackAnimation = Attack.Object;
	if (CrossAttack.Succeeded()) CrossAttackAnimation = CrossAttack.Object;
	if (HookAttack.Succeeded()) HookAttackAnimation = HookAttack.Object;
	if (ChargedAttack.Succeeded()) ChargedAttackAnimation = ChargedAttack.Object;
	if (Hit.Succeeded()) HitAnimation = Hit.Object;
}

void AUBFCombatCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyCharacterDefinition();
	CurrentHealth = CharacterDefinition ? MaximumHealth : FMath::Clamp(CurrentHealth, 0.0f, MaximumHealth);
	UpdateLocomotionAnimation();
}

void AUBFCombatCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveRuntimeInputMapping();
	Super::EndPlay(EndPlayReason);
}

void AUBFCombatCharacter::UnPossessed()
{
	RemoveRuntimeInputMapping();
	Super::UnPossessed();
}

void AUBFCombatCharacter::RemoveRuntimeInputMapping()
{
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
				LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (RuntimeInputMappingContext)
				{
					InputSubsystem->RemoveMappingContext(RuntimeInputMappingContext);
				}
			}
		}
	}
	RuntimeInputMappingContext = nullptr;
	RuntimeInputActions.Reset();
}

void AUBFCombatCharacter::ApplyCharacterDefinition()
{
	if (!CharacterDefinition && GetGameInstance())
	{
		if (UUBFCharacterCatalogSubsystem* Catalog = GetGameInstance()->GetSubsystem<UUBFCharacterCatalogSubsystem>())
		{
			CharacterDefinition = Catalog->FindCharacter(CharacterId);
		}
	}
	if (!CharacterDefinition)
	{
		return;
	}

	CharacterId = CharacterDefinition->CharacterID.IsNone()
		? FName(TEXT("Wizz")) : CharacterDefinition->CharacterID;
	const FUBFCharacterCombatStats& Stats = CharacterDefinition->BaseStats;
	MaximumHealth = FMath::Max(1.0f, Stats.MaximumHealth);
	BasicAttackDamage = FMath::Max(0.0f, Stats.BasicAttackDamage);
	BasicAttackRange = FMath::Max(0.0f, Stats.BasicAttackRange);
	BasicAttackRadius = FMath::Max(0.0f, Stats.BasicAttackRadius);
	BasicAttackCooldown = FMath::Max(0.0f, Stats.BasicAttackCooldown);
	ChargedSpecialMaximumChargeTime = FMath::Max(0.1f, Stats.ChargedMaximumChargeTime);
	ChargedSpecialMaximumRange = FMath::Max(0.0f, Stats.ChargedMaximumRange);
	ChargedSpecialBaseDamage = FMath::Max(0.0f, Stats.ChargedBaseDamage);
	ChargedSpecialFullChargeBonusDamage = FMath::Max(0.0f, Stats.ChargedFullChargeBonusDamage);
	ChargedSpecialHitRadius = FMath::Max(0.0f, Stats.ChargedHitRadius);
	DashSpeed = FMath::Max(0.0f, Stats.DashSpeed);
	DashCooldown = FMath::Max(0.0f, Stats.DashCooldown);
	GrabRange = FMath::Max(0.0f, Stats.GrabRange);
	GrabDamage = FMath::Max(0.0f, Stats.GrabDamage);
	GrabStartupDuration = FMath::Max(0.0f, Stats.GrabStartupDuration);
	GrabRecoveryDuration = FMath::Max(0.1f, Stats.GrabRecoveryDuration);

	const FUBFSkillGaugeSettings& Gauge = CharacterDefinition->GaugeSettings;
	SkillGaugeMaximum = FMath::Max(1.0f, Gauge.Maximum);
	GaugeBasicHitReward = FMath::Max(0.0f, Gauge.BasicHitReward);
	GaugeComboHitReward = FMath::Max(0.0f, Gauge.ComboHitReward);
	GaugeChargedHitReward = FMath::Max(0.0f, Gauge.ChargedHitReward);
	GaugeReceiveHitReward = FMath::Max(0.0f, Gauge.ReceiveHitReward);
	GaugeReceiveHeavyReward = FMath::Max(0.0f, Gauge.ReceiveHeavyHitReward);

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = FMath::Max(0.0f, Stats.MaxWalkSpeed);
		Movement->JumpZVelocity = FMath::Max(0.0f, Stats.JumpZVelocity);
	}
}

void AUBFCombatCharacter::SetCharacterId(FName NewCharacterId)
{
	if (!HasAuthority() || NewCharacterId.IsNone() || GetWorld() == nullptr)
	{
		return;
	}

	UUBFCharacterCatalogSubsystem* Catalog = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UUBFCharacterCatalogSubsystem>() : nullptr;
	UUBFCharacterDefinition* NewDefinition = Catalog ? Catalog->FindCharacter(NewCharacterId) : nullptr;
	if (!NewDefinition)
	{
		return;
	}
	CharacterId = NewCharacterId;
	CharacterDefinition = NewDefinition;
	ApplyCharacterDefinition();
	CurrentHealth = MaximumHealth;
	CurrentSkillGauge = 0.0f;
	ForceNetUpdate();
}

void AUBFCombatCharacter::SetGoldenBearer(bool bNewValue)
{
	if (HasAuthority())
	{
		bIsGoldenBearer = bNewValue && !bIsMasterGolem;
		ForceNetUpdate();
	}
}

void AUBFCombatCharacter::SetAsMasterGolem()
{
	if (!HasAuthority())
	{
		return;
	}
	bIsMasterGolem = true;
	bIsGoldenBearer = false;
	CharacterDefinition = nullptr;
	CharacterId = TEXT("MasterGolem");
	MaximumHealth = 900.0f;
	CurrentHealth = MaximumHealth;
	CurrentSkillGauge = 0.0f;
	GetCapsuleComponent()->SetCapsuleSize(112.0f, 190.0f);
	if (GetMesh())
	{
		GetMesh()->SetRelativeScale3D(FVector(1.8f));
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	ForceNetUpdate();
}

FText AUBFCombatCharacter::GetCharacterDisplayName() const
{
	return CharacterDefinition && !CharacterDefinition->DisplayName.IsEmpty()
		? CharacterDefinition->DisplayName : FText::FromName(CharacterId);
}

const UUBFCharacterAIProfile* AUBFCombatCharacter::GetCharacterAIProfile() const
{
	return CharacterDefinition ? CharacterDefinition->AIProfile.Get() : nullptr;
}

const UUBFAbilityDefinition* AUBFCombatCharacter::GetAbilityDefinition(uint8 AbilitySlot) const
{
	if (!CharacterDefinition)
	{
		return nullptr;
	}

	switch (AbilitySlot)
	{
	case 0: return CharacterDefinition->PrimarySkill;
	case 1: return CharacterDefinition->SecondarySkill;
	case 2: return CharacterDefinition->Ultimate;
	default: return nullptr;
	}
}

float AUBFCombatCharacter::GetAbilityMaximumRange(uint8 AbilitySlot) const
{
	if (const UUBFAbilityDefinition* Ability = GetAbilityDefinition(AbilitySlot))
	{
		return Ability->Range;
	}
	return AbilitySlot == 0 ? AbilityQMaximumRange : (AbilitySlot == 1 ? AbilityEMaximumRange : UltimateMaximumRange);
}

float AUBFCombatCharacter::GetAbilityGaugeCost(uint8 AbilitySlot) const
{
	if (const UUBFAbilityDefinition* Ability = GetAbilityDefinition(AbilitySlot))
	{
		return Ability->GaugeCost;
	}
	return AbilitySlot == 0 ? AbilityQCost : (AbilitySlot == 1 ? AbilityECost : UltimateCost);
}

float AUBFCombatCharacter::GetAbilityCooldownRemaining(uint8 AbilitySlot) const
{
	if (AbilitySlot > 2 || !GetWorld())
	{
		return 0.0f;
	}
	return FMath::Max(0.0f, NextAbilityTimes[AbilitySlot] - GetWorld()->GetTimeSeconds());
}

float AUBFCombatCharacter::GetTimeSinceBasicAttack() const
{
	return GetWorld() ? FMath::Max(0.0f, GetWorld()->GetTimeSeconds() - LastBasicAttackTime)
		: TNumericLimits<float>::Max();
}

float AUBFCombatCharacter::GetAbilityCooldownRatio(uint8 AbilitySlot) const
{
	if (AbilitySlot > 2 || AbilityCooldownDurations[AbilitySlot] <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}
	return FMath::Clamp(GetAbilityCooldownRemaining(AbilitySlot) / AbilityCooldownDurations[AbilitySlot], 0.0f, 1.0f);
}

FText AUBFCombatCharacter::GetAbilityDisplayName(uint8 AbilitySlot) const
{
	if (const UUBFAbilityDefinition* Ability = GetAbilityDefinition(AbilitySlot))
	{
		if (!Ability->DisplayName.IsEmpty())
		{
			return Ability->DisplayName;
		}
	}
	return FText::FromString(AbilitySlot == 0 ? TEXT("Q") : (AbilitySlot == 1 ? TEXT("E") : TEXT("ULT")));
}

float AUBFCombatCharacter::GetOutgoingPassiveDamageMultiplier() const
{
	if (CharacterDefinition
		&& CharacterDefinition->PassiveMechanic == EUBFPassiveMechanic::LowHealthDamageBonus
		&& GetHealthRatio() <= 0.35f)
	{
		return 1.0f + FMath::Max(0.0f, CharacterDefinition->PassiveMagnitude);
	}
	return 1.0f;
}

void AUBFCombatCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ActionAnimationRemaining > 0.0f)
	{
		ActionAnimationRemaining = FMath::Max(0.0f, ActionAnimationRemaining - DeltaSeconds);
		if (ActionAnimationRemaining <= 0.0f)
		{
			bHasLocomotionAnimationStarted = false;
			UpdateLocomotionAnimation();
		}
	}
	else
	{
		UpdateLocomotionAnimation();
	}

}

void AUBFCombatCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		RuntimeInputMappingContext = NewObject<UInputMappingContext>(this, TEXT("UBFRuntimeInputContext"));
		RuntimeInputActions.Reset();
		UInputAction* MoveForwardAction = CreateRuntimeInputAction(TEXT("MoveForward"), 1);
		UInputAction* MoveRightAction = CreateRuntimeInputAction(TEXT("MoveRight"), 1);
		UInputAction* TurnAction = CreateRuntimeInputAction(TEXT("Turn"), 1);
		UInputAction* LookUpAction = CreateRuntimeInputAction(TEXT("LookUp"), 1);
		UInputAction* JumpAction = CreateRuntimeInputAction(TEXT("Jump"), 0);
		UInputAction* AttackAction = CreateRuntimeInputAction(TEXT("Attack"), 0);
		UInputAction* PrimaryAction = CreateRuntimeInputAction(TEXT("PrimarySkill"), 0);
		UInputAction* SecondaryAction = CreateRuntimeInputAction(TEXT("SecondarySkill"), 0);
		UInputAction* UltimateAction = CreateRuntimeInputAction(TEXT("Ultimate"), 0);
		UInputAction* DashAction = CreateRuntimeInputAction(TEXT("Dash"), 0);
		UInputAction* ChargedAttackAction = CreateRuntimeInputAction(TEXT("ChargedAttack"), 0);
		UInputAction* ReturnAction = CreateRuntimeInputAction(TEXT("ReturnToMenu"), 0);

		AddRuntimeInputMapping(MoveForwardAction, EKeys::W);
		AddRuntimeInputMapping(MoveForwardAction, EKeys::S, true);
		AddRuntimeInputMapping(MoveRightAction, EKeys::D);
		AddRuntimeInputMapping(MoveRightAction, EKeys::A, true);
		AddRuntimeInputMapping(TurnAction, EKeys::MouseX);
		AddRuntimeInputMapping(LookUpAction, EKeys::MouseY, true);
		AddRuntimeInputMapping(JumpAction, EKeys::SpaceBar);
		AddRuntimeInputMapping(AttackAction, EKeys::LeftMouseButton);
		AddRuntimeInputMapping(ChargedAttackAction, EKeys::RightMouseButton);
		AddRuntimeInputMapping(DashAction, EKeys::LeftControl);
		AddRuntimeInputMapping(ReturnAction, EKeys::Escape);

		UUBFPlayerDataSubsystem* PlayerData = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr;
		AddRuntimeInputMapping(PrimaryAction, PlayerData ? PlayerData->GetInputBinding(TEXT("PrimarySkill")) : EKeys::Q);
		AddRuntimeInputMapping(SecondaryAction, PlayerData ? PlayerData->GetInputBinding(TEXT("SecondarySkill")) : EKeys::E);
		AddRuntimeInputMapping(UltimateAction, PlayerData ? PlayerData->GetInputBinding(TEXT("Ultimate")) : EKeys::F);

		EnhancedInput->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &AUBFCombatCharacter::EnhancedMoveForward);
		EnhancedInput->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &AUBFCombatCharacter::EnhancedMoveRight);
		EnhancedInput->BindAction(TurnAction, ETriggerEvent::Triggered, this, &AUBFCombatCharacter::EnhancedTurn);
		EnhancedInput->BindAction(LookUpAction, ETriggerEvent::Triggered, this, &AUBFCombatCharacter::EnhancedLookUp);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AUBFCombatCharacter::EnhancedJumpStarted);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &AUBFCombatCharacter::EnhancedJumpCompleted);
		EnhancedInput->BindAction(AttackAction, ETriggerEvent::Started, this, &AUBFCombatCharacter::EnhancedBasicAttackStarted);
		EnhancedInput->BindAction(AttackAction, ETriggerEvent::Completed, this, &AUBFCombatCharacter::EnhancedBasicAttackCompleted);
		EnhancedInput->BindAction(PrimaryAction, ETriggerEvent::Started, this, &AUBFCombatCharacter::EnhancedAbilityQ);
		EnhancedInput->BindAction(SecondaryAction, ETriggerEvent::Started, this, &AUBFCombatCharacter::EnhancedAbilityE);
		EnhancedInput->BindAction(UltimateAction, ETriggerEvent::Started, this, &AUBFCombatCharacter::EnhancedUltimate);
		EnhancedInput->BindAction(DashAction, ETriggerEvent::Started, this, &AUBFCombatCharacter::EnhancedDash);
		EnhancedInput->BindAction(ChargedAttackAction, ETriggerEvent::Started, this, &AUBFCombatCharacter::EnhancedChargedStarted);
		EnhancedInput->BindAction(ChargedAttackAction, ETriggerEvent::Completed, this, &AUBFCombatCharacter::EnhancedChargedCompleted);
		EnhancedInput->BindAction(ReturnAction, ETriggerEvent::Started, this, &AUBFCombatCharacter::EnhancedReturnToMenu);

		if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
					LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
				{
					InputSubsystem->AddMappingContext(RuntimeInputMappingContext, 0);
					return;
				}
			}
		}
	}

	// Keep the existing project input settings as a compatibility path for input components
	// created outside Enhanced Input (for example, editor-authored test controllers).
	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AUBFCombatCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AUBFCombatCharacter::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction(TEXT("BasicAttack"), IE_Pressed, this, &AUBFCombatCharacter::HandleBasicAttackPressed);
	PlayerInputComponent->BindAction(TEXT("BasicAttack"), IE_Released, this, &AUBFCombatCharacter::HandleBasicAttackReleased);
	PlayerInputComponent->BindAction(TEXT("AbilityQ"), IE_Pressed, this, &AUBFCombatCharacter::RequestAbilityQ);
	PlayerInputComponent->BindAction(TEXT("AbilityE"), IE_Pressed, this, &AUBFCombatCharacter::RequestAbilityE);
	PlayerInputComponent->BindAction(TEXT("Ultimate"), IE_Pressed, this, &AUBFCombatCharacter::RequestUltimate);
	PlayerInputComponent->BindAction(TEXT("Dash"), IE_Pressed, this, &AUBFCombatCharacter::RequestDash);
	PlayerInputComponent->BindAction(TEXT("ChargedSpecial"), IE_Pressed, this, &AUBFCombatCharacter::HandleChargedSpecialPressed);
	PlayerInputComponent->BindAction(TEXT("ChargedSpecial"), IE_Released, this, &AUBFCombatCharacter::HandleChargedSpecialReleased);
	PlayerInputComponent->BindAction(TEXT("ReturnToMenu"), IE_Pressed, this, &AUBFCombatCharacter::ReturnToMenu);
}

UInputAction* AUBFCombatCharacter::CreateRuntimeInputAction(const FName& ActionId, uint8 ValueType)
{
	UInputAction* Action = NewObject<UInputAction>(this, ActionId);
	Action->ValueType = static_cast<EInputActionValueType>(ValueType);
	RuntimeInputActions.Add(ActionId, Action);
	return Action;
}

void AUBFCombatCharacter::AddRuntimeInputMapping(UInputAction* Action, const FKey& Key, bool bNegate)
{
	if (!RuntimeInputMappingContext || !Action || !Key.IsValid())
	{
		return;
	}
	FEnhancedActionKeyMapping& Mapping = RuntimeInputMappingContext->MapKey(Action, Key);
	if (bNegate)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(RuntimeInputMappingContext));
	}
}

void AUBFCombatCharacter::EnhancedMoveForward(const FInputActionValue& Value) { MoveForward(Value.Get<float>()); }
void AUBFCombatCharacter::EnhancedMoveRight(const FInputActionValue& Value) { MoveRight(Value.Get<float>()); }
void AUBFCombatCharacter::EnhancedTurn(const FInputActionValue& Value) { AddControllerYawInput(Value.Get<float>()); }
void AUBFCombatCharacter::EnhancedLookUp(const FInputActionValue& Value) { AddControllerPitchInput(Value.Get<float>()); }
void AUBFCombatCharacter::EnhancedJumpStarted(const FInputActionValue&) { Jump(); }
void AUBFCombatCharacter::EnhancedJumpCompleted(const FInputActionValue&) { StopJumping(); }
void AUBFCombatCharacter::EnhancedBasicAttackStarted(const FInputActionValue&) { HandleBasicAttackPressed(); }
void AUBFCombatCharacter::EnhancedBasicAttackCompleted(const FInputActionValue&) { HandleBasicAttackReleased(); }
void AUBFCombatCharacter::EnhancedAbilityQ(const FInputActionValue&) { RequestAbilityQ(); }
void AUBFCombatCharacter::EnhancedAbilityE(const FInputActionValue&) { RequestAbilityE(); }
void AUBFCombatCharacter::EnhancedUltimate(const FInputActionValue&) { RequestUltimate(); }
void AUBFCombatCharacter::EnhancedDash(const FInputActionValue&) { RequestDash(); }
void AUBFCombatCharacter::EnhancedChargedStarted(const FInputActionValue&) { HandleChargedSpecialPressed(); }
void AUBFCombatCharacter::EnhancedChargedCompleted(const FInputActionValue&) { HandleChargedSpecialReleased(); }
void AUBFCombatCharacter::EnhancedReturnToMenu(const FInputActionValue&) { ReturnToMenu(); }

void AUBFCombatCharacter::MoveForward(float Value)
{
	if (!Controller || FMath::IsNearlyZero(Value)) return;
	const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
}

void AUBFCombatCharacter::MoveRight(float Value)
{
	if (!Controller || FMath::IsNearlyZero(Value)) return;
	const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
}

void AUBFCombatCharacter::HandleBasicAttackPressed()
{
	bLocalLmbHeld = true;
	if (bLocalRmbHeld)
	{
		BeginLocalGrabGesture();
		return;
	}

	bPendingBasicAttackInput = true;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(PendingBasicAttackInputTimer, this,
			&AUBFCombatCharacter::DispatchPendingBasicAttack, 0.08f, false);
	}
	else
	{
		DispatchPendingBasicAttack();
	}
}

void AUBFCombatCharacter::HandleBasicAttackReleased()
{
	bLocalLmbHeld = false;
}

void AUBFCombatCharacter::DispatchPendingBasicAttack()
{
	if (!bPendingBasicAttackInput || bLocalGrabGestureActive)
	{
		return;
	}
	bPendingBasicAttackInput = false;
	ServerRequestBasicAttack();
}

void AUBFCombatCharacter::BeginLocalGrabGesture()
{
	bLocalGrabGestureActive = true;
	bPendingBasicAttackInput = false;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PendingBasicAttackInputTimer);
	}
	ServerCancelChargedSpecial();
	ServerRequestGrab();
}

void AUBFCombatCharacter::ServerRequestBasicAttack_Implementation()
{
	PerformBasicAttack();
}

void AUBFCombatCharacter::RequestAbilityQ()
{
	ServerRequestAbility(0);
}

void AUBFCombatCharacter::RequestAbilityE()
{
	ServerRequestAbility(1);
}

void AUBFCombatCharacter::RequestUltimate()
{
	ServerRequestAbility(2);
}

void AUBFCombatCharacter::RequestDash()
{
	ServerRequestDash();
}

void AUBFCombatCharacter::ServerRequestDash_Implementation()
{
	FVector DashDirection = GetLastMovementInputVector().GetSafeNormal2D();
	if (DashDirection.IsNearlyZero())
	{
		DashDirection = GetActorForwardVector().GetSafeNormal2D();
	}
	PerformDash(DashDirection);
}

bool AUBFCombatCharacter::PerformBotDash(const FVector& Direction)
{
	return HasAuthority() && PerformDash(Direction);
}

bool AUBFCombatCharacter::PerformDash(const FVector& Direction)
{
	if (!CanFight() || !HasAuthority() || !GetWorld())
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextDashTime)
	{
		return false;
	}

	const FVector DashDirection = Direction.GetSafeNormal2D();
	if (DashDirection.IsNearlyZero())
	{
		return false;
	}
	NextDashTime = Now + DashCooldown;
	const EUBFPassiveMechanic Passive = CharacterDefinition
		? CharacterDefinition->PassiveMechanic : EUBFPassiveMechanic::None;
	const float PassiveMagnitude = CharacterDefinition
		? FMath::Max(0.0f, CharacterDefinition->PassiveMagnitude) : 0.0f;
	const float DashMultiplier = Passive == EUBFPassiveMechanic::DashDistanceBonus
		? 1.0f + PassiveMagnitude : 1.0f;
	LaunchCharacter(DashDirection * DashSpeed * DashMultiplier, true, false);
	if (Passive == EUBFPassiveMechanic::NextAttackAfterDash)
	{
		NextBasicDamageMultiplier = FMath::Max(NextBasicDamageMultiplier, 1.0f + PassiveMagnitude);
		ClientShowCombatMessage(TEXT("NEXT STRIKE EMPOWERED"), 1.0f);
	}
	return true;
}

void AUBFCombatCharacter::ServerRequestGrab_Implementation()
{
	if (!CanFight() || !HasAuthority() || !GetWorld() || !GetController()
		|| bIsGrabStarting || GetWorld()->GetTimeSeconds() < NextGrabTime)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	NextGrabTime = Now + GrabRecoveryDuration;
	bIsGrabStarting = true;
	ClientShowCombatMessage(TEXT("GRAB STARTUP"), GrabStartupDuration + 0.25f);
	MulticastPlayActionAnimation(false, 0);
	GetWorld()->GetTimerManager().SetTimer(GrabResolveTimer, this, &AUBFCombatCharacter::ResolveGrab,
		GrabStartupDuration, false);
}

void AUBFCombatCharacter::HandleChargedSpecialPressed()
{
	bLocalRmbHeld = true;
	if (bLocalLmbHeld || bPendingBasicAttackInput)
	{
		BeginLocalGrabGesture();
		return;
	}
	ServerStartChargedSpecial();
}

void AUBFCombatCharacter::HandleChargedSpecialReleased()
{
	bLocalRmbHeld = false;
	if (bLocalGrabGestureActive)
	{
		bLocalGrabGestureActive = false;
		return;
	}
	ServerReleaseChargedSpecial();
}

void AUBFCombatCharacter::ServerStartChargedSpecial_Implementation()
{
	if (!CanFight() || !HasAuthority() || !GetWorld() || !GetController()
		|| bIsChargingSpecial || GetWorld()->GetTimeSeconds() < NextChargedAttackTime)
	{
		return;
	}

	bIsChargingSpecial = true;
	ChargeStartTime = GetWorld()->GetTimeSeconds();
	ClientShowCombatMessage(TEXT("CHARGING SPECIAL"), ChargedSpecialMaximumChargeTime + 0.35f);
}

void AUBFCombatCharacter::ServerReleaseChargedSpecial_Implementation()
{
	if (!CanFight() || !HasAuthority() || !GetWorld() || !bIsChargingSpecial)
	{
		return;
	}

	const float ChargeSeconds = FMath::Clamp(
		GetWorld()->GetTimeSeconds() - ChargeStartTime, 0.0f, ChargedSpecialMaximumChargeTime);
	bIsChargingSpecial = false;
	PerformChargedSpecial(ChargeSeconds);
}

void AUBFCombatCharacter::ServerCancelChargedSpecial_Implementation()
{
	if (HasAuthority() && bIsChargingSpecial)
	{
		bIsChargingSpecial = false;
	}
}

void AUBFCombatCharacter::ResolveGrab()
{
	bIsGrabStarting = false;
	if (!CanFight() || !HasAuthority() || !GetWorld() || !GetController())
	{
		return;
	}

	const FVector Facing = GetActorForwardVector().GetSafeNormal2D();
	AUBFCombatCharacter* BestTarget = nullptr;
	float BestDistanceSquared = FMath::Square(GrabRange);
	for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
	{
		AUBFCombatCharacter* Candidate = *It;
		if (!Candidate || Candidate == this || Candidate->GetTeamId() == TeamId
			|| Candidate->GetCurrentHealth() <= 0.0f || Candidate->bIsGrabStarting)
		{
			continue;
		}

		const FVector ToTarget = Candidate->GetActorLocation() - GetActorLocation();
		const float DistanceSquared = ToTarget.SizeSquared2D();
		if (DistanceSquared > BestDistanceSquared
			|| FVector::DotProduct(Facing, ToTarget.GetSafeNormal2D()) < 0.35f)
		{
			continue;
		}
		BestDistanceSquared = DistanceSquared;
		BestTarget = Candidate;
	}

	if (!BestTarget)
	{
		ClientShowCombatMessage(TEXT("GRAB MISS"), 1.1f);
		return;
	}

	PendingDamageGaugeReward = GaugeBasicHitReward;
	const float AppliedDamage = UGameplayStatics::ApplyDamage(
		BestTarget, GrabDamage, GetController(), this, UDamageType::StaticClass());
	PendingDamageGaugeReward = 0.0f;
	if (AppliedDamage > 0.0f)
	{
		BestTarget->ClientShowCombatMessage(TEXT("GRABBED"), 1.1f);
		ClientShowCombatMessage(TEXT("GRAB SUCCESS"), 1.2f);
	}
	else
	{
		ClientShowCombatMessage(TEXT("GRAB MISS"), 1.1f);
	}
}

void AUBFCombatCharacter::PerformChargedSpecial(float ChargeSeconds)
{
	if (!HasAuthority() || !GetWorld() || !GetController() || CurrentHealth <= 0.0f)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	NextChargedAttackTime = Now + 0.75f;
	const float ChargeRatio = FMath::Clamp(ChargeSeconds / ChargedSpecialMaximumChargeTime, 0.0f, 1.0f);
	float Damage = ChargedSpecialBaseDamage + ChargedSpecialFullChargeBonusDamage * ChargeRatio;
	Damage *= GetOutgoingPassiveDamageMultiplier();
	if (CharacterDefinition && CharacterDefinition->PassiveMechanic == EUBFPassiveMechanic::AirborneChargedBonus
		&& GetVelocity().Z > 80.0f)
	{
		Damage *= 1.0f + FMath::Max(0.0f, CharacterDefinition->PassiveMagnitude);
	}
	MulticastPlayActionAnimation(false, 3);

	const FVector Facing = GetActorForwardVector().GetSafeNormal2D();
	AUBFCombatCharacter* BestTarget = nullptr;
	float BestDistanceSquared = FMath::Square(ChargedSpecialMaximumRange);
	for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
	{
		AUBFCombatCharacter* Candidate = *It;
		if (!Candidate || Candidate == this || Candidate->GetTeamId() == TeamId || Candidate->GetCurrentHealth() <= 0.0f)
		{
			continue;
		}

		const FVector ToTarget = Candidate->GetActorLocation() - GetActorLocation();
		const float DistanceSquared = ToTarget.SizeSquared2D();
		const float ForwardDistance = FVector::DotProduct(ToTarget, Facing);
		const FVector LateralOffset = ToTarget - Facing * ForwardDistance;
		const float TargetRadius = Candidate->GetCapsuleComponent()
			? Candidate->GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.0f;
		if (DistanceSquared > BestDistanceSquared || ForwardDistance <= 0.0f
			|| ForwardDistance > ChargedSpecialMaximumRange + TargetRadius
			|| LateralOffset.Size2D() > ChargedSpecialHitRadius + TargetRadius)
		{
			continue;
		}

		BestDistanceSquared = DistanceSquared;
		BestTarget = Candidate;
	}

	bool bHit = false;
	if (BestTarget)
	{
		PendingDamageGaugeReward = GaugeChargedHitReward;
		const float AppliedDamage = UGameplayStatics::ApplyDamage(
			BestTarget, Damage, GetController(), this, UDamageType::StaticClass());
		PendingDamageGaugeReward = 0.0f;
		bHit = AppliedDamage > 0.0f;
	}

	if (IsLocallyControlled())
	{
		ClientShowCombatMessage(bHit ? TEXT("CHARGED HIT") : TEXT("CHARGED MISS"), 1.2f);
	}
	UE_LOG(LogTemp, Verbose, TEXT("UBF charged special %s: %.2fs charge, %.0f damage."),
		bHit ? TEXT("hit") : TEXT("miss"), ChargeSeconds, Damage);
}

void AUBFCombatCharacter::ServerRequestAbility_Implementation(uint8 AbilitySlot)
{
	ActivateAbility(AbilitySlot, true);
}

void AUBFCombatCharacter::ClientShowCombatMessage_Implementation(const FString& Message, float Duration)
{
	CombatFeedbackMessage = Message;
	CombatFeedbackExpiry = GetWorld() ? GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, Duration) : 0.0f;
}

void AUBFCombatCharacter::ClientFinishMatch_Implementation(const FString& Message, float DamageDealt,
	float DamageReceived, int32 Knockouts, int32 Deaths, int32 MaxComboHits, float DurationSeconds,
	int32 ExperienceReward, int32 GoldReward, int32 EventReward)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PendingBasicAttackInputTimer);
	}
	bPendingBasicAttackInput = false;
	bLocalGrabGestureActive = false;
	MatchResultLabel = Message;
	MatchDamageDealt = DamageDealt;
	MatchDamageReceived = DamageReceived;
	MatchKnockouts = Knockouts;
	MatchDeaths = Deaths;
	MaximumComboHits = MaxComboHits;
	MatchDurationSeconds = DurationSeconds;
	MatchExperienceReward = ExperienceReward;
	MatchGoldReward = GoldReward;
	MatchEventReward = EventReward;
	bHasMatchResult = true;
	if (APlayerController* Player = Cast<APlayerController>(GetController()))
	{
		Player->SetIgnoreMoveInput(true);
		Player->SetIgnoreLookInput(true);
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
	bIsChargingSpecial = false;
}

void AUBFCombatCharacter::ShowMatchResultMessage(const FString& Message, float DurationSeconds,
	int32 ExperienceReward, int32 GoldReward, int32 EventReward)
{
	if (HasAuthority())
	{
		ClientFinishMatch(Message, MatchDamageDealt, MatchDamageReceived, MatchKnockouts,
			MatchDeaths, MaximumComboHits, DurationSeconds, ExperienceReward, GoldReward, EventReward);
	}
}

void AUBFCombatCharacter::RegisterConfirmedComboHit(float Now)
{
	CurrentComboHits = Now - LastConfirmedComboTime <= ComboResetWindow ? CurrentComboHits + 1 : 1;
	MaximumComboHits = FMath::Max(MaximumComboHits, CurrentComboHits);
	LastConfirmedComboTime = Now;
	NextComboStage = static_cast<uint8>((NextComboStage + 1) % 3);
}

void AUBFCombatCharacter::AddSkillGauge(float Amount)
{
	if (HasAuthority() && Amount > 0.0f)
	{
		const float PreviousGauge = CurrentSkillGauge;
		CurrentSkillGauge = FMath::Clamp(CurrentSkillGauge + Amount, 0.0f, SkillGaugeMaximum);
		if (CurrentSkillGauge > PreviousGauge)
		{
			UE_LOG(LogTemp, Verbose, TEXT("UBF gauge %s: %.0f -> %.0f (+%.0f)."),
				*GetNameSafe(this), PreviousGauge, CurrentSkillGauge, CurrentSkillGauge - PreviousGauge);
		}
	}
}

bool AUBFCombatCharacter::ActivateAbility(uint8 AbilitySlot, bool bNotifyOwningPlayer)
{
	if (!CanFight() || !HasAuthority() || !GetWorld() || !GetController() || AbilitySlot > 2)
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextAbilityTimes[AbilitySlot])
	{
		return false;
	}

	const UUBFAbilityDefinition* AbilityDefinition = GetAbilityDefinition(AbilitySlot);
	const float Cost = AbilityDefinition ? AbilityDefinition->GaugeCost
		: (AbilitySlot == 0 ? AbilityQCost : (AbilitySlot == 1 ? AbilityECost : UltimateCost));
	float Damage = AbilityDefinition ? AbilityDefinition->Damage
		: (AbilitySlot == 0 ? AbilityQDamage : (AbilitySlot == 1 ? AbilityEDamage : UltimateDamage));
	Damage *= GetOutgoingPassiveDamageMultiplier();
	const float MaximumRange = AbilityDefinition ? AbilityDefinition->Range
		: (AbilitySlot == 0 ? AbilityQMaximumRange : (AbilitySlot == 1 ? AbilityEMaximumRange : UltimateMaximumRange));
	const float ImpactOffset = AbilityDefinition ? AbilityDefinition->ImpactOffset
		: (AbilitySlot == 0 ? AbilityQImpactOffset : (AbilitySlot == 1 ? AbilityEImpactOffset : UltimateImpactOffset));
	float ImpactRadius = AbilityDefinition ? AbilityDefinition->Radius
		: (AbilitySlot == 0 ? AbilityQImpactRadius : (AbilitySlot == 1 ? AbilityEImpactRadius : UltimateImpactRadius));
	const EUBFPassiveMechanic Passive = CharacterDefinition
		? CharacterDefinition->PassiveMechanic : EUBFPassiveMechanic::None;
	const float PassiveMagnitude = CharacterDefinition
		? FMath::Max(0.0f, CharacterDefinition->PassiveMagnitude) : 0.0f;
	if (Passive == EUBFPassiveMechanic::ExpandedAbilityArea)
	{
		ImpactRadius *= 1.0f + PassiveMagnitude;
	}
	const FString AbilityName = AbilityDefinition && !AbilityDefinition->DisplayName.IsEmpty()
		? AbilityDefinition->DisplayName.ToString()
		: (AbilitySlot == 0 ? TEXT("Q") : (AbilitySlot == 1 ? TEXT("E") : TEXT("ULTIMATE")));
	const EUBFAbilityEffect Effect = AbilityDefinition
		? AbilityDefinition->Effect : EUBFAbilityEffect::ForwardArea;

	if (CurrentSkillGauge + KINDA_SMALL_NUMBER < Cost)
	{
		if (bNotifyOwningPlayer)
		{
			ClientShowCombatMessage(TEXT("NOT ENOUGH GAUGE"), 1.7f);
		}
		return false;
	}

	// The server accepts the cast and spends the gauge before resolving its hit.
	CurrentSkillGauge = FMath::Max(0.0f, CurrentSkillGauge - Cost);
	AbilityCooldownDurations[AbilitySlot] = AbilityDefinition
		? FMath::Max(AbilityDefinition->Cooldown, AbilityDefinition->RecoveryTime)
		: AbilityRecoveryDuration;
	NextAbilityTimes[AbilitySlot] = Now + AbilityCooldownDurations[AbilitySlot];
	MulticastPlayActionAnimation(false, 0);

	const FVector Facing = GetActorForwardVector().GetSafeNormal2D();
	if (Effect == EUBFAbilityEffect::CounterStance)
	{
		CounterStanceEndTime = Now + (AbilityDefinition
			? FMath::Max(0.1f, AbilityDefinition->EffectDuration) : 0.8f);
		if (Passive == EUBFPassiveMechanic::ExtendedCounterWindow)
		{
			CounterStanceEndTime += PassiveMagnitude;
		}
		CounterStanceDamage = AbilityDefinition
			? FMath::Max(0.0f, AbilityDefinition->CounterDamage) : Damage * 0.65f;
		if (Passive == EUBFPassiveMechanic::CounterAmplifier)
		{
			CounterStanceDamage *= PassiveMagnitude;
		}
		bCounterStanceArmed = true;
		if (bNotifyOwningPlayer)
		{
			ClientShowCombatMessage(FString::Printf(TEXT("%s READY"), *AbilityName), 1.2f);
		}
		return true;
	}
	if (Effect == EUBFAbilityEffect::DashStrike)
	{
		float DashDistance = AbilityDefinition
			? FMath::Max(0.0f, AbilityDefinition->MovementDistance) : 360.0f;
		if (Passive == EUBFPassiveMechanic::DashDistanceBonus)
		{
			DashDistance *= 1.0f + PassiveMagnitude;
		}
		LaunchCharacter(Facing * DashDistance, true, false);
	}

	const FVector ImpactPoint = GetActorLocation() + Facing * ImpactOffset;
	int32 HitCount = 0;
	for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
	{
		AUBFCombatCharacter* Target = *It;
		if (!Target || Target == this || Target->GetTeamId() == TeamId || Target->GetCurrentHealth() <= 0.0f)
		{
			continue;
		}

		const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
		const float TargetDistance = ToTarget.Size2D();
		if (TargetDistance > MaximumRange + (Target->GetCapsuleComponent()
			? Target->GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.0f))
		{
			continue;
		}

		const float TargetRadius = Target->GetCapsuleComponent() ? Target->GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.0f;
		const float ForwardDistance = FVector::DotProduct(ToTarget, Facing);
		const FVector LateralOffset = ToTarget - Facing * ForwardDistance;
		bool bInsideEffect = false;
		switch (Effect)
		{
		case EUBFAbilityEffect::SelfPulse:
			bInsideEffect = TargetDistance <= ImpactRadius + TargetRadius;
			break;
		case EUBFAbilityEffect::ForwardLine:
			bInsideEffect = ForwardDistance > 0.0f
				&& ForwardDistance <= MaximumRange + TargetRadius
				&& LateralOffset.Size2D() <= FMath::Max(65.0f, ImpactRadius * 0.45f) + TargetRadius;
			break;
		case EUBFAbilityEffect::DashStrike:
			bInsideEffect = ForwardDistance > 0.0f
				&& ForwardDistance <= MaximumRange + TargetRadius
				&& LateralOffset.Size2D() <= ImpactRadius + TargetRadius;
			break;
		case EUBFAbilityEffect::ForwardArea:
		case EUBFAbilityEffect::PullWave:
		case EUBFAbilityEffect::PushWave:
		default:
			bInsideEffect = ForwardDistance > 0.0f
				&& FVector::DotProduct(Facing, ToTarget.GetSafeNormal2D()) >= 0.20f
				&& FVector::DistSquared2D(Target->GetActorLocation(), ImpactPoint)
					<= FMath::Square(ImpactRadius + TargetRadius);
			break;
		}
		if (!bInsideEffect)
		{
			continue;
		}

		PendingDamageGaugeReward = GaugeBasicHitReward;
		if (Passive == EUBFPassiveMechanic::AbilityHitGaugeBonus)
		{
			PendingDamageGaugeReward += PassiveMagnitude;
		}
		bApplyingAbilityDamage = true;
		const float AppliedDamage = UGameplayStatics::ApplyDamage(
			Target, Damage, GetController(), this, UDamageType::StaticClass());
		bApplyingAbilityDamage = false;
		PendingDamageGaugeReward = 0.0f;
		if (AppliedDamage > 0.0f)
		{
			++HitCount;
			if (Passive == EUBFPassiveMechanic::NextBasicAfterSkill)
			{
				NextBasicDamageMultiplier = FMath::Max(NextBasicDamageMultiplier, 1.0f + PassiveMagnitude);
			}
			if (Effect == EUBFAbilityEffect::PullWave || Effect == EUBFAbilityEffect::PushWave)
			{
				const FVector AwayFromCaster = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
				const float Knockback = AbilityDefinition
					? FMath::Max(0.0f, AbilityDefinition->Knockback) : 260.0f;
				const FVector Direction = Effect == EUBFAbilityEffect::PullWave
					? -AwayFromCaster : AwayFromCaster;
				Target->LaunchCharacter(Direction * Knockback + FVector(0.0f, 0.0f, 115.0f), true, true);
			}
		}
	}

	if (bNotifyOwningPlayer)
	{
		ClientShowCombatMessage(HitCount > 0
			? FString::Printf(TEXT("%s HIT  x%d"), *AbilityName, HitCount)
			: FString::Printf(TEXT("%s MISS"), *AbilityName), 1.4f);
	}
	UE_LOG(LogTemp, Verbose, TEXT("UBF ability %s from %s spent %.0f gauge, remaining %.0f, targets hit %d."),
		*AbilityName, *GetNameSafe(this), Cost, CurrentSkillGauge, HitCount);
	return true;
}

void AUBFCombatCharacter::PerformBotAbility(uint8 AbilitySlot)
{
	ActivateAbility(AbilitySlot, false);
}

void AUBFCombatCharacter::PerformBotBasicAttack(AUBFCombatCharacter* Target)
{
	if (!CanFight() || !HasAuthority() || !GetWorld() || !GetController() || !Target || Target == this
		|| Target->GetTeamId() == TeamId || Target->GetCurrentHealth() <= 0.0f)
	{
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	if (ToTarget.SizeSquared2D() > FMath::Square(GetBasicAttackReach()))
	{
		return;
	}
	const FVector Facing = GetActorForwardVector().GetSafeNormal2D();
	const FVector Direction = ToTarget.GetSafeNormal2D();
	if (FVector::DotProduct(Facing, Direction) < 0.25f)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastBasicAttackTime < BasicAttackCooldown)
	{
		return;
	}
	const uint8 AttackStage = Now - LastConfirmedComboTime <= ComboResetWindow ? NextComboStage : 0;
	LastBasicAttackTime = Now;
	MulticastPlayActionAnimation(false, AttackStage);
	const float ComboGaugeBonus = AttackStage > 0 && CharacterDefinition
		&& CharacterDefinition->PassiveMechanic == EUBFPassiveMechanic::ComboGaugeBonus
		? CharacterDefinition->PassiveMagnitude : 0.0f;
	PendingDamageGaugeReward = AttackStage == 0
		? GaugeBasicHitReward : GaugeComboHitReward + ComboGaugeBonus;
	const float AttackDamage = (BasicAttackDamage + static_cast<float>(AttackStage) * ComboBonusDamagePerHit)
		* GetOutgoingPassiveDamageMultiplier() * NextBasicDamageMultiplier;
	const float AppliedDamage = UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, UDamageType::StaticClass());
	PendingDamageGaugeReward = 0.0f;
	if (AppliedDamage > 0.0f)
	{
		NextBasicDamageMultiplier = 1.0f;
		RegisterConfirmedComboHit(Now);
	}
}

void AUBFCombatCharacter::PerformBasicAttack()
{
	if (!CanFight() || !HasAuthority() || !GetWorld() || !GetController())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastBasicAttackTime < BasicAttackCooldown)
	{
		return;
	}
	const uint8 AttackStage = Now - LastConfirmedComboTime <= ComboResetWindow ? NextComboStage : 0;
	LastBasicAttackTime = Now;
	MulticastPlayActionAnimation(false, AttackStage);

	const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
	const FVector End = Start + GetActorForwardVector() * BasicAttackRange;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(UBFBasicAttack), false, this);
	TArray<FOverlapResult> Overlaps;
	const bool bFoundTargets = GetWorld()->OverlapMultiByObjectType(
		Overlaps, End, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(BasicAttackRadius), QueryParams);
	if (!bFoundTargets)
	{
		return;
	}

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AUBFCombatCharacter* Target = Cast<AUBFCombatCharacter>(Overlap.GetActor());
		if (!Target || Target == this || Target->GetTeamId() == TeamId || Target->GetCurrentHealth() <= 0.0f)
		{
			continue;
		}
		const float ComboGaugeBonus = AttackStage > 0 && CharacterDefinition
			&& CharacterDefinition->PassiveMechanic == EUBFPassiveMechanic::ComboGaugeBonus
			? CharacterDefinition->PassiveMagnitude : 0.0f;
		PendingDamageGaugeReward = AttackStage == 0
			? GaugeBasicHitReward : GaugeComboHitReward + ComboGaugeBonus;
		const float AttackDamage = (BasicAttackDamage + static_cast<float>(AttackStage) * ComboBonusDamagePerHit)
			* GetOutgoingPassiveDamageMultiplier() * NextBasicDamageMultiplier;
		const float AppliedDamage = UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, UDamageType::StaticClass());
		PendingDamageGaugeReward = 0.0f;
		if (AppliedDamage > 0.0f)
		{
			NextBasicDamageMultiplier = 1.0f;
			RegisterConfirmedComboHit(Now);
			if (AttackStage > 0 && IsLocallyControlled())
			{
				ClientShowCombatMessage(FString::Printf(TEXT("%d HIT"), AttackStage + 1), 0.9f);
			}
		}
		break;
	}
}

void AUBFCombatCharacter::MulticastPlayActionAnimation_Implementation(bool bWasHit, uint8 AttackStage)
{
	PlayActionAnimation(bWasHit, AttackStage);
}

void AUBFCombatCharacter::PlayActionAnimation(bool bWasHit, uint8 AttackStage)
{
	UAnimSequence* Sequence = HitAnimation.Get();
	if (!bWasHit)
	{
		Sequence = AttackStage == 3 ? ChargedAttackAnimation.Get()
			: (AttackStage == 1 ? CrossAttackAnimation.Get()
				: (AttackStage == 2 ? HookAttackAnimation.Get() : AttackAnimation.Get()));
		if (!Sequence)
		{
			Sequence = AttackAnimation.Get();
		}
	}
	if (Sequence && GetMesh())
	{
		GetMesh()->PlayAnimation(Sequence, false);
		ActionAnimationRemaining = Sequence->GetPlayLength();
	}
}

float AUBFCombatCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!CanFight() || !HasAuthority() || DamageAmount <= 0.0f || CurrentHealth <= 0.0f)
	{
		return 0.0f;
	}

	AUBFCombatCharacter* Attacker = Cast<AUBFCombatCharacter>(DamageCauser);
	if (!Attacker && EventInstigator)
	{
		Attacker = Cast<AUBFCombatCharacter>(EventInstigator->GetPawn());
	}
	bool bCountered = false;
	float EffectiveDamage = DamageAmount;
	const float DamageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Attacker && MarkedByTeamId == Attacker->GetTeamId() && DamageTime < MarkExpiryTime)
	{
		EffectiveDamage *= 1.0f + FMath::Max(0.0f, MarkedDamageBonus);
	}
	if (Attacker && Attacker->bApplyingAbilityDamage && Attacker->CharacterDefinition
		&& Attacker->CharacterDefinition->PassiveMechanic == EUBFPassiveMechanic::MarkAbilityTargets)
	{
		MarkedByTeamId = Attacker->GetTeamId();
		MarkedDamageBonus = FMath::Max(0.0f, Attacker->CharacterDefinition->PassiveMagnitude);
		MarkExpiryTime = DamageTime + 6.0f;
	}
	if (bCounterStanceArmed)
	{
		if (DamageTime >= CounterStanceEndTime)
		{
			bCounterStanceArmed = false;
		}
		else if (Attacker && Attacker != this
			&& FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(),
				(Attacker->GetActorLocation() - GetActorLocation()).GetSafeNormal2D()) > 0.25f)
		{
			bCounterStanceArmed = false;
			EffectiveDamage *= 0.25f;
			bCountered = true;
		}
	}

	const float AppliedDamage = FMath::Min(CurrentHealth, EffectiveDamage);
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - AppliedDamage);
	MatchDamageReceived += AppliedDamage;
	if (bIsChargingSpecial)
	{
		bIsChargingSpecial = false;
		NextChargedAttackTime = GetWorld()->GetTimeSeconds() + 0.6f;
		ClientShowCombatMessage(TEXT("CHARGED SPECIAL INTERRUPTED"), 1.3f);
	}
	if (bIsGrabStarting)
	{
		bIsGrabStarting = false;
		GetWorld()->GetTimerManager().ClearTimer(GrabResolveTimer);
		NextGrabTime = GetWorld()->GetTimeSeconds() + 0.45f;
		ClientShowCombatMessage(TEXT("GRAB INTERRUPTED"), 1.2f);
	}
	AddSkillGauge(EffectiveDamage >= 25.0f ? GaugeReceiveHeavyReward : GaugeReceiveHitReward);
	if (AppliedDamage > 0.0f)
	{
		if (Attacker && Attacker != this)
		{
			Attacker->MatchDamageDealt += AppliedDamage;
			if (CurrentHealth <= 0.0f)
			{
				++Attacker->MatchKnockouts;
			}
			const float GaugeReward = Attacker->PendingDamageGaugeReward > 0.0f
				? Attacker->PendingDamageGaugeReward : Attacker->GaugeBasicHitReward;
			Attacker->AddSkillGauge(GaugeReward);
		}
	}
	if (bCountered && Attacker && Attacker->GetCurrentHealth() > 0.0f)
	{
		ClientShowCombatMessage(TEXT("COUNTER"), 1.0f);
		const float CounterDamage = CounterStanceDamage;
		CounterStanceDamage = 0.0f;
		PendingDamageGaugeReward = GaugeBasicHitReward;
		UGameplayStatics::ApplyDamage(Attacker, CounterDamage, GetController(), this, UDamageType::StaticClass());
		PendingDamageGaugeReward = 0.0f;
	}
	OnRep_CurrentHealth();
	MulticastPlayActionAnimation(true, 0);
	if (CurrentHealth <= 0.0f)
	{
		++MatchDeaths;
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
		if (AUBFCombatGameMode* MatchMode = GetWorld()->GetAuthGameMode<AUBFCombatGameMode>())
		{
			if (bIsMasterGolem)
			{
				MatchMode->NotifyMasterGolemEliminated(TeamId);
			}
			else
			{
				MatchMode->NotifyGoldenBearerEliminated(this);
				MatchMode->NotifyFighterEliminated();
			}
		}
	}
	return AppliedDamage;
}

bool AUBFCombatCharacter::CanFight() const
{
	if (CurrentHealth <= 0.0f)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	const AUBFCombatGameMode* MatchMode = World ? World->GetAuthGameMode<AUBFCombatGameMode>() : nullptr;
	return !MatchMode || !MatchMode->IsMatchFinished();
}

float AUBFCombatCharacter::GetHealthRatio() const
{
	return MaximumHealth > 0.0f ? FMath::Clamp(CurrentHealth / MaximumHealth, 0.0f, 1.0f) : 0.0f;
}

float AUBFCombatCharacter::GetSkillGaugeRatio() const
{
	return SkillGaugeMaximum > 0.0f ? FMath::Clamp(CurrentSkillGauge / SkillGaugeMaximum, 0.0f, 1.0f) : 0.0f;
}

FString AUBFCombatCharacter::GetCombatFeedbackMessage() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() < CombatFeedbackExpiry ? CombatFeedbackMessage : FString();
}

void AUBFCombatCharacter::SetTeamId(int32 NewTeamId)
{
	if (HasAuthority())
	{
		TeamId = FMath::Max(0, NewTeamId);
	}
}

void AUBFCombatCharacter::OnRep_CurrentHealth()
{
	CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, MaximumHealth);
}

void AUBFCombatCharacter::OnRep_CharacterId()
{
	CharacterDefinition = nullptr;
	ApplyCharacterDefinition();
	CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, MaximumHealth);
}

void AUBFCombatCharacter::UpdateLocomotionAnimation()
{
	if (!GetMesh()) return;
	const bool bMovingNow = GetVelocity().Size2D() > 12.0f;
	if (bHasLocomotionAnimationStarted && bMovingNow == bIsMoving)
	{
		return;
	}
	bIsMoving = bMovingNow;
	UAnimSequence* Sequence = bIsMoving ? WalkAnimation.Get() : IdleAnimation.Get();
	if (Sequence)
	{
		GetMesh()->PlayAnimation(Sequence, true);
		bHasLocomotionAnimationStarted = true;
	}
}

void AUBFCombatCharacter::ReturnToMenu()
{
	if (IsLocallyControlled())
	{
		UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")));
	}
}

void AUBFCombatCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUBFCombatCharacter, CurrentHealth);
	DOREPLIFETIME(AUBFCombatCharacter, TeamId);
	DOREPLIFETIME(AUBFCombatCharacter, CurrentSkillGauge);
	DOREPLIFETIME(AUBFCombatCharacter, bIsGoldenBearer);
	DOREPLIFETIME(AUBFCombatCharacter, bIsMasterGolem);
	DOREPLIFETIME(AUBFCombatCharacter, CharacterId);
}
