// Copyright It's Rewind Time 2024

#include "RewindCode.h"
#include "Input.h"

#include "Kismet/GameplayStatics.h"

#if 1
#define SLOG(x) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, x);
#define SLOGF(x) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, FString::SanitizeFloat(x));
#define POINT(x, c) DrawDebugPoint(GetWorld(), x, 10, c, false, 2.f);
#define LINE(x1, x2, c) DrawDebugPoint(GetWorld(), x1, x2, 10, c, false, 2.f);
#else
#define SLOG(x)
#define SLOGF(x)
#define POINT(x, c)
#define LINE(x1, x2, c)
#endif

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
	UMaterial* EndTileMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/EndTileMaterial"));

	StartGridPosition = FIntVector(0, 0, 0);

	/*Grid.Init(nullptr, WIDTH * LENGTH * HEIGHT);*/
	/*for (int32 Y = 0; Y < LENGTH; ++Y)
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
	}*/

	/*for (int32 Y = 0; Y < 1; ++Y)
	{
		for (int32 X = 0; X < 5; ++X)
		{
			for (int32 Z = 1; Z < 3; Z++)
			{
				if (X == 2 && Z == 2) continue;

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
				if (X == 4 && Z == 2) {
					Entity->GetStaticMeshComponent()->SetMaterial(0, EndTileMaterial);
				}
				if (X == 2 && Z == 1) {
					Entity->Flags |= REWIND;
					Entity->GetStaticMeshComponent()->SetMaterial(0, RewindTileMaterial);
				}
			}
		}
	}

	int X = 0;
	int Y = 1;
	int Z = 2;

	FIntVector IntLocation(X, Y, Z);
	FVector Location(X * BLOCK_SIZE, Y * BLOCK_SIZE, Z * BLOCK_SIZE);
	AEntity* Entity = WorldContext->SpawnActor<AEntity>(Location, FRotator::ZeroRotator);
	Entity->GridPosition = FIntVector(X, Y, Z);
	Entity->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Entity->GetStaticMeshComponent()->SetStaticMesh(BlockMesh);

	Grid[Flatten(Entity->GridPosition)] = Entity;*/
	

	LoadGridFromFile();


	Player = WorldContext->SpawnActor<APlayerEntity>(FVector(0, 0, BLOCK_SIZE * 3), FRotator::ZeroRotator);
	Player->SetMobility(EComponentMobility::Movable);
	Player->GridPosition = StartGridPosition;
	Grid[Flatten(Player->GridPosition)] = Player;

	Player->Flags |= CURRENT_PLAYER;

	Player->Init(StartGridPosition);

	Player->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/RewindCube")));
	PlayerMI = UMaterialInstanceDynamic::Create(Player->GetStaticMeshComponent()->GetMaterial(0), Player);
	//PlayerMI->SetVectorParameterValue(FName(TEXT("Color")), COLORS[0]);
	PlayerMI->SetScalarParameterValue(FName(TEXT("Glow")), 10.f);
	Player->GetStaticMeshComponent()->SetMaterial(0, PlayerMI);

	AllPlayers.Emplace(Player);

	Superposition = WorldContext->SpawnActor<ASuperposition>(FVector(BLOCK_SIZE * StartGridPosition.X, BLOCK_SIZE * StartGridPosition.Y, BLOCK_SIZE * StartGridPosition.Z), FRotator::ZeroRotator);
	Superposition->SetMobility(EComponentMobility::Movable);
	Superposition->SetActorHiddenInGame(true);
	Superposition->GridPosition = StartGridPosition;
	Superposition->Flags |= SUPER;

	Superposition->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/RewindCube")));
	Superposition->GetStaticMeshComponent()->SetMaterial(0, LoadObject<UMaterial>(nullptr, TEXT("/Game/Meshes/M_Superposition.M_Superposition")));

	Turns.Emplace(Turn()); //dummy to line up indices with turn counter
}

