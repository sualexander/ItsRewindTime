// Copyright It's Rewind Time 2024

#pragma once

#include "CoreMinimal.h"
#include "Tools/LegacyEdModeWidgetHelpers.h"
#include "Engine/StaticMeshActor.h"
#include "Components/BillboardComponent.h"

#include "RewindCommon.h"

#include "RewindLevelEditor.generated.h"


class SRewindEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRewindEditor) {}
		SLATE_ARGUMENT(UEdMode*, EditorMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	URewindEditorMode* EditorMode;

	TSharedPtr<STextBlock> BlockText;

	FReply OnCreateGrid();
	FReply OnSaveSettings();
};

class FRewindEditorToolkit : public FModeToolkit
{
public:
	void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
	void InvokeUI() override;

	TSharedPtr<SRewindEditor> MainWidget;
};

UCLASS()
class REWINDCODEPLUGIN_API URewindEditorMode : public UBaseLegacyWidgetEdMode
{
	GENERATED_BODY()

public:
	URewindEditorMode();

	void Enter() override;
	void Exit() override;
	void CreateToolkit() override { Toolkit = MakeShareable(new FRewindEditorToolkit); }

	void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	bool HandleClick(FEditorViewportClient* ViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;

	TMap<GridType, UStaticMesh*> MeshMap;
	enum GridType GridType = GridType::Solid;

	AGrid* Grid;
	FTransform GridTransform;
	FIntVector Dimensions = FIntVector(4, 4, 2);
	TArray<AGridActor*> GridInternal;

	TArray<FVector> Handles;
	int32 HoveredHandle = -1;
	void ResizeGrid(bool bIsShrink);
	void UpdateTiles();

	FVector OldAxis;
	AActor* PrevHit = nullptr;

	FIntVector HoveredTile = FIntVector(-1, -1, -1);
	FIntVector ScrollOffset = FIntVector(0, 0, 0);

	AGridActor* QueryAt(const FIntVector& Location);
	void SetAt(const FIntVector& Location, AGridActor* Actor);

	void SaveToSettings();
};

UCLASS()
class REWINDCODEPLUGIN_API AGrid : public AActor
{
	GENERATED_BODY()

public:
	AGrid();

	UBillboardComponent* BillboardComponent;
};

