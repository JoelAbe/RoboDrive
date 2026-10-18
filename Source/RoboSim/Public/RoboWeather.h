#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RoboWeather.generated.h"

/**
 * The weather the robot's sensors work in. Today that means fog.
 *
 * A world subsystem, like the mission, so that whatever builds the world (a
 * scenario loaded through the API) and whatever senses it (the lidar) meet
 * here without knowing about each other.
 */
UCLASS()
class ROBOSIM_API URoboWeatherSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor. Returns null outside a game or PIE world. */
	static URoboWeatherSubsystem* Get(const UObject* WorldContext);

	/** Fog with the given visibility, m. Must be positive; anything else is ignored. */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetFog(double InVisibilityM);

	/** Clear air. Sensors behave exactly as they would with no weather at all. */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetClear();

	UFUNCTION(BlueprintPure, Category = "Weather")
	bool HasFog() const { return bHasFog; }

	/** Visibility in the fog, m. Meaningless in clear air: check HasFog first. */
	UFUNCTION(BlueprintPure, Category = "Weather")
	double GetVisibilityM() const { return VisibilityM; }

	/** Fraction of light scattered per metre, 1/m. Zero in clear air. */
	double GetExtinctionPerM() const;

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool bHasFog = false;
	double VisibilityM = 0.0;
};