void UGameManager::HandleInput()
{
	//maybe change to state enum later
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
	UE_LOG(LogTemp, Warning, TEXT("Ending Turn %d"), TurnCounter);

	double Elapsed = InputTimerStart - WorldContext->RealTimeSeconds;
	InputTimerStart = 0;

	if (bRewindQueued) {
		UE_LOG(LogTemp, Warning, TEXT("IT'S REWIND TIME!!!!!"));
		DoRewind();
		return;
	}

	//check timeline collapse
	//

	//Process buffer
	if (PlayerController->Stack.IsEmpty()) {
		if (Buffer != NONE && (Elapsed >= 0 && Elapsed < 0.1f)) {
			ProcessTurn(Buffer);
		}
	}
	else {
		ProcessTurn(NONE);
	}
}

void UGameManager::QueueRewind()
{
	bRewindQueued = true;
}

void UGameManager::DoRewind()
{
	//Do animations
	//animations should update grid as well
	for (AEntity* P : AllPlayers)
	{
		Grid[Flatten(P->GridPosition)] = nullptr;
	}


	//Start new timeline
	bRewindQueued = false;
	TurnCounter = 0;

	Player->Flags &= (~CURRENT_PLAYER);

	Player = WorldContext->SpawnActor<APlayerEntity>(FVector(BLOCK_SIZE * StartGridPosition.X, BLOCK_SIZE * StartGridPosition.Y, BLOCK_SIZE * StartGridPosition.Z), FRotator::ZeroRotator);
	Player->GridPosition = StartGridPosition;

	Player->Flags |= CURRENT_PLAYER;
	Player->Init(StartGridPosition);

	Player->SetMobility(EComponentMobility::Movable);
	Player->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/RewindCube")));

	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Player->GetStaticMeshComponent()->GetMaterial(0), Player);
	Material->SetVectorParameterValue(FName(TEXT("Color")), FColor::MakeRandomColor());
	Player->GetStaticMeshComponent()->SetMaterial(0, Material);

	AllPlayers.Emplace(Player);
	for (APlayerEntity* P : AllPlayers)
	{
		P->Flags |= SUPER;

		P->Init(StartGridPosition); //do we need an init function???
		P->SetActorHiddenInGame(true);
	}

	//Superposition->SetActorLocation(FVector(0, 0, 900));
	Superposition->SetActorHiddenInGame(false);
	Superposition->bUpdated = false;
	Superposition->GridPosition = StartGridPosition;
	Grid[Flatten(Superposition->GridPosition)] = Superposition;
}

void UGameManager::ProcessTurn(EInputStates Input)
{
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
		if (!Entity /*|| (Entity->Flags & SUPER)*/) break;
		if (!(Entity->Flags & MOVEABLE)) return;
	}

	Turn* CurrentTurn = nullptr;
	if (++TurnCounter >= Turns.Num()) {
		Turn& NewTurn = Turns.Emplace_GetRef();
		NewTurn.SubTurns.Reserve(AllPlayers.Num());
		CurrentTurn = &NewTurn;
	}
	else {
		CurrentTurn = &Turns[TurnCounter];
	}

	int32 SubTurnIndex = CurrentTurn->SubTurns.Emplace(struct SubTurn(Player, MoveInput)); //Record input state for current player

	//Check superposition, need to check position
	//evluate mvmt for  all entities not in superposition first to see final location of superposition
	if (SubTurnIndex != 0) {
		for (int32 i = 0; i < SubTurnIndex; ++i)
		{
			SubTurn& SubTurn = CurrentTurn->SubTurns[i];
			if ((SubTurn.Player->Flags & SUPER) && (SubTurn.Move != CurrentTurn->SubTurns[i - 1].Move)) {
			}
		}
	}
	if ((Player->Flags & SUPER) && (SubTurnIndex != 0) && (CurrentTurn->SubTurns[SubTurnIndex].Move != CurrentTurn->SubTurns[SubTurnIndex - 1].Move)) {
		Player->Flags &= ~SUPER;
		if (SubTurnIndex == 1) {
			CurrentTurn->SubTurns[0].Player->Flags &= ~SUPER;
			CurrentTurn->SubTurns[0].Player->SetActorHiddenInGame(false);
			CurrentTurn->SubTurns[0].Player->SetActorLocation(Superposition->GetActorLocation());

			Superposition->SetActorHiddenInGame(true);
		}

		//Queue superposition-collapse animation
		Player->SetActorHiddenInGame(false);
		Player->SetActorLocation(Superposition->GetActorLocation());

		CurrentTurn->SubTurns[SubTurnIndex].bSuperCollapse = true;
	}

	for (int32 i = 0; i < SubTurnIndex; ++i)
	{
		if (CurrentTurn->SubTurns[i].bSuperCollapse) {
			CurrentTurn->SubTurns[i].Player->Flags &= ~SUPER;
			CurrentTurn->SubTurns[i].Player->SetActorHiddenInGame(false);
			CurrentTurn->SubTurns[i].Player->SetActorLocation(Superposition->GetActorLocation());

			if (i == 1) {
				CurrentTurn->SubTurns[0].Player->Flags &= ~SUPER;
				CurrentTurn->SubTurns[0].Player->SetActorHiddenInGame(false);
				CurrentTurn->SubTurns[0].Player->SetActorLocation(Superposition->GetActorLocation());

				Superposition->SetActorHiddenInGame(true);
			}

			break;
		}
	}

	Superposition->bUpdated = false; //idk where to put this best?

	//Evaluate players
	for (int32 i = CurrentTurn->SubTurns.Num() - 1; i >= 0; --i)
	{
		//make function in struct
		CurrentTurn->SubTurns[i].Entities.Reset();
		CurrentTurn->SubTurns[i].PathIndices.Reset();
		CurrentTurn->SubTurns[i].AllPaths.Reset();

		MoveEntity(CurrentTurn->SubTurns[i]);
	}

	Buffer = NONE;

	//Dispatch animations
	Animator->Start(*CurrentTurn);
}

