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


void URewindGameInstance::OnGamemodeInit(UGameManager* GameManager)
{
	GameManager->bIsOverworld = GetWorld() == OverworldLevel.Get();
	GameManager->LoadLevel();
}

void URewindGameInstance::EnterPuzzle()
{

}

//----------------------------------------------------------------------------------------------------------------

ARewindMenuMode::ARewindMenuMode()
{
	PlayerControllerClass = ARewindPlayerController::StaticClass();
}

void ARewindMenuMode::PostLogin(APlayerController* InController)
{
	AGameModeBase::PostLogin(InController);

	ARewindPlayerController* Controller = Cast<ARewindPlayerController>(InController);
	Controller->OnMouseClicked.BindUObject(this, &ARewindMenuMode::OnMouseClicked);
	Controller->OnEscapePressed.BindUObject(this, &ARewindMenuMode::OnEscape);
}

//------------------------------------------------------------------------------------------------------------

ARewindGameMode::ARewindGameMode()
{
	PlayerControllerClass = ARewindPlayerController::StaticClass();
	DefaultPawnClass = ARewindPawn::StaticClass();
}

void ARewindGameMode::PostLogin(APlayerController* InController)
{
	AGameModeBase::PostLogin(InController);

	GameManager = NewObject<UGameManager>(this);
	GameManager->Gamemode = this;
	GameManager->PlayerController = Cast<ARewindPlayerController>(InController);
	GameManager->BookClass = BookClass;

	ARewindPlayerController* Controller = GameManager->PlayerController;
	Controller->OnInputChanged.BindUObject(GameManager, &UGameManager::HandleMovementInput);
	Controller->OnPassPressed.BindUObject(GameManager, &UGameManager::HandlePassInput);
	Controller->OnRotateCamera.BindUObject(GameManager, &UGameManager::HandleCameraInput);
	Controller->OnUndoPressed.BindUObject(GameManager, &UGameManager::HandleUndoInput);
	Controller->OnRestartPressed.BindUObject(GameManager, &UGameManager::HandleRestartInput);
	Controller->OnEscapePressed.BindUObject(GameManager, &UGameManager::HandleEscapeInput);
	Controller->OnMouseClicked.BindUObject(GameManager, &UGameManager::HandleMouseClick);

	StaticCast<URewindGameInstance*>(GetGameInstance())->OnGamemodeInit(GameManager);
}

ARewindPawn::ARewindPawn()
{
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
}

//-------------------------------------------------------------------------------------------------------------------------

UGameManager::UGameManager()
{
	if (!GetWorld()) return;

	WorldContext = GetWorld();

	Animator = CreateDefaultSubobject<UEntityAnimator>(TEXT("Animator"));
	Animator->WorldContext = WorldContext;
	Animator->OnAnimationsFinished.BindUObject(this, &UGameManager::OnTurnEnd);

	//There must be a better way to do this...
	static ConstructorHelpers::FClassFinder<APlayerEntity> PlayerBP(TEXT("/Game/Blueprints/BP_Player"));
	static ConstructorHelpers::FClassFinder<ASuperposition> SuperBP(TEXT("/Game/Blueprints/BP_Superposition"));
	PlayerBlueprint = PlayerBP.Class;
	SuperBlueprint = SuperBP.Class;
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

	//Transfer puzzle data
	Grid.Dimensions = GridCoord(Settings->Dimensions);
	Dimensions = FVector(Settings->Dimensions);
	Transform = Settings->Transform;
	HeightMin = Settings->HeightMin;
	Rotation = FRotator(0, Transform.Rotator().Yaw, 0);

	Animator->Transform = Transform;
	Animator->Offset = Dimensions / -2 + 0.5;

	//Initialize grid
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

		if (bIsOverworld) {
			if (Settings->PuzzleMap.Find(i)) {
				PuzzleMap.Emplace(Entity, Settings->PuzzleMap[i]);
				Entity->Flags |= LOADING_TILE;
			}
		}
	}

	//Initialize cameras
	for (TActorIterator<ACameraActor> Itr(WorldContext); Itr; ++Itr)
	{
		if (Itr) Cameras.Emplace(*Itr);
	}
	FVector Center = Transform.GetLocation();
	Cameras.Sort([&Center](const ACameraActor& A, const ACameraActor& B)
		{
			float AngleA = FMath::Atan2(A.GetActorLocation().Y - Center.Y, A.GetActorLocation().X - Center.X);
			float AngleB = FMath::Atan2(B.GetActorLocation().Y - Center.Y, B.GetActorLocation().X - Center.X);

			return AngleA < AngleB;
		});
	if (!bIsOverworld) {
		float Min = 420;
		for (int32 i = 0; i < 4; ++i)
		{
			if (!Cameras[i]) continue;
			float Angle = FMath::Atan2(Cameras[i]->GetActorLocation().Y - Center.Y, Cameras[i]->GetActorLocation().X - Center.X);
			LOG("%s, %f", *Cameras[i]->GetActorLabel(), Angle);
			float Difference = FMath::Abs(Angle + (PI / 2));
			if (Difference < Min) {
				Min = Difference;
				StartingIndex = i;
			}
		}
	}
	CameraIndex = StartingIndex;
	PlayerController->SetViewTarget(Cameras[CameraIndex]);
	PlayerController->PlayerCameraManager->OnBlendComplete().AddUObject(this, &UGameManager::OnCameraBlendComplete);

	//Data
	APlayerEntity* Player = SpawnPlayer();
	Grid.SetAt(StartGridLocation, Player);

	SpawnSuperposition();

	Timelines.Emplace();
	CollapseQueue = -1;
	RewindQueue = nullptr;

	//more loading anims, camera pan???
	State = Waiting;
}

