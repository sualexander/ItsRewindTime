// Copyright It's Rewind Time 2024

#include "RewindCode.h"
#include "Input.h"

#include "Kismet/GameplayStatics.h"

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

	

	Player = GetWorld()->SpawnActor<APlayerEntity>(APlayerEntity::StaticClass(), FVector(0, 0, BlockSize*3), FRotator::ZeroRotator);
	Player->Init(StartTileLocation);
	Player->OnMoveFinished.BindUObject(this, &UGameManager::OnTurnEnd);
	Player->SetMobility(EComponentMobility::Movable);
	//Player->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EditorMeshes/ArcadeEditorSphere.ArcadeEditorSphere")));
	Player->bIsActivePlayer = true;
	Player->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/RewindCube")));

	PlayerMI = UMaterialInstanceDynamic::Create(Player->GetStaticMeshComponent()->GetMaterial(0), Player); // This is where I appear to be crashing at.
	PlayerMI->SetVectorParameterValue(FName(TEXT("Color")), COLORS[0]);
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
	Player->Init(StartTileLocation);
	bRewindQueued = false;

	PrevTimeline = CurrentTimeline;
	CurrentTimeline = TArray<Turn>();

	APlayerEntity* NewPlayer = GetWorld()->SpawnActor<APlayerEntity>(APlayerEntity::StaticClass(), FVector(0, 0, BlockSize * 3), FRotator::ZeroRotator);
	NewPlayer->Init(StartTileLocation);

	for (int i = 0; i < PastEntities.Num(); i++)
	{
		PastEntities[i]->Init(StartTileLocation);
	}

	//NewPlayer->OnMoveFinished.BindUObject(this, &UGameManager::OnTurnEnd);
	NewPlayer->SetMobility(EComponentMobility::Movable);
	NewPlayer->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/RewindCube")));
	PastEntities.Add(NewPlayer);

	UMaterialInstanceDynamic* NewPlayerMI = UMaterialInstanceDynamic::Create(NewPlayer->GetStaticMeshComponent()->GetMaterial(0), NewPlayer); // This is where I appear to be crashing at.
	NewPlayerMI->SetVectorParameterValue(FName(TEXT("Color")), COLORS[(PastEntities.Num() - 1) % COLORS.Num()]);
	NewPlayer->GetStaticMeshComponent()->SetMaterial(0, NewPlayerMI);

	PlayerMI->SetVectorParameterValue(FName(TEXT("Color")), COLORS[PastEntities.Num() % COLORS.Num()]);
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
			Grid.Add(IntLocation, ETileState::WALL);
			break;
		case REWIND_TILE:
			Grid.Add(IntLocation, ETileState::REWIND_TILE);
			Actor->GetStaticMeshComponent()->SetMaterial(0, RewindTileMaterial);
			break;
		case START_TILE:
			Grid.Add(IntLocation, ETileState::START_TILE);
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

	for (int i = PastEntities.Num() - 1; i >= 0; i--)
	{
		EntityState s;
		EInputStates pastMoveInput = EInputStates::NONE;
		if (CurrentTimeline.Num() < PrevTimeline.Num()) pastMoveInput = PrevTimeline[CurrentTimeline.Num()].States[i].Move;;
		PastEntities[i]->AttemptMove(pastMoveInput, Grid, s);
		Turn.States[i] = s;
	}
	

	Turn.States[PastEntities.Num()] = State;

	CurrentTimeline.Add(Turn);

	//Dispatch animations
	UE_LOG(LogTemp, Error, TEXT("Starting Turn with %s"), *EnumConvertTemp(PlayerController->NewestInput));
	bTurn = true;
	Buffer = NONE;



	UE_LOG(LogTemp, Warning, TEXT("Move Recorded. Length: %s"), *FString::FromInt(CurrentTimeline.Num()));

	

}