AEntity* UGameManager::QueryAt(const FIntVector& Location)
{
	if ((Location.X < 0 || Location.X > WIDTH) &&
		(Location.Y < 0 || Location.Y > LENGTH) &&
		(Location.Z < 0 || Location.Z > HEIGHT)) return nullptr;

	return Grid[Flatten(Location)];
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
		if (!Entity || Entity->IsA<ASuperposition>()) break;
		else if (!(Entity->Flags & MOVEABLE)) return;

		Connected.Emplace(Entity);
	}

	//Update from farthest to self
	bool bSuperUpdated = false;
	for (int32 i = Connected.Num() - 1; i >= 0; --i)
	{
		//Update from bottom up, and evaluate horizontal movement before gravity
		FIntVector Base = Connected[i]->GridPosition;

		bSuperUpdated |= UpdateEntityPosition(SubTurn, Connected[i], SubTurn.Move);

		AEntity* QueryV = QueryAt(Connected[i]->GridPosition + FIntVector(0, 0, -1));//Grid[Flatten(Connected[i]->GridPosition + FIntVector(0, 0, -1))];
		while (!QueryV || QueryV->IsA<ASuperposition>())
		{
			bSuperUpdated |= UpdateEntityPosition(SubTurn, Connected[i], FIntVector(0, 0, -1));
			QueryV = Grid[Flatten(Connected[i]->GridPosition + FIntVector(0, 0, -1))];
		}

		for (;;)
		{
<<<<<<< HEAD
			AEntity* QueryH = QueryAt(Base += FIntVector(0, 0, 1));//Grid[Flatten(Base += FIntVector(0, 0, 1))];
=======
			AEntity* QueryH = Grid[Flatten(Base += FIntVector(0, 0, 1))];
>>>>>>> 49a1fba93e42dcc8c07ce5a22c2ea583fc5998cd
			if (!QueryH || !(QueryH->Flags & MOVEABLE) || (QueryH->IsA<ASuperposition>())) break; //how to deal with superposition???

			bSuperUpdated |= UpdateEntityPosition(SubTurn, QueryH, SubTurn.Move);
			QueryV = Grid[Flatten(QueryH->GridPosition + FIntVector(0, 0, -1))];
			while (!QueryV || QueryV->IsA<ASuperposition>())
			{
				bSuperUpdated |= UpdateEntityPosition(SubTurn, QueryH, FIntVector(0, 0, -1));
				QueryV = Grid[Flatten(QueryH->GridPosition + FIntVector(0, 0, -1))];
			}
		}
	}

	Superposition->bUpdated = bSuperUpdated;
}

