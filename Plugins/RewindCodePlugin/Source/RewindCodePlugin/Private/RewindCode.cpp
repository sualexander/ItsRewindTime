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

TArray<FLinearColor> COLORS =
{
	FLinearColor(0.f, 0.f, 1.f),
	FLinearColor(0.f, 1.f, 0.f),
	FLinearColor(1.f, 0.f, 0.f),
	FLinearColor(1.f, 0.f, 1.f),
	FLinearColor(0.f, 1.f, 1.f),
	FLinearColor(1.f, 1.f, 0.f)
};

FString EnumConvertTemp(EInputStates Enum)
{
	if (Enum == S) return TEXT("S");
	else if (Enum == A) return TEXT("A");
	else if (Enum == D) return TEXT("D");
	else if (Enum == NONE) return TEXT("None");
	return TEXT("W");
}

bool HasCollision(UGameManager::ETileState Tile)
{
	return Tile == UGameManager::ETileState::WALL || Tile == UGameManager::ETileState::REWIND_TILE || Tile == UGameManager::ETileState::START_TILE;
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
	GameManager->Player->OnHitRewindTile.BindUObject(GameManager, &UGameManager::QueueRewind);
	//more input bindings

	GameManager->PlayerController->GetPawn()->GetRootComponent()->SetMobility(EComponentMobility::Static);
}


//-----------------------------------------------------------------------------

UGameManager::UGameManager()
{
	if (!GetWorld()) return;

	WorldContext = GetWorld();

	StartTileLocation = FIntVector(0, 0, 3);

	for (int32 Y = 0; Y < 10; ++Y) {
		for (int32 X = 0; X < 10; ++X) {
			FIntVector IntLocation(X, Y, 0);

			if (X == 5 && Y == 5)
			{
				Grid.Add(IntLocation, ETileState::REWIND_TILE);
			}
			else
			{
				Grid.Add(IntLocation, ETileState::WALL);
			}


		}
	}

	for (int32 Y = 0; Y < 4; ++Y) {
		for (int32 X = 0; X < 3; ++X) {
			for (int32 Z = 1; Z < 3; Z++)
			{
				FIntVector IntLocation(X, Y, Z);

				if (X == StartTileLocation.X && Y == StartTileLocation.Y && Z == StartTileLocation.Z - 1)
				{
					Grid.Add(IntLocation, ETileState::START_TILE);
				}
				else
				{
					Grid.Add(IntLocation, ETileState::WALL);
				}
			}
		}
	}


	BuildLevel();

	Animator = GetWorld()->SpawnActor<AEntityAnimator>(AEntityAnimator::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	Animator->OnMoveFinished.BindUObject(this, &UGameManager::OnTurnEnd);
	//Animator->Init(this);

	Player = GetWorld()->SpawnActor<APlayerEntity>(APlayerEntity::StaticClass(), FVector(0, 0, BlockSize * 3), FRotator::ZeroRotator);
	Player->Init(StartTileLocation);
	Player->SetMobility(EComponentMobility::Movable);
	//Player->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EditorMeshes/ArcadeEditorSphere.ArcadeEditorSphere")));
	Player->bIsActivePlayer = true;
	Player->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/RewindCube")));

	//Player->OnQueueAnimation.BindUObject(Animator, &AEntityAnimator::QueueAnimation);
	Animator->InitNewPlayer(Player);

	PlayerMI = UMaterialInstanceDynamic::Create(Player->GetStaticMeshComponent()->GetMaterial(0), Player);
	PlayerMI->SetVectorParameterValue(FName(TEXT("Color")), COLORS[0]);
	PlayerMI->SetScalarParameterValue(FName(TEXT("Glow")), 10.f);
	Player->GetStaticMeshComponent()->SetMaterial(0, PlayerMI);
}

void UGameManager::HandleInput()
{
	UE_LOG(LogTemp, Warning, TEXT("HandleInput with %s"), *EnumConvertTemp(PlayerController->NewestInput));

	if (bRewindQueued)
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't move with rewind queued"));
		return;
	}

	if (PlayerController->NewestInput != NONE) {
		if (/*Animator->IsAnimating()*/NumEntitiesAnimating > 0) InputTimerStart = WorldContext->RealTimeSeconds;
		else ProcessTurn(NONE);
	}
}

