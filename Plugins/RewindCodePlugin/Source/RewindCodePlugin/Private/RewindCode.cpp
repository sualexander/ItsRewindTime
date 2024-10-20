// Copyright It's Rewind Time 2024

#include "RewindCode.h"
#include "Input.h"

#include "Kismet/GameplayStatics.h"


DEFINE_LOG_CATEGORY_STATIC(Rewind, Log, All);

#define LOG(Str, ...) UE_LOG(Rewind, Log, TEXT(Str), ##__VA_ARGS__)
#define WARN(Str, ...) UE_LOG(Rewind, Warning, TEXT(Str), ##__VA_ARGS__);
#define ERROR(Str, ...) UE_LOG(Rewind, Error, TEXT(Str), ##__VA_ARGS__)

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

#pragma warning(disable: 4426) //line below suddenly started throwing compile error so...
#pragma optimize("", off) //remove when done or add #if WITH_EDITOR

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
	GameManager->PlayerController->OnInputChanged.BindUObject(GameManager, &UGameManager::HandleMovementInput);
	GameManager->PlayerController->OnPassPressed.BindUObject(GameManager, &UGameManager::HandlePassInput);
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

	Animator->Temp = this; //pls remove asap

	//There must be a better way to do this...
	static ConstructorHelpers::FClassFinder<APlayerEntity> PlayerBP(TEXT("/Game/Blueprints/BP_Player"));
	static ConstructorHelpers::FClassFinder<ASuperposition> SuperBP(TEXT("/Game/Blueprints/BP_Superposition"));
	PlayerBlueprint = PlayerBP.Class;
	SuperBlueprint = SuperBP.Class;

	//Move everything below here out to somewhere else
	LoadGridFromFile();

	APlayerEntity* Player = SpawnPlayer();
	Grid.SetAt(StartGridLocation, Player);

	SpawnSuperposition();

	Timelines.Emplace_GetRef();}

void UGameManager::HandleMovementInput()
{
	if (PlayerController->bIsDebugging) return;

	//maybe change to state enum later
	if (!RewindQueue.IsEmpty())
	{
		LOG("Can't move with rewind queued");
		return;
	}

	if (PlayerController->NewestInput != NONE && !bPassPressed) {
		if (Animator->bIsAnimating) {
			Buffer = PlayerController->NewestInput;
			InputTimerStart = WorldContext->RealTimeSeconds;
		}
		else ProcessTurn(NONE);
	}
}

void UGameManager::HandlePassInput(bool bStart)
{
	if (PlayerController->bIsDebugging) return;

	bPassPressed = bStart;
	if (!bStart) {
		bHasPassed = false;
	}
	else if (!Animator->bIsAnimating) {
		ProcessTurn(PASS);
	}
}

void UGameManager::OnTurnEnd()
{
	LOG("Ending Turn %d", TurnCounter);
	if (PlayerController->bIsDebugging) return;

	double Elapsed =  WorldContext->RealTimeSeconds - InputTimerStart;
	InputTimerStart = 0;

	if (!RewindQueue.IsEmpty()) {
		LOG("IT'S REWIND TIME!!!!!");
		DoRewind();
		return;
	}

	//check timeline collapse

	if (bPassPressed) {
		if (!bHasPassed) {
			bHasPassed = true;
			ProcessTurn(PASS);
		}
		return;
	}

	//Process input buffer or stack
	if (Buffer != NONE && (Elapsed >= 0 && Elapsed < 0.15f)) { //make into runtime parameters
		ProcessTurn(Buffer);
	}
	else if (!PlayerController->Stack.IsEmpty()) {
		ProcessTurn(NONE);
	}
}