bool UGameManager::UpdateEntityPosition(SubTurn& SubTurn, AEntity* Entity, const FIntVector& Delta)
{
	if (Entity->Flags & SUPER) {
		Entity->GridPosition = Entity->GridPosition + Delta;

		if (!Superposition->bUpdated) {
			Grid[Flatten(Superposition->GridPosition)] = nullptr;
			Grid[Flatten(Superposition->GridPosition + Delta)] = Superposition;
			Superposition->GridPosition = Superposition->GridPosition + Delta;

			int32 Index = SubTurn.AllPaths.Emplace(Superposition->GridPosition);

			if (SubTurn.Entities.IsEmpty() || SubTurn.Entities.Last() != Superposition) {
				SubTurn.Entities.Emplace(Superposition);
				SubTurn.PathIndices.Emplace(Index);
			}
		}

		return true;
	}
	else {
		Grid[Flatten(Entity->GridPosition)] = nullptr;
		Grid[Flatten(Entity->GridPosition + Delta)] = Entity;
		Entity->GridPosition = Entity->GridPosition + Delta;

		AEntity* Query = Grid[Flatten(Entity->GridPosition + FIntVector(0, 0, -1))];
		if (Query && (Query->Flags & REWIND) && (Entity->Flags & CURRENT_PLAYER)) {
			QueueRewind();
		}
		if (Query && (Query->Flags & GOAL)) {
			SLOG("YOU WIN!")
		}

		int32 Index = SubTurn.AllPaths.Emplace(Entity->GridPosition);

		if (SubTurn.Entities.IsEmpty() || (SubTurn.Entities.Last() != Entity)) {
			SubTurn.Entities.Emplace(Entity);
			SubTurn.PathIndices.Emplace(Index);
		}

		return false;
	}
}

