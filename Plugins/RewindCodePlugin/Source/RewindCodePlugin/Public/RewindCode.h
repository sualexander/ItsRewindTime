// Copyright It's Rewind Time 2024

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/GameModeBase.h"


#include "RewindCode.generated.h"


enum EInputStates;

enum EEntityAnimations {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT,
	DOWN
};

struct EntityState
{
	FIntVector StartingTileLocation;
	EInputStates Move;

};

struct Turn
{
	TArray<EntityState> States;


};

struct EntityAnimationData
{
	float TimeRemaining;
	FTransform PreviousTransform;
	TArray<EEntityAnimations> AnimationStack;
};

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

	AEntityAnimator* Animator;

	FIntVector StartTileLocation;

	void HandleInput();
	void ProcessTurn(EInputStates Input);
	void OnTurnEnd();

	void QueueRewind();
	void DoRewind();

	void BuildLevel();

	//bool bTurn = false;
	int NumEntitiesAnimating = 0;

	bool bRewindQueued = false;

	double InputTimerStart;
	EInputStates Buffer;

	APlayerEntity* Player;

	UMaterialInstanceDynamic* PlayerMI;

	TArray<APlayerEntity*> PastEntities;

	TArray<Turn> CurrentTimeline;
	TArray<Turn> PrevTimeline;

	int32 BlockSize = 300;

	enum ETileState {
		WALL,
		REWIND_TILE,
		START_TILE,
		AIR
	};

	TMap<FIntVector, ETileState> Grid;
};

UCLASS()
class REWINDCODEPLUGIN_API AEntity : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	AEntity();

	int32 Flags; //moveable, unmoveable, player, timelines?

	UGameManager* GameManager;

	FIntVector TileLocation;

	void Init(FIntVector Loc);

	void RefreshLocation();

	FVector GetTargetLocation();

};



UCLASS(Blueprintable)
class REWINDCODEPLUGIN_API APlayerEntity : public AEntity
{
	GENERATED_BODY()

public:
	APlayerEntity();

	void Move(FVector Destination);

	bool bIsActivePlayer = false;

	FVector Start, End;
	bool bIsMoving = false;
	float MoveDuration = 0.3f;
	float MoveTime;

	bool AttemptMove(EInputStates InputDir, TMap<FIntVector, UGameManager::ETileState> Grid, EntityState& State);

	DECLARE_DELEGATE(FOnHitRewindTile)
	FOnHitRewindTile OnHitRewindTile;

	DECLARE_DELEGATE_TwoParams(FOnQueueAnimation, AEntity*, EEntityAnimations)
	FOnQueueAnimation OnQueueAnimation;

};

UCLASS(Blueprintable)
class REWINDCODEPLUGIN_API AEntityAnimator : public AActor
{
	GENERATED_BODY()

public:

	AEntityAnimator();

	void Tick(float DeltaTime) override;

	void UpdateAnimation(float DeltaTime);

	TMap<AEntity*, EntityAnimationData> AnimationStacks;

	void QueueAnimation(AEntity* Entity, EEntityAnimations Animation);
	void ClearAnimQueues();

	UFUNCTION()
	bool DoAnimationFrame(AEntity* Entity, double DeltaTime);

	//UFUNCTION()
	static float GetDefaultAnimationTime(EEntityAnimations Animation);
	static FTransform InterpolateAnimation(EEntityAnimations Animation, float Progress, FTransform prev);

	UFUNCTION()
	bool IsAnimating();

	void InitNewPlayer(APlayerEntity* NewPlayer);

	DECLARE_DELEGATE(FOnMoveFinished)
	FOnMoveFinished OnMoveFinished;



};
