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

	//Move everything below here out to somewhere else
	LoadGridFromFile();

	APlayerEntity* Player = WorldContext->SpawnActor<APlayerEntity>(FVector(StartGridLocation) * BLOCK_SIZE, FRotator::ZeroRotator);
	Player->SetMobility(EComponentMobility::Movable);

	Players.Emplace(Player);
	Player->GridLocation = StartGridLocation;
	Grid.SetAt(Player->GridLocation, Player);

	Player->Flags |= CURRENT_PLAYER;

	Player->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/RewindCube")));
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Player->GetStaticMeshComponent()->GetMaterial(0), Player);
	Material->SetScalarParameterValue(FName(TEXT("Glow")), 10.f);
	Player->GetStaticMeshComponent()->SetMaterial(0, Material);


	ASuperposition* Superposition = WorldContext->SpawnActor<ASuperposition>(FVector(StartGridLocation) * BLOCK_SIZE, FRotator::ZeroRotator);
	Superpositions.Emplace(Superposition);
	Superposition->SetMobility(EComponentMobility::Movable);
	Superposition->SetActorHiddenInGame(true);

	Superposition->GridLocation = StartGridLocation;

	Superposition->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/RewindCube")));
	Superposition->GetStaticMeshComponent()->SetMaterial(0, LoadObject<UMaterial>(nullptr, TEXT("/Game/Meshes/M_Superposition.M_Superposition")));

	Turns.Emplace(Turn()); //dummy to line up indices with turn counter
}

