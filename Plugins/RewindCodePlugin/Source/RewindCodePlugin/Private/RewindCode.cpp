// Copyright It's Rewind Time 2024

#include "RewindCode.h"
#include "Input.h"

#include "Kismet/GameplayStatics.h"


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

//----------------------------------------------------------------------------

UGameManager::UGameManager()
{
	//This should all go out of constructor, into LoadLevel() maybe?
	if (!GetWorld()) return;

	WorldContext = GetWorld();

	Animator = NewObject<UEntityAnimator>(this);
	Animator->WorldContext = WorldContext;

	UStaticMesh* BlockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EngineMeshes/Cube.Cube"));
	UMaterial* RewindTileMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/RewindTileMaterial"));
	UMaterial* StartTileMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/StartTileMaterial"));

	StartGridPosition = FIntVector(0, 0, 3);

	Grid.Init(nullptr, WIDTH * LENGTH * HEIGHT);
	for (int32 Y = 0; Y < LENGTH; ++Y) 
	{
		for (int32 X = 0; X < WIDTH; ++X)
		{
			FVector Location(X * BLOCK_SIZE, Y * BLOCK_SIZE, 0);
			AEntity* Entity = WorldContext->SpawnActor<AEntity>(Location, FRotator::ZeroRotator);
			Entity->GridPosition = FIntVector(X, Y, 0);
			Entity->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Entity->GetStaticMeshComponent()->SetStaticMesh(BlockMesh);

			Grid[Flatten(Entity->GridPosition)] = Entity;

			if (X == 5 && Y == 5) {
				Entity->Flags |= REWIND;
				Entity->GetStaticMeshComponent()->SetMaterial(0, RewindTileMaterial);
			}
		}
	}

	for (int32 Y = 0; Y < 4; ++Y) 
	{
		for (int32 X = 0; X < 3; ++X) 
		{
			for (int32 Z = 1; Z < 3; Z++)
			{
				FIntVector IntLocation(X, Y, Z);
				FVector Location(X * BLOCK_SIZE, Y * BLOCK_SIZE, Z * BLOCK_SIZE);
				AEntity* Entity = WorldContext->SpawnActor<AEntity>(Location, FRotator::ZeroRotator);
				Entity->GridPosition = FIntVector(X, Y, Z);
				Entity->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				Entity->GetStaticMeshComponent()->SetStaticMesh(BlockMesh);

				Grid[Flatten(Entity->GridPosition)] = Entity;

				if (X == StartGridPosition.X && Y == StartGridPosition.Y && Z == StartGridPosition.Z - 1) {
					Entity->GetStaticMeshComponent()->SetMaterial(0, StartTileMaterial);
				}
			}
		}
	}

	Player = WorldContext->SpawnActor<APlayerEntity>(FVector(0, 0, BLOCK_SIZE * 3), FRotator::ZeroRotator);
	Player->GridPosition = FIntVector(0, 0, 3);
	Grid[Flatten(Player->GridPosition)] = Player;

	Player->Init(StartGridPosition);

	Player->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/RewindCube")));
	PlayerMI = UMaterialInstanceDynamic::Create(Player->GetStaticMeshComponent()->GetMaterial(0), Player);
	//PlayerMI->SetVectorParameterValue(FName(TEXT("Color")), COLORS[0]);
	PlayerMI->SetScalarParameterValue(FName(TEXT("Glow")), 10.f);
	Player->GetStaticMeshComponent()->SetMaterial(0, PlayerMI);
}

void UGameManager::HandleInput()
{
	if (bRewindQueued)
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't move with rewind queued"));
		return;
	}

	if (PlayerController->NewestInput != NONE) {
		if (Animator->bIsAnimating) InputTimerStart = WorldContext->RealTimeSeconds;
		else ProcessTurn(NONE);
	}
}

void UGameManager::OnTurnEnd()
{
	UE_LOG(LogTemp, Warning, TEXT("Ending Turn"));

	if (Animator->bIsAnimating) return;

	double Elapsed = InputTimerStart - WorldContext->RealTimeSeconds;
	InputTimerStart = 0;

	if (bRewindQueued)
	{
		UE_LOG(LogTemp, Warning, TEXT("IT'S REWIND TIME!!!!!"));
		DoRewind();
		return;
	}

	if (PlayerController->Stack.IsEmpty()) {
		if (Buffer != NONE && (Elapsed >= 0 && Elapsed < 0.1f)) {
			ProcessTurn(Buffer);
		}
	} else {
		ProcessTurn(NONE);
	}
}

