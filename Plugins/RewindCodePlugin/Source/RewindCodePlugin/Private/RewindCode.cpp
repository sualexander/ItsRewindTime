// Copyright It's Rewind Time 2024

#include "RewindCode.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"

#include "InputMappingContext.h"


ARewindGameMode::ARewindGameMode()
{
	PlayerControllerClass = ARewindPlayerController::StaticClass();
}

void ARewindPlayerController::SetupInputComponent()
{
	APlayerController::SetupInputComponent();

	//Using the CDO is a bit sus but should be fine I think
	UInputModifierSwizzleAxis* Swizzle = Cast<UInputModifierSwizzleAxis>(UInputModifierSwizzleAxis::StaticClass()->ClassDefaultObject);
	UInputModifierNegate* Negate = Cast<UInputModifierNegate>(UInputModifierNegate::StaticClass()->ClassDefaultObject);

	//We split Forward and Side because of an engine bug where negating on the same axis breaks input recognition in one direction 
	InputMapping = NewObject<UInputMappingContext>(this);
	ForwardMoveAction = NewObject<UInputAction>(this);
	ForwardMoveAction->ValueType = EInputActionValueType::Axis2D;
	SideMoveAction = NewObject<UInputAction>(this);
	SideMoveAction->ValueType = EInputActionValueType::Axis2D;
	PassTurnAction = NewObject<UInputAction>(this);

	InputMapping->MapKey(ForwardMoveAction, EKeys::W);
	InputMapping->MapKey(ForwardMoveAction, EKeys::S).Modifiers.Append({ Swizzle, Negate });
	InputMapping->MapKey(SideMoveAction, EKeys::A);
	InputMapping->MapKey(SideMoveAction, EKeys::D).Modifiers.Append({ Swizzle, Negate });
	InputMapping->MapKey(PassTurnAction, EKeys::SpaceBar);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	EnhancedInputComponent->BindAction(ForwardMoveAction, ETriggerEvent::Triggered, this, &ARewindPlayerController::OnMove);
	EnhancedInputComponent->BindAction(ForwardMoveAction, ETriggerEvent::Completed, this, &ARewindPlayerController::OnMoveCompleted);
	EnhancedInputComponent->BindAction(SideMoveAction, ETriggerEvent::Triggered, this, &ARewindPlayerController::OnMove);
	EnhancedInputComponent->BindAction(SideMoveAction, ETriggerEvent::Completed, this, &ARewindPlayerController::OnMoveCompleted);
	ForwardMoveValue = &EnhancedInputComponent->BindActionValue(ForwardMoveAction);
	SideMoveValue = &EnhancedInputComponent->BindActionValue(SideMoveAction);

	EnhancedInputComponent->BindAction(PassTurnAction, ETriggerEvent::Started, this, &ARewindPlayerController::OnPassTurn);
	EnhancedInputComponent->BindAction(PassTurnAction, ETriggerEvent::Completed, this, &ARewindPlayerController::OnPassTurn);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(InputMapping, 0);
}

void ARewindPlayerController::OnMove()
{
	FVector2D Forward = ForwardMoveValue->GetValue().Get<FVector2D>();
	FVector2D Side = SideMoveValue->GetValue().Get<FVector2D>();
	
	int32 NewMoveStates = 0;
	if (Forward.X > 0) NewMoveStates |= W;
	if (Forward.Y < 0) NewMoveStates |= S;
	if (Side.X > 0) NewMoveStates |= A;
	if (Side.Y < 0) NewMoveStates |= D;

	if (NewMoveStates == MoveStates) return;

	if ((NewMoveStates & W) != (MoveStates & W)) {
		NewMoveStates & W ? Stack.Add(W) : Stack.RemoveSingle(W);
	} else if ((NewMoveStates & S) != (MoveStates & S)) {
		NewMoveStates & S ? Stack.Add(S) : Stack.RemoveSingle(S);
	} else if ((NewMoveStates & A) != (MoveStates & A)) {
		NewMoveStates & A ? Stack.Add(A) : Stack.RemoveSingle(A);
	} else if ((NewMoveStates & D) != (MoveStates & D)) {
		NewMoveStates & D ? Stack.Add(D) : Stack.RemoveSingle(D);
	}

	if (Stack.IsEmpty()) {
		MoveState = NONE;
	} else {
		MoveState = Stack.Last();
		if (MoveStates == 0) UE_LOG(LogTemp, Warning, TEXT("Start"));
	}
	MoveStates = NewMoveStates;

	FString Dir = "W";
	if (MoveState == S) Dir = "S";
	else if (MoveState == A) Dir = "A";
	else if (MoveState == D) Dir = "D";
	UE_LOG(LogTemp, Warning, TEXT("%s"), *Dir);
}

void ARewindPlayerController::OnMoveCompleted()
{
	if (Stack.Num() == 1)
	{
		Stack.Empty();
		MoveState = NONE;
		MoveStates = 0;
		UE_LOG(LogTemp, Warning, TEXT("Stop"));
	}
}

void ARewindPlayerController::OnPassTurn(const FInputActionValue& Value)
{
	bool State = Value.Get<bool>();
	UE_LOG(LogTemp, Warning, TEXT("PassTurn %s"), State ? TEXT("Start") : TEXT("End"));
}

AEntity::AEntity()
{

}