void UGameManager::OnTurnEnd()
{
	UE_LOG(LogTemp, Warning, TEXT("Ending Turn"));
	//bTurn = false;
	NumEntitiesAnimating--;

	if (NumEntitiesAnimating > 0) return;
	//if (Animator->IsAnimating()) return;

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
	bRewindQueued = false;

	PrevTimeline = CurrentTimeline;
	CurrentTimeline = TArray<Turn>();

	APlayerEntity* NewPlayer = GetWorld()->SpawnActor<APlayerEntity>(APlayerEntity::StaticClass(), FVector(0, 0, BlockSize * 3), FRotator::ZeroRotator);
	NewPlayer->Init(StartTileLocation);

	for (int i = 0; i < PastEntities.Num(); i++)
	{
		PastEntities[i]->Init(StartTileLocation);
	}

	Player->Init(StartTileLocation);

	//NewPlayer->OnMoveFinished.BindUObject(this, &UGameManager::OnTurnEnd);
	NewPlayer->SetMobility(EComponentMobility::Movable);
	NewPlayer->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/RewindCube")));
	//NewPlayer->OnQueueAnimation.BindUObject(Animator, &AEntityAnimator::QueueAnimation);
	Animator->InitNewPlayer(NewPlayer);

	PastEntities.Add(NewPlayer);

	UMaterialInstanceDynamic* NewPlayerMI = UMaterialInstanceDynamic::Create(NewPlayer->GetStaticMeshComponent()->GetMaterial(0), NewPlayer);
	NewPlayerMI->SetVectorParameterValue(FName(TEXT("Color")), COLORS[(PastEntities.Num() - 1) % COLORS.Num()]);
	NewPlayer->GetStaticMeshComponent()->SetMaterial(0, NewPlayerMI);

	PlayerMI->SetVectorParameterValue(FName(TEXT("Color")), COLORS[PastEntities.Num() % COLORS.Num()]);

	Animator->ClearAnimQueues();
}

void UGameManager::BuildLevel()
{
	UStaticMesh* BlockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EngineMeshes/Cube.Cube"));
	UMaterial* RewindTileMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/RewindTileMaterial"));
	UMaterial* StartTileMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/StartTileMaterial"));


	for (TTuple<FIntVector, ETileState> Pair : Grid)
	{
		FIntVector IntLocation = Pair.Key;
		FVector Location(IntLocation.X * BlockSize, IntLocation.Y * BlockSize, IntLocation.Z * BlockSize);

		AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
		Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Actor->GetStaticMeshComponent()->SetStaticMesh(BlockMesh);

		switch (Pair.Value)
		{
		case WALL:
			//Grid.Add(IntLocation, ETileState::WALL);
			break;
		case REWIND_TILE:
			//Grid.Add(IntLocation, ETileState::REWIND_TILE);
			Actor->GetStaticMeshComponent()->SetMaterial(0, RewindTileMaterial);
			break;
		case START_TILE:
			//Grid.Add(IntLocation, ETileState::START_TILE);
			Actor->GetStaticMeshComponent()->SetMaterial(0, StartTileMaterial);
			break;

		}


	}
}

void UGameManager::ProcessTurn(EInputStates Input)
{
	//ResolveMoment

	//Check rewind tile
	//check timeline collapse

	EInputStates moveInput = Input == NONE ? PlayerController->NewestInput : Input;

	EntityState State;

	if (!Player->AttemptMove(moveInput, Grid, State)) return;

	Turn Turn;
	Turn.States.SetNum(PastEntities.Num() + 1);

	int n = 0;

	for (int i = PastEntities.Num() - 1; i >= 0; i--)
	{
		EntityState s;
		EInputStates pastMoveInput = EInputStates::NONE;
		if (CurrentTimeline.Num() < PrevTimeline.Num()) pastMoveInput = PrevTimeline[CurrentTimeline.Num()].States[i].Move;;
		if (PastEntities[i]->AttemptMove(pastMoveInput, Grid, s)) n++;
		Turn.States[i] = s;
	}


	Turn.States[PastEntities.Num()] = State;

	CurrentTimeline.Add(Turn);

	//Dispatch animations
	UE_LOG(LogTemp, Error, TEXT("Starting Turn with %s"), *EnumConvertTemp(PlayerController->NewestInput));
	//bTurn = true;
	NumEntitiesAnimating = n + 1;
	Buffer = NONE;



	UE_LOG(LogTemp, Warning, TEXT("Move Recorded. Length: %s"), *FString::FromInt(CurrentTimeline.Num()));



}

APlayerEntity::APlayerEntity()
{
	//PrimaryActorTick.bCanEverTick = true;
}

void APlayerEntity::Move(FVector Target)
{
	bIsMoving = true;
	Start = GetActorLocation();
	End = Target;
	MoveTime = 0;
}