void UGameManager::UnloadLevel()
{
	for (const Timeline& Timeline : Timelines)
	{
		for (const SubTurn& Subturn : Timeline.Subturns)
		{
			for (EntityAnimation* Anim : Subturn.Animations)
			{
				delete Anim;
			}
		}
	}
}

//INPUT HANDLING
//-----------------------------------------------------------------------------------------------------------

void UGameManager::HandleMovementInput()
{
	if (State == Waiting) {
		if (PlayerController->NewestInput != NONE && !bPassPressed) {
			if (Animator->bIsAnimating) {
				Buffer = PlayerController->NewestInput;
				InputTimerStart = WorldContext->RealTimeSeconds;
			}
			else ProcessTurn(NONE);
		}
	}
}

void UGameManager::HandlePassInput(bool bStart)
{
	if (State == Waiting) {
		bPassPressed = bStart;
		if (bPassPressed && !Animator->bIsAnimating) ProcessTurn(PASS);
	}
}

void UGameManager::HandleCameraInput(float Direction)
{
	if (State == Waiting) {
		State = Loading;
		CameraIndex = (CameraIndex + FMath::RoundToInt(Direction) + 4) % 4;
		RedundancyTimer = WorldContext->TimeSeconds;
		PlayerController->SetViewTargetWithBlend(Cameras[CameraIndex], 0.5);
	}
	else if (WorldContext->TimeSeconds - RedundancyTimer > 2) {
		LOG("OnCameraBlendComplete failed, fallback to timer :(");
		State = Waiting;
		HandleCameraInput(Direction);
	}
}

void UGameManager::OnCameraBlendComplete()
{
	State = Waiting;
}

void UGameManager::HandleUndoInput()
{
	if (State != Waiting) return;
	if (Animator->bIsAnimating) return;

	if (TurnCounter == 0) {
		if (TimelineCounter == 0) return;

		State = Undoing;
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
			AEntity* Query = Grid.QueryAt(Entity->GridLocation);
			if (Query && (Query->Flags & PERSISTENT)) {
				Query->Destroy();
				LOG("Another one destroyed");
				//TODO: anim
			}
			Grid.SetAt(Entity->GridLocation, Entity);

			Entity->SetActorLocation(GetWorldLocation(Entity));
		}

		RevaluateSuperpositions(true);
		Timelines.RemoveAt(Timelines.Num() - 1);

		//Do special undo animation
		OnTurnEnd();
	}
	else {
		State = Undoing;
		LOG("Undoed turn %d", TurnCounter);
		Timeline& Timeline = Timelines[TimelineCounter];

		int32 EndIndex = (TimelineCounter + 1) * (TurnCounter - 1);
		for (int32 i = EndIndex + TimelineCounter; i >= EndIndex; --i)
		{
			SubTurn& Subturn = Timeline.Subturns[i];
			if (Subturn.Entities.IsEmpty()) continue;

			for (int32 j = 0; j < Subturn.Entities.Num(); ++j)
			{
				AEntity* Entity = Subturn.Entities[j];
				if (Grid.QueryAt(Entity->GridLocation) == Entity) {
					Grid.SetAt(Entity->GridLocation, nullptr);
				}
				Entity->GridLocation = Subturn.Paths[Subturn.PathIndices[j]];
				Grid.SetAt(Entity->GridLocation, Entity);

				if (ASuperposition* Super = Cast<ASuperposition>(Entity)) {
					for (APlayerEntity* Player : Super->Players) {
						Player->GridLocation = Subturn.Paths[Subturn.PathIndices[j]];
					}
				}
			}
			RevaluateSuperpositions();
		}

		Animator->Start(Timeline.Subturns, EndIndex + TimelineCounter, EndIndex, true);

		Timeline.Headers.RemoveAt(Timeline.Headers.Num() - 1);
		Timeline.Subturns.RemoveAt(Timeline.Subturns.Num() - (TimelineCounter + 1), TimelineCounter + 1, true);
		--TurnCounter;
	}

	Gamemode->OnTurnChanged(false, TurnCounter);
}

void UGameManager::HandleRestartInput(bool bStart)
{
	if (State == Waiting) {
		Gamemode->OnTurnChanged(false, 0); //TODO: not sure about this

		bRestartPressed = bStart;
		//First time pressing it
		if (bStart && RestartPresses == 0) {
			RestartTimerStart = WorldContext->RealTimeSeconds;
			++RestartPresses;
			return;
		}
		//Second (or third for that matter) time pressing it
		if (bStart && RestartPresses >= 1) {
			++RestartPresses;
			return;
		}
		//Releasing it
		if (!bStart) {
			//bHasPassed = false;
			return;
		}
	}
}

