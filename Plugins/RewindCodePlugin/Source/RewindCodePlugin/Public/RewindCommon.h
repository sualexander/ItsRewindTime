// Copyright It's Rewind Time 2024

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"

#include "RewindCommon.generated.h"


UENUM()
enum class GridType : uint8
{
	None = 0,
	Solid = 1,
	Transparent = 2,
	Origin = 3,
	Goal = 4,
	Rewind = 5
};

static const TMap<GridType, FString> MeshPaths{
	{ GridType::Solid, TEXT("/Game/Entities/Solid.Solid") },
	{ GridType::Origin, TEXT("/Game/Entities/Origin.Origin") },
	{ GridType::Goal, TEXT("/Game/Entities/Goal.Goal") },
	{ GridType::Rewind, TEXT("/Game/Entities/Rewind.Rewind") },
	{ GridType::Transparent, TEXT("/Game/Entities/Transparent.Transparent") }
};

UCLASS()
class REWINDCODEPLUGIN_API AGridActor : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	UPROPERTY()
	GridType Type;
};

UCLASS()
class REWINDCODEPLUGIN_API ARewindWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (DisplayName = "Minimum Height"))
	int8 HeightMin = 0;

	UPROPERTY()
	FIntVector Dimensions;
	UPROPERTY()
	FTransform Transform;
	UPROPERTY()
	FVector BlockScale;
	UPROPERTY()
	TArray<uint8> GridData;
};
