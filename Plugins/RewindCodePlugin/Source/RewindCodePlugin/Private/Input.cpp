// Copyright It's Rewind Time 2024

#include "Input.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputMappingContext.h"


ARewindPlayerController::ARewindPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ARewindPlayerController::SetupInputComponent()
{
	APlayerController::SetupInputComponent();

	//Using the CDO is a bit sus but should be fine I think
	UInputModifierSwizzleAxis* Swizzle = Cast<UInputModifierSwizzleAxis>(UInputModifierSwizzleAxis::StaticClass()->ClassDefaultObject);
	UInputModifierNegate* Negate = Cast<UInputModifierNegate>(UInputModifierNegate::StaticClass()->ClassDefaultObject);

	InputMapping = NewObject<UInputMappingContext>(this);
	//We split Forward and Side because of an engine bug where negating on the same axis breaks input recognition in one direction 
	ForwardMoveAction = NewObject<UInputAction>(this);
	ForwardMoveAction->ValueType = EInputActionValueType::Axis2D;
	SideMoveAction = NewObject<UInputAction>(this);
	SideMoveAction->ValueType = EInputActionValueType::Axis2D;

	PassTurnAction = NewObject<UInputAction>(this);
	UndoAction = NewObject<UInputAction>(this);
	RestartAction = NewObject<UInputAction>(this);
	EscapeAction = NewObject<UInputAction>(this);

	InputMapping->MapKey(ForwardMoveAction, EKeys::W);
	InputMapping->MapKey(ForwardMoveAction, EKeys::Up);
	InputMapping->MapKey(ForwardMoveAction, EKeys::S).Modifiers.Append({ Swizzle, Negate });
	InputMapping->MapKey(ForwardMoveAction, EKeys::Down).Modifiers.Append({ Swizzle, Negate });
	InputMapping->MapKey(SideMoveAction, EKeys::A);
	InputMapping->MapKey(SideMoveAction, EKeys::Left);
	InputMapping->MapKey(SideMoveAction, EKeys::D).Modifiers.Append({ Swizzle, Negate });
	InputMapping->MapKey(SideMoveAction, EKeys::Right).Modifiers.Append({ Swizzle, Negate });

	InputMapping->MapKey(PassTurnAction, EKeys::SpaceBar);
	InputMapping->MapKey(UndoAction, EKeys::LeftShift);
	InputMapping->MapKey(RestartAction, EKeys::R);
	InputMapping->MapKey(EscapeAction, EKeys::Escape);

	//Movement keys needs to be checked on tick for logic to work
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	ForwardMoveValue = &EnhancedInputComponent->BindActionValue(ForwardMoveAction);
	SideMoveValue = &EnhancedInputComponent->BindActionValue(SideMoveAction);

	EnhancedInputComponent->BindAction(PassTurnAction, ETriggerEvent::Started, this, &ARewindPlayerController::OnPassTurn, true);
	EnhancedInputComponent->BindAction(PassTurnAction, ETriggerEvent::Completed, this, &ARewindPlayerController::OnPassTurn, false);
	//Undo

	//EnhancedInputComponent->BindAction(RestartAction, ETriggerEvent::Completed, this, &ARewindPlayerController::OnRestart, true);
	//EnhancedInputComponent->BindAction(EscapeAction, ETriggerEvent::Completed, this, &ARewindPlayerController::OnEscape);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(InputMapping, 0);
}

void ARewindPlayerController::Tick(float DeltaTime)
{
	FVector2D Forward = ForwardMoveValue->GetValue().Get<FVector2D>();
	FVector2D Side = SideMoveValue->GetValue().Get<FVector2D>();

	//Store the state (pressed/not-pressed) of WASD keys with 4 bits
	int32 NewInputState = 0;
	if (Forward.X > 0) NewInputState |= W;
	if (Forward.Y < 0) NewInputState |= S;
	if (Side.X > 0) NewInputState |= A;
	if (Side.Y < 0) NewInputState |= D;

	if (NewInputState == CurrentInputState) return;

	//Update movement stack to keep track of most recently pressed key
	if ((NewInputState & W) != (CurrentInputState & W)) {
		NewInputState & W ? Stack.Add(W) : Stack.RemoveSingle(W);
	}
	if ((NewInputState & S) != (CurrentInputState & S)) {
		NewInputState & S ? Stack.Add(S) : Stack.RemoveSingle(S);
	}
	if ((NewInputState & A) != (CurrentInputState & A)) {
		NewInputState & A ? Stack.Add(A) : Stack.RemoveSingle(A);
	}
	if ((NewInputState & D) != (CurrentInputState & D)) {
		NewInputState & D ? Stack.Add(D) : Stack.RemoveSingle(D);
	}

	CurrentInputState = NewInputState;
	NewestInput = Stack.IsEmpty() ? NONE : Stack.Last();
	OnInputChanged.Execute();
}
