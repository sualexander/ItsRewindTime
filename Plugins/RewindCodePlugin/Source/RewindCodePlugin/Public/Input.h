// Copyright It's Rewind Time 2024

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"

#include "Input.generated.h"


class UInputAction;
struct FInputActionValue;

enum EInputStates
{
	NONE	= 0,
	W		= 1U,
	S		= 1U << 1,
	A		= 1U << 2,
	D		= 1U << 3,
	PASS	= 1U << 4,
	UNDO	= 1U << 5
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

	uint32 CurrentInputState = 0;
	EInputStates NewestInput = NONE;
	TArray<EInputStates> Stack;

	DECLARE_DELEGATE(FOnInputChanged)
	FOnInputChanged OnInputChanged;

	DECLARE_DELEGATE_OneParam(FOnPassPressed, bool)
	FOnPassPressed OnPassPressed;

	void OnPassTurn(bool bStart) { OnPassPressed.Execute(bStart); }


	//void OnRestart(bool b);
	
	//void OnEscape();
};