void UGameManager::ProcessTurn(EInputStates Input)
{
	GridCoord MoveInput(0, 0, 0);
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
	//can be removed after debugging
	case PASS:
		LOG("Passed turn %d", TurnCounter + 1);
	}

	AEntity* CurrentPlayer = Players.Last();
	if (!MoveInput.IsZero()) {
		//Check if input results in a successful movement, otherwise don't advance turn
		for (GridCoord GridLocation = CurrentPlayer->GridLocation;;)
		{
			AEntity* Front = Grid.QueryAt(GridLocation += MoveInput);
			if (!Front) break;
			if (!(Front->Flags & MOVEABLE)) return;
		}
	}

	Timeline& Timeline = Timelines[TimelineCounter];
	Timeline.Headers.Emplace_GetRef(CurrentPlayer, MoveInput); //Create header for only the corresponding timeline's player


	int32 EndTimelineSubturnIndex = -1;  //needs to be more specific as a subturn also has duration technically

	//Evaluate subturns
	for (int32 i = TimelineCounter; i >= 0; --i)
	{
		SubTurn& Subturn = Timeline.Subturns.Emplace_GetRef();
		SubTurnHeader& Header = Timelines[TimelineCounter].Headers[TurnCounter];
		if (Header.Move.IsZero()) continue;

		EvaluateSubTurn(Header, Subturn);

		if (EndTimelineSubturnIndex == -1 && !RewindQueue.IsEmpty()) EndTimelineSubturnIndex = i;
	}

	//"Un-super" unmerged players
	for (APlayerEntity* Player : Players)
	{
		if (!Player->bInSuperposition) {
			Player->Flags &= ~SUPER;
			Player->Superposition = nullptr;
		}
	}



	//remove any animation paths after any of these end states
	//look through rewind queue

	Buffer = NONE;


	//Dispatch animations
	SLOGF(EndTimelineSubturnIndex)

	if (EndTimelineSubturnIndex == -1) EndTimelineSubturnIndex = 0;

	int32 StartIndex = 0; //what should this be?

	Animator->Start(Timeline.Subturns, StartIndex, EndTimelineSubturnIndex, false);
}

void UGameManager::DoRewind()
{
	//Do animations
	//animations should update grid as well


	//Start new timeline
	++TimelineCounter;
	RewindQueue.Empty();
	TurnCounter = 0;

	Players.Last()->Flags &= (~CURRENT_PLAYER);
	SpawnPlayer();

	Superpositions[0]->SetActorHiddenInGame(false);
	Superpositions[0]->GridLocation = StartGridLocation;
	Grid.SetAt(StartGridLocation, Superpositions[0]);
	Superpositions[0]->SetActorLocation(FVector(StartGridLocation) * BLOCK_SIZE);

	Superpositions[0]->Players.Reset();
	Superpositions[0]->Players.Append(Players);

	for (APlayerEntity* Player : Players)
	{
		Player->Flags |= SUPER;
		Player->bInSuperposition = true;
		Player->Superposition = Superpositions[0];

		Grid.SetAt(Player->GridLocation, nullptr);
		Player->GridLocation = StartGridLocation;
		Player->SetActorLocation(FVector(StartGridLocation) * BLOCK_SIZE);
		Player->SetActorHiddenInGame(true);
	}
}

static const GridCoord DownVector(0, 0, -1), UpVector(0, 0, 1);

