// Copyright It's Rewind Time 2024

#pragma once

#include "CoreMinimal.h"

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
	int8 HeightMin = -1;

	UPROPERTY()
	FIntVector Dimensions;
	UPROPERTY()
	FTransform Transform;
	UPROPERTY()
	float BlockSize;
	UPROPERTY()
	TArray<uint8> GridData;
};