void UGameManager::Tick(float DeltaTime) {
	if (!WorldContext) return;
	if (WorldContext->RealTimeSeconds - RestartTimerStart > 0.25) {
		//If at 0.3 seconds it's only pressed once, do normal
		if (RestartPresses <= 1) {
			LOG("I am just restarded %d", TurnCounter);
			if (TurnCounter > 0) {
				Timeline& Timeline = Timelines[TimelineCounter];
				TurnCounter = 0;

				//Remove all the subturns and headers of this timeline
				Timeline.Subturns.RemoveAt(0, Timeline.Subturns.Num(), true);
				Timeline.Headers.RemoveAt(0, Timeline.Headers.Num(), true);

				//Update Grid
				for (int32 i = 0; i < Timeline.Entities.Num(); ++i)
				{
					AEntity* Entity = Timeline.Entities[i];
					Grid.SetAt(Entity->GridLocation, nullptr);
					Entity->GridLocation = StartGridLocation;
					Grid.SetAt(Entity->GridLocation, Entity);
					Entity->SetActorLocation(GetWorldLocation(Entity));
				}
				RevaluateSuperpositions(true);
			}
		}
		//else do hard reset
		else {
			LOG("I am really restarded %d", TurnCounter);
			//TODO: Gamemode->something
		}

		//Set RestartTimeStart to max
		RestartTimerStart = MAX_dbl;
		//Set number of times pressed to zero
		RestartPresses = 0;
	}

	//Mouse raycast
	if (State == Paused) return;

	FVector Start, Direction;
	PlayerController->DeprojectMousePositionToWorld(Start, Direction);

	FHitResult OutHit;
	if (WorldContext->LineTraceSingleByChannel(OutHit, Start, Start + (Direction * 5000), ECollisionChannel::ECC_Visibility)) {
		if (bIsOverworld) {
			if (OutHit.GetActor() != HitActor) {
				if (OutHit.GetActor() && OutHit.GetActor()->IsA(BookClass)) Gamemode->UpdateBook(true);
				else if (HitActor && HitActor->IsA(BookClass)) Gamemode->UpdateBook(false);
				HitActor = OutHit.GetActor();
			}
		}
		else {

		}
	}
}

void UGameManager::HandleEscapeInput()
{
	if (State != Paused) {
		Gamemode->Pause(true);
		State = Paused;
	}
	else {
		Gamemode->Pause(false);
		State = Waiting;
	}
}

void UGameManager::HandleMouseClick()
{
	if (bIsOverworld) {
		if (State != Paused) {
			if (HitActor && HitActor->IsA(BookClass)) {
				State = Paused;
				Gamemode->Pause(true);
			}
		}
	}
}

//GAME LOOP
//----------------------------------------------------------------------------------------------------------------------------------

void UGameManager::OnTurnEnd()
{
	LOG("Ending Turn %d", TurnCounter);

	if (EnterPuzzle.IsPending()) {
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, EnterPuzzle);
		return;
	}

	switch (State)
	{
	case Win:
		break;
	case Rewinding:
		PostRewind();
		break;
	case Collapsing:
		//Do we need to do more here???
		State = Waiting;
		break;
	}

	InputTimerStart = 0;

	if (RewindQueue) {
		RewindTimeline();
		return;
	}
	if (CollapseQueue != -1) {
		CollapseTimeline(CollapseQueue);
		return;
	}

	State = Waiting;

	//Process input buffer or stack
	double Elapsed = WorldContext->RealTimeSeconds - InputTimerStart;
	if (Buffer != NONE && (Elapsed >= 0 && Elapsed < 0.15f)) {
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
	int32 Turns = (FMath::RoundToInt((Rotation.Yaw + (((CameraIndex + StartingIndex) % 4) * 90)) / 90) % 4 + 4) % 4;
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
		if (!CheckClimbing(CurrentPlayer, CurrentPlayer->GridLocation, MoveInput)) return;
	}
	Buffer = NONE;

	Timeline& Timeline = Timelines[TimelineCounter];
	Timeline.Headers.Emplace(CurrentPlayer, MoveInput); //Create header for only the corresponding timeline's player
	++TurnCounter;
	Gamemode->OnTurnChanged(true, TurnCounter);

	//TODO: This is temporary, might need to restructure SubTurn when we hv more entity types
	//Because we only have players for now
	Timelines[TimelineCounter].Entities.Empty();
	Timelines[TimelineCounter].Locations.Empty();
	for (APlayerEntity* Player : Players)
	{
		Timelines[TimelineCounter].Entities.Emplace(Player);
		Timelines[TimelineCounter].Locations.Emplace(Player->GridLocation);

		Player->Flags &= ~CARRIED;
	}

	TArray<TPair<AEntity*, int32>> CollapseCandidates;
	for (int32 n = 0; n < TimelineCounter; ++n)
	{
		if (Timelines[n].NumTurns == TurnCounter) {
			CollapseCandidates.Emplace(Timelines[n].Rewinder, n);
		}
	}

	//Evaluate subturns
	PotentialRewinders.Empty();
	int32 EndIndex = -1;
	for (int32 i = TimelineCounter; i >= 0; --i)
	{
		SubTurn& Subturn = Timeline.Subturns.Emplace_GetRef();
		if (TurnCounter - 1 >= Timelines[i].Headers.Num()) continue;
		if (EndIndex != -1) continue;

		SubTurnHeader& Header = Timelines[i].Headers[TurnCounter - 1];
		if (!Header.Move.IsZero()) {
			EvaluateSubTurn(Header, Subturn);
			Subturn.Durations.Init(0, Subturn.Entities.Num());
			Subturn.Animations.AddZeroed(Subturn.Entities.Num() * 2);

			if ((State == Win ) || (EndIndex == -1 && RewindQueue)) {
				EndIndex = (TimelineCounter + 1) * (TurnCounter - 1) + (TimelineCounter - i);
			}
		}

		//Check collapse
		for (int32 n = CollapseCandidates.Num() - 1; n >= 0; --n)
		{
			AEntity* Query = Grid.QueryAt(CollapseCandidates[n].Key->GridLocation + DownVector);
			if (Query && (Query->Flags & REWIND)) {
				CollapseCandidates.RemoveAt(n);
			}
		}
	}

	//"Un-super" unmerged players
	for (APlayerEntity* Player : Players)
	{
		if (!Player->bInSuperposition) {
			Player->Flags &= ~SUPER;
			Player->Superposition = nullptr;
		}
	}

	for (AEntity* Player : PotentialRewinders)
	{
		if (!(Player->Flags & SUPER)) {
			RewindQueue = Player;
			break;
		}
	}

	if (!RewindQueue && !CollapseCandidates.IsEmpty()) {
		CollapseQueue = CollapseCandidates[0].Value;
	}

	//Dispatch animations
	if (EndIndex == -1) {
		EndIndex = (TimelineCounter + 1) * (TurnCounter - 1) + TimelineCounter;
	}

	int32 StartIndex = (TimelineCounter + 1) * (TurnCounter - 1);

	Animator->Start(Timeline.Subturns, StartIndex, EndIndex, false);
}

