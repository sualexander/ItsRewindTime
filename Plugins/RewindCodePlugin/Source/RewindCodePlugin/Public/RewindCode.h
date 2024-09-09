// Copyright It's Rewind Time 2024

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/GameModeBase.h"

#include "RewindCode.generated.h"


class UInputAction;

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

	class UInputMappingContext* InputMapping;
	UInputAction* MoveAction, *PassTurnAction;

	void OnMove();
	void OnPassTurn();
	struct FEnhancedInputActionValueBinding* MoveValue;
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

class REWINDCODEPLUGIN_API UManager : public UObject
{

};