void UGameManager::EvaluateSubTurn(const SubTurnHeader& Header, SubTurn& SubTurn)
{
	//Query in direction of movement until wall or air
	TArray<AEntity*> Connected;
	Connected.Emplace(Header.Player);
	for (GridCoord GridLocation = Header.Player->GridLocation;;)
	{
		AEntity* Front = Grid.QueryAt(GridLocation += Header.Move);
		if (!Front) break;
		if (!(Front->Flags & MOVEABLE)) return;
		if (!Connected.IsEmpty() && CheckSuperposition(Front, Connected.Last())) break;

		Connected.Emplace(Front);
	}

	//Un-super this subturn's player
	if (Header.Player->Flags & SUPER) {
		if (Header.Player->bInSuperposition) {
			Header.Player->Superposition->Players.RemoveSingle(Header.Player);
			Header.Player->bInSuperposition = false;

			//superposition collapse animation
			Header.Player->SetActorHiddenInGame(false);

			if (Header.Player->Superposition->Players.Num() == 1) {
				Header.Player->Superposition->Players[0]->bInSuperposition = false;
				Header.Player->Superposition->Players[0]->SetActorHiddenInGame(false);

				Grid.SetAt(Header.Player->Superposition->GridLocation, Header.Player->Superposition->Players[0]);
				Header.Player->Superposition->GridLocation = GridCoord(-1, -1, -1);
				Header.Player->Superposition->Players.Empty();
			}
		}
	}

	//Update from farthest to self
	for (int32 i = Connected.Num() - 1; i >= 0; --i)
	{
		//Update from bottom up, and evaluate horizontal movement before gravity
		AEntity* Entity = Connected[i];
		GridCoord EntityOrigin = Entity->GridLocation;
		UpdateEntityPosition(SubTurn, Entity, Header.Move);

		for (;;)
		{
			AEntity* Below = Grid.QueryAt(Entity->GridLocation + DownVector);
			if (Below && !CheckSuperposition(Below, Entity)) break;

			if (Entity->GridLocation.Z - 1 <= HEIGHT_MIN) {
				if (i == 0) {
					//timeline collapse
					LOG("collapse");
				}
				//do stuff like stop rendering, play fade animation etc...
				LOG("Fell off da world");
				break;
			}

			UpdateEntityPosition(SubTurn, Entity, DownVector);
		}

		for (;;)
		{
			AEntity* Up = Grid.QueryAt(EntityOrigin += UpVector);
			if (!Up || !(Up->Flags & MOVEABLE)) break;

			AEntity* Front = Grid.QueryAt(EntityOrigin + Header.Move);
			if (Front && !(Front->Flags & MOVEABLE)) break;

			UpdateEntityPosition(SubTurn, Up, Header.Move);

			for (;;)
			{
				AEntity* Below = Grid.QueryAt(Up->GridLocation + DownVector);
				if (Below) break;
				if (Up->GridLocation.Z - 1 <= HEIGHT_MIN) {
					//do stuff like stop rendering, play fade animation etc...
					LOG("Should this even ever print??? Fell off da world");
					break;
				}

				UpdateEntityPosition(SubTurn, Up, DownVector);
			}
		}
	}

	TMap<GridCoord, TArray<APlayerEntity*>> NewSupers;
	for (APlayerEntity* Player : Players)
	{
		if ((Player->Flags & SUPER) && !Player->bInSuperposition) {
			//Merge into an existing superposition
			for (ASuperposition* Superposition : Superpositions)
			{
				if (Superposition->GridLocation == Player->GridLocation) {
					Superposition->Players.Emplace(Player);
					Player->bInSuperposition = true;
					Player->Superposition = Superposition;
					Player->SetActorHiddenInGame(true); //defer to animation
					
					goto LoopEnd;
				}
			}

			//New superposition
			NewSupers.FindOrAdd(Player->GridLocation).Emplace(Player);
		}
		LoopEnd: continue;
	}

	for (const auto& Pair : NewSupers)
	{
		if (Pair.Value.Num() > 1) {
			ASuperposition* NewSuper = nullptr;
			for (ASuperposition* Superposition : Superpositions)
			{
				if (Superposition->Players.IsEmpty()) {
					NewSuper = Superposition;

					NewSuper->SetActorHiddenInGame(false);

					break;
				}
			}
			if (!NewSuper) {
				NewSuper = SpawnSuperposition();
			}

			NewSuper->OldSuperposition = Pair.Value[0]->Superposition;

			for (APlayerEntity* Player : Pair.Value)
			{
				Player->bInSuperposition = true;
				Player->Superposition = NewSuper;
				NewSuper->Players.Emplace(Player);
			}

			NewSuper->GridLocation = Pair.Value[0]->GridLocation;
			Grid.SetAt(NewSuper->GridLocation, NewSuper);
			NewSuper->SetActorLocation(FVector(NewSuper->GridLocation) * BLOCK_SIZE);
		}
	}

	for (ASuperposition* Superposition : Superpositions)
	{
		if (Superposition->Players.Num() < 2) {
			Superposition->SetActorHiddenInGame(true);
		}
	}

	//if (SubTurn.bIsPlayersFinalMove && !(SubTurn.Player->Flags & CURRENT_PLAYER)) {
	//	//Check for timeline collapse

	//	AEntity* QueryBelow = Grid.QueryAt(SubTurn.Player->GridLocation + DownVector);
	//	if (QueryBelow && (QueryBelow->Flags & REWIND)) {
	//		LOG("Player successfully finished its moves and reached Rewind Tile");
	//	}
	//	else {
	//		LOG("TIMELINE COLLAPSE TRIGGERED");
	//		CollapseTimeline();
	//	}
	//}
}