void UGameManager::EvaluateSubTurn(SubTurnHeader& Header, SubTurn& Subturn)
{
	TArray<AEntity*> Connected;
	Connected.Emplace(Header.Player);

	int32 Height = 0; //For climbing
	for (GridCoord GridLocation = Header.Player->GridLocation;;)
	{
		AEntity* Front = Grid.QueryAt(GridLocation += Header.Move);
		if (!Front) break;
		if (!(Front->Flags & MOVEABLE)) {
			Connected.Empty();
			if (CheckClimbing(Header.Player, Header.Player->GridLocation, Header.Move, &Connected, &Height)) {
				Connected.EmplaceAt(0, Header.Player);
				break;
			}
			return;
		}
		if (CheckSuperposition(Front, Connected.Last())) break;

		Connected.Emplace(Front);
	}

	//Un-super this subturn's player
	struct Defer {
		AEntity* Owner;
		EntityAnimation* Anim;
	};
	TArray<Defer> Deferred;

	if (Header.Player->Flags & SUPER) {
		if (Header.Player->bInSuperposition) {
			Header.Player->Superposition->Players.RemoveSingle(Header.Player);
			Header.Player->bInSuperposition = false;

			Deferred.Emplace(Header.Player, new EntityFade(Header.Player, true));

			if (Header.Player->Superposition->Players.Num() == 1) {
				Header.Player->Superposition->Players[0]->bInSuperposition = false;
				Deferred.Emplace(Header.Player, new EntityFade(Header.Player->Superposition, false));
				Deferred.Emplace(Header.Player, new EntityFade(Header.Player->Superposition->Players[0], true));

				Grid.SetAt(Header.Player->Superposition->GridLocation, Header.Player->Superposition->Players[0]);
				Header.Player->Superposition->GridLocation = GridCoord(-1, -1, -1);
				Header.Player->Superposition->Players.Empty();

				AEntity* Query = Grid.QueryAt(Header.Player->GridLocation + DownVector);
				if (!RewindQueue && Query && (Query->Flags & REWIND)) {
					bool bSkip = false;
					for (const Timeline& Timeline : Timelines)
					{
						if (Timeline.Rewinder == Header.Player && Timeline.NumTurns == TurnCounter) bSkip = true;
					}
					if (!bSkip) RewindQueue = Header.Player;
				}
			}
		}
	}

	//Climb
	if (Height != 0) {
		UpdateEntityPosition(Subturn, Connected[0], GridCoord(0, 0, Height));
	}

	//Update from farthest to self
	for (int32 i = Connected.Num() - 1; i >= 0; --i)
	{
		//Update from bottom up, and evaluate horizontal movement before gravity
		AEntity* Entity = Connected[i];
		GridCoord EntityOrigin = Entity->GridLocation;
		UpdateEntityPosition(Subturn, Entity, Header.Move);

		for (;;)
		{
			AEntity* Below = Grid.QueryAt(Entity->GridLocation + DownVector);
			if (Below && !CheckSuperposition(Below, Entity)) break;

			if (Entity->GridLocation.Z - 1 <= HeightMin) {
				break;
			}

			UpdateEntityPosition(Subturn, Entity, DownVector);
		}

		for (;;)
		{
			AEntity* Up = Grid.QueryAt(EntityOrigin += UpVector);
			if (!Up || !(Up->Flags & MOVEABLE)) break;

			AEntity* Front = Grid.QueryAt(EntityOrigin + Header.Move);
			if (Front && !(Front->Flags & MOVEABLE)) break;

			Up->Flags |= CARRIED;
			UpdateEntityPosition(Subturn, Up, Header.Move);

			for (;;)
			{
				AEntity* Below = Grid.QueryAt(Up->GridLocation + DownVector);
				if (Below) break;

				UpdateEntityPosition(Subturn, Up, DownVector);
			}
		}
	}
	
	for (const Defer& Anim : Deferred) AddAnimation(Subturn, Anim.Owner, Anim.Anim);

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

					AddAnimation(Subturn, Player, new EntityFade(Player, false), false);
					
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
					break;
				}
			}
			if (!NewSuper) {
				NewSuper = SpawnSuperposition();
			}

			NewSuper->OldSuperposition = Pair.Value[0]->Superposition;
			check(Pair.Value.Num() == 2); //TODO: almost certain this is true;
			if (Header.Player != Pair.Value[0] && Header.Player != Pair.Value[1]) ERROR("NewSuper formed from not subturn player???");
			AddAnimation(Subturn, Pair.Value[0], new EntityFade(NewSuper, true), false);

			for (APlayerEntity* Player : Pair.Value)
			{
				Player->bInSuperposition = true;
				Player->Superposition = NewSuper;
				NewSuper->Players.Emplace(Player);
				AddAnimation(Subturn, Pair.Value[0], new EntityFade(Player, false), false);
			}

			NewSuper->GridLocation = Pair.Value[0]->GridLocation;
			Grid.SetAt(NewSuper->GridLocation, NewSuper);
			AddAnimation(Subturn, Pair.Value[0], new TeleportSuper(NewSuper, GetWorldLocation(NewSuper)), false);
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

	//LOL NOT TRUE!!! We assume entities can only move once contiguously in a subturn
	int32 Index = Subturn.Entities.Find(Entity);
	if (Index == -1) {
		Subturn.Entities.Emplace(Entity);
		Subturn.PathIndices.Emplace(Subturn.Paths.Emplace(OldLocation));
		Subturn.Paths.Emplace(Entity->GridLocation);
	}
	else if (Index == Subturn.Entities.Num() - 1) {
		Subturn.Paths.Emplace(Entity->GridLocation);
	}
	else {
		Subturn.Paths.EmplaceAt(Subturn.PathIndices[Index + 1], Entity->GridLocation);
		for (int32 i = Index + 1; i < Subturn.PathIndices.Num(); ++i) {
			++Subturn.PathIndices[i];
		}
	}

	//Check for rewind tile
	if (Entity->IsA<ASuperposition>()) return;
	if ((Entity->Flags & SUPER)) {
		PotentialRewinders.AddUnique(Entity);
		return;
	}

	AEntity* Query = Grid.QueryAt(Entity->GridLocation + DownVector);
	if (!Query) return;
	if (bIsOverworld && (Query->Flags & LOADING_TILE)) {
		EnterPuzzle = PuzzleMap[Query];
		return;
	}
	if (!RewindQueue && Query->Flags & REWIND) {
		for (const Timeline& Timeline : Timelines)
		{
			if (Timeline.Rewinder == Entity && Timeline.NumTurns == TurnCounter) return;
		}

		RewindQueue = Entity;
	}
	if (Query->Flags & GOAL) {
		State = Win;
	}
}

