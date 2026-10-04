#include "RoboApiSubsystem.h"

#include "Common/TcpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "RoboApi.h"
#include "RoboApiSettings.h"
#include "RoboSimSubsystem.h"
#include "RoboVehicle.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

namespace
{
	/** One request or response per line, so framing needs no length prefix. */
	constexpr uint8 LineTerminator = '\n';
	constexpr int32 ApiReceiveChunkBytes = 8192;
}

bool URoboApiSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId URoboApiSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URoboApiSubsystem, STATGROUP_Tickables);
}

void URoboApiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StartListening();
}

void URoboApiSubsystem::Deinitialize()
{
	StopListening();
	Super::Deinitialize();
}
bool URoboApiSubsystem::StartListening()
{
	const URoboApiSettings* Settings = GetDefault<URoboApiSettings>();
	if (!Settings->bAutoStart)
	{
		return false;
	}

	FIPv4Address Address;
	if (!FIPv4Address::Parse(Settings->BindAddress, Address))
	{
		UE_LOG(LogRoboApi, Error, TEXT("Invalid bind address '%s'"), *Settings->BindAddress);
		return false;
	}

	if (Address != FIPv4Address::InternalLoopback)
	{
		// Not forbidden, because a deliberate setup may want it, but it should
		// never happen by accident and silently.
		UE_LOG(LogRoboApi, Warning,
			TEXT("API bound to %s, which is reachable beyond this machine. There is no authentication."),
			*Settings->BindAddress);
	}

	BoundPort = Settings->ResolvePort();

	ListenSocket = FTcpSocketBuilder(TEXT("RoboApiListener"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToAddress(Address)
		.BoundToPort(BoundPort)
		.Listening(1)
		.Build();

	if (ListenSocket == nullptr)
	{
		UE_LOG(LogRoboApi, Error, TEXT("Could not listen on %s:%d"), *Settings->BindAddress, BoundPort);
		return false;
	}

	UE_LOG(LogRoboApi, Log, TEXT("API listening on %s:%d"), *Settings->BindAddress, BoundPort);
	return true;
}

void URoboApiSubsystem::StopListening()
{
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	for (FSocket** Socket : { &ClientSocket, &ListenSocket })
	{
		if (*Socket != nullptr)
		{
			(*Socket)->Close();
			if (Sockets != nullptr)
			{
				Sockets->DestroySocket(*Socket);
			}
			*Socket = nullptr;
		}
	}
}

void URoboApiSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ListenSocket == nullptr)
	{
		return;
	}

	AcceptPendingClient();
	ReadFromClient();
}

void URoboApiSubsystem::AcceptPendingClient()
{
	bool bPending = false;
	if (!ListenSocket->HasPendingConnection(bPending) || !bPending)
	{
		return;
	}

	FSocket* Incoming = ListenSocket->Accept(TEXT("RoboApiClient"));
	if (Incoming == nullptr)
	{
		return;
	}

	// One client at a time: a second connection replaces the first rather than
	// queueing, so a crashed client cannot lock the simulation out.
	if (ClientSocket != nullptr)
	{
		UE_LOG(LogRoboApi, Warning, TEXT("Replacing existing API client"));
		ClientSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
	}

	ClientSocket = Incoming;
	ReceiveBuffer.Reset();
	UE_LOG(LogRoboApi, Log, TEXT("API client connected"));
}

void URoboApiSubsystem::ReadFromClient()
{
	if (ClientSocket == nullptr)
	{
		return;
	}

	uint32 PendingBytes = 0;
	while (ClientSocket->HasPendingData(PendingBytes))
	{
		TArray<uint8> Chunk;
		Chunk.SetNumUninitialized(FMath::Min(static_cast<int32>(PendingBytes), ApiReceiveChunkBytes));

		int32 BytesRead = 0;
		if (!ClientSocket->Recv(Chunk.GetData(), Chunk.Num(), BytesRead) || BytesRead <= 0)
		{
			break;
		}

		ReceiveBuffer.Append(Chunk.GetData(), BytesRead);
	}

	// TCP is a stream, not a sequence of messages: one read may contain half a
	// request or three of them. The newline is what restores the boundaries.
	int32 Newline = INDEX_NONE;
	while (ReceiveBuffer.Find(LineTerminator, Newline))
	{
		const FString Line = FString(FUTF8ToTCHAR(
			reinterpret_cast<const ANSICHAR*>(ReceiveBuffer.GetData()), Newline));

		ReceiveBuffer.RemoveAt(0, Newline + 1, EAllowShrinking::No);

		if (!Line.IsEmpty())
		{
			HandleLine(Line);
		}
	}
}

void URoboApiSubsystem::HandleLine(const FString& Line)
{
	TSharedPtr<FJsonObject> Request;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);

	TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();

	if (!FJsonSerializer::Deserialize(Reader, Request) || !Request.IsValid())
	{
		Response->SetBoolField(TEXT("ok"), false);
		Response->SetStringField(TEXT("error"), TEXT("malformed JSON"));
		SendResponse(Response);
		return;
	}

	// Echoed so a client can match replies to requests.
	Response->SetNumberField(TEXT("id"), Request->GetNumberField(TEXT("id")));

	const FString Operation = Request->GetStringField(TEXT("op"));

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	FString Error;

	if (Dispatch(Operation, Request, Result, Error))
	{
		Response->SetBoolField(TEXT("ok"), true);
		Response->SetObjectField(TEXT("result"), Result);
	}
	else
	{
		Response->SetBoolField(TEXT("ok"), false);
		Response->SetStringField(TEXT("error"), Error);
	}

	SendResponse(Response);
}

bool URoboApiSubsystem::Dispatch(const FString& Operation, const TSharedPtr<FJsonObject>& Request,
	TSharedRef<FJsonObject>& OutResult, FString& OutError)
{
	if (Operation == TEXT("ping"))
	{
		OutResult->SetStringField(TEXT("pong"), TEXT("RoboDrive"));
		return true;
	}

	if (Operation == TEXT("get_state"))
	{
		const URoboSimSubsystem* Sim = GetWorld()->GetSubsystem<URoboSimSubsystem>();
		if (Sim == nullptr)
		{
			OutError = TEXT("simulation subsystem unavailable");
			return false;
		}

		OutResult->SetNumberField(TEXT("sim_time"), Sim->GetSimTimeSeconds());
		OutResult->SetNumberField(TEXT("step"), static_cast<double>(Sim->GetStepCount()));
		OutResult->SetNumberField(TEXT("real_time_factor"), Sim->GetRealTimeFactor());

		for (TActorIterator<ARoboVehicle> It(GetWorld()); It; ++It)
		{
			OutResult->SetStringField(TEXT("robot"), It->GetName());
			OutResult->SetNumberField(TEXT("speed_ms"), It->GetSpeedMS());
			break;
		}

		return true;
	}

	OutError = FString::Printf(TEXT("unknown op '%s'"), *Operation);
	return false;
}

void URoboApiSubsystem::SendResponse(const TSharedRef<FJsonObject>& Response)
{
	if (ClientSocket == nullptr)
	{
		return;
	}
	
	FString Serialised;
	// Condensed, not pretty: pretty-printed JSON contains newlines, and the
	// newline is what separates one message from the next.
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Serialised);
	FJsonSerializer::Serialize(Response, Writer);
	Serialised.AppendChar(LineTerminator);
	
	const FTCHARToUTF8 Utf8(*Serialised);
	int32 BytesSent = 0;
	ClientSocket->Send(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), BytesSent);
}