void UGameManager::UpdateEntityPosition(SubTurn& SubTurn, AEntity* Entity, const GridCoord& Delta)
{
	if (ASuperposition* Superposition = Cast<ASuperposition>(Entity)) {
		for (APlayerEntity* Player : Superposition->Players)
		{
			Player->GridLocation += Delta;
		}
	}

	if (Grid.QueryAt(Entity->GridLocation) == Entity) {
		Grid.SetAt(Entity->GridLocation, nullptr);
	}
	Grid.SetAt(Entity->GridLocation + Delta, Entity);
	Entity->GridLocation += Delta;

	//fix this
	AEntity* Query = Grid.QueryAt(Entity->GridLocation + DownVector);
	if (Query && (Query->Flags & REWIND) && (Entity->Flags & CURRENT_PLAYER)) {
		RewindQueue.Emplace(Entity);
		//Anything can rewind 
		//if (Entity == SubTurn.Player) SubTurn.bIsPlayersFinalMove = true;
	}
	if (Query && (Query->Flags & GOAL)) {
		SLOG("YOU WIN!")
	}

	int32 Index = SubTurn.Paths.Emplace(Entity->GridLocation);

	if (SubTurn.Entities.IsEmpty() || (SubTurn.Entities.Last() != Entity)) {
		SubTurn.Entities.Emplace(Entity);
		SubTurn.PathIndices.Emplace(Index);
	}
}

bool UGameManager::CheckSuperposition(AEntity* To, AEntity* From)
{
	if (From->Flags & SUPER) {
		if ((To->Flags & SUPER) &&
			StaticCast<APlayerEntity*>(To)->Superposition == StaticCast<APlayerEntity*>(From)->Superposition) {
			return true;
		}

		ASuperposition* Superposition = Cast<ASuperposition>(To);
		if (Superposition && (Superposition->OldSuperposition == StaticCast<APlayerEntity*>(From)->Superposition)) {
			return true;
		}
	}
	return false;
}

void UGameManager::CollapseTimeline(int32 Collapsed, int32 Current)
{
	////Find when to collapse to
	//int32 TurnIndex = Turns.Num() - 1;
	//for (; TurnIndex >= 0; --TurnIndex)
	//{
	//	TArray<SubTurn>& Subturns = Turns[TurnIndex].SubTurns;
	//	if (Subturns.Num() <= Collapsed || Subturns.Num() <= Current) continue;
	//	//Subturns.Num() always == Current?

	//	if (Subturns[Collapsed].Location == Subturns[Current].Location) break;
	//}

	//TurnIndex = TurnIndex == 0 ? TurnIndex : TurnIndex - 1;
}

