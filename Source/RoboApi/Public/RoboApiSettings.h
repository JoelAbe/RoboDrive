#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RoboApiSettings.generated.h"

/**
 * Settings for the external API, editable under Project Settings and stored
 * in DefaultEngine.ini.
 */
UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "RoboDrive API"))
class ROBOAPI_API URoboApiSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * Address to bind to. Loopback keeps the simulation unreachable from the
	 * network, which matters because the API has no authentication and will
	 * drive the robot for anyone who asks.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Network")
	FString BindAddress = TEXT("127.0.0.1");

	UPROPERTY(config, EditAnywhere, Category = "Network", meta = (ClampMin = "1024", ClampMax = "65535"))
	int32 Port = 8765;

	UPROPERTY(config, EditAnywhere, Category = "Network")
	bool bAutoStart = true;

	/** Port, with -RoboApiPort=NNNN winning, so several instances can run at once. */
	int32 ResolvePort() const;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};