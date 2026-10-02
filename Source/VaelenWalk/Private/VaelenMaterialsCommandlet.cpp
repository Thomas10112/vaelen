// VAELEN - VaelenWalk
// Phase 23 task 23.01: the materials, built by code. See VaelenMaterialsCommandlet.h.
//
// STATUS: UNVERIFIED (engine) - 23.01 (2026-10-02), not parsed (editor only)
// and not built: sitting S5 runs it.
#include "VaelenMaterialsCommandlet.h"

#if WITH_EDITOR
// clang-format off
#include "AssetRegistry/AssetRegistryModule.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
// clang-format on

DEFINE_LOG_CATEGORY_STATIC(LogVaelenMaterials, Log, All);

namespace
{
	constexpr const TCHAR* Folder = TEXT("/Game/Vaelen/Materials/");

	/// One node, placed on the graph so the editor shows it readably.
	template <typename T>
	T* Node(UMaterial* M, int32 X, int32 Y)
	{
		return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpression(M, T::StaticClass(), X, Y));
	}

	bool Wire(UMaterialExpression* From, UMaterialExpression* To, const TCHAR* Input)
	{
		return UMaterialEditingLibrary::ConnectMaterialExpressions(From, TEXT(""), To, Input);
	}

	bool Property(UMaterialExpression* From, EMaterialProperty What)
	{
		return UMaterialEditingLibrary::ConnectMaterialProperty(From, TEXT(""), What);
	}

	UMaterialExpressionConstant* Scalar(UMaterial* M, float Value, int32 X, int32 Y)
	{
		UMaterialExpressionConstant* C = Node<UMaterialExpressionConstant>(M, X, Y);
		C->R = Value;
		return C;
	}

	UMaterialExpressionConstant3Vector* Colour(UMaterial* M, const FLinearColor& Value, int32 X, int32 Y)
	{
		UMaterialExpressionConstant3Vector* C = Node<UMaterialExpressionConstant3Vector>(M, X, Y);
		C->Constant = Value;
		return C;
	}

	/// M_Ground: the vertex colour (the biome, the snow, the grass - what
	/// Scene::ApplyClimate painted) as the base, a rock grey blended in where
	/// the vertex normal leans past about thirty degrees, a grain of noise
	/// over it so a hillside is not one flat tint, rough as earth.
	bool BuildGround(UMaterial* M)
	{
		UMaterialExpressionVertexColor* Vertex = Node<UMaterialExpressionVertexColor>(M, -900, -200);
		UMaterialExpressionConstant3Vector* Rock = Colour(M, FLinearColor(0.42f, 0.40f, 0.38f), -900, 0);
		// The slope: 1 - normal.z, 0 flat, 1 vertical; rock from 0.13 (about 30
		// degrees) to 0.28 (about 45, the walkable floor), saturated.
		UMaterialExpressionVertexNormalWS* Normal = Node<UMaterialExpressionVertexNormalWS>(M, -1200, 250);
		UMaterialExpressionComponentMask* Up = Node<UMaterialExpressionComponentMask>(M, -1050, 250);
		Up->R = false;
		Up->G = false;
		Up->B = true;
		Up->A = false;
		UMaterialExpressionOneMinus* Slope = Node<UMaterialExpressionOneMinus>(M, -900, 250);
		UMaterialExpressionSubtract* Past = Node<UMaterialExpressionSubtract>(M, -750, 250);
		Past->ConstB = 0.13f;
		UMaterialExpressionMultiply* Steeper = Node<UMaterialExpressionMultiply>(M, -600, 250);
		Steeper->ConstB = 1.0f / 0.15f;
		UMaterialExpressionSaturate* Mask = Node<UMaterialExpressionSaturate>(M, -450, 250);
		UMaterialExpressionLinearInterpolate* Mixed = Node<UMaterialExpressionLinearInterpolate>(M, -300, -100);
		// The grain: a simplex noise over world position, 0..1 scaled to a
		// twelfth, added - the same bytes on every machine, no texture.
		UMaterialExpressionWorldPosition* Where = Node<UMaterialExpressionWorldPosition>(M, -600, 500);
		UMaterialExpressionNoise* Grain = Node<UMaterialExpressionNoise>(M, -450, 500);
		Grain->Scale = 0.004f;
		Grain->Levels = 3;
		Grain->OutputMin = -0.06f;
		Grain->OutputMax = 0.06f;
		UMaterialExpressionAdd* Base = Node<UMaterialExpressionAdd>(M, -150, -100);
		UMaterialExpressionConstant* Rough = Scalar(M, 0.9f, -150, 200);
		bool bOk = true;
		bOk &= Wire(Normal, Up, TEXT("Input"));
		bOk &= Wire(Up, Slope, TEXT("Input"));
		bOk &= Wire(Slope, Past, TEXT("A"));
		bOk &= Wire(Past, Steeper, TEXT("A"));
		bOk &= Wire(Steeper, Mask, TEXT("Input"));
		bOk &= Wire(Vertex, Mixed, TEXT("A"));
		bOk &= Wire(Rock, Mixed, TEXT("B"));
		bOk &= Wire(Mask, Mixed, TEXT("Alpha"));
		bOk &= Wire(Where, Grain, TEXT("Position"));
		bOk &= Wire(Mixed, Base, TEXT("A"));
		bOk &= Wire(Grain, Base, TEXT("B"));
		bOk &= Property(Base, MP_BaseColor);
		bOk &= Property(Rough, MP_Roughness);
		return bOk;
	}

	/// M_Water: translucent, the vertex colour the water sections carry
	/// (the sea's blue, a lake's), smooth so the sky and the sun reflect in
	/// it (screen-space reflections are on), seven tenths opaque so the bed
	/// shows near the shore.
	bool BuildWater(UMaterial* M)
	{
		M->BlendMode = BLEND_Translucent;
		M->TwoSided = false;
		UMaterialExpressionVertexColor* Vertex = Node<UMaterialExpressionVertexColor>(M, -600, -100);
		UMaterialExpressionConstant* Rough = Scalar(M, 0.08f, -600, 100);
		UMaterialExpressionConstant* Spec = Scalar(M, 0.6f, -600, 250);
		UMaterialExpressionConstant* Opacity = Scalar(M, 0.7f, -600, 400);
		bool bOk = true;
		bOk &= Property(Vertex, MP_BaseColor);
		bOk &= Property(Rough, MP_Roughness);
		bOk &= Property(Spec, MP_Specular);
		bOk &= Property(Opacity, MP_Opacity);
		return bOk;
	}

	/// M_Flat: an instanced shape tinted by its own three custom-data floats
	/// (the scenery sets a tree's by kind, 23.02), rough as bark and plaster;
	/// an instance that set nothing is a mid grey, not black.
	bool BuildFlat(UMaterial* M)
	{
		M->bUsedWithInstancedStaticMeshes = true;
		UMaterialExpressionPerInstanceCustomData* Channel[3] = {};
		for (int32 I = 0; I < 3; ++I)
		{
			Channel[I] = Node<UMaterialExpressionPerInstanceCustomData>(M, -900, -200 + 150 * I);
			Channel[I]->DataIndex = I;
			Channel[I]->DefaultValue = 0.5f;
		}
		UMaterialExpressionAppendVector* RG = Node<UMaterialExpressionAppendVector>(M, -700, -150);
		UMaterialExpressionAppendVector* RGB = Node<UMaterialExpressionAppendVector>(M, -500, -100);
		UMaterialExpressionConstant* Rough = Scalar(M, 0.85f, -500, 200);
		bool bOk = true;
		bOk &= Wire(Channel[0], RG, TEXT("A"));
		bOk &= Wire(Channel[1], RG, TEXT("B"));
		bOk &= Wire(RG, RGB, TEXT("A"));
		bOk &= Wire(Channel[2], RGB, TEXT("B"));
		bOk &= Property(RGB, MP_BaseColor);
		bOk &= Property(Rough, MP_Roughness);
		return bOk;
	}

	/// One material: a package, the graph, a recompile, the asset registry
	/// told, the file written. Says what it did, or what refused.
	bool Make(const TCHAR* Name, bool (*Build)(UMaterial*), int32& Saved)
	{
		const FString PackagePath = FString(Folder) + Name;
		UPackage* Package = CreatePackage(*PackagePath);
		if (Package == nullptr)
		{
			UE_LOG(LogVaelenMaterials, Error, TEXT("LogVaelenMaterials: %s: no package at %s"), Name, *PackagePath);
			return false;
		}
		Package->FullyLoad();
		UMaterial* M = NewObject<UMaterial>(Package, FName(Name), RF_Public | RF_Standalone);
		if (M == nullptr || !Build(M))
		{
			UE_LOG(LogVaelenMaterials, Error, TEXT("LogVaelenMaterials: %s: the graph could not be wired"), Name);
			return false;
		}
		UMaterialEditingLibrary::RecompileMaterial(M);
		FAssetRegistryModule::AssetCreated(M);
		Package->MarkPackageDirty();
		const FString File =
			FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Package, M, *File, Args))
		{
			UE_LOG(LogVaelenMaterials, Error, TEXT("LogVaelenMaterials: %s: could not save %s"), Name, *File);
			return false;
		}
		++Saved;
		UE_LOG(LogVaelenMaterials, Display, TEXT("LogVaelenMaterials: %s: %d expressions, saved %s"), Name,
			   M->GetExpressions().Num(), *File);
		return true;
	}
} // namespace

UVaelenMaterialsCommandlet::UVaelenMaterialsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UVaelenMaterialsCommandlet::Main(const FString& Params)
{
	int32 Saved = 0;
	Make(TEXT("M_Ground"), &BuildGround, Saved);
	Make(TEXT("M_Water"), &BuildWater, Saved);
	Make(TEXT("M_Flat"), &BuildFlat, Saved);
	UE_LOG(LogVaelenMaterials, Display, TEXT("LogVaelenMaterials: %d of 3 materials saved under %s"), Saved, Folder);
	return Saved == 3 ? 0 : 1;
}
#endif
