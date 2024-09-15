// Copyright It's Rewind Time 2024

#include "RewindCode.h"
#include "Input.h"


FString EnumConvertTemp(EInputStates Enum)
{
	if (Enum == S) return TEXT("S");
	else if (Enum == A) return TEXT("A");
	else if (Enum == D) return TEXT("D");
	else if (Enum == NONE) return TEXT("None");
	return TEXT("W");
}

ARewindGameMode::ARewindGameMode()
{
	PlayerControllerClass = ARewindPlayerController::StaticClass();
	//DefaultPawnClass = nullptr;
}

void ARewindGameMode::PostLogin(APlayerController* InController)
{
	AGameModeBase::PostLogin(InController);

	GameManager = NewObject<UGameManager>(this);
	GameManager->PlayerController = Cast<ARewindPlayerController>(InController);
	GameManager->PlayerController->OnInputChanged.BindUObject(GameManager, &UGameManager::HandleInput);
	//more input bindings

	GameManager->PlayerController->GetPawn()->GetRootComponent()->SetMobility(EComponentMobility::Static);
}

//-----------------------------------------------------------------------------

UGameManager::UGameManager()
{
	if (!GetWorld()) return;

	WorldContext = GetWorld();

	//All this is temp
	UStaticMesh* BlockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EngineMeshes/Cube.Cube"));

	for (int32 Y = 0; Y < 10; ++Y) {
		for (int32 X = 0; X < 10; ++X) {
			FVector Location(X * BlockSize, Y * BlockSize, 0);
			Grid.Add(Location);
			AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
			Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Actor->GetStaticMeshComponent()->SetStaticMesh(BlockMesh);
		}
	}

	Player = GetWorld()->SpawnActor<APlayerEntity>(APlayerEntity::StaticClass(), FVector(0, 0, BlockSize), FRotator::ZeroRotator);
	Player->OnMoveFinished.BindUObject(this, &UGameManager::OnTurnEnd);
	Player->SetMobility(EComponentMobility::Movable);
	Player->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EditorMeshes/ArcadeEditorSphere.ArcadeEditorSphere")));
}

void UGameManager::HandleInput()
{
	UE_LOG(LogTemp, Warning, TEXT("HandleInput with %s"), *EnumConvertTemp(PlayerController->NewestInput));

	if (PlayerController->NewestInput != NONE) {
		if (bTurn) InputTimerStart = WorldContext->RealTimeSeconds;
		else ProcessTurn(NONE);
	}
}

void UGameManager::OnTurnEnd()
{
	UE_LOG(LogTemp, Warning, TEXT("Ending Turn"));
	bTurn = false;

	double Elapsed = InputTimerStart - WorldContext->RealTimeSeconds;
	InputTimerStart = 0;

	if (PlayerController->Stack.IsEmpty()) {
		if (Buffer != NONE && (Elapsed >= 0 && Elapsed < 0.1f)) {
			ProcessTurn(Buffer);
		}
	} else {
		ProcessTurn(NONE);
	}
}

void UGameManager::ProcessTurn(EInputStates Input)
{
	//ResolveMoment

	//Check rewind tile
	//check timeline collapse

	//Dispatch animations
	UE_LOG(LogTemp, Error, TEXT("Starting Turn with %s"), *EnumConvertTemp(PlayerController->NewestInput));
	bTurn = true;
	Buffer = NONE;


	FVector Offset;
	switch (Input == NONE ? PlayerController->NewestInput : Input) {
	case W:
		Offset = FVector(0, BlockSize, 0);
		break;
	case S:
		Offset = FVector(0, -BlockSize, 0);
		break;
	case A:
		Offset = FVector(BlockSize, 0, 0);
		break;
	case D:
		Offset = FVector(-BlockSize, 0, 0);
		break;
	}

	Player->Move(Player->GetActorLocation() + Offset);
}


APlayerEntity::APlayerEntity()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APlayerEntity::Tick(float DeltaTime)
{
	AEntity::Tick(DeltaTime);

	if (bIsMoving) {
		MoveTime += DeltaTime;
		float Alpha = FMath::Clamp(MoveTime / MoveDuration, 0.0f, 1.0f);

		FVector NewLocation = FMath::Lerp(Start, End, Alpha);
		SetActorLocation(NewLocation);

		if (Alpha >= 1.0f)
		{
			bIsMoving = false;
			OnMoveFinished.Execute();
		}
	}
}

void APlayerEntity::Move(FVector Target)
{
	bIsMoving = true;
	Start = GetActorLocation();
	End = Target;
	MoveTime = 0;
}


AEntity::AEntity()
{

}
