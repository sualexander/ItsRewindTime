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

UCLASS(BlueprintType)
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
	void HandleMovementInput();

	bool bHasPassed = false;
	bool bPassPressed = false;
	void HandlePassInput(bool bStart);
	void ProcessTurn(EInputStates Input);
	void OnTurnEnd();

	//Main
	TArray<struct Turn> Turns;
	int32 TurnCounter = 0;
	int32 TimelineCounter = 0;

	TArray<APlayerEntity*> Players;
	TArray<ASuperposition*> Superpositions;

	void EvaluateSubTurn(struct SubTurn& SubTurn);
	void UpdateEntityPosition(struct SubTurn& SubTurn, AEntity* Entity, const FIntVector& Delta);
	bool CheckSuperposition(AEntity* To, AEntity* From);

	APlayerEntity* SpawnPlayer();
	ASuperposition* SpawnSuperposition();

	//Rewind
	TArray<AEntity*> RewindQueue;
	void DoRewind();

	//Grid
	EntityGrid Grid;
	int32 BLOCK_SIZE = 300;
	FIntVector StartGridLocation;
	int32 HEIGHT_MIN = -5;

	void LoadGridFromFile();

	//Blueprints
	UClass* PlayerBlueprint;
	UClass* SuperBlueprint;

	//Debug
	UFUNCTION(BlueprintCallable)
	void VisualizeGrid();
};

//move base entity stuff into seperate file
enum EntityFlags : uint32
{
	MOVEABLE			= 1U,
	REWIND				= 1U << 1,
	SUPER				= 1U << 2,
	GOAL				= 1U << 3,
	CURRENT_PLAYER		= 1U << 4
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
	ASuperposition* Superposition;
	bool bInSuperposition = false;
};

UCLASS(Blueprintable)
class REWINDCODEPLUGIN_API ASuperposition : public AEntity
{
	GENERATED_BODY()

public:
	TArray<APlayerEntity*> Players;
	ASuperposition* OldSuperposition;
};

//------------------------------------------------

struct Turn
{
	//Per turn data flags? 
	TArray<struct SubTurn> SubTurns;
};

struct SubTurn //Timelines
{
	APlayerEntity* Player;
	FIntVector Move;
	FIntVector Location;

	TArray<AEntity*> Entities;
	TArray<uint16> PathIndices; //Maps to AllPaths

	TArray<FIntVector> AllPaths;
	//TArray<void*> Other;

	bool bIsPlayersFinalMove; //Can be changed to a uint32 flags if more bools are needed for SubTurn

	SubTurn(AEntity* Player, FIntVector& Move);
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

	void Start(const struct Turn& Turn, int EndIndex);
	UWorld* WorldContext;

	DECLARE_DELEGATE(FOnAnimationsFinished)
	FOnAnimationsFinished OnAnimationsFinished;

	TArray<EntityAnimationPath> Queue;
	TArray<uint16> QueueIndices;
	int32 QueueIndex;

	UGameManager* Temp;
	float HorizontalSpeed = 0.25, VerticalSpeed = 0.1;
};