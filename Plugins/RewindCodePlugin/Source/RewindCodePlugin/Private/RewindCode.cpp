// Copyright It's Rewind Time 2024

#include "RewindCode.h"
#include "RewindCommon.h"
#include "Input.h"

#include "Engine.h"
#include "Kismet/GameplayStatics.h"

#include "Camera/CameraActor.h"


DEFINE_LOG_CATEGORY_STATIC(RewindGame, Log, All);

#define LOG(Str, ...) UE_LOG(RewindGame, Log, TEXT(Str), ##__VA_ARGS__)
#define WARN(Str, ...) UE_LOG(RewindGame, Warning, TEXT(Str), ##__VA_ARGS__);
#define ERROR(Str, ...) UE_LOG(RewindGame, Error, TEXT(Str), ##__VA_ARGS__)

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
	DefaultPawnClass = ARewindPawn::StaticClass();
}

void ARewindGameMode::PostLogin(APlayerController* InController)
{
	AGameModeBase::PostLogin(InController);

	GameManager = NewObject<UGameManager>(this);
	GameManager->PlayerController = Cast<ARewindPlayerController>(InController);
	GameManager->Gamemode = this;
	ARewindPlayerController* Controller = GameManager->PlayerController;
	Controller->OnInputChanged.BindUObject(GameManager, &UGameManager::HandleMovementInput);
	Controller->OnPassPressed.BindUObject(GameManager, &UGameManager::HandlePassInput);
	Controller->OnUndoPressed.BindUObject(GameManager, &UGameManager::HandleUndoInput);

	//Need to have actual custom camera pawn instantiation here
	AActor* Camera = UGameplayStatics::GetActorOfClass(GetWorld(), ACameraActor::StaticClass());
	if (ACameraActor* Cam = Cast<ACameraActor>(Camera))
	{
		Controller->SetViewTarget(Cam);
	}
}

ARewindPawn::ARewindPawn()
{
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
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

	LoadLevel();
}

void UGameManager::LoadLevel()
{
	ARewindWorldSettings* Settings = nullptr;
	Settings = Cast<ARewindWorldSettings>(WorldContext->GetWorldSettings());
 	if (!Settings) {
		ERROR("Failed to load world settings, level loading failed");
		return;
	}

	for (TActorIterator<AGridActor> Itr(WorldContext); Itr; ++Itr)
	{
		if (*Itr) Itr->SetActorHiddenInGame(true);
	}

	Grid.Dimensions = GridCoord(Settings->Dimensions);
	Dimensions = FVector(Settings->Dimensions);
	Transform = Settings->Transform;
	HeightMin = Settings->HeightMin;
	Rotation = FRotator(0, Transform.Rotator().Yaw, 0);

	Animator->Transform = Transform;
	Animator->Offset = Dimensions / -2 + 0.5;

	for (int32 i = 0; i < Settings->GridData.Num(); ++i)
	{
		int32 Index = i;
		int32 Z = Index / (Grid.Dimensions.X * Grid.Dimensions.Y);
		Index -= (Z * (Grid.Dimensions.X * Grid.Dimensions.Y));
		int32 Y = Index / Grid.Dimensions.X;
		int32 X = Index % Grid.Dimensions.X;

		if (Settings->GridData[i] == 0) {
			Grid.Grid.Emplace(nullptr);
			continue;
		}

		AEntity* Entity = WorldContext->SpawnActor<AEntity>(GetWorldLocation(GridCoord(X, Y, Z)), Rotation);
		Entity->GridLocation = GridCoord(X, Y, Z);
		Grid.Grid.Emplace(Entity);

		Entity->SetActorScale3D(Settings->BlockScale);
		UStaticMeshComponent* SMComponent = Entity->GetStaticMeshComponent();
		SMComponent->SetMobility(EComponentMobility::Movable);

		GridType Type = StaticCast<GridType>(Settings->GridData[i]);
		const TCHAR* Path = **MeshPaths.Find(Type);
		SMComponent->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Path));
		switch (Type)
		{
		case GridType::Solid:
			break;
		case GridType::Transparent:
			break;
		case GridType::Origin:
			StartGridLocation = GridCoord(X, Y, Z + 1);
			break;
		case GridType::Goal:
			Entity->Flags |= GOAL;
			break;
		case GridType::Rewind:
			Entity->Flags |= REWIND;
			break;
		}	
	}

	//TODO: a lot

	APlayerEntity* Player = SpawnPlayer();
	Grid.SetAt(StartGridLocation, Player);

	SpawnSuperposition();

	Timelines.Emplace();
	CollapseQueue = -1;
	RewindQueue = nullptr;
}

