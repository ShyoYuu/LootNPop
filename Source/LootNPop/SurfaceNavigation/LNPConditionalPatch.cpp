// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPConditionalPatch.h"

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
	void SerializePatchNode(FArchive& Ar, FLNPLocalNavNodeRef& Node)
	{
		Ar << Node.LocalNavLayerId << Node.TileId << Node.LocalCellIndex;
	}

	template<typename T, typename F>
	void SerializeArray(FArchive& Ar, TArray<T>& Values, const int32 MinBytes, F Serialize)
	{
		int32 Count = Values.Num();
		Ar << Count;
		if (Ar.IsError()) { return; }
		if (Ar.IsLoading())
		{
			if (Count < 0 || Count > 16 * 1024 * 1024 || Count > (Ar.TotalSize() - Ar.Tell()) / MinBytes)
			{
				Ar.SetError();
				return;
			}
			Values.SetNum(Count);
		}
		for (T& Value : Values)
		{
			Serialize(Ar, Value);
			if (Ar.IsError()) { return; }
		}
	}

	void SerializeLayer(FArchive& Ar, FLNPSupportAtlasLayer& Layer)
	{
		Ar << Layer.SourceIndex << Layer.BaseRadius << Layer.RadiusStep
			<< Layer.Layout.Subdivisions << Layer.Layout.J0;
		SerializeArray(Ar, Layer.Layout.Rows, 8, [](FArchive& A, FLNPSupportRowSpan& Row) { A << Row.IStart << Row.Count; });
		SerializeArray(Ar, Layer.RadiusQ, 2, [](FArchive& A, int16& Q) { A << Q; });
		SerializeArray(Ar, Layer.NormalQ, 2, [](FArchive& A, int16& Q) { A << Q; });
		SerializeArray(Ar, Layer.Flags, 1, [](FArchive& A, uint8& Flag) { A << Flag; });
	}

	bool EdgeLess(const FLNPConditionalPatchEdge& A, const FLNPConditionalPatchEdge& B)
	{
		return LNPConditionalPatch::NodeLess(A.A, B.A)
			|| (A.A == B.A && LNPConditionalPatch::NodeLess(A.B, B.B));
	}

	bool SamePatchAgent(const FLNPNavAgentProfile& A, const FLNPNavAgentProfile& B)
	{
		return A.Radius == B.Radius && A.HalfHeight == B.HalfHeight && A.MaxStepUp == B.MaxStepUp
			&& A.MaxStepDown == B.MaxStepDown && A.WalkableMinDot == B.WalkableMinDot;
	}
}

bool LNPConditionalPatch::MarkerLess(const FGuid& A, const FGuid& B)
{
	return A.A != B.A ? A.A < B.A : A.B != B.B ? A.B < B.B : A.C != B.C ? A.C < B.C : A.D < B.D;
}

bool LNPConditionalPatch::NodeLess(const FLNPLocalNavNodeRef& A, const FLNPLocalNavNodeRef& B)
{
	return A.LocalNavLayerId != B.LocalNavLayerId ? A.LocalNavLayerId < B.LocalNavLayerId
		: A.TileId != B.TileId ? A.TileId < B.TileId : A.LocalCellIndex < B.LocalCellIndex;
}

void LNPConditionalPatch::Canonicalize(FLNPConditionalPatch& Patch)
{
	Patch.BlockedNodes.Sort(NodeLess);
	for (int32 Index = Patch.BlockedNodes.Num() - 1; Index > 0; --Index)
	{
		if (Patch.BlockedNodes[Index] == Patch.BlockedNodes[Index - 1]) { Patch.BlockedNodes.RemoveAt(Index); }
	}
	for (TArray<FLNPConditionalPatchEdge>* Edges : {&Patch.BlockedEdges, &Patch.WalkEdges})
	{
		for (FLNPConditionalPatchEdge& Edge : *Edges)
		{
			if (NodeLess(Edge.B, Edge.A)) { Swap(Edge.A, Edge.B); }
		}
		Edges->Sort(EdgeLess);
		for (int32 Index = Edges->Num() - 1; Index > 0; --Index)
		{
			if ((*Edges)[Index].A == (*Edges)[Index - 1].A && (*Edges)[Index].B == (*Edges)[Index - 1].B)
			{
				Edges->RemoveAt(Index);
			}
		}
	}
}

