// Copyright It's Rewind Time 2024


#include "RewindLevelEditor.h"

#include "Engine.h"
#include "Kismet/GameplayStatics.h"
#include "LevelUtils.h"
#include "UObject/SavePackage.h"


DEFINE_LOG_CATEGORY_STATIC(RewindEditor, Log, All);
#define LOG(Str, ...) UE_LOG(RewindEditor, Log, TEXT(Str), ##__VA_ARGS__)

#define LOCTEXT_NAMESPACE "RewindModule"

#pragma warning(disable: 4426)
#pragma optimize("", off)

void SRewindEditor::Construct(const FArguments& Args)
{
	EditorMode = StaticCast<URewindEditorMode*>(Args._EditorMode);

	ChildSlot
		[
			SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoHeight()
				.Padding(16)
				[
					SNew(STextBlock)
						.Text(FText::FromString(TEXT("REWIND LEVEL EDITOR :)")))
				]
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoHeight()
				.Padding(8)
				[
					SNew(SButton)
						.OnClicked(this, &SRewindEditor::OnCreateGrid)
						.ContentPadding(8)
						[
							SNew(STextBlock)
								.Text(FText::FromString(TEXT("Create Grid")))
						]
				]
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoHeight()
				.Padding(8)
				[
					SNew(SButton)
						.OnClicked(this, &SRewindEditor::OnSaveSettings)
						.ContentPadding(8)
						[
							SNew(STextBlock)
								.Text(FText::FromString(TEXT("Save Level")))
						]
				]
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoHeight()
				.Padding(8)
				[
					SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(STextBlock)
								.Text(FText::FromString(TEXT("Block Type: ")))
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Right)
						.AutoWidth()
						[
							SAssignNew(BlockText, STextBlock)
								.Text(FText::FromString(TEXT("SOLID")))
						]
				]
		];
}

void FRewindEditorToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	SAssignNew(ToolkitWidget, SBorder)
 		[
			SAssignNew(MainWidget, SRewindEditor)
				.EditorMode(InOwningMode.Get())
 		];

	FModeToolkit::Init(InitToolkitHost, InOwningMode);
}

void FRewindEditorToolkit::InvokeUI()
{
	FModeToolkit::InvokeUI();
	InlineContentHolder->SetContent(ToolkitWidget.ToSharedRef()); 
}

FReply SRewindEditor::OnCreateGrid()
{
	for (TActorIterator<AGridActor> Itr(EditorMode->GetWorld()); Itr; ++Itr)
	{
		if (*Itr) Itr->Destroy();
	}

	for (TActorIterator<AGrid> Itr(EditorMode->GetWorld()); Itr; ++Itr)
	{
		if (*Itr) Itr->Destroy();
	}

	EditorMode->Grid = EditorMode->GetWorld()->SpawnActor<AGrid>(EditorMode->GridTransform.GetLocation(), FRotator::ZeroRotator);
	EditorMode->Grid->SetActorScale3D(FVector(10));
	EditorMode->GridTransform.SetRotation(FQuat::Identity);
	EditorMode->GridTransform.SetScale3D(FVector(1));

	EditorMode->Dimensions = FIntVector(4, 4, 2);
	EditorMode->GridInternal.Empty();
	EditorMode->GridInternal.Init(nullptr, 32);

	return FReply::Handled();
}

FReply SRewindEditor::OnSaveSettings()
{
	EditorMode->SaveToSettings();
	return FReply::Handled();
}

 //------------------------------------------------------------------------------------------------------------------------------

URewindEditorMode::URewindEditorMode()
{
	Info = FEditorModeInfo(TEXT("RewindEditorMode"), LOCTEXT("ModeName", "Rewind Editor"), FSlateIcon(), true);

	for (const auto& Pair : MeshPaths)
	{
		MeshMap.Emplace(Pair.Key, LoadObject<UStaticMesh>(nullptr, *Pair.Value));
	}
}

void URewindEditorMode::Exit()
{
	SaveToSettings();

	UEdMode::Exit();
}

