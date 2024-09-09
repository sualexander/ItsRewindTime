// Copyright It's Rewind Time 2024

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/GameModeBase.h"

#include "RewindCode.generated.h"


class UInputAction;
struct FInputActionValue;

UCLASS()
class REWINDCODEPLUGIN_API ARewindGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARewindGameMode();
};

UCLASS(Blueprintable, BlueprintType)
class REWINDCODEPLUGIN_API ARewindPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	void SetupInputComponent() override;

	UPROPERTY()
	class UInputMappingContext* InputMapping;
	UPROPERTY()
	UInputAction* ForwardMoveAction;
	UPROPERTY()
	UInputAction* SideMoveAction;
	UPROPERTY()
	UInputAction* PassTurnAction;

	void OnMove();
	void OnMoveCompleted();
	struct FEnhancedInputActionValueBinding* ForwardMoveValue, *SideMoveValue;
	
	enum EMoveStates
	{
		NONE = 0,
		W = 1 << 0,
		S = 1 << 1,
		A = 1 << 2,
		D = 1 << 3
	};

	int32 MoveStates;
	EMoveStates MoveState;
	TArray<EMoveStates> Stack;

	void OnPassTurn(const FInputActionValue& Value);
};

class REWINDCODEPLUGIN_API UManager : public UObject
{

};

UCLASS()
class REWINDCODEPLUGIN_API AEntity : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	AEntity();

	int32 Flags; //moveable, unmoveable, player, timelines?

};

UCLASS()
class REWINDCODEPLUGIN_API APlayerEntity : public AEntity
{
	GENERATED_BODY()

};


struct EntityState
{

};

struct Turn
{
	TArray<EntityState*> States;


};
