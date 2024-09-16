// Copyright It's Rewind Time 2024

#include "RewindCode.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimMontage.h"

#include "InputMappingContext.h"


#if 1
#define SLOG(x) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, x);
#define SLOGF(x) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, FString::SanitizeFloat(x));
#define POINT(x, c) DrawDebugPoint(GetWorld(), x, 10, c, false, 2.f);
#define LINE(x1, x2, c) DrawDebugPoint(GetWorld(), x1, x2, 10, c, false, 2.f);
#else
#define SLOG(x)
#define SLOGF(x)
#define POINT(x, c)
#define LINE(x1, x2, c)
#endif

ARewindGameMode::ARewindGameMode()
{
	PlayerControllerClass = ARewindPlayerController::StaticClass();
}

void ARewindPlayerController::BeginPlay()
{

	Super::BeginPlay();

	//Gets a reference to the player entity placed in scene
	//This is done because there is currently a BP class that inherits from APlayerEntity for ease of assigning staticmesh asset
	/*TODO: Make PlayerController get starttile location instead and spawn first entity on top*/
	PlayerEntity = Cast<APlayerEntity>(UGameplayStatics::GetActorOfClass(GetWorld(), APlayerEntity::StaticClass()));


	if (PlayerEntity)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerFound"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("NO PLAYER FOUND"));
		PlayerEntity = GetWorld()->SpawnActor<APlayerEntity>(APlayerEntity::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	}


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
		NewMoveStates& W ? Stack.Add(W) : Stack.RemoveSingle(W);
	}
	if ((NewMoveStates & S) != (MoveStates & S)) {
		NewMoveStates& S ? Stack.Add(S) : Stack.RemoveSingle(S);
	}
	if ((NewMoveStates & A) != (MoveStates & A)) {
		NewMoveStates& A ? Stack.Add(A) : Stack.RemoveSingle(A);
	}
	if ((NewMoveStates & D) != (MoveStates & D)) {
		NewMoveStates& D ? Stack.Add(D) : Stack.RemoveSingle(D);
	}

	if (Stack.IsEmpty()) {
		MoveState = NONE;
		return;
	}
	else {
		MoveState = Stack.Last();
		if (MoveStates == 0) UE_LOG(LogTemp, Warning, TEXT("Start"));
	}
	MoveStates = NewMoveStates;


	//Moving is not allowed while the previous move's animation is still playing
	if (PlayerEntity->GetAnimator()->IsInAnimationMode()) return;


	//FString Dir = "W";
	int dir = 0;
	if (MoveState == S) dir = 1; // Dir = "S";
	else if (MoveState == A) dir = 2; //Dir = "A";
	else if (MoveState == D) dir = 3; //Dir = "D";
	PlayerEntity->TryMove(dir);


	//UE_LOG(LogTemp, Warning, TEXT("%s"), Stack.Num());
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
	Animator = CreateDefaultSubobject<UEntityAnimatorComponent>(TEXT("Animator"));

	TileLocation = FIntVector(0, 0, 0);
}

void AEntity::BeginPlay()
{
	Super::BeginPlay();

	StartLocation = GetActorLocation();
}

bool AEntity::TryMove(int moveDir)
{
	FVector Loc = GetAbsoluteTargetPosition();
	FVector Dir = FVector::Zero();
	FIntVector IntDir = FIntVector::ZeroValue;

	UEntityAnimatorComponent::EEntityAnimations Animation;

	switch (moveDir)
	{
	case 0:
		Dir = FVector::ForwardVector * 100;
		Animation = UEntityAnimatorComponent::EEntityAnimations::FORWARD;
		IntDir.X = 1;
		UE_LOG(LogTemp, Warning, TEXT("%s"), "W");
		break;
	case 1:
		Dir = FVector::BackwardVector * 100;
		Animation = UEntityAnimatorComponent::EEntityAnimations::BACKWARD;
		IntDir.X = -1;
		UE_LOG(LogTemp, Warning, TEXT("%s"), "S");
		break;
	case 2:
		Dir = FVector::LeftVector * 100;
		Animation = UEntityAnimatorComponent::EEntityAnimations::LEFT;
		IntDir.Y = -1;
		UE_LOG(LogTemp, Warning, TEXT("%s"), "A");
		break;
	case 3:
		Dir = FVector::RightVector * 100;
		IntDir.Y = 1;
		Animation = UEntityAnimatorComponent::EEntityAnimations::RIGHT;
		UE_LOG(LogTemp, Warning, TEXT("%s"), "D");
		break;

	}

	//Check for horizontal collisions
	//TODO: Transition away from built-in UE5 collision detection to using a 3D array to represent each level's tile states
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Loc, Loc + Dir, ECollisionChannel::ECC_WorldStatic))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s"), "COLLIDING");
		return false;


	}

	Animator->QueueAnimation(Animation);
	TileLocation += IntDir;
	Loc += Dir;


	//Fall if there is space below
	while (!GetWorld()->LineTraceSingleByChannel(Hit, Loc, Loc + FVector::DownVector * 100, ECollisionChannel::ECC_WorldStatic))
	{
		Animator->QueueAnimation(UEntityAnimatorComponent::EEntityAnimations::DOWN);
		Loc.Z -= 100;
		TileLocation.Z--;
		if (Loc.Z < -1000.0) return true;
	}


	return true;
}