APlayerEntity::APlayerEntity()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APlayerEntity::Tick(float DeltaTime)
{
	AEntity::Tick(DeltaTime);

	/*if (bIsMoving) {
		MoveTime += DeltaTime;
		float Alpha = FMath::Clamp(MoveTime / MoveDuration, 0.0f, 1.0f);

		FVector NewLocation = FMath::Lerp(Start, End, Alpha);
		SetActorLocation(NewLocation);

		if (Alpha >= 1.0f)
		{
			bIsMoving = false;
			OnMoveFinished.Execute();
		}
	}*/

	if (Animator->IsAnimating())
	{
		Animator->UpdateAnimation();
		
		if (bIsActivePlayer && !Animator->IsAnimating())
		{
			OnMoveFinished.Execute();
			//RefreshLocation();
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
	Animator = CreateDefaultSubobject<UEntityAnimatorComponent>(TEXT("Animator"));
}

bool APlayerEntity::AttemptMove(EInputStates InputDir, TMap<FIntVector, UGameManager::ETileState> Grid, EntityState &State)
{
	RefreshLocation();

	State.StartingTileLocation = TileLocation;
	State.Move = InputDir;

	UEntityAnimatorComponent::EEntityAnimations Animation;
	FIntVector IntDir;
	switch (InputDir) {
	case W:
		Animation = UEntityAnimatorComponent::EEntityAnimations::FORWARD;
		IntDir = FIntVector(0, 1, 0);
		break;
	case S:
		Animation = UEntityAnimatorComponent::EEntityAnimations::BACKWARD;
		IntDir = FIntVector(0, -1, 0);
		break;
	case A:
		Animation = UEntityAnimatorComponent::EEntityAnimations::LEFT;
		IntDir = FIntVector(1, 0, 0);
		break;
	case D:
		Animation = UEntityAnimatorComponent::EEntityAnimations::RIGHT;
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

	Animator->QueueAnimation(Animation);

	while (!Grid.Contains(TileLocation - FIntVector(0, 0, 1)) || !HasCollision(Grid[TileLocation - FIntVector(0, 0, 1)]))
	{
		Animator->QueueAnimation(UEntityAnimatorComponent::DOWN);
		TileLocation -= FIntVector(0, 0, 1);

		if (TileLocation.Z < -5) return false;

	}

	if (bIsActivePlayer && Grid.Contains(TileLocation - FIntVector(0, 0, 1)) && Grid[TileLocation - FIntVector(0, 0, 1)] == UGameManager::ETileState::REWIND_TILE) OnHitRewindTile.Execute();

	return true;
}

void AEntity::Init(FIntVector Loc)
{
	TileLocation = Loc;
	RefreshLocation();
}

void AEntity::RefreshLocation()
{
	float BlockSize = 300.f;
	SetActorLocation(FVector(BlockSize * TileLocation.X, BlockSize * TileLocation.Y, BlockSize * TileLocation.Z));
}

UEntityAnimatorComponent::UEntityAnimatorComponent()
{
	//PrimaryComponentTick.bCanEverTick = true;
}



void UEntityAnimatorComponent::UpdateAnimation()
{
	if (AnimationStack.Num() > 0)
		if (DoAnimationFrame())
			DoAnimationFrame(); //If the previous animation completed, start the next one

}

void UEntityAnimatorComponent::QueueAnimation(EEntityAnimations Animation)
{
	AnimationStack.Add(Animation);
}

bool UEntityAnimatorComponent::DoAnimationFrame()
{
	if (AnimationStack.Num() == 0)
	{
		bIsAnimating = false;
		return false;
	}

	EEntityAnimations Animation = AnimationStack[0];

	if (!bIsAnimating)
	{
		bIsAnimating = true;
		PreviousTransform = GetOwner()->GetTransform();
		AnimStartTime = UGameplayStatics::GetTimeSeconds(GetWorld());
	}

	double Elapsed = UGameplayStatics::GetTimeSeconds(GetWorld()) - AnimStartTime;
	double AnimTotalTime = GetDefaultAnimationTime(Animation);

	if (Elapsed >= AnimTotalTime)
	{

		FTransform t = InterpolateAnimation(Animation, AnimTotalTime, AnimTotalTime, PreviousTransform);
		GetOwner()->SetActorLocation(PreviousTransform.GetLocation() + t.GetLocation());
		GetOwner()->SetActorRotation(PreviousTransform.GetRotation() + t.GetRotation());

		GetOwner()->SetActorRotation(FRotator::ZeroRotator);
		PreviousTransform = GetOwner()->GetActorTransform();
		AnimStartTime += AnimTotalTime;

		AnimationStack.RemoveAt(0);

		if (AnimationStack.Num() == 0)
			bIsAnimating = false;

		return true;
	}

	FTransform t = InterpolateAnimation(Animation, Elapsed, AnimTotalTime, PreviousTransform);
	GetOwner()->SetActorLocation(PreviousTransform.GetLocation() + t.GetLocation());
	GetOwner()->SetActorRotation(PreviousTransform.GetRotation() + t.GetRotation());
	return false;
}

double UEntityAnimatorComponent::GetDefaultAnimationTime(EEntityAnimations Animation)
{
	//Could be integrated directly into InterpolateAnimation instead
	if (Animation == EEntityAnimations::DOWN) return 0.3;
	return 0.35;
}

FTransform UEntityAnimatorComponent::InterpolateAnimation(EEntityAnimations Animation, double Time, double MaxTime, FTransform prev)
{
	double progress = Time / MaxTime;
	FVector loc = FVector::ZeroVector;
	FRotator rot = FRotator::ZeroRotator;

	//Hardcoded motion of cube based on which animation is playing
	//This could be substituted for getting data from transform curves instead, allowing for more control over animations

	//Extract this from Game Manager instead
	float BlockSize = 300.f;

	switch (Animation)
	{
	case UEntityAnimatorComponent::LEFT:
		loc = (FVector(-BlockSize / 2.f, 0, BlockSize / 2.f)).RotateAngleAxis(90.f * progress, FVector::RightVector) + (FVector(BlockSize / 2.f, 0, -BlockSize / 2.f));
		rot = FRotator::MakeFromEuler(FVector(0.f, progress * -180.f, 0.f));
		break;
	case UEntityAnimatorComponent::RIGHT:
		loc = (FVector(BlockSize / 2.f, 0, BlockSize / 2.f)).RotateAngleAxis(90.f * progress, FVector::LeftVector) + (FVector(-BlockSize / 2.f, 0, -BlockSize / 2.f));
		rot = FRotator::MakeFromEuler(FVector(0.f, progress * 180.f, 0.f));
		break;
	case UEntityAnimatorComponent::BACKWARD:
		loc = (FVector(0, BlockSize / 2.f, BlockSize / 2.f)).RotateAngleAxis(90.f * progress, FVector::ForwardVector) + (FVector(0, -BlockSize / 2.f, -BlockSize / 2.f));
		rot = FRotator::MakeFromEuler(FVector(progress * -180.f, 0.f, 0.f));
		break;
	case UEntityAnimatorComponent::FORWARD:
		loc = (FVector(0, -BlockSize / 2.f, BlockSize / 2.f)).RotateAngleAxis(90.f * progress, FVector::BackwardVector) + (FVector(0, BlockSize / 2.f, -BlockSize / 2.f));
		rot = FRotator::MakeFromEuler(FVector(progress * 180.f, 0.f, 0.f));
		break;
	case UEntityAnimatorComponent::DOWN:
		loc = FVector::DownVector * BlockSize * progress;
		break;
	default:
		break;
	}

	return FTransform(rot.Quaternion(), loc, prev.GetScale3D());
}