//GAME LOOP AUXILIARY
//----------------------------------------------------------------------------------------------------------------

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
		return (Superposition && !(Superposition->Flags & PERSISTENT) &&
			   (Superposition->OldSuperposition == StaticCast<APlayerEntity*>(From)->Superposition));
	}
	return false;
}

bool UGameManager::CheckClimbing(AEntity* Entity, const GridCoord& Location, const GridCoord& Delta, TArray<AEntity*>* Connected, int32* Height)
{
	for (GridCoord GridLocation = Location;;)
	{
		AEntity* Front = Grid.QueryAt(GridLocation += Delta);
		if (!Front) {
			if (!bIsOverworld) return true;
			return Grid.QueryAt(GridLocation + DownVector) != nullptr;
		}
		if (!(Front->Flags & MOVEABLE)) {
			if (Grid.QueryAt(Location + Delta)->Flags & CLIMBABLE && !Grid.QueryAt(Location + UpVector)) {
				if (Connected) {
					Connected->Empty();
					*Height += 1;
				}
				return CheckClimbing(Entity, Location + UpVector, Delta, Connected, Height);
			}
			return false;
		}
		AEntity* Back = Grid.QueryAt(GridLocation - Delta);
		if (CheckSuperposition(Front, Back ? Back : Entity)) return true;

		if (Connected) Connected->Emplace(Front);
	}
}

