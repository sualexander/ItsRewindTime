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

protected:
	void BeginPlay() override;

public:
	void SetupInputComponent() override;

	UPROPERTY()
	class APlayerEntity* PlayerEntity;

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
	struct FEnhancedInputActionValueBinding* ForwardMoveValue, * SideMoveValue;

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

	/*UPROPERTY(EditDefaultsOnly)
	class UStaticMeshComponent* Mesh;*/

	/*UPROPERTY(EditDefaultsOnly)
	class UAnimMontage* Montage_Move;*/

	UPROPERTY()
	UEntityAnimatorComponent* Animator;

	UPROPERTY()
	FVector StartLocation;

	UPROPERTY()
	FIntVector TileLocation;

public:
	AEntity();

	void BeginPlay() override;

	int32 Flags; //moveable, unmoveable, player, timelines?

	bool TryMove(int moveDir);

	UFUNCTION()
	FORCEINLINE UEntityAnimatorComponent* GetAnimator() { return Animator; }

	UFUNCTION()
	FVector GetAbsoluteTargetPosition();

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

	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction);

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
	FORCEINLINE bool IsInAnimationMode() { return bIsAnimating || (AnimationStack.Num() > 0); }



};