void UGameManager::HandleInput()
{
	//maybe change to state enum later
	if (!RewindQueue.IsEmpty())
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

	if (!RewindQueue.IsEmpty()) {
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

void UGameManager::DoRewind()
{
	//Do animations
	//animations should update grid as well
	for (AEntity* P : Players)
	{
		Grid.SetAt(P->GridLocation, nullptr);
	}


	//Start new timeline
	RewindQueue.Empty();
	TurnCounter = 0;

	APlayerEntity* Player = Players.Last();
	Player->Flags &= (~CURRENT_PLAYER);

	Player = WorldContext->SpawnActor<APlayerEntity>(FVector(StartGridLocation) * BLOCK_SIZE, FRotator::ZeroRotator);
	Player->SetMobility(EComponentMobility::Movable);

	Players.Emplace(Player);
	Player->GridLocation = StartGridLocation;

	Player->Flags |= CURRENT_PLAYER;

	Player->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/RewindCube")));
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Player->GetStaticMeshComponent()->GetMaterial(0), Player);
	Material->SetVectorParameterValue(FName(TEXT("Color")), FColor::MakeRandomColor());
	Player->GetStaticMeshComponent()->SetMaterial(0, Material);


	if (Superpositions.IsEmpty()) {
		ASuperposition* Superposition = WorldContext->SpawnActor<ASuperposition>(FVector(StartGridLocation) * BLOCK_SIZE, FRotator::ZeroRotator);
		Superpositions.Emplace(Superposition);
		Superposition->SetMobility(EComponentMobility::Movable);
		Superposition->SetActorHiddenInGame(true);

		Superposition->GridLocation = StartGridLocation;

		Superposition->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/RewindCube")));
		Superposition->GetStaticMeshComponent()->SetMaterial(0, LoadObject<UMaterial>(nullptr, TEXT("/Game/Meshes/M_Superposition.M_Superposition")));
	}
	Superpositions[0]->SetActorLocation(FVector(StartGridLocation) * BLOCK_SIZE);
	Superpositions[0]->SetActorHiddenInGame(false);
	Superpositions[0]->GridLocation = StartGridLocation;
	Grid.SetAt(StartGridLocation, Superpositions[0]);

	for (APlayerEntity* P : Players)
	{
		P->Flags |= SUPER;
		P->SetActorHiddenInGame(true);
		P->Superposition = Superpositions[0];
	}
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
	AEntity* CurrentPlayer = Players.Last();
	for (FIntVector GridLocation = CurrentPlayer->GridLocation;;)
	{
		AEntity* Entity = Grid.QueryAt(GridLocation += MoveInput);
		if (!Entity) break;
		if (!(Entity->Flags & MOVEABLE)) return;
	}

	//Allocate new turn object if current timeline is the longest so far
	Turn* CurrentTurn = nullptr;
	if (++TurnCounter >= Turns.Num()) {
		Turn& NewTurn = Turns.Emplace_GetRef();
		NewTurn.SubTurns.Reserve(Players.Num());
		CurrentTurn = &NewTurn;
	}
	else {
		CurrentTurn = &Turns[TurnCounter];
	}

	int32 SubTurnIndex = CurrentTurn->SubTurns.Emplace(struct SubTurn(CurrentPlayer, MoveInput)); //Record input state for current player

	//Evaluate subturns
	for (int32 i = CurrentTurn->SubTurns.Num() - 1; i >= 0; --i)
	{
		CurrentTurn->SubTurns[i].Entities.Reset();
		CurrentTurn->SubTurns[i].PathIndices.Reset();
		CurrentTurn->SubTurns[i].AllPaths.Reset();

		EvaluateSubTurn(CurrentTurn->SubTurns[i]);
	}

	//This is kinda retarted but it does its job
	TMap<FIntVector, TArray<APlayerEntity*>> SuperGroups;
	for (APlayerEntity* Player : Players)
	{	
		if (Player->Flags & SUPER) {
			SuperGroups.FindOrAdd(Player->GridLocation).Emplace(Player);
		}
	}

	for (ASuperposition* Superposition : Superpositions)
	{
		Superposition->bOccupied = false;
	}

	for (const auto& Pair : SuperGroups)
	{
		if (Pair.Value.Num() > 1) {
			ASuperposition* Superposition = Pair.Value[0]->Superposition;
			if (Superposition->bOccupied) {
				//spawn another one
			}
			
			int32 SubturnIndex = 0; //Superposition has precedence of its most recent player
			for (APlayerEntity* Player : Pair.Value)
			{
				Player->Superposition = Superposition;
				SubturnIndex = FMath::Max(Players.Find(Player), SubTurnIndex);
			}

			Grid.SetAt(Superposition->GridLocation, nullptr);
			Grid.SetAt(Pair.Value[0]->GridLocation, Superposition);
			Superposition->GridLocation = Pair.Value[0]->GridLocation;

			SubTurn& Subturn = CurrentTurn->SubTurns[SubturnIndex];
			int32 Index = Subturn.AllPaths.Emplace(Superposition->GridLocation);

			if (Subturn.Entities.IsEmpty() || Subturn.Entities.Last() != Superposition) {
				Subturn.Entities.Emplace(Superposition);
				Subturn.PathIndices.Emplace(Index);
			}
		}
		else {
			APlayerEntity* Player = Pair.Value[0];
			Player->Flags &= ~SUPER;

			//Queue superposition-collapse animation
			Player->SetActorHiddenInGame(false);
			Player->SetActorLocation(Player->Superposition->GetActorLocation());
			
			//should make re-use queue later instead of destroying
			if (!Player->Superposition->bOccupied) {
				Superpositions.RemoveSingle(Player->Superposition);
				Player->Superposition->Destroy();
			}
			Player->Superposition = nullptr;

			//add animation path???

		}
	}

	//remove any animation paths after any of these end states
	//look through rewind queue

	Buffer = NONE;

	//Dispatch animations
	Animator->Start(*CurrentTurn);
}

static const FIntVector DownVector(0, 0, -1), UpVector(0, 0, 1);

void UGameManager::EvaluateSubTurn(SubTurn& SubTurn)
{
	//Query in direction of movement until wall or air
	TArray<AEntity*> Connected;
	Connected.Emplace(SubTurn.Player);
	for (FIntVector GridLocation = SubTurn.Player->GridLocation;;)
	{
		AEntity* Entity = Grid.QueryAt(GridLocation += SubTurn.Move);
		if (!Entity) break;
		if (!(Entity->Flags & MOVEABLE)) return;

		if (Connected.Num() > 1) {
			AEntity* Prev = Connected[Connected.Num() - 2];
			if (EvaluateSuperposition(SubTurn, Entity, Prev)) return;
		}

		Connected.Emplace(Entity);
	}

	//Update from farthest to self
	for (int32 i = Connected.Num() - 1; i >= 0; --i)
	{
		//Update from bottom up, and evaluate horizontal movement before gravity
		FIntVector Base = Connected[i]->GridLocation;

		UpdateEntityPosition(SubTurn, Connected[i], SubTurn.Move);

		for (AEntity* Below = Grid.QueryAt(Connected[i]->GridLocation + DownVector)
			;; Below = Grid.QueryAt(Connected[i]->GridLocation + DownVector))
		{
			if (Below && EvaluateSuperposition(SubTurn, Below, Connected[i])) break;

			UpdateEntityPosition(SubTurn, Connected[i], DownVector);

			if (Connected[i]->GridLocation.Z == HEIGHT_MIN) {
				//do stuff like stop rendering, play fade animation etc...
				UE_LOG(LogTemp, Warning, TEXT("Fell off da world"));
				break;
			}
		}

		for (;;)
		{
			AEntity* Entity = Grid.QueryAt(Base += UpVector);
			if (!Entity || !(Entity->Flags & MOVEABLE)) break;

			AEntity* Front = Grid.QueryAt(Base + SubTurn.Move);
			if (Front && !(Front->Flags & MOVEABLE)) break;

			UpdateEntityPosition(SubTurn, Entity, SubTurn.Move);
	
			for (AEntity* Below = Grid.QueryAt(Entity->GridLocation + DownVector);
				 !Below; Below = Grid.QueryAt(Entity->GridLocation + DownVector))
			{
				UpdateEntityPosition(SubTurn, Entity, DownVector);
				//I'm pretty sure this will never infinite loop
			}
		}
	}
}

bool UGameManager::EvaluateSuperposition(const SubTurn& SubTurn, AEntity* A, AEntity* B)
{
	if (!(A->Flags & SUPER) || !(B->Flags & SUPER)) return false;

	FIntVector Origin(-1, -1, -1);
	for (int32 i = Turns[TurnCounter].SubTurns.Num() - 1;; --i)
	{
		const struct SubTurn& Sub = Turns[TurnCounter].SubTurns[i];
		if (&Sub == &SubTurn) break;
		if (int32 Index = Sub.Entities.Find(A)) {
			Origin = Sub.AllPaths[Sub.PathIndices[Index]];
			break;
		}
	}

	return Origin == B->GridLocation;
}

void UGameManager::UpdateEntityPosition(SubTurn& SubTurn, AEntity* Entity, const FIntVector& Delta)
{
	if (Entity->Flags & SUPER) {
		AEntity* Query = Grid.QueryAt(Entity->GridLocation + Delta);
		if (Query && (Query->Flags & SUPER)) {
			Entity->GridLocation += Delta;
			return;
		}
	}

	Grid.SetAt(Entity->GridLocation, nullptr);
	Grid.SetAt(Entity->GridLocation + Delta, Entity);
	Entity->GridLocation += Delta;

	//fix this
	AEntity* Query = Grid.QueryAt(Entity->GridLocation + DownVector);
	if (Query && (Query->Flags & REWIND) && (Entity->Flags & CURRENT_PLAYER)) {
		RewindQueue.Emplace(Entity);
	}
	if (Query && (Query->Flags & GOAL)) {
		SLOG("YOU WIN!")
	}

	int32 Index = SubTurn.AllPaths.Emplace(Entity->GridLocation);

	if (SubTurn.Entities.IsEmpty() || (SubTurn.Entities.Last() != Entity)) {
		SubTurn.Entities.Emplace(Entity);
		SubTurn.PathIndices.Emplace(Index);
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

		int32 WIDTH = X, LENGTH = Y, HEIGHT = Z + 2;

		Grid.WIDTH = WIDTH;
		Grid.LENGTH = LENGTH;
		Grid.HEIGHT = HEIGHT;
		Grid.Grid.Init(nullptr, LENGTH * WIDTH * HEIGHT);

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
						Entity->GridLocation = FIntVector(x, y, z);
						Entity->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
						Entity->GetStaticMeshComponent()->SetStaticMesh(BlockMesh);

						Grid.SetAt(Entity->GridLocation, Entity);

						if (val == "2") {
							Entity->GetStaticMeshComponent()->SetMaterial(0, StartTileMaterial);
							StartGridLocation = FIntVector(x, y, z + 1);
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

AEntity* EntityGrid::QueryAt(const FIntVector& Location, bool* bIsValid)
{
	if (Location.X < 0 || Location.X >= WIDTH ||
		Location.Y < 0 || Location.Y >= LENGTH ||
		Location.Z < 0 || Location.Z >= HEIGHT) 
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid query at x: %d, y: %d, z: %d"), Location.X, Location.Y, Location.Z);
		if (bIsValid) *bIsValid = false;
		return nullptr;
	}

	if (bIsValid) *bIsValid = true;
	return Grid[Location.X + (Location.Y * WIDTH) + (Location.Z * WIDTH * LENGTH)];
}

void EntityGrid::SetAt(const FIntVector& Location, AEntity* Entity)
{
	if (Location.X < 0 || Location.X >= WIDTH ||
		Location.Y < 0 || Location.Y >= LENGTH ||
		Location.Z < 0 || Location.Z >= HEIGHT)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid set at x: %d, y: %d, z: %d"), Location.X, Location.Y, Location.Z);
		return;
	}

	Grid[Location.X + (Location.Y * WIDTH) + (Location.Z * WIDTH * LENGTH)] = Entity;
}

//------------------------------------------------------------------

APlayerEntity::APlayerEntity()
{
	Flags |= PLAYER | MOVEABLE;
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