void UGameManager::RewindTimeline()
{
	State = Rewinding;

	//Check for new persistent superpositions
	TMap<GridCoord, TArray<APlayerEntity*>> NewPersistent;
	for (APlayerEntity* Player : Players)
	{
		if (Player->bInSuperposition) {
			NewPersistent.FindOrAdd(Player->GridLocation).Emplace(Player);
		}
	}

	for (ASuperposition* Super : Superpositions)
	{
		Super->Players.Empty();
		Super->OldSuperposition = nullptr;

		if (Grid.QueryAt(Super->GridLocation) == Super) {
			Grid.SetAt(Super->GridLocation, nullptr);
		}
		Super->GridLocation = GridCoord(-1, -1, -1);
		Super->SetActorHiddenInGame(true);
	}

	for (const auto& Pair : NewPersistent)
	{
		AEntity* Entity = WorldContext->SpawnActor<AEntity>(SuperBlueprint, FVector::Zero(), Rotation);
		Entity->Flags |= MOVEABLE | CLIMBABLE | PERSISTENT;
		Entity->GridLocation = Pair.Key;
		Entity->SetActorLocation(GetWorldLocation(Entity));
		Entity->SetActorHiddenInGame(false);
		//Slighly different material;

		Grid.SetAt(Entity->GridLocation, Entity);
	}

	//Traverse paths in reverse accounting for immovable blockers
	TArray<EntityAnimationPath> AnimationGroups;
	TArray<uint16> GroupIndices;
	for (int32 SubturnIndex = Timelines[TimelineCounter].Subturns.Num() - 1; SubturnIndex >= 0; --SubturnIndex)
	{
		GroupIndices.Emplace(AnimationGroups.Num());

		SubTurn& Subturn = Timelines[TimelineCounter].Subturns[SubturnIndex];
		float MaxDuration = FMath::Max(Subturn.Durations);
		for (int32 EntityIndex = 0; EntityIndex < Subturn.Entities.Num(); ++EntityIndex)
		{
			AEntity* Entity = Subturn.Entities[EntityIndex];
			if ((Entity->Flags & PERSISTENT) || Entity->IsA<ASuperposition>()) continue;

			EntityAnimationPath& AnimPath = AnimationGroups.Emplace_GetRef(Entity, MaxDuration - Subturn.Durations[EntityIndex], SubturnIndex);
			AnimPath.Path.Emplace(GetWorldLocation(Entity));

			int32 PathIndex = EntityIndex == Subturn.PathIndices.Num() - 1 ? Subturn.Paths.Num() - 2 : Subturn.PathIndices[EntityIndex + 1] - 1;
			for (; PathIndex >= Subturn.PathIndices[EntityIndex]; --PathIndex)
			{
				AEntity* Query = Grid.QueryAt(Subturn.Paths[PathIndex]);
				if (Query && !Query->IsA<APlayerEntity>()) {
					if (Query->Flags & MOVEABLE) {
						Query->Destroy();
						//TODO: animation
						LOG("Destroying persistent cuz conflict");
					}
					else continue;
				}

				if (Grid.QueryAt(Entity->GridLocation) == Entity) {
					Grid.SetAt(Entity->GridLocation, nullptr);
				}
				Entity->GridLocation = Subturn.Paths[PathIndex];
				Grid.SetAt(Entity->GridLocation, Entity);

				AnimPath.Path.Emplace(GetWorldLocation(Entity));
			}
		}
	}

	Animator->Start(AnimationGroups, GroupIndices);
	Gamemode->OnRewind(true, TurnCounter, Cameras[CameraIndex]);
}

void UGameManager::PostRewind()
{
	Gamemode->OnRewind(false);

	//Start new timeline
	Timelines[TimelineCounter].Rewinder = RewindQueue;
	Timelines[TimelineCounter].NumTurns = TurnCounter;

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
	Superpositions[0]->SetActorRotation(Rotation);
	Superpositions[0]->Players.Append(Players);

	for (APlayerEntity* Player : Players)
	{
		Player->Flags |= SUPER;
		Player->bInSuperposition = true;
		Player->Superposition = Superpositions[0];

		if (Grid.QueryAt(Player->GridLocation) == Player) {
			Grid.SetAt(Player->GridLocation, nullptr);
		}
		Player->GridLocation = StartGridLocation;
		Player->SetActorHiddenInGame(true);
		Player->SetActorRotation(Rotation);
	}

	State = Waiting;
}

