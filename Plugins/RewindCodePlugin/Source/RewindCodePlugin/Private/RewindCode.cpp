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

	UInputModifierSwizzleAxis* Swizzle = Cast<UInputModifierSwizzleAxis>(UInputModifierSwizzleAxis::StaticClass()->ClassDefaultObject);
	UInputModifierNegate* Negate = Cast<UInputModifierNegate>(UInputModifierNegate::StaticClass()->ClassDefaultObject);

	InputMapping = NewObject<UInputMappingContext>();
	MoveAction = NewObject<UInputAction>();
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	PassTurnAction = NewObject<UInputAction>();

	InputMapping->MapKey(MoveAction, EKeys::W).Modifiers.Add(Swizzle);
	InputMapping->MapKey(MoveAction, EKeys::S).Modifiers.Append({ Swizzle, Negate });
	InputMapping->MapKey(MoveAction, EKeys::A).Modifiers.Add(Negate);
	InputMapping->MapKey(MoveAction, EKeys::D);
	InputMapping->MapKey(PassTurnAction, EKeys::SpaceBar);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARewindPlayerController::OnMove);
	MoveValue = &EnhancedInputComponent->BindActionValue(MoveAction);
	EnhancedInputComponent->BindAction(PassTurnAction, ETriggerEvent::Triggered, this, &ARewindPlayerController::OnPassTurn);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(InputMapping, 0);
}

void ARewindPlayerController::OnMove()
{
	FVector2D Data = MoveValue->GetValue().Get<FVector2D>();
	UE_LOG(LogTemp, Warning, TEXT("Moving: X=%f, Y=%f"), Data.X, Data.Y);
}

void ARewindPlayerController::OnPassTurn()
{
	UE_LOG(LogTemp, Warning, TEXT("PassedTurn"));
}

AEntity::AEntity()
{

}