FVector AEntity::GetAbsoluteTargetPosition()
{
	FVector ret = StartLocation;
	ret.X += 100.f * TileLocation.X;
	ret.Y += 100.f * TileLocation.Y;
	ret.Z += 100.f * TileLocation.Z;
	return ret;
}

UEntityAnimatorComponent::UEntityAnimatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}



void UEntityAnimatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (AnimationStack.Num() > 0)
		if (DoAnimationFrame())
			DoAnimationFrame(); //If the previous animation completed, start the next one

}

void UEntityAnimatorComponent::QueueAnimation(EEntityAnimations Animation)
{
	AnimationStack.Add(Animation);
}

bool UEntityAnimatorComponent::DoAnimationFrame()
{
	if (AnimationStack.Num() == 0)
	{
		bIsAnimating = false;
		return false;
	}

	EEntityAnimations Animation = AnimationStack[0];

	if (!bIsAnimating)
	{
		bIsAnimating = true;
		PreviousTransform = GetOwner()->GetTransform();
		AnimStartTime = UGameplayStatics::GetTimeSeconds(GetWorld());
	}

	double Elapsed = UGameplayStatics::GetTimeSeconds(GetWorld()) - AnimStartTime;
	double AnimTotalTime = GetDefaultAnimationTime(Animation);

	if (Elapsed >= AnimTotalTime)
	{

		FTransform t = InterpolateAnimation(Animation, AnimTotalTime, AnimTotalTime, PreviousTransform);
		GetOwner()->SetActorLocation(PreviousTransform.GetLocation() + t.GetLocation());
		GetOwner()->SetActorRotation(PreviousTransform.GetRotation() + t.GetRotation());

		GetOwner()->SetActorRotation(FRotator::ZeroRotator);
		PreviousTransform = GetOwner()->GetActorTransform();
		AnimStartTime += AnimTotalTime;

		AnimationStack.RemoveAt(0);

		if (AnimationStack.Num() == 0)
			bIsAnimating = false;

		return true;
	}

	FTransform t = InterpolateAnimation(Animation, Elapsed, AnimTotalTime, PreviousTransform);
	GetOwner()->SetActorLocation(PreviousTransform.GetLocation() + t.GetLocation());
	GetOwner()->SetActorRotation(PreviousTransform.GetRotation() + t.GetRotation());
	return false;
}

double UEntityAnimatorComponent::GetDefaultAnimationTime(EEntityAnimations Animation)
{
	//Could be integrated directly into InterpolateAnimation instead
	if (Animation == EEntityAnimations::DOWN) return 0.3;
	return 0.35;
}

FTransform UEntityAnimatorComponent::InterpolateAnimation(EEntityAnimations Animation, double Time, double MaxTime, FTransform prev)
{
	double progress = Time / MaxTime;
	FVector loc = FVector::ZeroVector;
	FRotator rot = FRotator::ZeroRotator;

	//Hardcoded motion of cube based on which animation is playing
	//This could be substituted for getting data from transform curves instead, allowing for more control over animations

	switch (Animation)
	{
	case UEntityAnimatorComponent::FORWARD:
		loc = (FVector(-50.f, 0, 50.f)).RotateAngleAxis(90.f * progress, FVector::RightVector) + (FVector(50.f, 0, -50.f));
		rot = FRotator::MakeFromEuler(FVector(0.f, progress * -180.f, 0.f));
		break;
	case UEntityAnimatorComponent::BACKWARD:
		loc = (FVector(50.f, 0, 50.f)).RotateAngleAxis(90.f * progress, FVector::LeftVector) + (FVector(-50.f, 0, -50.f));
		rot = FRotator::MakeFromEuler(FVector(0.f, progress * 180.f, 0.f));
		break;
	case UEntityAnimatorComponent::LEFT:
		loc = (FVector(0, 50.f, 50.f)).RotateAngleAxis(90.f * progress, FVector::ForwardVector) + (FVector(0, -50.f, -50.f));
		rot = FRotator::MakeFromEuler(FVector(progress * -180.f, 0.f, 0.f));
		break;
	case UEntityAnimatorComponent::RIGHT:
		loc = (FVector(0, -50.f, 50.f)).RotateAngleAxis(90.f * progress, FVector::BackwardVector) + (FVector(0, 50.f, -50.f));
		rot = FRotator::MakeFromEuler(FVector(progress * 180.f, 0.f, 0.f));
		break;
	case UEntityAnimatorComponent::DOWN:
		loc = FVector::DownVector * 100 * progress;
		break;
	default:
		break;
	}

	return FTransform(rot.Quaternion(), loc, prev.GetScale3D());
}
