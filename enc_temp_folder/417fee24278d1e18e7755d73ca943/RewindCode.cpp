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

	Animator = CreateDefaultSubobject<UEntityAnimator>(TEXT("Animator"));
	Animator->WorldContext = WorldContext;
	Animator->OnAnimationsFinished.BindUObject(this, &UGameManager::OnTurnEnd);

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
	//bRewindQueued = false;

	//TurnCounter = 0;

	//APlayerEntity* NewPlayer = GetWorld()->SpawnActor<APlayerEntity>(APlayerEntity::StaticClass(), FVector(0, 0, BLOCK_SIZE * 3), FRotator::ZeroRotator);
	//NewPlayer->Init(StartGridPosition);

	//for (int i = 0; i < AllPlayers.Num(); i++)
	//{
	//	AllPlayers[i]->Init(StartGridPosition);
	//}

	//Player->Init(StartGridPosition);

	//NewPlayer->SetMobility(EComponentMobility::Movable);
	//NewPlayer->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/RewindCube")));
	//AllPlayers.Add(NewPlayer);

	//UMaterialInstanceDynamic* NewPlayerMI = UMaterialInstanceDynamic::Create(NewPlayer->GetStaticMeshComponent()->GetMaterial(0), NewPlayer);
	////NewPlayerMI->SetVectorParameterValue(FName(TEXT("Color")), COLORS[(PastPlayers.Num() - 1) % COLORS.Num()]);
	//NewPlayer->GetStaticMeshComponent()->SetMaterial(0, NewPlayerMI);

	////PlayerMI->SetVectorParameterValue(FName(TEXT("Color")), COLORS[PastPlayers.Num() % COLORS.Num()]);
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

	//Check if input results in a successful movement, otherwise don't advance turn
	FIntVector GridPosition = Player->GridPosition;
	for (;;)
	{
		AEntity* Entity = Grid[Flatten(GridPosition += MoveInput)];
		if (!Entity) break; 
		if (!(Entity->Flags & MOVEABLE)) return;
	}

	Turn* CurrentTurn = nullptr;
	if (++TurnCounter >= Turns.Num()) {
		Turn& NewTurn = Turns.Emplace_GetRef();
		NewTurn.SubTurns.Reserve(PastPlayers.Num() + 1);
		CurrentTurn = &NewTurn;
	} else {
		CurrentTurn = &Turns[TurnCounter];
	}

	SubTurn& SubTurn = CurrentTurn->SubTurns.Emplace_GetRef(struct SubTurn(Player, MoveInput)); //Record input state for current player

	//Evaluate players
	for (int i = CurrentTurn->SubTurns.Num() - 1; i >= 0; --i)
	{
		MoveEntity(CurrentTurn->SubTurns[i]);
	}

	//Dispatch animations
	Animator->Start(*CurrentTurn);

	Buffer = NONE;
}

//TODO: Remember to hardcode grid math
//TODO: Bounds checks + gravity infinite loop
void UGameManager::MoveEntity(SubTurn& SubTurn)
{
	//Query in direction of movement until wall or air
	TArray<AEntity*> Connected;
	Connected.Emplace(SubTurn.Player);

	FIntVector GridPosition = SubTurn.Player->GridPosition;
	for (;;) 
	{
		AEntity* Entity = Grid[Flatten(GridPosition += SubTurn.Move)];
		if (!Entity) break;
		else if (!(Entity->Flags & MOVEABLE)) return;

		Connected.Emplace(Entity);
	}

	//Update from farthest to self
	for (int32 i = Connected.Num() - 1; i >= 0; --i)
	{
		//Update from bottom up, and evaluate horizontal movement before gravity
		UpdateEntityPosition(SubTurn, Connected[i], SubTurn.Move);
		while (!Grid[Flatten(Connected[i]->GridPosition + FIntVector(0, 0, -1))])
		{
			UpdateEntityPosition(SubTurn, Connected[i], FIntVector(0, 0, -1));
		}

		for (;;)
		{
			AEntity* Entity = Grid[Flatten(Connected[i]->GridPosition + FIntVector(0, 0, 1))];
			if (!Entity || !(Entity->Flags & MOVEABLE)) break;

			UpdateEntityPosition(SubTurn, Entity, SubTurn.Move);
			while (!Grid[Flatten(Entity->GridPosition + FIntVector(0, 0, -1))])
			{
				UpdateEntityPosition(SubTurn, Entity, FIntVector(0, 0, -1));
			}
		}
	}
}