void UGameManager::CollapseTimeline(int32 Target)
{
	Gamemode->OnCollapse();
	State = Collapsing;
	LOG("Collapsing to timeline %d", Target);

	CollapseQueue = -1;

	GridCoord CollapseRemnant = Timelines[Target].Rewinder->GridLocation;
	Timelines.RemoveAt(Target + 1, Timelines.Num() - Target - 1, true);
	TimelineCounter = Target;
	TurnCounter = 0;
	Timeline& Timeline = Timelines[TimelineCounter];

	//Destroy collapsed players
	TArray<GridCoord> Persistent;
	for (int32 i = Players.Num() - 1; i >= 0; --i)
	{
		if (Players[i]->bInSuperposition) {
			Persistent.AddUnique(Players[i]->GridLocation);
		}
		if (i > Target) {
			APlayerEntity* Player = Players.Pop();
			Grid.SetAt(Player->GridLocation, nullptr);
			Player->Destroy();
		}
	}

	//Reset locations to turn 0
 	for (int32 i = 0; i < Timeline.Entities.Num(); ++i)
	{
		AEntity* Entity = Timeline.Entities[i];
		if (Entity->Flags & PERSISTENT) continue;
		Grid.SetAt(Entity->GridLocation, nullptr);
		Entity->GridLocation = Timeline.Locations[i];
		Grid.SetAt(Entity->GridLocation, Entity);
	}
	for (int32 i = Timeline.Subturns.Num() - 1; i >= 0; --i)
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
	for (AEntity* Entity : Timeline.Entities)
	{
		Entity->SetActorLocation(GetWorldLocation(Entity));
	}
	RevaluateSuperpositions(true);

	//Spawn new persistent "superpositions"
	for (const GridCoord& Location : Persistent)
	{
		AEntity* Entity = WorldContext->SpawnActor<AEntity>(SuperBlueprint, FVector::Zero(), Rotation);
		Entity->Flags |= MOVEABLE | CLIMBABLE | PERSISTENT;
		Entity->GridLocation = Location;
		Entity->SetActorLocation(GetWorldLocation(Entity));
		Entity->SetActorHiddenInGame(false);
		//Slighly different material;

		Grid.SetAt(Location, Entity);
	}

	//"Collpase Remnant"
	AEntity* Entity = WorldContext->SpawnActor<AEntity>(PlayerBlueprint, FVector::Zero(), Rotation);
	Entity->Flags |= CLIMBABLE | PERSISTENT;
	Entity->GridLocation = CollapseRemnant;
	Entity->SetActorLocation(GetWorldLocation(Entity));
	//Slighly different material;

	Grid.SetAt(CollapseRemnant, Entity);

	Timeline.Headers.Empty();
	Timeline.Subturns.Empty();
	Timeline.Rewinder = nullptr;

	OnTurnEnd();
}

void UGameManager::RevaluateSuperpositions(bool bDoAnim)
{
	for (ASuperposition* Super : Superpositions)
	{
		Super->Players.Empty();
		Super->OldSuperposition = nullptr;
		if (bDoAnim) Super->SetActorHiddenInGame(true);

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
				if (bDoAnim) Player->SetActorHiddenInGame(true);
			}

			Super->OldSuperposition = nullptr;
			Super->GridLocation = Pair.Value[0]->GridLocation;
			Grid.SetAt(Super->GridLocation, Super);
			Super->SetActorLocation(GetWorldLocation(Super));
			if (bDoAnim) Super->SetActorHiddenInGame(false);
		}
		else {
			Pair.Value[0]->Flags &= ~SUPER;
			Pair.Value[0]->bInSuperposition = false;
			Pair.Value[0]->Superposition = nullptr;
			if (bDoAnim) Pair.Value[0]->SetActorHiddenInGame(false);
		}
	}
}

//HELPERS
//-----------------------------------------------------------------------------------------------------------

APlayerEntity* UGameManager::SpawnPlayer()
{
	APlayerEntity* Player = WorldContext->SpawnActor<APlayerEntity>(PlayerBlueprint, GetWorldLocation(StartGridLocation), Rotation);
	Players.Emplace(Player);

	Player->AddActorWorldOffset(Player->Offset);
	Player->Flags |= MOVEABLE | CLIMBABLE;
	Player->GridLocation = StartGridLocation;

	Player->GetStaticMeshComponent()->SetCustomPrimitiveDataFloat(0, TimelineCounter);
	return Player;
}

ASuperposition* UGameManager::SpawnSuperposition()
{
	ASuperposition* Superposition = WorldContext->SpawnActor<ASuperposition>(SuperBlueprint, GetWorldLocation(StartGridLocation), Rotation);
	Superpositions.Emplace(Superposition);

	Superposition->Flags |= MOVEABLE | CLIMBABLE;
	Superposition->GridLocation = StartGridLocation;

	return Superposition;
}

void UGameManager::AddAnimation(SubTurn& Subturn, AEntity* Owner, EntityAnimation* Anim, bool bIsStart)
{
	int32 Index = Subturn.Entities.Find(Owner) * 2 + !bIsStart;
	if (Index < 0) {
		ERROR("Couldn't find %s in Subturn.Entities", *Owner->GetName());
		return;
	}
	if (Subturn.Animations.IsValidIndex(Index) && Subturn.Animations[Index]) {
		EntityAnimation* Slot = Subturn.Animations[Index];
		while (Slot->Additional)
		{
			Slot = Slot->Additional;
		}
		Slot->Additional = Anim;
	}
	else {
		if (Subturn.Animations.IsEmpty()) Subturn.Animations.Init(nullptr, 2); //Need or else crash during RelocateConstructItems...
		Subturn.Animations.EmplaceAt(Index, Anim);
	}
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
		
		AEntity* Entity = Grid.Grid[i];
		if (!Entity || !(Entity->Flags & MOVEABLE)) {
			DrawDebugString(WorldContext, GetWorldLocation(GridCoord(X, Y, Z)), FString::FromInt(i), NULL, FColor(0, 0, 0, 150));
		}
		else {
			DrawDebugString(WorldContext, GetWorldLocation(GridCoord(X, Y, Z)), Entity->GetActorLabel(), NULL, FColor::Red);
		}
	}
}

//GRID
//------------------------------------------------------------------------------------------------------------