void URewindEditorMode::Enter()
{
	UEdMode::Enter();

	//Derive dimensions
	TArray<AGridActor*> GridActors;
	for (TActorIterator<AGridActor> Itr(GetWorld()); Itr; ++Itr)
	{
		if (*Itr) GridActors.Emplace(*Itr);
	}
	if (GridActors.IsEmpty()) {
		GridInternal.Init(nullptr, Dimensions.X * Dimensions.Y * Dimensions.Z);
		return;
	}

	const FTransform Transform = GridActors[0]->GetActorTransform();

	TArray<FVector> Locals;
	for (AGridActor* GridActor : GridActors)
	{
		Locals.Emplace(Transform.InverseTransformPosition(GridActor->GetActorLocation()));
	}

	FVector MinBound(MAX_dbl), MaxBound(MIN_dbl);
	for (const FVector& Location : Locals)
	{
		MinBound = MinBound.ComponentMin(Location);
		MaxBound = MaxBound.ComponentMax(Location);
	}

	FVector MeshSize = GridActors[0]->GetStaticMeshComponent()->GetStaticMesh()->GetBoundingBox().GetSize();
	MinBound -= MeshSize / 2;
	MaxBound += MeshSize / 2;

	FVector TotalSize = ((MaxBound - MinBound) / MeshSize);
	Dimensions = FIntVector(FMath::RoundToInt(TotalSize.X), FMath::RoundToInt(TotalSize.Y), FMath::RoundToInt(TotalSize.Z));

	//Initialize variables
	GridInternal.Init(nullptr, Dimensions.X * Dimensions.Y * Dimensions.Z);

	FVector CenterOffset = MinBound + ((MaxBound - MinBound) / 2);
	FTransform NewTransform = Transform;
	NewTransform.AddToTranslation(Transform.TransformVector(CenterOffset));
	NewTransform.SetScale3D(Transform.GetScale3D() * MeshSize);

	for (TActorIterator<AGrid> Itr(GetWorld()); Itr; ++Itr)
	{
		Grid = *Itr;
	}
	if (!(Grid && Grid->IsValidLowLevel())) {
		Grid = GetWorld()->SpawnActor<AGrid>(FVector::ZeroVector, FRotator::ZeroRotator);
	}
	Grid->SetActorTransform(NewTransform);

	//Fill array
	for (int32 i = 0; i < GridActors.Num(); ++i)
	{
		FVector Unquantized = (Locals[i] - MinBound) / MeshSize;
		FIntVector GridLocation(FMath::FloorToInt(Unquantized.X), FMath::FloorToInt(Unquantized.Y), FMath::FloorToInt(Unquantized.Z));

		GridLocation.X = FMath::Clamp(GridLocation.X, 0, Dimensions.X - 1);
		GridLocation.Y = FMath::Clamp(GridLocation.Y, 0, Dimensions.Y - 1);
		GridLocation.Z = FMath::Clamp(GridLocation.Z, 0, Dimensions.Z - 1);
		SetAt(GridLocation, GridActors[i]);
	}
}

void URewindEditorMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	UBaseLegacyWidgetEdMode::Tick(ViewportClient, DeltaTime);

	HoveredTile = FIntVector(-1, -1, -1);
	if (!ViewportClient) return;
	for (TActorIterator<AGrid> Itr(GetWorld()); Itr; ++Itr)
	{
		Grid = *Itr;
		if (Grid) {
			if (!Grid->GetActorTransform().Equals(GridTransform)) {
				GridTransform = Grid->GetActorTransform();
				UpdateTiles();
			}

			FIntPoint MousePosition;
			ViewportClient->Viewport->GetMousePos(MousePosition);

			FSceneViewFamily ViewFamily = FSceneViewFamily::ConstructionValues(
				ViewportClient->Viewport, ViewportClient->GetScene(), ViewportClient->EngineShowFlags);
			FSceneView* SceneView = ViewportClient->CalcSceneView(&ViewFamily);

			//Grid handles--------------------------------------------------------------------------------------------
			{
				FMatrix ProjectionMatrix = SceneView->ViewMatrices.GetViewProjectionMatrix();
				FIntPoint ViewportSize = ViewportClient->Viewport->GetSizeXY();

				const FVector Bounds = FVector(Dimensions) / 2;
				const double HOffset = 1;
				Handles = { FVector(Bounds.X + HOffset, 0, 0), FVector(-Bounds.X - HOffset, 0, 0),
							FVector(0, Bounds.Y + HOffset, 0), FVector(0, -Bounds.Y - HOffset, 0),
							FVector(0, 0, Bounds.Z + HOffset), FVector(0, 0, -Bounds.Z - HOffset)
				};

				const double HandleRadius = Grid->GetActorScale3D().X;
				HoveredHandle = -1;
				for (int32 i = 0; i < 6; ++i)
				{
					Handles[i] = Grid->GetActorTransform().TransformPosition(Handles[i]);
					FVector4 ScreenPosition = ProjectionMatrix.TransformPosition(Handles[i]);

					//Check if handle is behind camera
					if (ScreenPosition.W <= 0) continue;

					ScreenPosition.X *= 1 / ScreenPosition.W;
					ScreenPosition.Y *= 1 / ScreenPosition.W;

					//Convert to viewport coordinates
					const double DistanceSquared = FVector2D::DistSquared(
						FVector2D((ScreenPosition.X + 1) * ViewportSize.X / 2, (1 - ScreenPosition.Y) * ViewportSize.Y / 2),
						FVector2D(MousePosition));
					if (DistanceSquared < HandleRadius * HandleRadius) {
						HoveredHandle = i;
					}
				}
			}

			FVector Start, Direction;
			SceneView->DeprojectFVector2D(MousePosition, Start, Direction);

			//Depth scroll reset------------------------------------------------------------------------
			FVector DominantAxis(1, 0, 0);
			float MaxComponent = FMath::Abs(Direction.X);
			if (FMath::Abs(Direction.Y) > MaxComponent) {
				DominantAxis = FVector(0, 1, 0);
				MaxComponent = FMath::Abs(Direction.Y);
			}
			if (FMath::Abs(Direction.Z) > MaxComponent) {
				DominantAxis = FVector(0, 0, 1);
			}
			if (!DominantAxis.Equals(OldAxis)) {
				OldAxis = DominantAxis;
				ScrollOffset = FIntVector::ZeroValue;
			}

			//Trace for geometry first-----------------------------------------------------------------------------------------
			FHitResult OutHit;
			if (GetWorld()->LineTraceSingleByChannel(OutHit, Start, Start + (Direction * 10000), ECC_Camera)) {
				for (AActor* Actor : GridInternal)
				{
					if (OutHit.GetActor() == Actor) {
						FTransform InverseTransform = Grid->GetTransform().Inverse();
						FVector HitPoint = InverseTransform.TransformPosition(OutHit.Location + (Direction * Actor->GetActorScale3D().X));
						FVector GridPosition(HitPoint - (FVector(Dimensions) / -2));
						HoveredTile.X = FMath::FloorToInt(GridPosition.X);
						HoveredTile.Y = FMath::FloorToInt(GridPosition.Y);
						HoveredTile.Z = FMath::FloorToInt(GridPosition.Z);

						FVector Normal = InverseTransform.TransformVector(OutHit.Normal).GetSafeNormal();
						if (FMath::Abs(Normal.X) > 0.9) HoveredTile.X += FMath::Sign(Normal.X);
						else if (FMath::Abs(Normal.Y) > 0.9) HoveredTile.Y += FMath::Sign(Normal.Y);
						else if (FMath::Abs(Normal.Z) > 0.9) HoveredTile.Z += FMath::Sign(Normal.Z);

						if (Actor != PrevHit) {
							PrevHit = Actor;
							ScrollOffset = FIntVector::ZeroValue;
						}
						HoveredTile += ScrollOffset;

						HoveredTile.X = FMath::Clamp(HoveredTile.X, 0, Dimensions.X - 1);
						HoveredTile.Y = FMath::Clamp(HoveredTile.Y, 0, Dimensions.Y - 1);
						HoveredTile.Z = FMath::Clamp(HoveredTile.Z, 0, Dimensions.Z - 1);
						return;
					}
				}
			}

			//Trace for grid----------------------------------------------------------------------------------------
			//World space to Grid's local space
			FTransform InverseTransform = Grid->GetActorTransform().Inverse();
			Start  = InverseTransform.TransformPosition(Start);
			Direction = InverseTransform.TransformVector(Direction);
			Direction.Normalize();

			//Slab method
			const FVector MinBound = FVector(Dimensions) / -2;
			const FVector MaxBound = FVector(Dimensions) / 2;

			FVector InverseDirection(1 / Direction.X, 1 / Direction.Y, 1 / Direction.Z);
			float T1 = (MinBound.X - Start.X) * InverseDirection.X;
			float T2 = (MaxBound.X - Start.X) * InverseDirection.X;
			float T3 = (MinBound.Y - Start.Y) * InverseDirection.Y;
			float T4 = (MaxBound.Y - Start.Y) * InverseDirection.Y;
			float T5 = (MinBound.Z - Start.Z) * InverseDirection.Z;
			float T6 = (MaxBound.Z - Start.Z) * InverseDirection.Z;

			float TMin = FMath::Max(FMath::Max(FMath::Min(T1, T2), FMath::Min(T3, T4)), FMath::Min(T5, T6));
			float TMax = FMath::Min(FMath::Min(FMath::Max(T1, T2), FMath::Max(T3, T4)), FMath::Max(T5, T6));
			if (TMax < 0 || TMin > TMax) return;
			PrevHit = nullptr;

			//Convert local position to grid tile
			FVector HitPoint = Start + Direction * TMin;
			FVector GridPosition = (HitPoint - MinBound);
			HoveredTile.X = FMath::FloorToInt(GridPosition.X);
			HoveredTile.Y = FMath::FloorToInt(GridPosition.Y);
			HoveredTile.Z = FMath::FloorToInt(GridPosition.Z);

			HoveredTile += ScrollOffset;

			HoveredTile.X = FMath::Clamp(HoveredTile.X, 0, Dimensions.X - 1);
			HoveredTile.Y = FMath::Clamp(HoveredTile.Y, 0, Dimensions.Y - 1);
			HoveredTile.Z = FMath::Clamp(HoveredTile.Z, 0, Dimensions.Z - 1);
		}
	}
}

void URewindEditorMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	for (TActorIterator<AGrid> Itr(GetWorld()); Itr; ++Itr)
	{
		Grid = *Itr;
		if (Grid) {
			const float Thickness = 0.2;
			const FLinearColor GridColor(0, 0.25, 1, 0.1);

			const FIntVector NumLines(Dimensions.X + 1, Dimensions.Y + 1, Dimensions.Z + 1);
			const double ScaledBlockSize = Grid->GetActorScale().X;
			const FVector MinBound = FVector(Dimensions) * ScaledBlockSize / -2;
			const FVector MaxBound = FVector(Dimensions) * ScaledBlockSize / 2;

			const FQuat Rotor(FRotator(0, Grid->GetActorRotation().Yaw, 0));
			const FVector Location = Grid->GetActorLocation();
			for (int32 X = 0; X < NumLines.X; ++X)
			{
				for (int32 Y = 0; Y < NumLines.Y; ++Y)
				{
					FVector Start = FVector(MinBound.X + (X * ScaledBlockSize), MinBound.Y + (Y * ScaledBlockSize), MinBound.Z);
					FVector End = Start + FVector(0, 0, MaxBound.Z - MinBound.Z);

					Start = Rotor.RotateVector(Start) + Location;
					End = Rotor.RotateVector(End) + Location;
					PDI->DrawTranslucentLine(Start, End, GridColor, SDPG_Foreground, Thickness);
				}

				for (int32 Z = 0; Z < NumLines.Z; ++Z)
				{
					FVector Start = FVector(MinBound.X + (X * ScaledBlockSize), MinBound.Y, MinBound.Z + (Z * ScaledBlockSize));
					FVector End = Start + FVector(0, MaxBound.Y - MinBound.Y, 0);

					Start = Rotor.RotateVector(Start) + Location;
					End = Rotor.RotateVector(End) + Location;
					PDI->DrawTranslucentLine(Start, End, GridColor, SDPG_Foreground, Thickness);
				}
			}

			for (int32 Y = 0; Y < NumLines.Y; ++Y)
			{
				for (int32 Z = 0; Z < NumLines.Z; ++Z)
				{
					FVector Start = FVector(MinBound.X, MinBound.Y + (Y * ScaledBlockSize), MinBound.Z + (Z * ScaledBlockSize));
					FVector End = Start + FVector(MaxBound.X - MinBound.X, 0, 0);

					Start = Rotor.RotateVector(Start) + Location;
					End = Rotor.RotateVector(End) + Location;
					PDI->DrawTranslucentLine(Start, End, GridColor, SDPG_Foreground, Thickness);
				}
			}

			//-----------------------------------------------------------------------------------------------
			if (HoveredTile != FIntVector(-1, -1, -1)) {
				FVector CellMin = MinBound + FVector(HoveredTile) * ScaledBlockSize;
				FVector CellMax = CellMin + FVector(ScaledBlockSize);

				GridTransform.SetScale3D(FVector::OneVector);

				FVector Vertices[8];
				Vertices[0] = GridTransform.TransformPosition(FVector(CellMin.X, CellMin.Y, CellMin.Z));
				Vertices[1] = GridTransform.TransformPosition(FVector(CellMax.X, CellMin.Y, CellMin.Z));
				Vertices[2] = GridTransform.TransformPosition(FVector(CellMin.X, CellMax.Y, CellMin.Z));
				Vertices[3] = GridTransform.TransformPosition(FVector(CellMax.X, CellMax.Y, CellMin.Z));
				Vertices[4] = GridTransform.TransformPosition(FVector(CellMin.X, CellMin.Y, CellMax.Z));
				Vertices[5] = GridTransform.TransformPosition(FVector(CellMax.X, CellMin.Y, CellMax.Z));
				Vertices[6] = GridTransform.TransformPosition(FVector(CellMin.X, CellMax.Y, CellMax.Z));
				Vertices[7] = GridTransform.TransformPosition(FVector(CellMax.X, CellMax.Y, CellMax.Z));

				const FLinearColor HighlightColor(1.0, 0.15, 0.5, 0.5);
				const float HighlightThickness = 0.3;

				//Bottom face
				PDI->DrawLine(Vertices[0], Vertices[1], HighlightColor, SDPG_Foreground, HighlightThickness);
				PDI->DrawLine(Vertices[1], Vertices[3], HighlightColor, SDPG_Foreground, HighlightThickness);
				PDI->DrawLine(Vertices[3], Vertices[2], HighlightColor, SDPG_Foreground, HighlightThickness);
				PDI->DrawLine(Vertices[2], Vertices[0], HighlightColor, SDPG_Foreground, HighlightThickness);
				//Top face
				PDI->DrawLine(Vertices[4], Vertices[5], HighlightColor, SDPG_Foreground, HighlightThickness);
				PDI->DrawLine(Vertices[5], Vertices[7], HighlightColor, SDPG_Foreground, HighlightThickness);
				PDI->DrawLine(Vertices[7], Vertices[6], HighlightColor, SDPG_Foreground, HighlightThickness);
				PDI->DrawLine(Vertices[6], Vertices[4], HighlightColor, SDPG_Foreground, HighlightThickness);
				//Vertical edges
				PDI->DrawLine(Vertices[0], Vertices[4], HighlightColor, SDPG_Foreground, HighlightThickness);
				PDI->DrawLine(Vertices[1], Vertices[5], HighlightColor, SDPG_Foreground, HighlightThickness);
				PDI->DrawLine(Vertices[2], Vertices[6], HighlightColor, SDPG_Foreground, HighlightThickness);
				PDI->DrawLine(Vertices[3], Vertices[7], HighlightColor, SDPG_Foreground, HighlightThickness);
			}

			for (int32 i = 0; i < Handles.Num(); ++i)
			{
				if (i == HoveredHandle) {
					PDI->DrawPoint(Handles[i], Viewport->KeyState(EKeys::LeftAlt) ? FLinearColor::Red : FLinearColor::Green, 30, SDPG_Foreground);
				}
				else PDI->DrawPoint(Handles[i], FLinearColor::White, 20, SDPG_Foreground);
			}
		}
	}
}

