#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PhotoModeFreecamView.generated.h"

class AFGCharacterPlayer;

/**
 * Optional Smart Camera integration. Remembers where the free camera was when camera mode closed,
 * then shows that spot in Smart Camera's build viewport until the player stops building.
 */
UCLASS()
class UPhotoModeFreecamView : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual TStatId GetStatId() const override;

	void OnPhotoModeOpened(AFGCharacterPlayer* Character);
	void OnPhotoModeClosed();

private:
	bool IsStillBuilding() const;
	bool SamplePhotoCamera();

	TWeakObjectPtr<AFGCharacterPlayer> BuildingCharacter;
	FVector PoseLocation = FVector::ZeroVector;
	FRotator PoseRotation = FRotator::ZeroRotator;
	float PoseFieldOfView = 90.0f;

	bool bSmartCameraSupported = false;
	bool bRegistered = false;
	bool bPhotoModeOpen = false;
	bool bHasPose = false;
	bool bViewActive = false;
};
