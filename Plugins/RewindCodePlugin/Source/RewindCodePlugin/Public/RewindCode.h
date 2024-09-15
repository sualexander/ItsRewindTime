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

	void HandleInput();
	void ProcessTurn(EInputStates Input);
	void OnTurnEnd();

	bool bTurn = false;

	double InputTimerStart;
	EInputStates Buffer;

	APlayerEntity* Player;
	TArray<FVector> Grid;
	int32 BlockSize = 300;
};

UCLASS()
class REWINDCODEPLUGIN_API AEntity : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	AEntity();

	int32 Flags; //moveable, unmoveable, player, timelines?

};



UCLASS(Blueprintable)
class REWINDCODEPLUGIN_API APlayerEntity : public AEntity
{
	GENERATED_BODY()

public:
	APlayerEntity();

	void Tick(float DeltaTime) override;

	void Move(FVector Destination);

	FVector Start, End;
	bool bIsMoving = false;
	float MoveDuration = 0.3f;
	float MoveTime;

	DECLARE_DELEGATE(FOnMoveFinished)
	FOnMoveFinished OnMoveFinished;
};


struct EntityState
{

};

struct Turn
{
	TArray<EntityState*> States;


};