bool URewindEditorMode::HandleClick(FEditorViewportClient* ViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	if (Click.GetKey() != EKeys::LeftMouseButton) return true;
	if (!Grid) return false;
	if (HoveredHandle != -1) {
		ResizeGrid(ViewportClient->Viewport->KeyState(EKeys::LeftAlt));
	}
	else if (HoveredTile != FIntVector(-1, -1, -1)) {
		if (AGridActor* Actor = QueryAt(HoveredTile)) {
			if (ViewportClient->Viewport->KeyState(EKeys::LeftAlt)) {
				Actor->Destroy();
				SetAt(HoveredTile, nullptr);
			}
		}
		else if (!ViewportClient->Viewport->KeyState(EKeys::LeftAlt)) {
			FVector MinBound = FVector(Dimensions) / -2;
			FVector Location = MinBound + (FVector(HoveredTile)) + 0.5;
			Location = Grid->GetActorTransform().TransformPosition(Location);
			AGridActor* NewActor = GetWorld()->SpawnActor<AGridActor>(Location, FRotator(0, Grid->GetActorRotation().Yaw, 0));

			NewActor->Type = GridType;
			UStaticMesh* BlockMesh = *MeshMap.Find(GridType);
			NewActor->GetStaticMeshComponent()->SetStaticMesh(BlockMesh);
			FVector Bounds = BlockMesh->GetBoundingBox().GetSize();
			NewActor->SetActorScale3D(Grid->GetActorScale3D() / Bounds);

			SetAt(HoveredTile, NewActor);
		}
	}
	return true;
}