bool APlayerEntity::AttemptMove(EInputStates InputDir, TMap<FIntVector, UGameManager::ETileState> Grid, EntityState& State)
{
	RefreshLocation();

	State.StartingTileLocation = TileLocation;
	State.Move = InputDir;

	EEntityAnimations Animation;
	FIntVector IntDir;
	switch (InputDir) {
	case W:
		Animation = EEntityAnimations::FORWARD;
		IntDir = FIntVector(0, 1, 0);
		break;
	case S:
		Animation = EEntityAnimations::BACKWARD;
		IntDir = FIntVector(0, -1, 0);
		break;
	case A:
		Animation = EEntityAnimations::LEFT;
		IntDir = FIntVector(1, 0, 0);
		break;
	case D:
		Animation = EEntityAnimations::RIGHT;
		IntDir = FIntVector(-1, 0, 0);
		break;
	default:
		return false;
	}

	FIntVector NewLoc = TileLocation + IntDir;

	if (Grid.Contains(NewLoc) && HasCollision(Grid[NewLoc]))
	{
		UE_LOG(LogTemp, Warning, TEXT("COLLISION"));
		return false;
	}

	TileLocation = NewLoc;

	OnQueueAnimation.Execute(this, Animation);

	while (!Grid.Contains(TileLocation - FIntVector(0, 0, 1)) || !HasCollision(Grid[TileLocation - FIntVector(0, 0, 1)]))
	{
		OnQueueAnimation.Execute(this, DOWN);
		TileLocation -= FIntVector(0, 0, 1);

		if (TileLocation.Z < -5) return false;

	}

	if (bIsActivePlayer && Grid.Contains(TileLocation - FIntVector(0, 0, 1)) && Grid[TileLocation - FIntVector(0, 0, 1)] == UGameManager::ETileState::REWIND_TILE) OnHitRewindTile.Execute();

	return true;
}

AEntity::AEntity()
{
	//Animator = CreateDefaultSubobject<AEntityAnimator>(TEXT("Animator"));
}

FVector AEntity::GetTargetLocation()
{
	float BlockSize = 300.f;//Extract this from elsewhere
	return FVector(BlockSize * TileLocation.X, BlockSize * TileLocation.Y, BlockSize * TileLocation.Z);
}

void AEntity::Init(FIntVector Loc)
{
	//Animator->ClearAnimQueue();
	TileLocation = Loc;
	RefreshLocation();

	SetActorRotation(FRotator::ZeroRotator);

	//Animator->PreviousTransform = GetTransform();
}

void AEntity::RefreshLocation()
{
	SetActorLocation(GetTargetLocation());
}

AEntityAnimator::AEntityAnimator()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AEntityAnimator::UpdateAnimation(float DeltaTime)
{
	if (AnimationStacks.IsEmpty()) return;

	for (TTuple<AEntity*, EntityAnimationData> t : AnimationStacks)
	{

		if (t.Value.AnimationStack.Num() > 0)
		{


			if (DoAnimationFrame(t.Key, DeltaTime))
				DoAnimationFrame(t.Key, 0.f); //If the previous animation completed, start the next one

			if (AnimationStacks.Find(t.Key)->AnimationStack.Num() == 0)
			{
				OnMoveFinished.Execute();
			}

		}

	}



}

void AEntityAnimator::QueueAnimation(AEntity* Entity, EEntityAnimations Animation)
{
	EntityAnimationData* eData = AnimationStacks.Find(Entity);

	if (eData->AnimationStack.Num() == 0)
		eData->TimeRemaining = GetDefaultAnimationTime(Animation);

	AnimationStacks.Find(Entity)->AnimationStack.Add(Animation);
}