bool LNPConditionalPatch::ExtractNavigationDelta(const FLNPNavData& BaseNavigation,
	const FLNPNavData& CombinedNavigation, const FLNPNavTraversalData& CombinedTraversal,
	FLNPConditionalPatch& OutPatch, FString& OutError)
{
	const int32 BaseLayers = BaseNavigation.Layers.Num();
	if (CombinedNavigation.Layers.Num() != BaseLayers + OutPatch.SupportLayers.Num()
		|| CombinedNavigation.LocalStaticComponentCount < BaseNavigation.LocalStaticComponentCount)
	{
		OutError = TEXT("Conditional Patch Nav bake does not preserve the base graph.");
		return false;
	}
	OutPatch.Navigation = FLNPNavData();
	// base의 tile/cell 주소가 바뀌면 patch의 endpoint도 무효다. 동일성은 기존 codec의 canonical bytes로 확인한다.
	FLNPNavData RebuiltBase = CombinedNavigation;
	RebuiltBase.Layers.SetNum(BaseLayers);
	RebuiltBase.LocalStaticComponentCount = BaseNavigation.LocalStaticComponentCount;
	TArray<uint8> ExpectedBase, ActualBase;
	if (!LNPNavData::EncodeNavigation(BaseNavigation, ExpectedBase, OutError)
		|| !LNPNavData::EncodeNavigation(RebuiltBase, ActualBase, OutError) || ExpectedBase != ActualBase)
	{
		OutError = TEXT("Conditional Patch Nav bake changed base nodes or edges.");
		return false;
	}
	OutPatch.Navigation.Agent = BaseNavigation.Agent;
	OutPatch.Navigation.LocalStaticComponentCount = CombinedNavigation.LocalStaticComponentCount - BaseNavigation.LocalStaticComponentCount;
	for (int32 Index = BaseLayers; Index < CombinedNavigation.Layers.Num(); ++Index)
	{
		FLNPNavLayer& Layer = OutPatch.Navigation.Layers.Add_GetRef(CombinedNavigation.Layers[Index]);
		Layer.LocalNavLayerId = OutPatch.FirstSupportLayer + Index - BaseLayers;
		Layer.LocalSupportLayerId = Layer.LocalNavLayerId;
		for (FLNPNavTile& Tile : Layer.Tiles)
		{
			for (FLNPNavCell& Cell : Tile.Cells)
			{
				if (Cell.LocalStaticComponentId < BaseNavigation.LocalStaticComponentCount)
				{
					OutError = TEXT("Conditional Patch component ID overlaps base components.");
					return false;
				}
				Cell.LocalStaticComponentId -= BaseNavigation.LocalStaticComponentCount;
			}
		}
	}
	for (const FLNPNavPortal& Portal : CombinedTraversal.Portals)
	{
		if (Portal.A.LocalNavLayerId < BaseLayers && Portal.B.LocalNavLayerId < BaseLayers) { continue; }
		FLNPConditionalPatchEdge& Edge = OutPatch.WalkEdges.AddDefaulted_GetRef();
		Edge = {Portal.A, Portal.B};
		for (FLNPLocalNavNodeRef* Node : {&Edge.A, &Edge.B})
		{
			if (Node->LocalNavLayerId >= BaseLayers)
			{
				Node->LocalNavLayerId += OutPatch.FirstSupportLayer - BaseLayers;
			}
		}
	}
	Canonicalize(OutPatch);
	return LNPNavData::ValidateNavigation(OutPatch.Navigation, OutError);
}

