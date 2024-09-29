// Copyright It's Rewind Time 2024

#pragma once

#include "CoreMinimal.h"

#include "EnhancedInputComponent.h"

#include "Input.generated.h"


class UInputAction;
struct FInputActionValue;

enum EInputStates
{
	NONE = 0,
	W = 1 << 0,
	S = 1 << 1,
	A = 1 << 2,
	D = 1 << 3
};

UCLASS(Blueprintable, BlueprintType)
class REWINDCODEPLUGIN_API ARewindPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARewindPlayerController();

	void SetupInputComponent() override;
	void Tick(float DeltaTime) override;

	UPROPERTY()
	class UInputMappingContext* InputMapping;
	UPROPERTY()
	UInputAction* ForwardMoveAction;
	UPROPERTY()
	UInputAction* SideMoveAction;
	UPROPERTY()
	UInputAction* PassTurnAction;
	UPROPERTY()
	UInputAction* UndoAction;
	UPROPERTY()
	UInputAction* RestartAction;
	UPROPERTY()
	UInputAction* EscapeAction;

	struct FEnhancedInputActionValueBinding* ForwardMoveValue, *SideMoveValue;

	int32 CurrentInputState = 0;
	EInputStates NewestInput = NONE;
	TArray<EInputStates> Stack;

	DECLARE_DELEGATE(FOnInputChanged)
	FOnInputChanged OnInputChanged;


	void OnPassTurn(const FInputActionValue& Value);


	//void OnRestart(bool b);
	
	//void OnEscape();
};
