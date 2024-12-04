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
	W		= 1,
	S		= 1 << 1,
	A		= 1 << 2,
	D		= 1 << 3,
	PASS	= 1 << 4,
	UNDO	= 1 << 5
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

	DECLARE_DELEGATE(FOnUndoPressed)
	FOnUndoPressed OnUndoPressed;
	void OnUndo() { OnUndoPressed.Execute(); }

	DECLARE_DELEGATE_OneParam(FOnRestartPressed, bool)
	FOnRestartPressed OnRestartPressed;
	void OnRestart(bool bStart) { OnRestartPressed.Execute(bStart); }

	DECLARE_DELEGATE(FOnEscapePressed)
	FOnEscapePressed OnEscapePressed;
	void OnEscape() { OnEscapePressed.Execute(); }
};
