#include "UBFDraftShowcaseActor.h"

#include "Animation/AnimSequence.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* DraftMannequinPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple");
	const TCHAR* DraftIdlePath = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle");
}

AUBFDraftShowcaseActor::AUBFDraftShowcaseActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DraftPreviewRoot"));
	SetRootComponent(SceneRoot);
	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("DraftPreviewCapture"));
	SceneCapture->SetupAttachment(SceneRoot);
	SceneCapture->bCaptureEveryFrame = true;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCapture->bUseRayTracingIfEnabled = false;
	SceneCapture->bAlwaysPersistRenderingState = false;
	SceneCapture->bUseCustomProjectionMatrix = false;
	SceneCapture->ProjectionType = ECameraProjectionMode::Orthographic;
	SceneCapture->OrthoWidth = 3500.0f;
	SceneCapture->SetRelativeLocation(FVector(0.0f, -2600.0f, 430.0f));
	SceneCapture->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	SceneCapture->ShowOnlyActors.Add(this);
	SceneCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
}

void AUBFDraftShowcaseActor::BeginPlay()
{
	Super::BeginPlay();
	PreviewTexture = NewObject<UTextureRenderTarget2D>(this, TEXT("DraftPreviewTexture"));
	if (PreviewTexture)
	{
		PreviewTexture->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
		PreviewTexture->ClearColor = FLinearColor(0.006f, 0.012f, 0.022f, 0.0f);
		PreviewTexture->InitAutoFormat(1920, 570);
		PreviewTexture->UpdateResourceImmediate(true);
		SceneCapture->TextureTarget = PreviewTexture;
	}
	RebuildLineup();
}

void AUBFDraftShowcaseActor::SetTeamLineupCounts(int32 NewRedTeamCount, int32 NewBlueTeamCount)
{
	NewRedTeamCount = FMath::Clamp(NewRedTeamCount, 0, 5);
	NewBlueTeamCount = FMath::Clamp(NewBlueTeamCount, 0, 5);
	if (RedTeamCount == NewRedTeamCount && BlueTeamCount == NewBlueTeamCount && !Fighters.IsEmpty())
	{
		return;
	}
	RedTeamCount = NewRedTeamCount;
	BlueTeamCount = NewBlueTeamCount;
	if (HasActorBegunPlay())
	{
		RebuildLineup();
	}
}

void AUBFDraftShowcaseActor::RebuildLineup()
{
	for (USkeletalMeshComponent* Fighter : Fighters)
	{
		if (IsValid(Fighter))
		{
			Fighter->DestroyComponent();
		}
	}
	Fighters.Reset();
	USkeletalMesh* Mannequin = LoadObject<USkeletalMesh>(nullptr, DraftMannequinPath);
	UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, DraftIdlePath);
	if (!Mannequin)
	{
		UE_LOG(LogTemp, Warning, TEXT("Draft showcase mannequin mesh is missing: %s"), DraftMannequinPath);
		return;
	}

	auto AddTeam = [this, Mannequin, Idle](int32 Count, bool bRedTeam)
	{
		const FLinearColor TeamTint = bRedTeam
			? FLinearColor(0.78f, 0.12f, 0.17f, 1.0f) : FLinearColor(0.10f, 0.34f, 0.83f, 1.0f);
		for (int32 Slot = 0; Slot < Count; ++Slot)
		{
			USkeletalMeshComponent* Fighter = NewObject<USkeletalMeshComponent>(this,
				FName(*FString::Printf(TEXT("DraftFighter_%s_%d"), bRedTeam ? TEXT("Red") : TEXT("Blue"), Slot)));
			if (!Fighter)
			{
				continue;
			}
			Fighter->SetupAttachment(SceneRoot);
			Fighter->SetSkeletalMesh(Mannequin);
			Fighter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Fighter->SetGenerateOverlapEvents(false);
			Fighter->SetIsReplicated(false);
			const float SizeScale = Count <= 1 ? 1.55f : (Count <= 3 ? 1.34f : 1.14f);
			const float SideDistance = 390.0f + static_cast<float>(Slot) * 255.0f;
			const float DepthOffset = (static_cast<float>(Slot) - (static_cast<float>(Count) - 1.0f) * 0.5f) * 38.0f;
			const float StartScale = SizeScale * 0.74f;
			Fighter->SetRelativeLocation(FVector(bRedTeam ? -SideDistance : SideDistance,
				DepthOffset, 0.0f));
			Fighter->SetRelativeRotation(FRotator(0.0f, bRedTeam ? 0.0f : 180.0f, 0.0f));
			Fighter->SetRelativeScale3D(FVector(StartScale));
			Fighter->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			if (Idle)
			{
				Fighter->SetAnimation(Idle);
				Fighter->Play(true);
			}
			for (int32 MaterialIndex = 0; MaterialIndex < Fighter->GetNumMaterials(); ++MaterialIndex)
			{
				if (UMaterialInstanceDynamic* Material = Fighter->CreateDynamicMaterialInstance(MaterialIndex))
				{
					Material->SetVectorParameterValue(TEXT("BaseColor"), TeamTint);
					Material->SetVectorParameterValue(TEXT("Color"), TeamTint);
				}
			}
			Fighter->RegisterComponent();
			Fighters.Add(Fighter);
		}
	};
	AddTeam(RedTeamCount, true);
	AddTeam(BlueTeamCount, false);
}

void AUBFDraftShowcaseActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	EntryElapsed += DeltaSeconds;
	const float Reveal = FMath::InterpEaseOut(0.74f, 1.0f,
		FMath::Clamp(EntryElapsed / 0.48f, 0.0f, 1.0f), 2.0f);
	for (int32 Index = 0; Index < Fighters.Num(); ++Index)
	{
		USkeletalMeshComponent* Fighter = Fighters[Index];
		if (!IsValid(Fighter))
		{
			continue;
		}
		const float Count = Index < RedTeamCount ? RedTeamCount : BlueTeamCount;
		const float TargetScale = Count <= 1.0f ? 1.55f : (Count <= 3.0f ? 1.34f : 1.14f);
		Fighter->SetRelativeScale3D(FVector(TargetScale * Reveal));
		const float BaseYaw = Index < RedTeamCount ? 0.0f : 180.0f;
		const float Motion = FMath::Sin(EntryElapsed * 2.7f + static_cast<float>(Index) * 0.6f) * 2.2f;
		Fighter->SetRelativeRotation(FRotator(0.0f, BaseYaw + Motion, 0.0f));
	}
}
