// Copyright It's Rewind Time 2024

#pragma once

#include "CoreMinimal.h"
#include "Tools/LegacyEdModeWidgetHelpers.h"
#include "Engine/StaticMeshActor.h"

#include "Components/BillboardComponent.h"

#include "RewindLevelEditor.generated.h"


class SRewindEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRewindEditor) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);
};

class FRewindEditorToolkit : public FModeToolkit
{
public:
	void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
	void InvokeUI() override;
};

UCLASS()
class REWINDCODEPLUGIN_API URewindEditorMode : public UBaseLegacyWidgetEdMode, public ILegacyEdModeSelectInterface
{
	GENERATED_BODY()

public:
	URewindEditorMode();

	void Enter() override;
	void CreateToolkit() override;

	void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	bool HandleClick(FEditorViewportClient* ViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;

	bool BoxSelect(FBox& InBox, bool InSelect = true) override;
	bool FrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, bool InSelect) override;

	AGrid* Grid;
	FIntVector Dimensions = FIntVector(6, 6, 4);
	TArray<AGridActor*> GridInternal;

	FIntVector HoveredTile = FIntVector(-1, -1, -1);
	FIntVector Offset = FIntVector(0, 0, 0);

	AGridActor* QueryAt(const FIntVector& Location);
	void SetAt(const FIntVector& Location, AGridActor* Actor);
};

UCLASS()
class REWINDCODEPLUGIN_API AGrid : public AActor
{
	GENERATED_BODY()

public:
	AGrid();

	UBillboardComponent* BillboardComponent;
};

enum GridType
{
	Solid,
	Transparent,
	Origin,
	Goal,
	Rewind,
};

UCLASS()
class REWINDCODEPLUGIN_API AGridActor : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	enum GridType Type;

};