void UGameManager::HandleMovementInput()
{
	if (PlayerController->bIsDebugging) return;

	//maybe change to state enum later
	//if (!RewindQueue.IsEmpty())
	//{
	//	LOG("Can't move with rewind queued");
	//	return;
	//}

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

void UGameManager::HandleUndoInput()
{
	//Need to handle game states in general
	if (Animator->bIsAnimating) return;

	if (TurnCounter == 0) {
		if (TimelineCounter == 0) return;

		LOG("Undoed turn %d", TurnCounter);

		--TimelineCounter;
		Timeline& Timeline = Timelines[TimelineCounter];
		Timeline.Headers.RemoveAt(Timeline.Headers.Num() - 1);
		TurnCounter = Timeline.Subturns.Num() / (TimelineCounter + 1) - 1;
		Timeline.Subturns.RemoveAt(Timeline.Subturns.Num() - (TimelineCounter + 1), TimelineCounter + 1, true);
		Timeline.Rewinder = nullptr;

		//Update grid
		AEntity* Player = Players.Pop(true);
		Grid.SetAt(Player->GridLocation, nullptr);
		Player->Destroy();
			
		for (int32 i = 0; i < Timeline.Entities.Num(); ++i)
		{
			AEntity* Entity = Timeline.Entities[i];
			Grid.SetAt(Entity->GridLocation, nullptr);
			Entity->GridLocation = Timeline.Locations[i];
			Grid.SetAt(Entity->GridLocation, Entity);

			Entity->SetActorLocation(GetWorldLocation(Entity));
		}

		RevaluateSuperpositions();
		Timelines.RemoveAt(Timelines.Num() - 1);
	}
	else {
		LOG("Undoed turn %d", TurnCounter);
		Timeline& Timeline = Timelines[TimelineCounter];

		int32 EndIndex = (TimelineCounter + 1) * (TurnCounter - 1);
		for (int32 i = EndIndex + TimelineCounter; i >= EndIndex; --i)
		{
			SubTurn& Subturn = Timeline.Subturns[i];
			for (int32 j = 0; j < Subturn.Entities.Num(); ++j)
			{
				AEntity* Entity = Subturn.Entities[j];
				if (Grid.QueryAt(Entity->GridLocation) == Entity) {
					Grid.SetAt(Entity->GridLocation, nullptr);
				}
				Entity->GridLocation = Subturn.Paths[Subturn.PathIndices[j]];
				Grid.SetAt(Entity->GridLocation, Entity);
			}

			RevaluateSuperpositions();
		}

		Animator->Start(Timeline.Subturns, EndIndex + TimelineCounter, EndIndex, true);

		Timeline.Headers.RemoveAt(Timeline.Headers.Num() - 1);
		Timeline.Subturns.RemoveAt(Timeline.Subturns.Num() - (TimelineCounter + 1), TimelineCounter + 1, true);
		--TurnCounter;
	}
}

void UGameManager::RevaluateSuperpositions()
{
	for (ASuperposition* Super : Superpositions)
	{
		Super->Players.Empty();
		Super->OldSuperposition = nullptr;
		Super->SetActorHiddenInGame(true);

		Grid.SetAt(Super->GridLocation, nullptr);
		Super->GridLocation = GridCoord(-1, -1, -1);
	}

	TMap<GridCoord, TArray<APlayerEntity*>> Overlapping;
	for (APlayerEntity* Player : Players)
	{
		Overlapping.FindOrAdd(Player->GridLocation).Emplace(Player);
	}

	int32 Index = 0;
	for (const auto& Pair : Overlapping)
	{
		if (Pair.Value.Num() > 1) {
			ASuperposition* Super = Superpositions[Index++];

			for (APlayerEntity* Player : Pair.Value) {
				Super->Players.Emplace(Player);

				Player->Flags |= SUPER;
				Player->Superposition = Super;
				Player->bInSuperposition = true;
				Player->SetActorHiddenInGame(true);
			}

			Super->OldSuperposition = nullptr;
			Super->GridLocation = Pair.Value[0]->GridLocation;
			Grid.SetAt(Super->GridLocation, Super);
			Super->SetActorLocation(GetWorldLocation(Super));
			Super->SetActorHiddenInGame(false);
		}
		else {
			Pair.Value[0]->Flags &= ~SUPER;
			Pair.Value[0]->bInSuperposition = false;
			Pair.Value[0]->Superposition = nullptr;
			Pair.Value[0]->SetActorHiddenInGame(false);
		}
	}
}

void UGameManager::OnTurnEnd()
{
	LOG("Ending Turn %d", TurnCounter);
	if (PlayerController->bIsDebugging) return;

	//VisualizeGrid();
	InputTimerStart = 0;

	if (RewindQueue) {
		RewindTimeline();
		return;
	}
	if (CollapseQueue != -1) {
		CollapseTimeline(CollapseQueue);
		return;
	}

	if (bPassPressed) {
		if (!bHasPassed) {
			bHasPassed = true;
			ProcessTurn(PASS);
		}
		return;
	}

	//Process input buffer or stack
	double Elapsed = WorldContext->RealTimeSeconds - InputTimerStart;
	if (Buffer != NONE && (Elapsed >= 0 && Elapsed < 0.15f)) { //TODO: make into runtime parameters
		ProcessTurn(Buffer);
	}
	else if (!PlayerController->Stack.IsEmpty()) {
		ProcessTurn(NONE);
	}
}

static const GridCoord DownVector(0, 0, -1), UpVector(0, 0, 1);

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
	case PASS:
		//TODO: need visual cue
		LOG("Passed turn %d", TurnCounter + 1);
	}
	int32 Turns = (FMath::RoundToInt((Rotation.Yaw + CameraRotation) / 90) % 4 + 4) % 4;
	switch (Turns)
	{
	case 0: break;
	case 1:
		MoveInput = { -MoveInput.Y, MoveInput.X, 0 };
		break;
	case 2:
		MoveInput = { -MoveInput.X, -MoveInput.Y, 0 };
		break;
	case 3:
		MoveInput = { MoveInput.Y, -MoveInput.X, 0 };
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
	Buffer = NONE;

	Timeline& Timeline = Timelines[TimelineCounter];
	Timeline.Headers.Emplace(CurrentPlayer, MoveInput); //Create header for only the corresponding timeline's player
	++TurnCounter;

	//TODO: This is temporary, might need to restructure SubTurn when we hv more entity types
	//Because we only have players for now
	Timelines[TimelineCounter].Entities.Empty();
	Timelines[TimelineCounter].Locations.Empty();
	for (APlayerEntity* Player : Players)
	{
		Timelines[TimelineCounter].Entities.Emplace(Player);
		Timelines[TimelineCounter].Locations.Emplace(Player->GridLocation);
	}

	TArray<TPair<AEntity*, int32>> CollapseCandidates;
	for (int32 n = 0; n < TimelineCounter; ++n)
	{
		if (Timelines[n].NumTurns == TurnCounter) {
			CollapseCandidates.Emplace(Timelines[n].Rewinder, n);
		}
	}

	//Evaluate subturns
	int32 EndIndex = -1;
	for (int32 i = TimelineCounter; i >= 0; --i)
	{
		SubTurn& Subturn = Timeline.Subturns.Emplace_GetRef();
		if (TurnCounter - 1 >= Timelines[i].Headers.Num()) continue;

		SubTurnHeader& Header = Timelines[i].Headers[TurnCounter - 1];
		if (!Header.Move.IsZero()) {
			EvaluateSubTurn(Header, Subturn);
			Subturn.Durations.Init(0, Subturn.Entities.Num());
			if (EndIndex == -1 && RewindQueue) {
				EndIndex = (TimelineCounter + 1) * (TurnCounter - 1) + (TimelineCounter - i);
			}
		}

		//Check collapse
		for (int32 n = CollapseCandidates.Num() - 1; n >= 0; --n)
		{
			AEntity* Entity = CollapseCandidates[n].Key;
			//if (Entity->Flags & SUPER && StaticCast<APlayerEntity*>(Entity)->bInSuperposition == true) continue;
			AEntity* Query = Grid.QueryAt(Entity->GridLocation + DownVector);
			if (Query && (Query->Flags & REWIND)) {
				CollapseCandidates.RemoveAt(n);
			}
		}
	}

	//Collapse from premature rewind
	if (RewindQueue) {
		for (int32 n = 0; n < TimelineCounter; ++n)
		{
			//Not actually sure if it's > or >=
			if (Timelines[n].Rewinder == RewindQueue && Timelines[n].NumTurns >= TurnCounter) {
				CollapseQueue = n;
				RewindQueue = nullptr;
				break;
			}
		}
	}
	else if (!CollapseCandidates.IsEmpty()) {
		CollapseQueue = CollapseCandidates[0].Value;
	}

	//"Un-super" unmerged players
	for (APlayerEntity* Player : Players)
	{
		if (!Player->bInSuperposition) {
			Player->Flags &= ~SUPER;
			Player->Superposition = nullptr;
		}
	}

	//Dispatch animations
	if (EndIndex == -1) {
		EndIndex = (TimelineCounter + 1) * (TurnCounter - 1) + TimelineCounter;
	}

	int32 StartIndex = (TimelineCounter + 1) * (TurnCounter - 1);

	Animator->Start(Timeline.Subturns, StartIndex, EndIndex, false);
}