void UGameManager::QueueRewind()
{
	bRewindQueued = true;
}

void UGameManager::DoRewind()
{
	bRewindQueued = false;

	TurnCounter = 0;

	APlayerEntity* NewPlayer = GetWorld()->SpawnActor<APlayerEntity>(APlayerEntity::StaticClass(), FVector(0, 0, BLOCK_SIZE * 3), FRotator::ZeroRotator);
	NewPlayer->Init(StartGridPosition);

	for (int i = 0; i < AllPlayers.Num(); i++)
	{
		AllPlayers[i]->Init(StartGridPosition);
	}

	Player->Init(StartGridPosition);

	NewPlayer->SetMobility(EComponentMobility::Movable);
	NewPlayer->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/RewindCube")));
	AllPlayers.Add(NewPlayer);

	UMaterialInstanceDynamic* NewPlayerMI = UMaterialInstanceDynamic::Create(NewPlayer->GetStaticMeshComponent()->GetMaterial(0), NewPlayer);
	//NewPlayerMI->SetVectorParameterValue(FName(TEXT("Color")), COLORS[(PastPlayers.Num() - 1) % COLORS.Num()]);
	NewPlayer->GetStaticMeshComponent()->SetMaterial(0, NewPlayerMI);

	//PlayerMI->SetVectorParameterValue(FName(TEXT("Color")), COLORS[PastPlayers.Num() % COLORS.Num()]);
}

void UGameManager::ProcessTurn(EInputStates Input)
{
	//ResolveMoment

	//Check rewind tile
	//check timeline collapse

	FIntVector MoveInput(0, 0, 0);
	switch (Input == NONE ? PlayerController->NewestInput : Input) {
		case W:
			MoveInput.Y = 1;
			break;
		case S:
			MoveInput.Y = -1;
			break;
		case A:
			MoveInput.X = 1;
			break;
		case D:
			MoveInput.X = -1;
			break;
	}

	AEntity* DestinationTile = Grid[Flatten(Player->GridPosition + MoveInput)];
	if (DestinationTile && !(DestinationTile->Flags & (MOVEABLE | REWIND))) return;

	Turn Turn;
	Turn.States.SetNum(AllPlayers.Num() + 1);

	EntityState State;
	State.Move = MoveInput;
	Turn.States[AllPlayers.Num()] = State;

	//Iterate through past players
	TArray<EntityPath> Paths;
	Paths.Reserve(AllPlayers.Num());

	for (int32 PlayerIndex = AllPlayers.Num() - 1; PlayerIndex >= 0; --PlayerIndex)
	{
		FIntVector CurrentMove = TurnCounter < Turns.Num() ? Turns[TurnCounter].States[PlayerIndex].Move : MoveInput;

		AEntity* CurrentPlayer = AllPlayers[PlayerIndex];

		//check moveable entity above "riding"
		UpdateEntityPosition(CurrentPlayer, CurrentMove);

		while (!Grid[Flatten(CurrentPlayer->GridPosition + FIntVector(0, 0, -1))])
		{
			UpdateEntityPosition(CurrentPlayer, FIntVector(0, 0, -1));
		}

		Turn.States[PlayerIndex].GridPosition = CurrentPlayer->GridPosition - Turn.States[PlayerIndex].GridPosition;
	}
	
	Turns.Add(Turn);
	++TurnCounter;

	//Dispatch animations
	Buffer = NONE;
	Animator->Start(Paths);
}

void UGameManager::UpdateEntityPosition(AEntity* Entity, const FIntVector& Delta)
{
	Grid[Flatten(Entity->GridPosition)] = nullptr;
	Grid[Flatten(Entity->GridPosition + Delta)] = Entity;
	Entity->GridPosition = Entity->GridPosition + Delta;
}

//------------------------------------------------------------------

APlayerEntity::APlayerEntity()
{
	Flags |= PLAYER | MOVEABLE;
}

