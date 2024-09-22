// Copyright It's Rewind Time 2024

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/GameModeBase.h"

#include "RewindCode.generated.h"


enum EInputStates;

UCLASS()
class REWINDCODEPLUGIN_API ARewindGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARewindGameMode();

	void PostLogin(APlayerController* Controller) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UGameManager* GameManager;
};

class APlayerEntity;

UCLASS()
class REWINDCODEPLUGIN_API UGameManager : public UObject
{
	GENERATED_BODY()

public:
	UGameManager();

	UWorld* WorldContext;
	class ARewindPlayerController* PlayerController;
	UEntityAnimator* Animator;

	FIntVector StartGridPosition;

	void HandleInput();
	void ProcessTurn(EInputStates Input);
	void OnTurnEnd();

	void QueueRewind();
	void DoRewind();

	bool bRewindQueued = false;

	double InputTimerStart;
	EInputStates Buffer;

	UMaterialInstanceDynamic* PlayerMI;

	APlayerEntity* Player;
	TArray<APlayerEntity*> PastPlayers;

	TArray<Turn> Turns;
	int32 TurnCounter = 0;
	
	TArray<AEntity*> Grid;
	int32 BLOCK_SIZE = 300;
	int32 WIDTH = 10, LENGTH = 10, HEIGHT = 5;

	int32 Flatten(int32 X, int32 Y, int32 Z) 
	{
		return X + (Y * WIDTH) + (Z * WIDTH * LENGTH);
	}
	int32 Flatten(const FIntVector& Location)
	{
		return Location.X + (Location.Y * WIDTH) + (Location.Z * WIDTH * LENGTH);
	}

	bool MoveEntity(AEntity* Entity, const FIntVector& Move);
	void UpdateEntityPosition(AEntity* Entity, const FIntVector& Delta);
};

//move base entity stuff into seperate file
enum EntityFlags : uint32
{
	PLAYER = 1U,
	MOVEABLE = 2U,
	REWIND = 4U
};

UCLASS()
class REWINDCODEPLUGIN_API AEntity : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	uint32 Flags = 0;
	FIntVector GridPosition;
};

UCLASS(Blueprintable)
class REWINDCODEPLUGIN_API APlayerEntity : public AEntity
{
	GENERATED_BODY()

public:
	APlayerEntity();

	void Init(FIntVector Loc);
};

//------------------------------------------------

struct EntityState
{
	FIntVector Move;
	TArray<FIntVector> Path; //In grid positions
};

struct Turn
{
	TMap<AEntity*, EntityState> EntityStates;
};

struct EntityAnimationPath
{
	EntityAnimationPath(AEntity* Entity, TArray<FIntVector>& Path);

	AEntity* Entity;
	TArray<FIntVector> Path;
	TArray<uint8> PathIndices; 

	int32 PathIndex = 0;
	double StartTime = 0;
};

UCLASS()
class REWINDCODEPLUGIN_API UEntityAnimator : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	void Tick(float DeltaTime) override;
	bool IsTickable() const override { return bIsAnimating; }
	bool bIsAnimating = false;
	TStatId GetStatId() const override 
	{ 
		RETURN_QUICK_DECLARE_CYCLE_STAT(UEntityAnimator, STATGROUP_Tickables); 
	}

	void Start();
	UWorld* WorldContext;

	DECLARE_DELEGATE(FOnAnimationsFinished)
	FOnAnimationsFinished OnAnimationsFinished;

	TArray<EntityAnimationPath> Paths;
	int32 QueueIndex = 0;

	float StartTime;
	float HorizontalSpeed = 0.5, VerticalSpeed = 0.3;

	/*UPROPERTY(EditDefaultsOnly, Category = "Animation", BlueprintReadWrite)
	FVectorCurve MoveLocationCurve;*/
};
