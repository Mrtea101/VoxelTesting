

#pragma once

#include "CoreMinimal.h"


enum NodeType : int8
{
	Interior,		// Contains materials, intersections and reference to 8 children
	Homogeneous,	// All materials are the same (contains 1 material)
	Heterogeneous,	// At maximum depth and intersects the surface (contains data about materials and intersections)
	Null
};

struct NodeData     //20 bytes (18 + 2 padding)
{
	NodeType Type;
	int8 Depth;
	uint8 Materials[8];

	// for interior nodes
	int32 ChildrenDataKey;

	// for heterogeneous & interior nodes
	int32 IntersectionsDataKey;

	NodeData(NodeType InType, int8 InDepth)
	{
		Type = InType;
		Depth = InDepth;
		for (int i = 0; i < 8; i++) Materials[i] = 255;
		ChildrenDataKey = -1;
		IntersectionsDataKey = -1;
	}
};

struct ChildrenData     // 32 bytes
{
	int32 ChildrenKeys[8];

	FORCEINLINE int32& operator[](int32 Index)
	{
		return ChildrenKeys[Index];
	}

	FORCEINLINE const int32& operator[](int32 Index) const
	{
		return ChildrenKeys[Index];
	}
};

struct NodeIntersections        // 12 bytes
{
	int8 IntersectionValues[12];  // 12 edges, one intersection value per edge

	FORCEINLINE int8& operator[](int32 Index)
	{
		return IntersectionValues[Index];
	}

	FORCEINLINE const int8& operator[](int32 Index) const
	{
		return IntersectionValues[Index];
	}
};