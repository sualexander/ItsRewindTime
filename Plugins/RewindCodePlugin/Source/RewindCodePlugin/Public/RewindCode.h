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

//This should be moved elsewhere
struct EntityGrid
{
	int32 WIDTH, LENGTH, HEIGHT;
	TArray<AEntity*> Grid;

	AEntity* QueryAt(const FIntVector& Location, bool* bIsValid = nullptr);
	void SetAt(const FIntVector& Location, AEntity* Entity);
};

UCLASS()
class REWINDCODEPLUGIN_API UGameManager : public UObject
{
	GENERATED_BODY()

public:
	UGameManager();

	UWorld* WorldContext;
	class ARewindPlayerController* PlayerController;
	UPROPERTY()
	UEntityAnimator* Animator;

	//Input
	EInputStates Buffer;
	double InputTimerStart;

	void HandleInput();
	void ProcessTurn(EInputStates Input);
	void OnTurnEnd();

	//Main
	TArray<struct Turn> Turns;
	int32 TurnCounter = 0;

	TArray<APlayerEntity*> Players;
	TArray<ASuperposition*> Superpositions;

	void EvaluateSubTurn(struct SubTurn& SubTurn);
	bool EvaluateSuperposition(const struct SubTurn& SubTurn, AEntity* A, AEntity* B);
	void UpdateEntityPosition(struct SubTurn& SubTurn, AEntity* Entity, const FIntVector& Delta);

	//Rewind
	TArray<AEntity*> RewindQueue;
	void DoRewind();

	//Grid
	EntityGrid Grid;
	int32 BLOCK_SIZE = 300;
	FIntVector StartGridLocation;
	int32 HEIGHT_MIN = -5;

	void LoadGridFromFile();
};

//move base entity stuff into seperate file
enum EntityFlags : uint32
{
	PLAYER = 1U,
	MOVEABLE = 2U,
	REWIND = 4U,
	SUPER = 8U,
	CURRENT_PLAYER = 16U,
	GOAL = 32U
};

UCLASS()
class REWINDCODEPLUGIN_API AEntity : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	uint32 Flags = 0;
	FIntVector GridLocation;
};

UCLASS(Blueprintable)
class REWINDCODEPLUGIN_API APlayerEntity : public AEntity
{
	GENERATED_BODY()

public:
	APlayerEntity();

	ASuperposition* Superposition;
};

UCLASS(Blueprintable)
class REWINDCODEPLUGIN_API ASuperposition : public AEntity
{
	GENERATED_BODY()

public:
	bool bOccupied = false;
};

//------------------------------------------------

struct Turn
{
	//Per turn data flags? 
	TArray<struct SubTurn> SubTurns;
};

struct SubTurn //Timelines
{
	AEntity* Player;
	FIntVector Move;
	FIntVector Location;

	TArray<AEntity*> Entities;
	TArray<uint16> PathIndices; //Maps to AllPaths

	TArray<FIntVector> AllPaths;
	//TArray<void*> Other;

	SubTurn(AEntity* Player, FIntVector& Move) : Player(Player), Move(Move) {}
};

struct EntityAnimationPath
{
	AEntity* Entity;
	TArray<FVector> Path;

	int32 PathIndex = -2;
	double StartTime;
	FVector StartLocation;

	EntityAnimationPath(AEntity* Entity) : Entity(Entity) {}

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

	void Start(const struct Turn& Turn);
	UWorld* WorldContext;

	DECLARE_DELEGATE(FOnAnimationsFinished)
	FOnAnimationsFinished OnAnimationsFinished;

	TArray<EntityAnimationPath> Queue;
	TArray<uint16> QueueIndices;
	int32 QueueIndex;

	float HorizontalSpeed = 0.25, VerticalSpeed = 0.15;

	/*UPROPERTY(EditDefaultsOnly, Category = "Animation", BlueprintReadWrite)
	FVectorCurve MoveLocationCurve;*/
};