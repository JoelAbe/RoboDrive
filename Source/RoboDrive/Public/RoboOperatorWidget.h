#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RoboOperatorWidget.generated.h"

class ARoboVehicle;
class UTextBlock;

/**
 * The operator's view of the robot: what it is doing and why, as text.
 *
 * Logic here, layout in a Widget Blueprint that derives from this class. The
 * Blueprint must contain text blocks named exactly as the BindWidget
 * properties below, or it will not compile, so the two cannot drift apart.
 *
 * Everything shown is read, never written. Ground truth has its own section,
 * because an operator watching a simulation may see it and the robot may not.
 */
UCLASS(Abstract)
class URoboOperatorWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Simulation time and speed. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SimText;

	/** Who is driving, and what they asked for. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DrivingText;

	/** The goal and how it is going, as the robot believes. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MissionText;

	/** The brain, and the conditions its sensors work in. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SensingText;

	/** Ground truth the robot cannot see: how wrong its belief is. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TruthText;

private:
	FString DescribeSim() const;
	FString DescribeDriving(const ARoboVehicle& Vehicle) const;
	FString DescribeMission(const ARoboVehicle& Vehicle) const;
	FString DescribeSensing(const ARoboVehicle& Vehicle) const;
	FString DescribeTruth(const ARoboVehicle& Vehicle) const;
};