bool LNPConditionalPatch::Validate(const FLNPConditionalPatchData& Data,
	const FLNPSupportAtlas& BaseSupport, const FLNPNavData& BaseNavigation, FString& OutError)
{
	auto Fail = [&](const TCHAR* Error) { OutError = Error; return false; };
	if (BaseSupport.Layers.IsEmpty() || Data.Patches.Num() >= MAX_uint16)
	{
		return Fail(TEXT("Invalid Conditional Patch base data or patch count."));
	}
	int32 NextLayer = BaseSupport.Layers.Num();
	const int32 CrustN = BaseSupport.Layers[0].Layout.Subdivisions;
	for (int32 PatchIndex = 0; PatchIndex < Data.Patches.Num(); ++PatchIndex)
	{
		const FLNPConditionalPatch& Patch = Data.Patches[PatchIndex];
		if (!Patch.MarkerId.IsValid() || (PatchIndex > 0 && !MarkerLess(Data.Patches[PatchIndex - 1].MarkerId, Patch.MarkerId))
			|| static_cast<uint8>(Patch.Condition) > static_cast<uint8>(ELNPConditionalPatchCondition::Bridge)
			|| Patch.FirstSupportLayer != NextLayer || NextLayer + Patch.SupportLayers.Num() >= MAX_uint16
			|| Patch.Navigation.Layers.Num() != Patch.SupportLayers.Num()
			|| !SamePatchAgent(Patch.Navigation.Agent, BaseNavigation.Agent)
			|| !LNPNavData::ValidateNavigation(Patch.Navigation, OutError))
		{
			return Fail(TEXT("Invalid Conditional Patch marker, Layer IDs or Navigation."));
		}
		for (int32 LayerIndex = 0; LayerIndex < Patch.SupportLayers.Num(); ++LayerIndex)
		{
			const FLNPSupportAtlasLayer& Layer = Patch.SupportLayers[LayerIndex];
			const FLNPSupportLayout& Layout = Layer.Layout;
			if (CrustN < 1 || Layout.Subdivisions < 1 || Layout.Subdivisions % CrustN != 0
				|| Layout.J0 < 0 || Layout.Rows.Num() > Layout.Subdivisions + 1 - Layout.J0
				|| !(Layer.BaseRadius > 0.0) || !FMath::IsFinite(Layer.BaseRadius)
				|| !(Layer.RadiusStep > 0.0) || !FMath::IsFinite(Layer.RadiusStep)
				|| Patch.Navigation.Layers[LayerIndex].LocalSupportLayerId != NextLayer + LayerIndex)
			{
				return Fail(TEXT("Invalid Conditional Patch Support Layer metadata."));
			}
			int64 Samples = 0;
			for (int32 RowIndex = 0; RowIndex < Layout.Rows.Num(); ++RowIndex)
			{
				const FLNPSupportRowSpan& Row = Layout.Rows[RowIndex];
				if (Row.IStart < 0 || Row.Count < 0 || static_cast<int64>(Row.IStart) + Row.Count
					> static_cast<int64>(Layout.Subdivisions) - Layout.J0 - RowIndex + 1)
				{
					return Fail(TEXT("Conditional Patch row leaves the octant grid."));
				}
				Samples += Row.Count;
			}
			if (Samples == 0 || Samples != Layer.Flags.Num() || Samples != Layer.RadiusQ.Num()
				|| Samples * 2 != Layer.NormalQ.Num())
			{
				return Fail(TEXT("Conditional Patch Support arrays do not match the layout."));
			}
			for (int32 Sample = 0; Sample < Layer.Num(); ++Sample)
			{
				const uint8 Flags = Layer.Flags[Sample];
				if ((Flags & ~7u) != 0 || ((Flags & 2u) != 0 && (Flags & 1u) == 0)
					|| ((Flags & 1u) != 0 && !(Layer.GetRadius(Sample) > 0.0)))
				{
					return Fail(TEXT("Invalid Conditional Patch Support sample."));
				}
			}
		}
		for (int32 Index = 0; Index < Patch.FaceMaps.Num(); ++Index)
		{
			const FLNPConditionalPatchFaceMap& Map = Patch.FaceMaps[Index];
			if (Index > 0 && Patch.FaceMaps[Index - 1].SourceIndex >= Map.SourceIndex)
			{
				return Fail(TEXT("Conditional Patch source IDs must be unique and sorted."));
			}
			auto ValidLayer = [&](uint16 Layer) { return Layer == LNPSupportLayers::NoLayer
				|| (Layer >= NextLayer && Layer < NextLayer + Patch.SupportLayers.Num()); };
			if (Map.FaceMap.IsUniform() ? !ValidLayer(Map.FaceMap.UniformLayer)
				: Map.FaceMap.LayerByExternalFace.ContainsByPredicate([&](uint16 Layer) { return !ValidLayer(Layer); }))
			{
				return Fail(TEXT("Conditional Patch face map refers outside its own Layers."));
			}
		}
		for (const FLNPSupportAtlasLayer& Layer : Patch.SupportLayers)
		{
			if (!Patch.FaceMaps.ContainsByPredicate([&](const FLNPConditionalPatchFaceMap& Map) { return Map.SourceIndex == Layer.SourceIndex; }))
			{
				return Fail(TEXT("Conditional Patch Layer has no source face map."));
			}
		}
		for (int32 Index = 0; Index < Patch.BlockedNodes.Num(); ++Index)
		{
			if (!LNPNavData::ResolveLocalNode(BaseNavigation, Patch.BlockedNodes[Index])
				|| (Index > 0 && !NodeLess(Patch.BlockedNodes[Index - 1], Patch.BlockedNodes[Index])))
			{
				return Fail(TEXT("Invalid or duplicate Conditional Patch blocked node."));
			}
		}
		auto ValidNode = [&](const FLNPLocalNavNodeRef& Node) { return LNPNavData::ResolveLocalNode(BaseNavigation, Node)
			|| LNPNavData::ResolveLocalNode(Patch.Navigation, Node); };
		for (const TArray<FLNPConditionalPatchEdge>* Edges : {&Patch.BlockedEdges, &Patch.WalkEdges})
		{
			for (int32 Index = 0; Index < Edges->Num(); ++Index)
			{
				const FLNPConditionalPatchEdge& Edge = (*Edges)[Index];
				const bool bBlocked = Edges == &Patch.BlockedEdges;
				if (!ValidNode(Edge.A) || !ValidNode(Edge.B) || !NodeLess(Edge.A, Edge.B)
					|| (Index > 0 && !EdgeLess((*Edges)[Index - 1], Edge))
					|| (bBlocked ? (!LNPNavData::ResolveLocalNode(BaseNavigation, Edge.A)
						|| !LNPNavData::ResolveLocalNode(BaseNavigation, Edge.B))
						: (Edge.A.LocalNavLayerId < BaseSupport.Layers.Num() && Edge.B.LocalNavLayerId < BaseSupport.Layers.Num())))
				{
					return Fail(TEXT("Invalid Conditional Patch edge endpoint or ordering."));
				}
			}
		}
		NextLayer += Patch.SupportLayers.Num();
	}
	return true;
}

