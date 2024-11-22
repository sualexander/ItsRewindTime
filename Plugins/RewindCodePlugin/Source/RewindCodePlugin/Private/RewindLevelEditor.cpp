// Copyright It's Rewind Time 2024


#include "RewindLevelEditor.h"

#include "Engine.h"
#include "Kismet/GameplayStatics.h"


DEFINE_LOG_CATEGORY_STATIC(RewindEditor, Log, All);
#define LOG(Str, ...) UE_LOG(RewindEditor, Log, TEXT(Str), ##__VA_ARGS__)

#define LOCTEXT_NAMESPACE "RewindModule"


void SRewindEditor::Construct(const FArguments& InArgs)
{
	ChildSlot
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("Yo momma")))
		];
}

 void FRewindEditorToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
 {
 	SAssignNew(ToolkitWidget, SBorder)
 		[
 			SNew(SRewindEditor)
 		];

 	FModeToolkit::Init(InitToolkitHost, InOwningMode);
 }

 void FRewindEditorToolkit::InvokeUI()
 {
 	FModeToolkit::InvokeUI();
 	InlineContentHolder->SetContent(ToolkitWidget.ToSharedRef()); 
 }

 //-----------------------------------------------------------------------------------------

URewindEditorMode::URewindEditorMode()
{
	Info = FEditorModeInfo(TEXT("RewindEditorMode"), LOCTEXT("ModeName", "Rewind Editor"), FSlateIcon(), true);

	GridInternal.Init(nullptr, Dimensions.X * Dimensions.Y * Dimensions.Z);
}

void URewindEditorMode::Enter()
{
	UEdMode::Enter();

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationPreInputKeyDownListener().AddUObject(this, &URewindEditorMode::OnKeyDown);
	}
}

void URewindEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FRewindEditorToolkit);
}