void UGameManager::EvaluateSubTurn(SubTurnHeader& Header, SubTurn& SubTurn)
{
	//Query in direction of movement until wall or air
	TArray<AEntity*> Connected;
	Connected.Emplace(Header.Player);
	for (GridCoord GridLocation = Header.Player->GridLocation;;)
	{
		AEntity* Front = Grid.QueryAt(GridLocation += Header.Move);
		if (!Front) break;
		if (!(Front->Flags & MOVEABLE)) return;
		if (CheckSuperposition(Front, Connected.Last())) break;

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

			if (Entity->GridLocation.Z - 1 <= HeightMin) {
				if (i == 0) {
					//timeline collapse
					//LOG("collapse");
				}
				//do stuff like stop rendering, play fade animation etc...
				//LOG("Fell off da world");
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
				if (Up->GridLocation.Z - 1 <= HeightMin) {
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
			NewSuper->SetActorLocation(GetWorldLocation(NewSuper));
		}
	}

	for (ASuperposition* Superposition : Superpositions)
	{
		if (Superposition->Players.Num() < 2) {
			Superposition->SetActorHiddenInGame(true);
		}
	}
}

void UGameManager::UpdateEntityPosition(SubTurn& Subturn, AEntity* Entity, const GridCoord& Delta)
{
	//Update players "inside" when superposition entity is moved
	if (ASuperposition* Superposition = Cast<ASuperposition>(Entity)) {
		for (APlayerEntity* Player : Superposition->Players)
		{
			Player->GridLocation += Delta;
		}
	}

	GridCoord OldLocation = Entity->GridLocation;
	if (Grid.QueryAt(Entity->GridLocation) == Entity) {
		Grid.SetAt(Entity->GridLocation, nullptr);
	}
	if (!Grid.QueryAt(Entity->GridLocation + Delta)) {
		Grid.SetAt(Entity->GridLocation + Delta, Entity);
	}
	Entity->GridLocation += Delta;

	//We assume entities can only move once contiguously in a subturn
	if (Subturn.Entities.IsEmpty() || (Subturn.Entities.Last() != Entity)) {
		Subturn.Entities.Emplace(Entity);
		Subturn.PathIndices.Emplace(Subturn.Paths.Emplace(OldLocation));
	}
	Subturn.Paths.Emplace(Entity->GridLocation);

	//Check for rewind tile
	if (Entity->Flags & SUPER || Entity->IsA<ASuperposition>()) return;

	AEntity* Query = Grid.QueryAt(Entity->GridLocation + DownVector);
	if (!Query) return;
	if (!RewindQueue && Query->Flags & REWIND) {
		for (const Timeline& Timeline : Timelines)
		{
			if (Timeline.Rewinder == Entity && Timeline.NumTurns == TurnCounter) return;
		}

		RewindQueue = Entity;
	}
	if (Query->Flags & GOAL) {
		SLOG("YOU WIN!");
	}
}

bool UGameManager::CheckSuperposition(AEntity* To, AEntity* From)
{
	if (From->Flags & SUPER) {
		//When one player moves out, it doesn't form a new superposition yet, so it's superposition field is still the old superposition
		if ((To->Flags & SUPER) &&
			StaticCast<APlayerEntity*>(To)->Superposition == StaticCast<APlayerEntity*>(From)->Superposition) {
			return true;
		}
		//When all players in the superposition move out and form a new superposition, OldSuperposition keeps track of where they came from
		ASuperposition* Superposition = Cast<ASuperposition>(To);
		if (Superposition && (Superposition->OldSuperposition == StaticCast<APlayerEntity*>(From)->Superposition)) {
			return true;
		}
	}
	return false;
}

void UGameManager::RewindTimeline()
{
	Timelines[TimelineCounter].Rewinder = RewindQueue;
	Timelines[TimelineCounter].NumTurns = TurnCounter;

	//Do animations
	//animations should update grid as well

	//Start new timeline
	++TimelineCounter;
	Timelines.Emplace();
	TurnCounter = 0;
	RewindQueue = nullptr;

	Players.Last()->Flags &= (~CURRENT_PLAYER);
	SpawnPlayer();

	Superpositions[0]->SetActorHiddenInGame(false);
	Superpositions[0]->GridLocation = StartGridLocation;
	Grid.SetAt(StartGridLocation, Superpositions[0]);
	Superpositions[0]->SetActorLocation(GetWorldLocation(Superpositions[0]));

	Superpositions[0]->Players.Empty();
	Superpositions[0]->Players.Append(Players);

	for (APlayerEntity* Player : Players)
	{
		Player->Flags |= SUPER;
		Player->bInSuperposition = true;
		Player->Superposition = Superpositions[0];

		Grid.SetAt(Player->GridLocation, nullptr);
		Player->GridLocation = StartGridLocation;
		Player->SetActorLocation(GetWorldLocation(Player));
		Player->SetActorHiddenInGame(true);
	}
	
	////Reset other timelines
	//for ()
	//{

	//}
}

void UGameManager::CollapseTimeline(int32 Target)
{
	CollapseQueue = -1;

	//Find when to collapse to
	AEntity* Rewinder = Timelines[Target].Rewinder;
	int32 Equality = 0;

	GridCoord Past(0, 0, 0);
	GridCoord Current(0, 0, 0);
	int32 End = FMath::Min(Timelines[Target].NumTurns, TurnCounter);
	for (int32 Turn = 0; Turn < End; ++Turn)
	{
		for (int32 i = 0; i < Target + 1; ++i)
		{
			SubTurn& Subturn = Timelines[Target].Subturns[(Turn * (Target + 1)) + i];
			int32 Index = Subturn.Entities.Find(Rewinder);
			if (Index != -1) {
				Past = Index == Subturn.PathIndices.Num() - 1 ? Subturn.Paths.Last() : Subturn.Paths[Subturn.PathIndices[Index + 1] - 1];
			}
		}
	
		for (int32 i = 0; i < TimelineCounter + 1; ++i)
		{
			SubTurn& Subturn = Timelines[TimelineCounter].Subturns[(Turn * (TimelineCounter + 1)) + i];
			int32 Index = Subturn.Entities.Find(Rewinder);
			if (Index != -1) {
				Current = Index == Subturn.PathIndices.Num() - 1 ? Subturn.Paths.Last() : Subturn.Paths[Subturn.PathIndices[Index + 1] - 1];
			}
		}

		if (Past == Current) {
			Equality = Turn + 1;
			if (Turn + 1 == End) --Equality;
		}
	}

	LOG("Collapsing to timeline %d, turn %d", Target, Equality);
	//Actually collapse
	Timelines.RemoveAt(Target + 1, Timelines.Num() - Target - 1, true);
	TMap<GridCoord, TArray<APlayerEntity*>> Persistent;
	for (int32 i = Players.Num() - 1; i > Target; --i)
	{
		APlayerEntity* Player = Players.Pop(true);
		if (Player->bInSuperposition) {
			Player->Flags |= PERSISTENT;
			Persistent.FindOrAdd(Player->GridLocation).Emplace(Player);
		}
		else {
			Grid.SetAt(Player->GridLocation, nullptr);
			Player->Destroy();
		}
	}

	TimelineCounter = Target;
	Timeline& Timeline = Timelines[TimelineCounter];
	TurnCounter = Equality;

	for (int32 i = 0; i < Timeline.Entities.Num(); ++i)
	{
		AEntity* Entity = Timeline.Entities[i];
		if (Entity->Flags & PERSISTENT) continue;
		Grid.SetAt(Entity->GridLocation, nullptr);
		Entity->GridLocation = Timeline.Locations[i];
		Grid.SetAt(Entity->GridLocation, Entity);
	}

	int32 Index = Equality * (TimelineCounter + 1);
	for (int32 i = Timeline.Subturns.Num() - 1; i >= Index; --i)
	{
		SubTurn& Subturn = Timeline.Subturns[i];
		for (int32 j = 0; j < Subturn.Entities.Num(); ++j)
		{
			AEntity* Entity = Subturn.Entities[j];
			if (Entity->Flags & PERSISTENT) continue;
			Grid.SetAt(Entity->GridLocation, nullptr);
			Entity->GridLocation = Subturn.Paths[Subturn.PathIndices[j]];
			Grid.SetAt(Entity->GridLocation, Entity);
		}
	}

	RevaluateSuperpositions();

	for (const auto& Pair : Persistent)
	{
		AEntity* Query = Grid.QueryAt(Pair.Value[0]->GridLocation);
		if (ASuperposition* Super = Cast<ASuperposition>(Query)) {
			for (APlayerEntity* Player : Pair.Value)
			{
				Super->Players.Emplace(Player);
				Player->Superposition = Super; //Is this necessary at all???
			}
		}
		else {
			if (Pair.Value.Num() == 1) {
				APlayerEntity* Player = Pair.Value[0];
				Grid.SetAt(Player->GridLocation, Player);
				Player->Flags &= ~SUPER;
				Player->Superposition = nullptr;
				Player->bInSuperposition = false;
				Player->SetActorLocation(GetWorldLocation(Player));
				Player->SetActorHiddenInGame(false);
			}
			else {
				for (ASuperposition* S : Superpositions)
				{
					if (S->Players.IsEmpty()) {
						Super = S;
						break;
					}
				}
				for (APlayerEntity* Player : Pair.Value)
				{
					Super->Players.Emplace(Player);
				}

				Super->OldSuperposition = nullptr;
				Super->GridLocation = Pair.Value[0]->GridLocation;
				Grid.SetAt(Super->GridLocation, Super);
				Super->SetActorLocation(GetWorldLocation(Super));
				Super->SetActorHiddenInGame(false);
			}
		}
	}

	for (AEntity* Entity : Timeline.Entities)
	{
		Entity->SetActorLocation(GetWorldLocation(Entity));
	}

	Timeline.Headers.RemoveAt(Equality, Timeline.Headers.Num() - Equality, true);
	Timeline.Subturns.RemoveAt(Index, Timeline.Subturns.Num() - Index, true);
	Timeline.Rewinder = nullptr;
}

APlayerEntity* UGameManager::SpawnPlayer()
{
	APlayerEntity* Player = WorldContext->SpawnActor<APlayerEntity>(PlayerBlueprint, GetWorldLocation(StartGridLocation), Rotation);
	Players.Emplace(Player);

	Player->AddActorWorldOffset(Player->Offset);
	Player->Flags |= MOVEABLE | CURRENT_PLAYER;
	Player->GridLocation = StartGridLocation;

	Player->GetStaticMeshComponent()->SetCustomPrimitiveDataFloat(0, TimelineCounter);
	return Player;
}

ASuperposition* UGameManager::SpawnSuperposition()
{
	ASuperposition* Superposition = WorldContext->SpawnActor<ASuperposition>(SuperBlueprint, GetWorldLocation(StartGridLocation), Rotation);
	Superpositions.Emplace(Superposition);

	Superposition->Flags |= MOVEABLE;
	Superposition->GridLocation = StartGridLocation;

	return Superposition;
}

FVector UGameManager::GetWorldLocation(const GridCoord& GridLocation)
{
	return Transform.TransformPosition(FVector(GridLocation) + (Dimensions / -2) + 0.5);
}

FVector UGameManager::GetWorldLocation(AEntity* Entity)
{
	return Transform.TransformPosition(FVector(Entity->GridLocation) + (Dimensions / -2) + 0.5) + Entity->Offset;
}

void UGameManager::VisualizeGrid()
{
	FlushDebugStrings(WorldContext);
	for (int32 i = 0; i < Grid.Grid.Num(); ++i)
	{
		int32 Index = i;
		int32 Z = Index / (Grid.Dimensions.X * Grid.Dimensions.Y);
		Index -= (Z * (Grid.Dimensions.X * Grid.Dimensions.Y));
		int32 Y = Index / Grid.Dimensions.X;
		int32 X = Index % Grid.Dimensions.X;

		DrawDebugString(WorldContext, GetWorldLocation(GridCoord(X, Y, Z)), FString::Printf(TEXT("%d, %d, %d"), X, Y, Z), NULL, FColor(1, 1, 1, 255));

		//AEntity* Entity = Grid.Grid[i];
		//if (!Entity || !(Entity->Flags & MOVEABLE)) {
		//	DrawDebugString(WorldContext, FVector(X, Y, Z) * BlockSize + Offset, FString::FromInt(i), NULL, FColor(0, 0, 0, 150));
		//}
		//else {
		//	DrawDebugString(WorldContext, FVector(X, Y, Z) * BlockSize + Offset, Entity->GetActorLabel(), NULL, FColor::Red);
		//}
	}
}

//-----------------------------------------------------------------------------------------------------------

AEntity* EntityGrid::QueryAt(const GridCoord& Location, bool* bIsValid)
{
	if (Location.X < 0 || Location.X >= Dimensions.X ||
		Location.Y < 0 || Location.Y >= Dimensions.Y ||
		Location.Z < 0 || Location.Z >= Dimensions.Z)
	{
		ERROR("Invalid query at x: %d, y: %d, z: %d", Location.X, Location.Y, Location.Z);
		if (bIsValid) *bIsValid = false;
		return nullptr;
	}

	if (bIsValid) *bIsValid = true;
	return Grid[Location.X + (Location.Y * Dimensions.X) + (Location.Z * Dimensions.X * Dimensions.Y)];
}

void EntityGrid::SetAt(const GridCoord& Location, AEntity* Entity)
{
	if (Location.X < 0 || Location.X >= Dimensions.X ||
		Location.Y < 0 || Location.Y >= Dimensions.Y ||
		Location.Z < 0 || Location.Z >= Dimensions.Z)
	{
		ERROR("Invalid set at x: %d, y: %d, z: %d", Location.X, Location.Y, Location.Z);
		return;
	}

	Grid[Location.X + (Location.Y * Dimensions.X) + (Location.Z * Dimensions.X * Dimensions.Y)] = Entity;
}

//-----------------------------------------------------------------------------

void UEntityAnimator::Start(TArray<SubTurn>& InSubturns, int32 Start, int32 End, bool bReverse)
{
	for (int32 SubturnIndex = Start; bReverse ? SubturnIndex >= End : SubturnIndex <= End; SubturnIndex += bReverse ? -1 : 1)
	{
		const SubTurn& Subturn = InSubturns[SubturnIndex];
		if (Subturn.Entities.IsEmpty()) continue;

		GroupIndices.Emplace(GroupQueue.Num());

		float MaxDuration = bReverse * FMath::Max(Subturn.Durations);
		int32 EntityIndex = 0;
		for (; EntityIndex < Subturn.Entities.Num(); ++EntityIndex)
		{
			EntityAnimationPath& Path = GroupQueue.Emplace_GetRef(Subturn.Entities[EntityIndex], MaxDuration - Subturn.Durations[EntityIndex], SubturnIndex);

			int32 PathIndex = Subturn.PathIndices[EntityIndex];
			int32 EndIndex = EntityIndex == Subturn.PathIndices.Num() - 1 ? Subturn.Paths.Num() - 1 : Subturn.PathIndices[EntityIndex + 1] - 1;

			if (bReverse) {
				Swap(PathIndex, EndIndex);

				for (; PathIndex >= EndIndex; --PathIndex)
				{
					Path.Path.Emplace(Transform.TransformPosition(FVector(Subturn.Paths[PathIndex]) + Offset) + Path.Entity->Offset);
				}
			}
			else {
				for (; PathIndex <= EndIndex; ++PathIndex)
				{
					Path.Path.Emplace(Transform.TransformPosition(FVector(Subturn.Paths[PathIndex]) + Offset) + Path.Entity->Offset);
				}
			}
		}
	}
	if (GroupIndices.IsEmpty()) {
		OnAnimationsFinished.Execute();
		return;
	}

	Subturns = &InSubturns;
	bIsUndo = bReverse;

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
		if (Animation.PathIndex == -2) {
			if (Animation.StartTime == 0 || CurrentTime - GroupStartTime >= Animation.StartTime) {
				Animation.PathIndex = 0;
				Animation.SubstepTime = CurrentTime;
			} 
			else continue;
		}	

		float MoveTime = 0.25;//(Animation.Path[Animation.PathIndex] - Animation.StartLocation).Z < 0 ? VerticalSpeed : HorizontalSpeed;
		//MoveTime *= Temp->PlayerController->SpeedMultiplier;
		float Alpha = FMath::Clamp((CurrentTime - Animation.SubstepTime) / MoveTime, 0, 1);

		int32 PathEnd = Animation.PathIndex == Animation.Path.Num() - 1 ? Animation.Path.Num() - 1 : Animation.PathIndex + 1;
		Animation.Entity->SetActorLocation(FMath::Lerp(Animation.Path[Animation.PathIndex], Animation.Path[PathEnd], Alpha));

		if (Animation.Entity->GetActorLocation() == Animation.Path[PathEnd]) {
			if (Animation.PathIndex == Animation.Path.Num() - 1) {
				Animation.PathIndex = -1;
				
				if (!bIsUndo) {
					SubTurn& Subturn = (*Subturns)[Animation.SubturnIndex];
					Subturn.Durations[Subturn.Entities.Find(Animation.Entity)] = CurrentTime - Animation.SubstepTime;
				}
			}
			else {
				++Animation.PathIndex;
				Animation.SubstepTime = CurrentTime;
			}
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