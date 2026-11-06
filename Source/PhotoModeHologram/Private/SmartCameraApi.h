#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

class AFGHologram;

// Smart Camera's public consumer API. Its source is not public, so this was rebuilt from the type
// records in the PDB that ships with Smart Camera 2.0.0. Nothing here links against Smart Camera:
// the registry is found at runtime through IModularFeatures and called through its vtable.
//
// A change to the virtual order or these structs would break this without any compile error, so
// PhotoModeFreecamView.cpp only uses the registry on Smart Camera versions listed there.

enum class ESmartCameraMode : uint8
{
	Off,
	Left45,
	Front45,
	Right45,
	Back45,
	TopDown,
	ForwardLevel,
	Periscope,
};

struct FSmartCameraViewportSettings
{
	bool bEnabled = true;
	ESmartCameraMode Mode = ESmartCameraMode::Front45;
	FVector2D Position = FVector2D::ZeroVector;
	FIntPoint Size = FIntPoint::ZeroValue;
	float Scale = 1.0f;
	float Zoom = 1.0f;
	float UpdateRate = 20.0f;
	float Brightness = 0.0f;
};

struct FSmartCameraViewId
{
	FString Namespace;
	FString Camera;
};

struct FSmartCameraViewPose
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float FieldOfView = 90.0f;
};

struct FSmartCameraViewDefinition
{
	FText DisplayName;
	FSmartCameraViewportSettings SuggestedSettings;
	FName ExclusiveGroup;
	int32 Priority = 0;
	bool bFollowNativeHologram = false;
	bool bUsesBuildAngles = false;
};

static_assert(sizeof(FSmartCameraViewportSettings) == 48);
static_assert(sizeof(FSmartCameraViewId) == 32);
static_assert(sizeof(FSmartCameraViewPose) == 56);
static_assert(STRUCT_OFFSET(FSmartCameraViewportSettings, Position) == 8);
static_assert(STRUCT_OFFSET(FSmartCameraViewportSettings, Size) == 24);
static_assert(STRUCT_OFFSET(FSmartCameraViewportSettings, Brightness) == 44);
static_assert(STRUCT_OFFSET(FSmartCameraViewPose, FieldOfView) == 48);
static_assert(STRUCT_OFFSET(FSmartCameraViewDefinition, SuggestedSettings) == 16);
static_assert(STRUCT_OFFSET(FSmartCameraViewDefinition, ExclusiveGroup) == 64);

// FName is larger in editor builds, but Smart Camera only ever runs against game builds.
#if !WITH_EDITOR
static_assert(sizeof(FSmartCameraViewDefinition) == 80);
static_assert(STRUCT_OFFSET(FSmartCameraViewDefinition, Priority) == 72);
static_assert(STRUCT_OFFSET(FSmartCameraViewDefinition, bUsesBuildAngles) == 77);
#endif

class ISmartCameraViewportRegistry : public IModularFeature
{
public:
	static FName GetNamedFeatureName() { return TEXT("SmartCameraNamedViewportRegistry"); }

	virtual UWorld* GetCameraWorld() const = 0;

	// Slots 1 to 5 are the numbered viewport API, which this mod does not call.
	virtual void Unused_ConfigureViewport() = 0;
	virtual void Unused_SetViewportTarget() = 0;
	virtual void Unused_ClearViewportTarget() = 0;
	virtual void Unused_ReleaseViewport() = 0;
	virtual void Unused_GetViewportSettings() = 0;

	virtual bool RegisterConsumer(const FString& ConsumerId, UObject* Owner, const FText& DisplayName) = 0;
	virtual bool RegisterView(const FSmartCameraViewId& Id, UObject* Owner, const FSmartCameraViewDefinition& Definition) = 0;
	virtual void Unused_SetViewFrame() = 0;
	virtual bool SetViewPose(const FSmartCameraViewId& Id, UObject* Owner, const FSmartCameraViewPose& Pose) = 0;
	virtual bool DeactivateView(const FSmartCameraViewId& Id, UObject* Owner) = 0;
	virtual void Unused_RemoveView() = 0;
	virtual void UnregisterConsumer(const FString& ConsumerId, UObject* Owner) = 0;
	virtual void Unused_GetRegisteredViews() = 0;
	virtual void Unused_GetCurrentSaveIdentifier() = 0;
	virtual FSmartCameraViewportSettings GetBuildPreviewDefaults() const = 0;
};