bool AEntityAnimator::DoAnimationFrame(AEntity* Entity, double DeltaTime)
{
	EntityAnimationData* eData = AnimationStacks.Find(Entity);

	//TArray<EEntityAnimations> AnimationStack = eData->AnimationStack;

	eData->TimeRemaining -= DeltaTime;


	if (eData->AnimationStack.Num() == 0)
	{
		/*bIsAnimating = false;*/
		return false;
	}

	EEntityAnimations Animation = eData->AnimationStack[0];

	/*if (!bIsAnimating)
	{
		bIsAnimating = true;
		PreviousTransform = GetOwner()->GetTransform();
		AnimStartTime = UGameplayStatics::GetTimeSeconds(GetWorld());
	}*/



	if (eData->TimeRemaining <= 0)
	{

		FTransform t = InterpolateAnimation(Animation, 1.f, eData->PreviousTransform);
		Entity->SetActorLocation(eData->PreviousTransform.GetLocation() + t.GetLocation());
		//Entity->SetActorRotation(eData->PreviousTransform.GetRotation() + t.GetRotation());

		Entity->SetActorRotation(FRotator::ZeroRotator);
		eData->PreviousTransform = Entity->GetActorTransform();

		eData->AnimationStack.RemoveAt(0);

		if (eData->AnimationStack.Num() == 0)
			eData->TimeRemaining = 0.f;
		else
			eData->TimeRemaining += GetDefaultAnimationTime(eData->AnimationStack[0]);

		return true;
	}

	FTransform t = InterpolateAnimation(Animation, 1.f - eData->TimeRemaining / GetDefaultAnimationTime(Animation), eData->PreviousTransform);
	Entity->SetActorLocation(eData->PreviousTransform.GetLocation() + t.GetLocation());
	Entity->SetActorRotation(eData->PreviousTransform.GetRotation() + t.GetRotation());
	return false;
}

float AEntityAnimator::GetDefaultAnimationTime(EEntityAnimations Animation)
{
	//Could be integrated directly into InterpolateAnimation instead
	if (Animation == EEntityAnimations::DOWN) return 0.3;
	return 0.35;
}

FTransform AEntityAnimator::InterpolateAnimation(EEntityAnimations Animation, float Progress, FTransform prev)
{
	//double progress = Time / MaxTime;
	FVector loc = FVector::ZeroVector;
	FRotator rot = FRotator::ZeroRotator;

	//Hardcoded motion of cube based on which animation is playing
	//This could be substituted for getting data from transform curves instead, allowing for more control over animations

	//Extract this from Game Manager instead
	float BlockSize = 300.f;

	switch (Animation)
	{
	case LEFT:
		loc = (FVector(-BlockSize / 2.f, 0, BlockSize / 2.f)).RotateAngleAxis(90.f * Progress, FVector::RightVector) + (FVector(BlockSize / 2.f, 0, -BlockSize / 2.f));
		rot = FRotator::MakeFromEuler(FVector(0.f, Progress * -180.f, 0.f));
		break;
	case RIGHT:
		loc = (FVector(BlockSize / 2.f, 0, BlockSize / 2.f)).RotateAngleAxis(90.f * Progress, FVector::LeftVector) + (FVector(-BlockSize / 2.f, 0, -BlockSize / 2.f));
		rot = FRotator::MakeFromEuler(FVector(0.f, Progress * 180.f, 0.f));
		break;
	case BACKWARD:
		loc = (FVector(0, BlockSize / 2.f, BlockSize / 2.f)).RotateAngleAxis(90.f * Progress, FVector::ForwardVector) + (FVector(0, -BlockSize / 2.f, -BlockSize / 2.f));
		rot = FRotator::MakeFromEuler(FVector(Progress * -180.f, 0.f, 0.f));
		break;
	case FORWARD:
		loc = (FVector(0, -BlockSize / 2.f, BlockSize / 2.f)).RotateAngleAxis(90.f * Progress, FVector::BackwardVector) + (FVector(0, BlockSize / 2.f, -BlockSize / 2.f));
		rot = FRotator::MakeFromEuler(FVector(Progress * 180.f, 0.f, 0.f));
		break;
	case DOWN:
		loc = FVector::DownVector * BlockSize * Progress;
		break;
	default:
		break;
	}

	return FTransform(rot.Quaternion(), loc, prev.GetScale3D());
}

bool AEntityAnimator::IsAnimating()
{
	for (TTuple<AEntity*, EntityAnimationData> t : AnimationStacks)
	{
		if (t.Value.AnimationStack.Num() != 0)
			return false;
	}

	return true;
}

void AEntityAnimator::ClearAnimQueues()
{
	for (TTuple<AEntity*, EntityAnimationData> t : AnimationStacks)
	{
		EntityAnimationData* eData = AnimationStacks.Find(t.Key);

		eData->AnimationStack.Empty();
		eData->TimeRemaining = 0.f;
		eData->PreviousTransform = t.Key->GetTransform();
	}
}

void AEntityAnimator::InitNewPlayer(APlayerEntity* Player)
{
	EntityAnimationData eData = EntityAnimationData();
	eData.PreviousTransform = Player->GetTransform();
	AnimationStacks.Add(Player, eData);

	Player->OnQueueAnimation.BindUObject(this, &AEntityAnimator::QueueAnimation);
}

void AEntityAnimator::Tick(float DeltaTime)
{
	UpdateAnimation(DeltaTime);
}