bool LNPConditionalPatch::Encode(const FLNPConditionalPatchData& Data,
	const FLNPSupportAtlas& BaseSupport, const FLNPNavData& BaseNavigation,
	TArray<uint8>& OutPayload, FString& OutError)
{
	OutPayload.Reset();
	if (!Validate(Data, BaseSupport, BaseNavigation, OutError)) { return false; }
	FMemoryWriter Writer(OutPayload);
	uint16 Version = CodecVersion;
	uint16 Count = static_cast<uint16>(Data.Patches.Num());
	Writer << Version << Count;
	for (const FLNPConditionalPatch& Original : Data.Patches)
	{
		FLNPConditionalPatch Patch = Original;
		uint8 Condition = static_cast<uint8>(Patch.Condition);
		Writer << Patch.MarkerId << Condition << Patch.FirstSupportLayer;
		SerializeArray(Writer, Patch.SupportLayers, 44, SerializeLayer);
		SerializeArray(Writer, Patch.FaceMaps, 8, [](FArchive& Ar, FLNPConditionalPatchFaceMap& Map)
		{
			Ar << Map.SourceIndex << Map.FaceMap.UniformLayer;
			SerializeArray(Ar, Map.FaceMap.LayerByExternalFace, 2, [](FArchive& A, uint16& Layer) { A << Layer; });
		});
		TArray<uint8> NavPayload;
		if (!LNPNavData::EncodeNavigation(Patch.Navigation, NavPayload, OutError)) { OutPayload.Reset(); return false; }
		SerializeArray(Writer, NavPayload, 1, [](FArchive& A, uint8& Byte) { A << Byte; });
		SerializeArray(Writer, Patch.BlockedNodes, 6, SerializePatchNode);
		for (TArray<FLNPConditionalPatchEdge>* Edges : {&Patch.BlockedEdges, &Patch.WalkEdges})
		{
			SerializeArray(Writer, *Edges, 12, [](FArchive& A, FLNPConditionalPatchEdge& Edge)
			{
				SerializePatchNode(A, Edge.A); SerializePatchNode(A, Edge.B);
			});
		}
	}
	return !Writer.IsError();
}