APlayerEntity* UGameManager::SpawnPlayer()
{
	APlayerEntity* Player = WorldContext->SpawnActor<APlayerEntity>(PlayerBlueprint, FVector(StartGridLocation) * BLOCK_SIZE, FRotator::ZeroRotator);
	Players.Emplace(Player);

	Player->Flags |= MOVEABLE | CURRENT_PLAYER;
	Player->GridLocation = StartGridLocation;

	Player->GetStaticMeshComponent()->SetCustomPrimitiveDataFloat(0, TimelineCounter);

	return Player;
}

ASuperposition* UGameManager::SpawnSuperposition()
{
	ASuperposition* Superposition = WorldContext->SpawnActor<ASuperposition>(SuperBlueprint, FVector(StartGridLocation) * BLOCK_SIZE, FRotator::ZeroRotator);
	Superpositions.Emplace(Superposition);

	Superposition->Flags |= MOVEABLE;
	Superposition->GridLocation = StartGridLocation;

	return Superposition;
}

void UGameManager::VisualizeGrid()
{
	FlushDebugStrings(WorldContext);
	for (int32 i = 0; i < Grid.Grid.Num(); ++i)
	{
		AEntity* Entity = Grid.Grid[i];
		if (!Entity || !(Entity->Flags & MOVEABLE)) {
			int32 Index = i;
			int32 Z = Index / 100;
			Index -= (Z * 100);
			int32 Y = Index / 10;
			int32 X = Index % 10;
			DrawDebugString(WorldContext, FVector(X, Y, Z) * BLOCK_SIZE, FString::FromInt(i));
		}
		else {
			DrawDebugString(WorldContext, FVector(Entity->GridLocation) * BLOCK_SIZE, Entity->GetActorLabel(), NULL, FColor::Red);
		}
	}
}

