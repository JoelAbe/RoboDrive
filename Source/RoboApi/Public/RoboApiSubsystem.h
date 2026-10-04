#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RoboApiSubsystem.generated.h"

class FSocket;
class FJsonObject;

/**
 * A TCP server exposing the simulation to external clients.
 *
 * Newline-delimited JSON over a loopback socket: no third-party dependencies
 * on either side, and debuggable by typing at it. One client at a time,
 * polled on the game thread, which keeps actor access free of thread-safety
 * questions at the cost of throughput this does not need.
 *
 * Binds to 127.0.0.1 only. There is no authentication, and a simulator that
 * accepts drive commands from the network should not be reachable from it.
 */
UCLASS()
class ROBOAPI_API URoboApiSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool StartListening();
	void StopListening();

	void AcceptPendingClient();
	void ReadFromClient();
	void HandleLine(const FString& Line);
	void SendResponse(const TSharedRef<FJsonObject>& Response);

	/** Dispatch one request. Returns false and fills OutError if it failed. */
	bool Dispatch(const FString& Operation, const TSharedPtr<FJsonObject>& Request,
		TSharedRef<FJsonObject>& OutResult, FString& OutError);

	FSocket* ListenSocket = nullptr;
	FSocket* ClientSocket = nullptr;

	/** Bytes received but not yet forming a complete line. */
	TArray<uint8> ReceiveBuffer;

	int32 BoundPort = 0;
};