bool LNPConditionalPatch::Decode(TConstArrayView<uint8> Payload,
	const FLNPSupportAtlas& BaseSupport, const FLNPNavData& BaseNavigation,
	FLNPConditionalPatchData& OutData, FString& OutError)
{
	OutData = FLNPConditionalPatchData();
	auto Fail = [&](const TCHAR* Error) { OutError = Error; OutData = FLNPConditionalPatchData(); return false; };
	if (Payload.Num() < 4) { return Fail(TEXT("Truncated Conditional Patch header.")); }
	FMemoryReaderView Reader(Payload);
	uint16 Version = 0, Count = 0;
	Reader << Version << Count;
	if (Version != CodecVersion || Count > (Payload.Num() - 4) / 43)
	{
		return Fail(TEXT("Invalid Conditional Patch codec version or count."));
	}
	OutData.Patches.SetNum(Count);
	for (FLNPConditionalPatch& Patch : OutData.Patches)
	{
		uint8 Condition = 0;
		Reader << Patch.MarkerId << Condition << Patch.FirstSupportLayer;
		Patch.Condition = static_cast<ELNPConditionalPatchCondition>(Condition);
		SerializeArray(Reader, Patch.SupportLayers, 44, SerializeLayer);
		SerializeArray(Reader, Patch.FaceMaps, 8, [](FArchive& Ar, FLNPConditionalPatchFaceMap& Map)
		{
			Ar << Map.SourceIndex << Map.FaceMap.UniformLayer;
			SerializeArray(Ar, Map.FaceMap.LayerByExternalFace, 2, [](FArchive& A, uint16& Layer) { A << Layer; });
		});
		TArray<uint8> NavPayload;
		SerializeArray(Reader, NavPayload, 1, [](FArchive& A, uint8& Byte) { A << Byte; });
		if (Reader.IsError()) { return Fail(TEXT("Truncated Conditional Patch Support or Navigation.")); }
		if (!LNPNavData::DecodeNavigation(NavPayload, Patch.Navigation, OutError)) { OutData = FLNPConditionalPatchData(); return false; }
		SerializeArray(Reader, Patch.BlockedNodes, 6, SerializePatchNode);
		for (TArray<FLNPConditionalPatchEdge>* Edges : {&Patch.BlockedEdges, &Patch.WalkEdges})
		{
			SerializeArray(Reader, *Edges, 12, [](FArchive& A, FLNPConditionalPatchEdge& Edge)
			{
				SerializePatchNode(A, Edge.A); SerializePatchNode(A, Edge.B);
			});
		}
		if (Reader.IsError()) { return Fail(TEXT("Truncated Conditional Patch node or edge records.")); }
	}
	if (Reader.Tell() != Reader.TotalSize()) { return Fail(TEXT("Conditional Patch payload has trailing bytes.")); }
	if (!Validate(OutData, BaseSupport, BaseNavigation, OutError)) { OutData = FLNPConditionalPatchData(); return false; }
	for (FLNPConditionalPatch& Patch : OutData.Patches)
	{
		for (FLNPSupportAtlasLayer& Layer : Patch.SupportLayers) { Layer.Layout.BuildOffsets(); }
	}
	return true;
}