bool URewindEditorMode::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{	
	if (Key != EKeys::MouseScrollDown && Key != EKeys::MouseScrollUp) return UBaseLegacyWidgetEdMode::InputKey(ViewportClient, Viewport, Key, Event);
	if (!Viewport->KeyState(EKeys::RightMouseButton)) {
		if (Event == IE_Pressed && Grid) {
			if (Viewport->KeyState(EKeys::LeftAlt)) {
				int32 ScrollDirection = Key == EKeys::MouseScrollUp ? 1 : -1;
				int8 Temp = StaticCast<int8>(GridType);
				Temp += ScrollDirection;
				Temp = FMath::Clamp(Temp, 1, 5);
				GridType = StaticCast<enum GridType>(Temp);

				FText Block = FText::FromString(UEnum::GetValueAsString<enum GridType>(GridType).RightChop(10).ToUpper());
				StaticCast<FRewindEditorToolkit*>(GetToolkit().Pin().Get())->MainWidget->BlockText->SetText(Block);
			}
			else {
				FVector CameraForward = ViewportClient->GetViewRotation().Vector();
				FVector Direction = Grid->GetActorTransform().Inverse().TransformVector(CameraForward);
				Direction.Normalize();

				int32 DominantAxis = 0;
				float MaxComponent = FMath::Abs(Direction.X);
				if (FMath::Abs(Direction.Y) > MaxComponent) {
					DominantAxis = 1;
					MaxComponent = FMath::Abs(Direction.Y);
				}
				if (FMath::Abs(Direction.Z) > MaxComponent) {
					DominantAxis = 2;
				}
				int32 ScrollDirection = Key == EKeys::MouseScrollUp ? 1 : -1;
				switch (DominantAxis)
				{
				case 0:
					ScrollOffset.X += (FMath::Abs(ScrollOffset.X + (ScrollDirection * FMath::Sign(Direction.X))) < Dimensions.X) * ScrollDirection * FMath::Sign(Direction.X);
					break;
				case 1:
					ScrollOffset.Y += (FMath::Abs(ScrollOffset.Y + (ScrollDirection * FMath::Sign(Direction.Y))) < Dimensions.Y) * ScrollDirection * FMath::Sign(Direction.Y);
					break;
				case 2:
					ScrollOffset.Z += (FMath::Abs(ScrollOffset.Z + (ScrollDirection * FMath::Sign(Direction.Z))) < Dimensions.Z) * ScrollDirection * FMath::Sign(Direction.Z);
					break;
				}
			}
		}
		return true;
	}
	return UBaseLegacyWidgetEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}