AEntity* EntityGrid::QueryAt(const GridCoord& Location, bool* bIsValid)
{
	if (Location.X < 0 || Location.X >= Dimensions.X ||
		Location.Y < 0 || Location.Y >= Dimensions.Y ||
		Location.Z < 0 || Location.Z >= Dimensions.Z)
	{
		WARN("Invalid query at x: %d, y: %d, z: %d", Location.X, Location.Y, Location.Z);
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
		WARN("Invalid set at x: %d, y: %d, z: %d", Location.X, Location.Y, Location.Z);
		return;
	}

	Grid[Location.X + (Location.Y * Dimensions.X) + (Location.Z * Dimensions.X * Dimensions.Y)] = Entity;
}

//ANIMATOR
//------------------------------------------------------------------------------------------------------------------------------------------

void UEntityAnimator::Start(TArray<SubTurn>& InSubturns, int32 Start, int32 End, bool bReverse)
{
	for (int32 SubturnIndex = Start; bReverse ? SubturnIndex >= End : SubturnIndex <= End; SubturnIndex += bReverse ? -1 : 1)
	{
		const SubTurn& Subturn = InSubturns[SubturnIndex];
		if (Subturn.Entities.IsEmpty()) continue;

		GroupIndices.Emplace(GroupQueue.Num());

		float MaxDuration = bReverse * FMath::Max(Subturn.Durations);		
		for (int32 EntityIndex = 0; EntityIndex < Subturn.Entities.Num(); ++EntityIndex)
		{
			EntityAnimationPath& Path = GroupQueue.Emplace_GetRef(
				Subturn.Entities[EntityIndex], MaxDuration - Subturn.Durations[EntityIndex], SubturnIndex,
				Subturn.Animations[EntityIndex * 2], Subturn.Animations[EntityIndex * 2 + 1]
			);

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

	GroupStartTime = WorldContext->TimeSeconds;
	bIsAnimating = true;
}

void UEntityAnimator::Start(TArray<EntityAnimationPath>& InGroups, TArray<uint16> InGroupIndices)
{
	GroupQueue = MoveTemp(InGroups);
	GroupIndices = MoveTemp(InGroupIndices);

	Subturns = nullptr;
	bIsUndo = true;
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

				if (APlayerEntity* Player = Cast<APlayerEntity>(Animation.Entity)) {
					if (!(Player->Flags & CARRIED)) {
						FVector Direction = Animation.Path.Last() - Animation.Path[0];
						FRotator Rotation = FRotator(0, FMath::RadiansToDegrees(Direction.HeadingAngle()) - 90 + (bIsUndo * 180), 0);
						Player->SetActorRotation(Rotation);
						if (Player->Superposition) Player->Superposition->SetActorRotation(Rotation);
					}
				}

				if (bIsUndo) {
					EntityAnimation* Anim = Animation.EndAnim;
					while (Anim)
					{
						Anim->Play(bIsUndo);
						Anim = Anim->Additional;
					}
				}
				else {
					if (Animation.StartAnim) {
						EntityAnimation* Anim = Animation.StartAnim;
						while (Anim)
						{
							Anim->Play(bIsUndo);
							Anim = Anim->Additional;
						}
					}
				}
			}
			else continue;
		}	

		int32 PathEnd = Animation.PathIndex == Animation.Path.Num() - 1 ? Animation.Path.Num() - 1 : Animation.PathIndex + 1;

		float MoveTime = (Animation.Path[PathEnd] - Animation.Path[Animation.PathIndex]).Z < 0 ? 0.2 : 0.25;
		MoveTime *= bIsUndo ? 0.5 : 1;
		float Alpha = FMath::Clamp((CurrentTime - Animation.SubstepTime) / MoveTime, 0, 1);

		Animation.Entity->SetActorLocation(FMath::Lerp(Animation.Path[Animation.PathIndex], Animation.Path[PathEnd], Alpha));

		if (Animation.Entity->GetActorLocation().Equals(Animation.Path[PathEnd])) {
			//Animation finish
			if (Animation.PathIndex == Animation.Path.Num() - 1) {
				Animation.PathIndex = -1;
				
				if (!bIsUndo) {
					SubTurn& Subturn = (*Subturns)[Animation.SubturnIndex];
					Subturn.Durations[Subturn.Entities.Find(Animation.Entity)] = CurrentTime - Animation.SubstepTime;

					if (Animation.EndAnim) {
						EntityAnimation* Anim = Animation.EndAnim;
						while (Anim)
						{
							Anim->Play(bIsUndo);
							Anim = Anim->Additional;
						}
					}
				}
				else if (Animation.StartAnim) {
					EntityAnimation* Anim = Animation.StartAnim;
					while (Anim)
					{
						Anim->Play(bIsUndo);
						Anim = Anim->Additional;
					}
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

//------------------------------------------------------------------------------------------------

void EntityFade::Play(bool bIsUndo)
{
	if (bFadeIn) Target->SetActorHiddenInGame(bIsUndo ? true : false);
	else Target->SetActorHiddenInGame(bIsUndo ? false : true);
}

void TeleportSuper::Play(bool bIsUndo)
{
	if (!bIsUndo) Super->SetActorLocation(Destination);
}