void UGameManager::LoadGridFromFile()
{

	FString FilePath = FPaths::ProjectContentDir() / TEXT("Grids/MainLevelGrid.txt");
	FString FileContent;
	if (FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		// File successfully read
		UE_LOG(LogTemp, Warning, TEXT("File content: %s"), *FileContent);
		int a = 0;
		int b = 0;
		while (FileContent.Mid(b, 1) != ",") b++;
		int X = FCString::Atoi(*FileContent.Mid(a, b - a));
		b += 1;
		a = b;
		while (FileContent.Mid(b, 1) != ",") b++;
		int Y = FCString::Atoi(*FileContent.Mid(a, b - a));
		b += 1;
		a = b;
		while (FileContent.Mid(b, 1) != "\n") b++;
		int Z = FCString::Atoi(*FileContent.Mid(a, b - a));

		WIDTH = X;
		LENGTH = Y;
		HEIGHT = Z + 2;

		Grid.Init(nullptr, LENGTH * WIDTH * HEIGHT);

		int i = b + 2;


		UStaticMesh* BlockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EngineMeshes/Cube.Cube"));
		UMaterial* RewindTileMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/RewindTileMaterial"));
		UMaterial* StartTileMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/StartTileMaterial"));
		UMaterial* EndTileMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/EndTileMaterial"));

		for (int z = 0; z < HEIGHT - 2; z++) {
			for (int x = 0; x < WIDTH; x++) {
				for (int y = 0; y < LENGTH; y++) {
				
					//if (FileContent.Mid(i, 1) == " ") i++;
					//if (FileContent.Mid(i, 1) == "\n") i ++;
					//if (FileContent.Mid(i, 1) == "\n") i ++;

					while (FileContent.Mid(i, 1) != "0" &&
							FileContent.Mid(i, 1) != "1" &&
							FileContent.Mid(i, 1) != "2" &&
							FileContent.Mid(i, 1) != "3" &&
							FileContent.Mid(i, 1) != "4") i++;

					FString val = FileContent.Mid(i, 1);

					if (val != "0") {

						FIntVector IntLocation(x, y, z);
						FVector Location(x * BLOCK_SIZE, y * BLOCK_SIZE, z * BLOCK_SIZE);
						AEntity* Entity = WorldContext->SpawnActor<AEntity>(Location, FRotator::ZeroRotator);
						Entity->GridPosition = FIntVector(x, y, z);
						Entity->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
						Entity->GetStaticMeshComponent()->SetStaticMesh(BlockMesh);

						Grid[Flatten(Entity->GridPosition)] = Entity;

						if (val == "2") {
							Entity->GetStaticMeshComponent()->SetMaterial(0, StartTileMaterial);
							StartGridPosition = FIntVector(x, y, z + 1);
						}
						else if (val == "3") {
							Entity->GetStaticMeshComponent()->SetMaterial(0, EndTileMaterial);
							Entity->Flags |= GOAL;
						}
						else if (val == "4") {
							Entity->Flags |= REWIND;
							Entity->GetStaticMeshComponent()->SetMaterial(0, RewindTileMaterial);
						}
					}

					i++;

				}
			}
		}

	}
	else
	{
		// Error reading file
		UE_LOG(LogTemp, Error, TEXT("Failed to read file."));
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
	SetActorLocation(FVector(GridPosition) * 300);
}

//-----------------------------------------------------------------------------

void UEntityAnimator::Start(const Turn& Turn)
{
	Queue.Reset();
	QueueIndices.Reset();
	QueueIndex = 0;

	double CurrentTime = WorldContext->TimeSeconds;

	for (int32 SubTurnIndex = Turn.SubTurns.Num() - 1; SubTurnIndex >= 0; --SubTurnIndex)
	{
		const SubTurn& SubTurn = Turn.SubTurns[SubTurnIndex];
		if (SubTurn.Entities.IsEmpty()) continue;

		QueueIndices.Emplace(Queue.Num());

		int32 EntityIndex = 0;
		for (; EntityIndex < SubTurn.Entities.Num(); ++EntityIndex)
		{
			EntityAnimationPath& Path = Queue.Emplace_GetRef(EntityAnimationPath(SubTurn.Entities[EntityIndex]));

			int32 EndIndex = EntityIndex == SubTurn.PathIndices.Num() - 1 ? SubTurn.AllPaths.Num() : SubTurn.PathIndices[EntityIndex + 1];
			for (int32 PathIndex = SubTurn.PathIndices[EntityIndex]; PathIndex < EndIndex; ++PathIndex)
			{
				Path.Path.Emplace(FVector(SubTurn.AllPaths[PathIndex]) * 300/*BLOCKS_SIZE*/);
			}
		}
	}

	if (QueueIndices.IsEmpty()) return;
	//QueueIndices.Shrink();

	//Group adjacent "subturns" if all entities' paths are non-intersecting
	int32 Index = 0;
	for (; false;)
	{

	}

	bIsAnimating = true;
}

void UEntityAnimator::Tick(float DeltaTime)
{
	bool bNextGroup = true;
	double CurrentTime = WorldContext->TimeSeconds;

	int32 StartIndex = QueueIndices[QueueIndex];
	int32 EndIndex = QueueIndex == QueueIndices.Num() - 1 ? Queue.Num() : QueueIndices[QueueIndex + 1];
	for (int32 i = StartIndex; i < EndIndex; ++i)
	{

		EntityAnimationPath& Animation = Queue[i];
		if (Animation.PathIndex == -1) continue;

		if (Animation.PathIndex == -2) {
			Animation.PathIndex = 0;
			Animation.StartTime = CurrentTime;
			Animation.StartLocation = Animation.Entity->GetActorLocation();
		}

		float MoveTime = (Animation.Path[Animation.PathIndex] - Animation.StartLocation).Z < 0 ? VerticalSpeed : HorizontalSpeed; //should be saved in animation struct
		float Alpha = FMath::Clamp((CurrentTime - Animation.StartTime) / MoveTime, 0, 1);
		Animation.Entity->SetActorLocation(FMath::Lerp(Animation.StartLocation, Animation.Path[Animation.PathIndex], Alpha));

		if (CurrentTime - Animation.StartTime >= MoveTime) {
			Animation.PathIndex = Animation.PathIndex + 1 == Animation.Path.Num() ? -1 : ++Animation.PathIndex;
			Animation.StartTime = CurrentTime;
			Animation.StartLocation = Animation.Entity->GetActorLocation();
		}

		bNextGroup = false;
	}

	if (bNextGroup) {
		if (++QueueIndex == QueueIndices.Num()) {
			bIsAnimating = false;
			OnAnimationsFinished.Execute();
		}
	}
}