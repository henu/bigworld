#ifndef BIGWORLD_TYPES_HPP
#define BIGWORLD_TYPES_HPP

#include <Urho3D/Container/HashMap.h>
#include <Urho3D/Container/Vector.h>
#include <Urho3D/Graphics/VertexBuffer.h>
#include <Urho3D/Math/Vector2.h>
#include <Urho3D/Math/BoundingBox.h>

#include <cstdint>

namespace BigWorld
{

enum ChunkLodType {
	CL_CENTER = 0
};

struct ChunkLod
{
	ChunkLodType type;
	uint8_t detail;

	inline ChunkLod() :
	type(CL_CENTER),
	detail(0)
	{
	}

	inline ChunkLod(ChunkLodType type, uint8_t detail) :
	type(type),
	detail(detail)
	{
	}

	inline bool operator==(ChunkLod const& other) const
	{
		return type == other.type && detail == other.detail;
	}

	inline unsigned ToHash() const
	{
		return type * 31 + detail;
	}
};

struct ChunkPosAndLod
{
	Urho3D::IntVector2 pos;
	ChunkLod lod;

	inline ChunkPosAndLod()
	{
	}

	inline ChunkPosAndLod(Urho3D::IntVector2 const& pos, ChunkLod const& lod) :
	pos(pos),
	lod(lod)
	{
	}

	inline bool operator==(ChunkPosAndLod const& other) const
	{
		return pos == other.pos && lod == other.lod;
	}

	inline unsigned ToHash() const
	{
		return pos.ToHash() * 31 * 31 + lod.type * 31 + lod.detail;
	}
};

typedef Urho3D::HashMap<Urho3D::IntVector2, ChunkLod> ViewArea;
typedef Urho3D::HashMap<uint8_t, float> TTypesByWeight;
typedef Urho3D::PODVector<uint8_t> TTypes;

struct HashedTTypes
{
	TTypes ttypes;

	inline bool operator==(HashedTTypes const& other) const
	{
		return ttypes == other.ttypes;
	}

	inline unsigned ToHash() const
	{
		unsigned hash = ttypes.Size();
		for (TTypes::ConstIterator i = ttypes.Begin(); i != ttypes.End(); ++ i) {
			hash *= 31;
			hash += *i;
		}
		return hash;
	}
};

struct Corner
{
	uint16_t height;
	TTypesByWeight ttypes;
};
typedef Urho3D::Vector<Corner> Corners;

struct LodBuildingTaskData : public Urho3D::RefCounted
{
	// Input
	Urho3D::IntVector2 pos;
	ChunkLod lod;
	Corners corners;
	unsigned baseheight;
	// World options
	unsigned chunk_width;
	float sqr_width;
	float heightstep;
	// Output
	Urho3D::PODVector<char> vrts_data;
	Urho3D::PODVector<Urho3D::VertexElement> vrts_elems;
	Urho3D::PODVector<uint32_t> idxs_data;
	Urho3D::BoundingBox boundingbox;
	Urho3D::PODVector<uint8_t> used_ttypes;
};


}

#endif