void UGameManager::LoadGridFromFile()
{
	FString FilePath = FPaths::ProjectContentDir() / TEXT("Grids/MainLevelGrid.txt");
	FString FileContent;
	if (FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		// File successfully read
		LOG("File content: %s", *FileContent);
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

						GridCoord IntLocation(x, y, z);
						FVector Location(x * BLOCK_SIZE, y * BLOCK_SIZE, z * BLOCK_SIZE);
						AEntity* Entity = WorldContext->SpawnActor<AEntity>(Location, FRotator::ZeroRotator);
						Entity->GridLocation = GridCoord(x, y, z);
						Entity->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
						Entity->GetStaticMeshComponent()->SetStaticMesh(BlockMesh);

						Grid.SetAt(Entity->GridLocation, Entity);

						if (val == "2") {
							Entity->GetStaticMeshComponent()->SetMaterial(0, StartTileMaterial);
							StartGridLocation = GridCoord(x, y, z + 1);
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
		ERROR("Failed to read file.");
	}
}

//------------------------------------------------------------------

AEntity* EntityGrid::QueryAt(const GridCoord& Location, bool* bIsValid)
{
	if (Location.X < 0 || Location.X >= WIDTH ||
		Location.Y < 0 || Location.Y >= LENGTH ||
		Location.Z < 0 || Location.Z >= HEIGHT) 
	{
		ERROR("Invalid query at x: %d, y: %d, z: %d", Location.X, Location.Y, Location.Z);
		if (bIsValid) *bIsValid = false;
		return nullptr;
	}

	if (bIsValid) *bIsValid = true;
	return Grid[Location.X + (Location.Y * WIDTH) + (Location.Z * WIDTH * LENGTH)];
}

void EntityGrid::SetAt(const GridCoord& Location, AEntity* Entity)
{
	if (Location.X < 0 || Location.X >= WIDTH ||
		Location.Y < 0 || Location.Y >= LENGTH ||
		Location.Z < 0 || Location.Z >= HEIGHT)
	{
		ERROR("Invalid set at x: %d, y: %d, z: %d", Location.X, Location.Y, Location.Z);
		return;
	}

	Grid[Location.X + (Location.Y * WIDTH) + (Location.Z * WIDTH * LENGTH)] = Entity;
}

//-----------------------------------------------------------------------------

void UEntityAnimator::Start(const TArray<SubTurn>& Subturns, int32 Start, int32 End, bool bReverse)
{
	for (int32 SubturnIndex = Start; SubturnIndex >= End; --SubturnIndex)
	{
		const SubTurn& Subturn = Subturns[SubturnIndex];
		if (Subturn.Entities.IsEmpty()) continue;

		GroupIndices.Emplace(GroupQueue.Num());

		int32 MaxDuration = bReverse * FMath::Max(Subturn.Durations);
		int32 EntityIndex = 0;
		for (; EntityIndex < Subturn.Entities.Num(); ++EntityIndex)
		{
			EntityAnimationPath& Path = GroupQueue.Emplace_GetRef(Subturn.Entities[EntityIndex], MaxDuration - Subturn.Durations[EntityIndex]);

			int32 PathIndex = Subturn.PathIndices[EntityIndex];
			int32 EndIndex = EntityIndex == Subturn.PathIndices.Num() - 1 ? Subturn.Paths.Num() - 1 : Subturn.PathIndices[EntityIndex + 1] - 1;

			if (bReverse) {
				Swap(PathIndex, EndIndex);
			}

			for (; PathIndex <= EndIndex; ++PathIndex)
			{
				Path.Path.Emplace(Subturn.Paths[PathIndex] * BlockSize);
			}
		}
	}
	if (GroupIndices.IsEmpty()) return;

	//Group adjacent "subturns" if all entities' paths are non-intersecting
	int32 Index = 0;
	for (; false;)
	{

	}

	GroupStartTime = WorldContext->TimeSeconds;
	bIsAnimating = true;
}

void UEntityAnimator::Tick(float DeltaTime)
{
	bool bNextGroup = true;
	double CurrentTime = WorldContext->TimeSeconds;

	//Loop through group to animate simultaneously
	int32 StartIndex = GroupIndices[QueueIndex];
	int32 EndIndex = QueueIndex == GroupIndices.Num() - 1 ? GroupQueue.Num() - 1 : GroupIndices[QueueIndex + 1] - 1;
	for (int32 i = StartIndex; i <= EndIndex; ++i)
	{
		EntityAnimationPath& Animation = GroupQueue[i];
		if (Animation.PathIndex == -1) continue; //Animation is complete

		//Start animation path
		if (Animation.PathIndex == -2 && (Animation.StartTime == 0 || CurrentTime - GroupStartTime >= Animation.StartTime)) {
			Animation.PathIndex = 0;
			Animation.SubstepTime = CurrentTime;
			Animation.StartLocation = Animation.Entity->GetActorLocation();
		}

		float MoveTime = (Animation.Path[Animation.PathIndex] - Animation.StartLocation).Z < 0 ? VerticalSpeed : HorizontalSpeed;
		MoveTime *= Temp->PlayerController->SpeedMultiplier;
		float Alpha = FMath::Clamp((CurrentTime - Animation.SubstepTime) / MoveTime, 0, 1);
		Animation.Entity->SetActorLocation(FMath::Lerp(Animation.StartLocation, Animation.Path[Animation.PathIndex], Alpha));

		if (CurrentTime - Animation.SubstepTime >= MoveTime) {
			Animation.PathIndex = Animation.PathIndex + 1 == Animation.Path.Num() ? -1 : ++Animation.PathIndex;
			Animation.SubstepTime = CurrentTime;
			Animation.StartLocation = Animation.Entity->GetActorLocation();
		}

		bNextGroup = false;
	}

	if (bNextGroup) {
		GroupStartTime = WorldContext->TimeSeconds;

		if (++QueueIndex == GroupIndices.Num()) {
			bIsAnimating = false;

			GroupQueue.Empty();
			GroupIndices.Empty();
			QueueIndex = 0;

			OnAnimationsFinished.Execute();
		}
	}
}