void URewindEditorMode::ResizeGrid(bool bIsShrink)
{
	FIntVector NewDimensions = Dimensions;

	FIntVector Offset(0, 0, 0);
	switch (HoveredHandle)
	{
	case 0:
		if (!bIsShrink) ++NewDimensions.X;
		else if (NewDimensions.X > 1) --NewDimensions.X;
		break;
	case 1:
		if (!bIsShrink) {
			++NewDimensions.X;
			Offset.X = 1;
		}
		else if (NewDimensions.X > 1) {
			--NewDimensions.X;
			Offset.X = -1;
		}
		break;
	case 2:
		if (!bIsShrink) ++NewDimensions.Y;
		else if (NewDimensions.Y > 1) --NewDimensions.Y;
		break;
	case 3:
		if (!bIsShrink) {
			++NewDimensions.Y;
			Offset.Y = 1;
		}
		else if (NewDimensions.Y > 1) {
			--NewDimensions.Y;
			Offset.Y = -1;
		}
		break;
	case 4:
		if (!bIsShrink) ++NewDimensions.Z;
		else if (NewDimensions.Z > 1) --NewDimensions.Z;
		break;
	case 5:
		if (!bIsShrink) {
			++NewDimensions.Z;
			Offset.Z = 1;
		}
		else if (NewDimensions.Z > 1) {
			--NewDimensions.Z;
			Offset.Z = -1;
		}
	}
	if (NewDimensions == Dimensions) return;

	TArray<AGridActor*> Temp;
	Temp.Init(nullptr, NewDimensions.X * NewDimensions.Y * NewDimensions.Z);
	for (int32 Z = 0; Z < Dimensions.Z; ++Z)
	{
		for (int32 Y = 0; Y < Dimensions.Y; ++Y)
		{
			for (int32 X = 0; X < Dimensions.X; ++X)
			{
				int32 OldIndex = (Z * Dimensions.X * Dimensions.Y) + (Y * Dimensions.X) + X;
				int32 NewX = X + Offset.X;
				int32 NewY = Y + Offset.Y;
				int32 NewZ = Z + Offset.Z;

				if (NewX >= 0 && NewX < NewDimensions.X &&
					NewY >= 0 && NewY < NewDimensions.Y &&
					NewZ >= 0 && NewZ < NewDimensions.Z)
				{
					int32 NewIndex = (NewZ * NewDimensions.X * NewDimensions.Y) + (NewY * NewDimensions.X) + NewX;
					Temp[NewIndex] = GridInternal[OldIndex];

					if (GridInternal[OldIndex]) {
						FVector MinBound = FVector(NewDimensions) / -2;
						FVector Location = MinBound + (FVector(NewX, NewY, NewZ)) + 0.5;
						Location = Grid->GetActorTransform().TransformPosition(Location);
						GridInternal[OldIndex]->SetActorLocation(Location);
					}
				}
				else if (GridInternal[OldIndex]) {
					GridInternal[OldIndex]->Destroy();
				}
			}
		}
	}

	Dimensions = NewDimensions;
	GridInternal = MoveTemp(Temp);
}