void APlayerEntity::Init(FIntVector Loc)
{
	GridPosition = Loc;
}

//-----------------------------------------------------------------------------

EntityPath::EntityPath(AEntity* Entity, TArray<FIntVector>& InPath) : Entity(Entity)
{
	Path.Reserve(InPath.Num());

	for (FIntVector& GridLocation : InPath)
	{
		Path.Emplace(FVector(GridLocation) * 300/*BLOCKS_SIZE*/);
	}
}

void UEntityAnimator::Start(TArray<EntityPath>& Paths)
{
	Queue.Empty();
	//group paths

	Queue.Emplace(Paths);
	bIsAnimating = true;
}

void UEntityAnimator::Tick(float DeltaTime)
{
	bool bNextGroup = true;
	double CurrentTime = WorldContext->TimeSeconds;
	for (EntityPath& Path : Queue[QueueIndex])
	{
		if (Path.PathIndex == -1) {
			bNextGroup = false;
			continue;
		}

		float MoveTime = (Path.Path[Path.PathIndex + 1] - Path.Path[Path.PathIndex]).Z < 0 ? VerticalSpeed : HorizontalSpeed;
		float Alpha = FMath::Clamp((CurrentTime - Path.StartTime) / MoveTime, 0, 1);
		Path.Entity->SetActorLocation(FMath::Lerp(FVector(Path.Path[Path.PathIndex]), FVector(Path.Path[Path.PathIndex + 1]), Alpha));
		
		if (FMath::IsNearlyEqual(CurrentTime - Path.StartTime, MoveTime)) {
			Path.StartTime = 0;
			Path.PathIndex = ++Path.PathIndex == Path.Path.Num() - 1 ? -1 : Path.PathIndex;
		} else {
			Path.StartTime = CurrentTime;
		}
	}

	if (!bNextGroup) return;
	if (++QueueIndex == Queue.Num() - 1) {
		bIsAnimating = false;
		OnAnimationsFinished.Execute();
	}
}


//bool APlayerEntity::AttemptMove(EInputStates InputDir, TMap<FIntVector, UGameManager::ETileState> Grid, EntityState &State)
//{
//	State.Move = InputDir;
//
//	UEntityAnimatorComponent::EEntityAnimations Animation;
//	FIntVector IntDir;
//	switch (InputDir) {
//	case W:
//		Animation = UEntityAnimatorComponent::EEntityAnimations::FORWARD;
//		IntDir = FIntVector(0, 1, 0);
//		break;
//	case S:
//		Animation = UEntityAnimatorComponent::EEntityAnimations::BACKWARD;
//		IntDir = FIntVector(0, -1, 0);
//		break;
//	case A:
//		Animation = UEntityAnimatorComponent::EEntityAnimations::LEFT;
//		IntDir = FIntVector(1, 0, 0);
//		break;
//	case D:
//		Animation = UEntityAnimatorComponent::EEntityAnimations::RIGHT;
//		IntDir = FIntVector(-1, 0, 0);
//		break;
//	default:
//		return false;
//	}
//
//	FIntVector NewLoc = GridLocation + IntDir;
//
//	if (Grid.Contains(NewLoc) && HasCollision(Grid[NewLoc]))
//	{
//		UE_LOG(LogTemp, Warning, TEXT("COLLISION"));
//		return false;
//	}
//
//	State.DeltaPosition = IntDir * -1;
//
//	GridLocation = NewLoc;
//
//	Animator->QueueAnimation(Animation);
//
//	while (!Grid.Contains(GridLocation - FIntVector(0, 0, 1)) || !HasCollision(Grid[GridLocation - FIntVector(0, 0, 1)]))
//	{
//		Animator->QueueAnimation(UEntityAnimatorComponent::DOWN);
//		GridLocation -= FIntVector(0, 0, 1);
//
//		if (GridLocation.Z < -5) return false;
//
//	}
//
//	if (bIsActivePlayer && Grid.Contains(GridLocation - FIntVector(0, 0, 1)) && Grid[GridLocation - FIntVector(0, 0, 1)] == UGameManager::ETileState::REWIND_TILE) OnHitRewindTile.Execute();
//
//	return true;
//}