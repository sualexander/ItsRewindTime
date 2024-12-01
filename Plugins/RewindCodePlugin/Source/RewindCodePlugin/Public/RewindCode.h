// Copyright It's Rewind Time 2024

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/StaticMeshActor.h"

#include "Camera/CameraComponent.h"

#include "RewindCode.generated.h"


enum EInputStates;
using GridCoord = UE::Math::TIntVector3<int8>;

UCLASS()
class REWINDCODEPLUGIN_API ARewindGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARewindGameMode();

	void PostLogin(APlayerController* Controller) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UGameManager* GameManager;

	UFUNCTION(BlueprintImplementableEvent)
	void TurnChanged(int32 TurnCount);
};


UCLASS(BlueprintType, Blueprintable)
class REWINDCODEPLUGIN_API ARewindPawn : public APawn
{
	GENERATED_BODY()

public:
	ARewindPawn();

	UPROPERTY()
	UCameraComponent* Camera;
};

//-----------------------------------------------------------------------------------

struct EntityGrid
{
	TArray<AEntity*> Grid;
	GridCoord Dimensions;

	AEntity* QueryAt(const GridCoord& Location, bool* bIsValid = nullptr);
	void SetAt(const GridCoord& Location, AEntity* Entity);
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
	ARewindGameMode* Gamemode;

	//Loading
	void LoadLevel();

	//Input
	EInputStates Buffer;
	double InputTimerStart;
	void HandleMovementInput();

	bool bPassPressed = false;
	void HandlePassInput(bool bStart);

	void HandleUndoInput();

	int32 CameraRotation = 0;

	void ProcessTurn(EInputStates Input);
	void OnTurnEnd();

	//Main
	enum GameState {
		Paused,
		Waiting,
		Rewinding,
		Collapsing,
		Undoing,
		Loading,
		Win
	};

	GameState State = Loading;
	TArray<struct Timeline> Timelines;
	int32 TurnCounter = 0;
	int32 TimelineCounter = 0;

	TArray<APlayerEntity*> Players;
	TArray<ASuperposition*> Superpositions;

	void EvaluateSubTurn(struct SubTurnHeader& Header, struct SubTurn& Subturn);
	void UpdateEntityPosition(struct SubTurn& Subturn, AEntity* Entity, const GridCoord& Delta);
	bool CheckSuperposition(AEntity* To, AEntity* From);
	bool CheckClimbing(
		AEntity* Entity, const GridCoord& Location, const GridCoord& Delta, 
		TArray<AEntity*>* Connected = nullptr, int32* Height = nullptr);
	APlayerEntity* SpawnPlayer();
	ASuperposition* SpawnSuperposition();

	//Rewind
	AEntity* RewindQueue;
	void RewindTimeline();

	void RevaluateSuperpositions();

	int32 CollapseQueue;
	void CollapseTimeline(int32 Target);

	//Grid
	EntityGrid Grid;
	FVector Dimensions;
	FTransform Transform;
	FRotator Rotation;
	GridCoord StartGridLocation;
	int32 HeightMin = -1;

	//Blueprints
	UClass* PlayerBlueprint;
	UClass* SuperBlueprint;

	//Debug
	UFUNCTION(BlueprintCallable)
	void VisualizeGrid();

	FVector GetWorldLocation(const GridCoord& GridLocation);
	FVector GetWorldLocation(AEntity* Entity);
};

//move base entity stuff into seperate file
enum EntityFlags : uint32
{
	MOVEABLE			= 1U,
	CLIMBABLE			= 1U << 1,
	PERSISTENT			= 1U << 2,
	SUPER				= 1U << 3,
	GOAL				= 1U << 4,
	REWIND				= 1U << 5,
	CURRENT_PLAYER		= 1U << 6
};

UCLASS()
class REWINDCODEPLUGIN_API AEntity : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	uint32 Flags = 0;
	GridCoord GridLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Offset;
};

UCLASS(Blueprintable)
class REWINDCODEPLUGIN_API APlayerEntity : public AEntity
{
	GENERATED_BODY()

public:
	ASuperposition* Superposition;
	bool bInSuperposition = false;

	//UFUNCTION(BlueprintImplementableEvent)
	//void UpdateVisuals(int32 Type) {}
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

struct Timeline
{
	TArray<struct SubTurnHeader> Headers;
	TArray<struct SubTurn> Subturns;

	AEntity* Rewinder;
	int32 NumTurns;

	//Final locations
	TArray<AEntity*> Entities;
	TArray<GridCoord> Locations;
};

struct SubTurnHeader
{
	APlayerEntity* Player;
	GridCoord Move;

	SubTurnHeader() {}
	SubTurnHeader(AEntity* InPlayer, GridCoord& Move) : Move(Move)
	{
		Player = StaticCast<APlayerEntity*>(InPlayer);
	}

};

struct SubTurn
{
	TArray<AEntity*> Entities;
	TArray<float> Durations;
	TArray<struct EntityAnimation> Animations; //Double Entities length
	
	TArray<uint16> PathIndices;
	TArray<GridCoord> Paths;
};

struct EntityAnimation
{
	enum AnimationType Type;


};

enum AnimationType
{
	PLAYER_IN_SUPER,
	PLAYER_OUT_SUPER,
	SUPER_IN,
	SUPER_OUT,
	PFX,
};

//---------------------------------------------------------------------

struct EntityAnimationPath
{
	AEntity* Entity;
	TArray<FVector> Path;
	double StartTime;
	int32 SubturnIndex;

	int32 PathIndex = -2;
	double SubstepTime;

	EntityAnimationPath(AEntity* Entity, double StartTime, int32 SubturnIndex)
		: Entity(Entity), StartTime(StartTime), SubturnIndex(SubturnIndex) {}
};

UCLASS()
class REWINDCODEPLUGIN_API UEntityAnimator : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UWorld* WorldContext;
	FTransform Transform;
	FVector Offset;

	TArray<SubTurn>* Subturns;

	void Tick(float DeltaTime) override;
	bool IsTickable() const override { return bIsAnimating; }
	TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UEntityAnimator, STATGROUP_Tickables);
	}

	bool bIsAnimating = false;
	void Start(TArray<SubTurn>& Subturns, int32 Start, int32 End, bool bReverse);
	void Start(TArray<EntityAnimationPath>& InGroups, TArray<uint16> InGroupIndices);

	TArray<EntityAnimationPath> GroupQueue;
	TArray<uint16> GroupIndices;
	int32 QueueIndex;
	double GroupStartTime;
	bool bIsUndo;

	DECLARE_DELEGATE(FOnAnimationsFinished)
	FOnAnimationsFinished OnAnimationsFinished;

	float HorizontalSpeed = 0.25, VerticalSpeed = 0.1;
};