void URewindEditorMode::UpdateTiles()
{
	for (int32 Z = 0; Z < Dimensions.Z; ++Z)
	{
		for (int32 Y = 0; Y < Dimensions.Y; ++Y)
		{
			for (int32 X = 0; X < Dimensions.X; ++X)
			{
				if (AGridActor* Actor = GridInternal[(Z * Dimensions.X * Dimensions.Y) + (Y * Dimensions.X) + X]) {
					FVector MinBound = FVector(Dimensions) / -2;
					FVector Location = MinBound + (FVector(X, Y, Z)) + 0.5;
					Location = GridTransform.TransformPosition(Location);

					Actor->SetActorLocation(Location);
					Actor->SetActorRotation(FRotator(0, Grid->GetActorRotation().Yaw, 0));

					FVector Bounds = Actor->GetStaticMeshComponent()->GetStaticMesh()->GetBoundingBox().GetSize();
					Actor->SetActorScale3D(FVector((Grid->GetActorScale3D().X) / Bounds.X));
				}
			}
		}
	}
}

AGridActor* URewindEditorMode::QueryAt(const FIntVector& Location)
{
	if (Location.X < 0 || Location.X >= Dimensions.X ||
		Location.Y < 0 || Location.Y >= Dimensions.Y ||
		Location.Z < 0 || Location.Z >= Dimensions.Z)
	{
		LOG("Invalid query at x: %d, y: %d, z: %d", Location.X, Location.Y, Location.Z);
		return nullptr;
	}

	return GridInternal[Location.X + (Location.Y * Dimensions.X) + (Location.Z * Dimensions.X * Dimensions.Y)];
}

void URewindEditorMode::SetAt(const FIntVector& Location, AGridActor* Actor)
{
	if (Location.X < 0 || Location.X >= Dimensions.X ||
		Location.Y < 0 || Location.Y >= Dimensions.Y ||
		Location.Z < 0 || Location.Z >= Dimensions.Z)
	{
		LOG("Invalid set at x: %d, y: %d, z: %d", Location.X, Location.Y, Location.Z);
		return;
	}

	GridInternal[Location.X + (Location.Y * Dimensions.X) + (Location.Z * Dimensions.X * Dimensions.Y)] = Actor;
}

void URewindEditorMode::SaveToSettings()
{
	if (!Grid) return;
	if (ARewindWorldSettings* Settings = Cast<ARewindWorldSettings>(GetWorld()->GetWorldSettings())) {
		Settings->Modify();

		FVector Scale;
		TArray<uint8>& Data = Settings->GridData;

		FIntVector NewDimensions = Dimensions + FIntVector(2, 2, 2);
		Data.Init(0, NewDimensions.X * NewDimensions.Y * NewDimensions.Z);
		for (int32 Z = 0; Z < Dimensions.Z; Z++) {
			for (int32 Y = 0; Y < Dimensions.Y; Y++) {
				for (int32 X = 0; X < Dimensions.X; X++) {
					int32 OldIndex = X + Y * Dimensions.X + Z * Dimensions.X * Dimensions.Y;
					int32 NewIndex = (X + 1) + (Y + 1) * NewDimensions.X + (Z + 1 ) * NewDimensions.X * NewDimensions.Y;

					if (GridInternal[OldIndex]) {
						Data[NewIndex] = StaticCast<uint8>(GridInternal[OldIndex]->Type);
						Scale = GridInternal[OldIndex]->GetStaticMeshComponent()->GetStaticMesh()->GetBoundingBox().GetSize();
					}
				}
			}
		}

		Settings->Dimensions = NewDimensions;
		Settings->Transform = Grid->GetActorTransform(); 
		Settings->BlockScale = Grid->GetActorScale3D() / Scale;

		Settings->MarkPackageDirty();
	}
}

//-------------------------------------------------------------------------------------------------------------------------------

AGrid::AGrid()
{
	BillboardComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Grid"));

	static ConstructorHelpers::FObjectFinder<UTexture2D> Sprite(TEXT("/Game/GridIcon"));
	BillboardComponent->Sprite = Sprite.Object;
	BillboardComponent->bIsScreenSizeScaled = false;
	BillboardComponent->bUseInEditorScaling = false;
	BillboardComponent->EditorScale = 0.005;
	BillboardComponent->bReceivesDecals = false;
}