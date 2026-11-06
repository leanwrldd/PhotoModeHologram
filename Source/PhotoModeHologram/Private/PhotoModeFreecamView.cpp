#include "PhotoModeFreecamView.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunBuild.h"
#include "FGCharacterPlayer.h"
#include "Features/IModularFeatures.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/IPluginManager.h"
#include "PhotoModeHologram.h"
#include "SmartCameraApi.h"

#define LOCTEXT_NAMESPACE "PhotoModeHologram"

namespace
{
	const TCHAR* const ConsumerId = TEXT("PhotoModeHologram");

	// Smart Camera shows only the highest priority active view of an exclusive group. Smart! uses
	// priority 10 in this group, so the freecam view takes over the build viewport while active.
	const FName BuildPreviewGroup(TEXT("build-preview"));
	constexpr int32 FreecamViewPriority = 100;

	// SetAbsoluteViewPose rejects anything outside this range.
	constexpr float MinFieldOfView = 5.0f;
	constexpr float MaxFieldOfView = 170.0f;

	const FSmartCameraViewId& GetViewId()
	{
		static const FSmartCameraViewId Id{ConsumerId, TEXT("freecam")};
		return Id;
	}

	ISmartCameraViewportRegistry* FindRegistry(UWorld* World)
	{
		const TArray<ISmartCameraViewportRegistry*> Registries = IModularFeatures::Get()
			.GetModularFeatureImplementations<ISmartCameraViewportRegistry>(ISmartCameraViewportRegistry::GetNamedFeatureName());
		for (ISmartCameraViewportRegistry* Registry : Registries)
		{
			if (Registry->GetCameraWorld() == World)
				return Registry;
		}
		return nullptr;
	}

	bool RegisterFreecamView(ISmartCameraViewportRegistry* Registry, UObject* Owner)
	{
		if (!Registry->RegisterConsumer(ConsumerId, Owner, LOCTEXT("ConsumerName", "Photo Mode Hologram")))
			return false;

		FSmartCameraViewDefinition View;
		View.DisplayName = LOCTEXT("ViewName", "Camera Mode View");
		View.SuggestedSettings = Registry->GetBuildPreviewDefaults();
		View.SuggestedSettings.bEnabled = true;
		if (View.SuggestedSettings.Mode == ESmartCameraMode::Off)
			View.SuggestedSettings.Mode = ESmartCameraMode::ForwardLevel;
		View.ExclusiveGroup = BuildPreviewGroup;
		View.Priority = FreecamViewPriority;
		return Registry->RegisterView(GetViewId(), Owner, View);
	}
}

bool UPhotoModeFreecamView::ShouldCreateSubsystem(UObject* Outer) const
{
	// Smart Camera renders on the local client only.
	const UWorld* World = Cast<UWorld>(Outer);
	return !IsRunningDedicatedServer() && World &&
		(World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) &&
		Super::ShouldCreateSubsystem(Outer);
}

void UPhotoModeFreecamView::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const TSharedPtr<IPlugin> SmartCamera = IPluginManager::Get().FindPlugin(TEXT("SmartCamera"));
	if (!SmartCamera.IsValid())
		return;

	// SmartCameraApi.h matches this version line only. Calling a different vtable could crash.
	const FString Version = SmartCamera->GetDescriptor().VersionName;
	bSmartCameraSupported = Version.StartsWith(TEXT("2.0."));
	UE_LOG(LogPhotoModeHologram, Log, TEXT("Smart Camera %s found, viewport integration %s."),
		*Version, bSmartCameraSupported ? TEXT("on") : TEXT("off (untested version)"));
}

void UPhotoModeFreecamView::Deinitialize()
{
	if (bRegistered)
	{
		if (ISmartCameraViewportRegistry* Registry = FindRegistry(GetWorld()))
			Registry->UnregisterConsumer(ConsumerId, this);
		bRegistered = false;
	}
	Super::Deinitialize();
}

TStatId UPhotoModeFreecamView::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPhotoModeFreecamView, STATGROUP_Tickables);
}

void UPhotoModeFreecamView::OnPhotoModeOpened(AFGCharacterPlayer* Character)
{
	BuildingCharacter = Character;
	bPhotoModeOpen = true;
	bHasPose = false;
}

void UPhotoModeFreecamView::OnPhotoModeClosed()
{
	bPhotoModeOpen = false;
}

bool UPhotoModeFreecamView::IsStillBuilding() const
{
	const AFGCharacterPlayer* Character = BuildingCharacter.Get();
	const AFGBuildGun* BuildGun = Character ? Character->GetBuildGun() : nullptr;
	return Character && BuildGun && Character->IsBuildGunEquipped() &&
		Cast<UFGBuildGunStateBuild>(BuildGun->GetCurrentState()) != nullptr;
}

bool UPhotoModeFreecamView::SamplePhotoCamera()
{
	const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	const APlayerCameraManager* CameraManager = Controller ? Controller->PlayerCameraManager.Get() : nullptr;
	if (!CameraManager)
		return false;

	PoseLocation = CameraManager->GetCameraLocation();
	PoseRotation = CameraManager->GetCameraRotation();
	PoseFieldOfView = FMath::Clamp(CameraManager->GetFOVAngle(), MinFieldOfView, MaxFieldOfView);
	return true;
}

void UPhotoModeFreecamView::Tick(float DeltaTime)
{
	if (!bSmartCameraSupported)
		return;

	ISmartCameraViewportRegistry* Registry = FindRegistry(GetWorld());
	if (!Registry)
		return;

	if (!bRegistered)
	{
		bRegistered = RegisterFreecamView(Registry, this);
		if (!bRegistered)
			return;
		UE_LOG(LogPhotoModeHologram, Log, TEXT("Registered the Smart Camera view."));
	}

	if (bPhotoModeOpen)
	{
		bHasPose = SamplePhotoCamera() || bHasPose;
		if (bViewActive)
		{
			Registry->DeactivateView(GetViewId(), this);
			bViewActive = false;
		}
		return;
	}

	if (!bHasPose)
		return;

	if (!IsStillBuilding())
	{
		UE_LOG(LogPhotoModeHologram, Log, TEXT("Stopped building; handing the viewport back."));
		Registry->DeactivateView(GetViewId(), this);
		bViewActive = false;
		bHasPose = false;
		return;
	}

	FSmartCameraViewPose Pose;
	Pose.Location = PoseLocation;
	Pose.Rotation = PoseRotation;
	Pose.FieldOfView = PoseFieldOfView;
	const bool bApplied = Registry->SetViewPose(GetViewId(), this, Pose);
	if (bApplied != bViewActive)
	{
		UE_LOG(LogPhotoModeHologram, Log, TEXT("Freecam view %s at %s, %s, FOV %.1f."),
			bApplied ? TEXT("showing") : TEXT("rejected"), *PoseLocation.ToCompactString(),
			*PoseRotation.ToCompactString(), PoseFieldOfView);
	}
	bViewActive = bApplied;
}

#undef LOCTEXT_NAMESPACE
