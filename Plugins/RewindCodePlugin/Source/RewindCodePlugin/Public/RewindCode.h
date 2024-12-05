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


UCLASS()
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

UCLASS()
class REWINDCODEPLUGIN_API UGameManager : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UGameManager();

	UWorld* WorldContext;
	ARewindGameMode* Gamemode;
	class ARewindPlayerController* PlayerController;
	UPROPERTY()
	UEntityAnimator* Animator;

	//Loading
	void LoadLevel();
	void UnloadLevel();

	//Input
	void Tick(float DeltaTime) override;
	TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UEntityAnimator, STATGROUP_Tickables);
	}
	bool IsTickable() const override
	{
		return RestartPresses != 0;
	}

	void HandleMovementInput();
	EInputStates Buffer;
	double InputTimerStart = 0;

	void HandlePassInput(bool bStart);
	bool bPassPressed = false;

	void HandleCameraInput(float Direction);
	void OnCameraBlendComplete();
	TArray<class ACameraActor*> Cameras;
	int32 CameraIndex = 0;
	double RedundancyTimer = 0;

	void HandleUndoInput();

	void HandleRestartInput(bool bStart);
	double RestartTimerStart = 0;
	int32 RestartPresses = 0;
	bool bRestartPressed = false;
	bool bRestartSecond = false;

	void HandleEscapeInput();

	//

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

	void AddAnimation(SubTurn& Subturn, AEntity* Owner, struct EntityAnimation* Anim, bool bIsStart = true);

	//Rewind
	AEntity* RewindQueue;
	void RewindTimeline();
	void PostRewind();

	void RevaluateSuperpositions(bool bDoAnim = false);

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
	MOVEABLE			= 1,
	CLIMBABLE			= 1 << 1,
	PERSISTENT			= 1 << 2,
	SUPER				= 1 << 3,
	GOAL				= 1 << 4,
	REWIND				= 1 << 5,
	CARRIED				= 1 << 6,
	CURRENT_PLAYER		= 1 << 7
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
	TArray<struct EntityAnimation*> Animations; //Double Entities length
	
	TArray<uint16> PathIndices;
	TArray<GridCoord> Paths;
};

struct EntityAnimation
{
	EntityAnimation* Additional = nullptr;
	virtual void Play(bool bIsUndo) = 0;

	virtual ~EntityAnimation() { delete Additional; }
};

struct EntityFade : public EntityAnimation
{
	EntityFade(AEntity* Target, bool bFadeIn) : Target(Target), bFadeIn(bFadeIn) {}
	AEntity* Target;
	bool bFadeIn;

	void Play(bool bIsUndo) override;
};

//Very bad but idk what else
struct TeleportSuper : public EntityAnimation
{
	TeleportSuper(AEntity* Super, FVector Destination) : Super(Super), Destination(Destination) {}
	AEntity* Super;
	FVector Destination;

	void Play(bool bIsUndo) override;
};

//---------------------------------------------------------------------

struct EntityAnimationPath
{
	AEntity* Entity;
	TArray<FVector> Path;
	EntityAnimation* StartAnim, *EndAnim;
	int32 SubturnIndex;
	double StartTime;

	int32 PathIndex = -2;
	double SubstepTime;

	EntityAnimationPath(
		AEntity* Entity, double StartTime, int32 SubturnIndex, 
		EntityAnimation* StartAnim = nullptr, EntityAnimation* EndAnim = nullptr)
		: Entity(Entity), SubturnIndex(SubturnIndex), StartTime(StartTime),
		StartAnim(StartAnim), EndAnim(EndAnim) {}
};

UCLASS()
class REWINDCODEPLUGIN_API UEntityAnimator : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UWorld* WorldContext;
	FTransform Transform;
	FVector Offset;

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

	TArray<SubTurn>* Subturns;

	DECLARE_DELEGATE(FOnAnimationsFinished)
	FOnAnimationsFinished OnAnimationsFinished;
};