void URewindEditorMode::OnKeyDown(const FKeyEvent& Event)
{
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
			FIntPoint MousePosition;
			ViewportClient->Viewport->GetMousePos(MousePosition);

			FSceneViewFamily ViewFamily = FSceneViewFamily::ConstructionValues(
				ViewportClient->Viewport, ViewportClient->GetScene(), ViewportClient->EngineShowFlags);
			FSceneView* SceneView = ViewportClient->CalcSceneView(&ViewFamily);

			FVector Start, Direction;
			SceneView->DeprojectFVector2D(MousePosition, Start, Direction);

			//Trace for geometry first--------------------------------------------------------------------------------------
			FHitResult OutHit;
			if (GetWorld()->LineTraceSingleByChannel(OutHit, Start, Start + (Direction * 1000), ECC_Camera)) {
				for (AActor* Actor : GridInternal)
				{
					if (OutHit.GetActor() == Actor) {

						LOG("Hit %s", *Actor->GetName());
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
			const double BlockSize = 10;
			const FVector MinBound = FVector(Dimensions) * BlockSize * -0.5f;
			const FVector MaxBound = FVector(Dimensions) * BlockSize * 0.5f;

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

			//Convert position to grid tile
			FVector HitPoint = Start + Direction * TMin;
			FVector GridPosition = (HitPoint - MinBound) / BlockSize;
			HoveredTile.X = FMath::FloorToInt(GridPosition.X);
			HoveredTile.Y = FMath::FloorToInt(GridPosition.Y);
			HoveredTile.Z = FMath::FloorToInt(GridPosition.Z);
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
			const float Thickness = 0.1;
			const FLinearColor GridColor(0, 0.25, 1, 0.2);

			const FIntVector NumLines(Dimensions.X + 1, Dimensions.Y + 1, Dimensions.Z + 1);
			const double BlockSize = 10 * Grid->GetActorScale().X;
			const FVector MinBound = FVector(Dimensions) * BlockSize * -0.5;
			const FVector MaxBound = FVector(Dimensions) * BlockSize * 0.5;

			const FQuat Rotor(FRotator(0, Grid->GetActorRotation().Yaw, 0));
			const FVector Location = Grid->GetActorLocation();
			for (int32 X = 0; X < NumLines.X; ++X)
			{
				for (int32 Y = 0; Y < NumLines.Y; ++Y)
				{
					FVector Start = FVector(MinBound.X + (X * BlockSize), MinBound.Y + (Y * BlockSize), MinBound.Z);
					FVector End = Start + FVector(0, 0, MaxBound.Z - MinBound.Z);

					Start = Rotor.RotateVector(Start) + Location;
					End = Rotor.RotateVector(End) + Location;
					PDI->DrawTranslucentLine(Start, End, GridColor, SDPG_Foreground, Thickness);
				}

				for (int32 Z = 0; Z < NumLines.Z; ++Z)
				{
					FVector Start = FVector(MinBound.X + (X * BlockSize), MinBound.Y, MinBound.Z + (Z * BlockSize));
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
					FVector Start = FVector(MinBound.X, MinBound.Y + (Y * BlockSize), MinBound.Z + (Z * BlockSize));
					FVector End = Start + FVector(MaxBound.X - MinBound.X, 0, 0);

					Start = Rotor.RotateVector(Start) + Location;
					End = Rotor.RotateVector(End) + Location;
					PDI->DrawTranslucentLine(Start, End, GridColor, SDPG_Foreground, Thickness);
				}
			}

			//-----------------------------------------------------------------------------------------------
			if (HoveredTile != FIntVector(-1, -1, -1)) {
				FVector CellMin = MinBound + FVector(HoveredTile) * BlockSize;
				FVector CellMax = CellMin + FVector(BlockSize);

				FTransform GridTransform = Grid->GetActorTransform();
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

				const FLinearColor HighlightColor(1.0, 0, 0.5, 0.5f);
				const float HighlightThickness = 0.2;

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
		}
	}
}

//IMPLEMENT_HIT_PROXY(HGridProxy, HHitProxy);
bool URewindEditorMode::HandleClick(FEditorViewportClient* ViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	//if (!HitProxy || !HitProxy->IsA(HGridProxy::StaticGetType())) return false;
	//
	//HGridProxy* Proxy = StaticCast<HGridProxy*>(HitProxy);
	//LOG("Selected %s", *Proxy->Grid->GetName());

	if (!Grid) return false;
	if (AGridActor* Actor = QueryAt(HoveredTile)) {

	}
	else {
		float BlockSize = 10 * Grid->GetActorScale().X;
		FVector MinBound = FVector(Dimensions) * BlockSize * -0.5;
		FVector Location = MinBound + (FVector(HoveredTile) * BlockSize) + (FVector(BlockSize) * 0.5);
		Grid->GetActorTransform().TransformPosition(Location);

		AGridActor* NewActor = GetWorld()->SpawnActor<AGridActor>(Location, FRotator(0, Grid->GetActorRotation().Yaw, 0));

		NewActor->Type = GridType::Solid;
		UStaticMesh* BlockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Blocks/Cubes/WoodenCube.WoodenCube"));
		NewActor->GetStaticMeshComponent()->SetStaticMesh(BlockMesh);
		FVector Bounds = BlockMesh->GetBoundingBox().GetSize();
		NewActor->SetActorScale3D(FVector(BlockSize / Bounds.X));
		
		SetAt(HoveredTile, NewActor);
	}

	return true;
}

bool URewindEditorMode::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{	
	if (!Viewport->KeyState(EKeys::RightMouseButton) && (Key == EKeys::MouseScrollDown || Key == EKeys::MouseScrollUp)) {
		LOG("scrollling");
		return true;
	}
	return UBaseLegacyWidgetEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}

bool URewindEditorMode::BoxSelect(FBox& InBox, bool InSelect /*= true*/)
{
	//FConvexVolume BoxVolume(GetVolumeFromBox(InBox));
	//return FrustumSelect(BoxVolume, nullptr, InSelect);
	return false;
}

bool URewindEditorMode::FrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, bool InSelect /*= true*/)
{
	//bool bStrictDragSelection = GetDefault<ULevelEditorViewportSettings>()->bStrictBoxSelection;
	//bool bSelectedBones = false;

	//if (USelection* SelectedActors = GEditor->GetSelectedActors())
	//{
	//	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	//	{
	//		AActor* Actor = Cast<AActor>(*Iter);
	//		bSelectedBones = UpdateSelectionInFrustum(InFrustum, Actor, bStrictDragSelection, false, false) || bSelectedBones;
	//	}
	//}

	//return bSelectedBones;
	return false;
}

AGrid::AGrid()
{
	BillboardComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Grid"));

	if (BillboardComponent)
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> Sprite(TEXT("/Game/GridIcon"));
		BillboardComponent->SetWorldScale3D(FVector(0.1, 0.1, 0.1));
		BillboardComponent->Sprite = Sprite.Object;
		BillboardComponent->bIsScreenSizeScaled = true;
		BillboardComponent->bReceivesDecals = false;
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