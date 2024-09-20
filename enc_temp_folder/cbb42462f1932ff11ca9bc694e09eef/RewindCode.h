// Copyright It's Rewind Time 2024

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/GameModeBase.h"


#include "RewindCode.generated.h"


enum EInputStates;

TArray<FLinearColor> COLORS = 
{
	FLinearColor(0.f, 0.f, 1.f),
	FLinearColor(0.f, 1.f, 0.f),
	FLinearColor(1.f, 0.f, 0.f),
	FLinearColor(1.f, 0.f, 1.f),
	FLinearColor(0.f, 1.f, 1.f),
	FLinearColor(1.f, 1.f, 0.f)
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

	FIntVector StartTileLocation;

	void HandleInput();
	void ProcessTurn(EInputStates Input);
	void OnTurnEnd();

	void QueueRewind();
	void DoRewind();

	void BuildLevel();

	bool bTurn = false;

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

	UPROPERTY()
	UEntityAnimatorComponent* Animator;

	FIntVector TileLocation;

	void Init(FIntVector Loc);

	void RefreshLocation();

};



UCLASS(Blueprintable)
class REWINDCODEPLUGIN_API APlayerEntity : public AEntity
{
	GENERATED_BODY()

public:
	APlayerEntity();

	void Tick(float DeltaTime) override;

	void Move(FVector Destination);

	bool bIsActivePlayer = false;

	FVector Start, End;
	bool bIsMoving = false;
	float MoveDuration = 0.3f;
	float MoveTime;

	bool AttemptMove(EInputStates InputDir, TMap<FIntVector, UGameManager::ETileState> Grid, EntityState& State);

	DECLARE_DELEGATE(FOnMoveFinished)
	FOnMoveFinished OnMoveFinished;

	DECLARE_DELEGATE(FOnHitRewindTile)
	FOnHitRewindTile OnHitRewindTile;
};

UCLASS(Blueprintable)
class REWINDCODEPLUGIN_API UEntityAnimatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FTransform PreviousTransform;

	bool bIsAnimating;

	double AnimStartTime;

	UPROPERTY(EditDefaultsOnly, Category = "Animation", BlueprintReadWrite)
	FVectorCurve MoveLocationCurve;

	UPROPERTY(EditDefaultsOnly, Category = "Animation", BlueprintReadWrite)
	FVectorCurve MoveRotationCurve;


	UEntityAnimatorComponent();

	void UpdateAnimation();

	enum EEntityAnimations {
		FORWARD,
		BACKWARD,
		LEFT,
		RIGHT,
		DOWN
	};

	TArray<EEntityAnimations> AnimationStack;

	void QueueAnimation(EEntityAnimations Animation);

	UFUNCTION()
	bool DoAnimationFrame();

	//UFUNCTION()
	static double GetDefaultAnimationTime(EEntityAnimations Animation);
	static FTransform InterpolateAnimation(EEntityAnimations Animation, double Time, double MaxTime, FTransform prev);

	UFUNCTION()
	FORCEINLINE bool IsAnimating() { return bIsAnimating || (AnimationStack.Num() > 0); }



};