void UGameManager::UpdateEntityPosition(SubTurn& SubTurn, AEntity* Entity, const FIntVector& Delta)
{
	Grid[Flatten(Entity->GridPosition)] = nullptr;
	Grid[Flatten(Entity->GridPosition + Delta)] = Entity;
	Entity->GridPosition = Entity->GridPosition + Delta;

	int32 Index = SubTurn.AllPaths.Emplace(Entity->GridPosition);

	if (SubTurn.Entities.IsEmpty() || (SubTurn.Entities.Last() != Entity)) {
		SubTurn.Entities.Emplace(Entity);
		SubTurn.PathIndices.Emplace(Index);
	}
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

void UEntityAnimator::Start(const Turn& Turn)
{
	UE_LOG(LogTemp, Warning, TEXT("Start Animation"));

	Queue.Reset();
	QueueIndices.Reset();
	QueueIndex = 0;

	for (int32 SubTurnIndex = Turn.SubTurns.Num() - 1; SubTurnIndex >= 0; --SubTurnIndex)
	{
		const SubTurn& SubTurn = Turn.SubTurns[SubTurnIndex];
		int32 EntityIndex = 0;
		for (;EntityIndex < SubTurn.Entities.Num(); ++EntityIndex)
		{
			EntityAnimationPath& Path = Queue.Emplace_GetRef(EntityAnimationPath(SubTurn.Entities[EntityIndex]));

			int32 EndIndex = EntityIndex < SubTurn.PathIndices.Num() - 1 ? SubTurn.PathIndices[EntityIndex + 1] : SubTurn.AllPaths.Num();
			for (int32 PathIndex = SubTurn.PathIndices[EntityIndex]; PathIndex < EndIndex; ++PathIndex)
			{
				Path.Path.Emplace(FVector(SubTurn.AllPaths[PathIndex]) * 300/*BLOCKS_SIZE*/);
			}
		}

		QueueIndices.Emplace(EntityIndex + 1);
		UE_LOG(LogTemp, Warning, TEXT("Add Animation Path"));
	}

	//Group adjacent "subturns" if all entities' paths are non-intersecting
	int32 Index = 0;
	for (;false;)
	{

	}

	bIsAnimating = true;
}

void UEntityAnimator::Tick(float DeltaTime)
{
	bool bNextGroup = true;
	double CurrentTime = WorldContext->TimeSeconds;

	int32 EndIndex = QueueIndex < QueueIndices.Num() - 1 ? QueueIndices[QueueIndex] : Queue.Num();
	for (int32 i = QueueIndices[QueueIndex]; i < EndIndex; ++i)
	{
		if (Queue[i].PathIndex != -1) {
			bNextGroup = false;
		} else {
			float MoveTime = (Queue[i].Path[Queue[i].PathIndex + 1] - Queue[i].Path[Queue[i].PathIndex]).Z < 0 ? VerticalSpeed : HorizontalSpeed;
			float Alpha = FMath::Clamp((CurrentTime - Queue[i].StartTime) / MoveTime, 0, 1);
			Queue[i].Entity->SetActorLocation(FMath::Lerp(FVector(Queue[i].Path[Queue[i].PathIndex]), FVector(Queue[i].Path[Queue[i].PathIndex + 1]), Alpha));

			if (FMath::IsNearlyEqual(CurrentTime - Queue[i].StartTime, MoveTime)) {
				Queue[i].StartTime = 0;
				Queue[i].PathIndex = Queue[i].PathIndex + 1 == Queue[i].Path.Num() - 1 ? -1 : ++Queue[i].PathIndex;
			}
			else {
				Queue[i].StartTime = CurrentTime;
			}
		}
	}

	if (bNextGroup) {
		if (++QueueIndex == QueueIndices.Num()) {
			UE_LOG(LogTemp, Warning, TEXT("Stop Animating"));
			bIsAnimating = false;
			OnAnimationsFinished.Execute();
		}
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