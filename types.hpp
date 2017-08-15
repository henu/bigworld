#ifndef BIGWORLD_TYPES_HPP
#define BIGWORLD_TYPES_HPP

#include <Urho3D/Container/HashMap.h>
#include <Urho3D/Container/Vector.h>
#include <Urho3D/Graphics/VertexBuffer.h>
#include <Urho3D/IO/Deserializer.h>
#include <Urho3D/IO/Serializer.h>
#include <Urho3D/Math/Vector2.h>
#include <Urho3D/Math/BoundingBox.h>
#include <Urho3D/Resource/Image.h>

#include <cstdint>

namespace BigWorld
{

struct ChunkPosAndLod
{
	Urho3D::IntVector2 pos;
	uint8_t lod;

	inline ChunkPosAndLod()
	{
	}

	inline ChunkPosAndLod(Urho3D::IntVector2 const& pos, uint8_t lod) :
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
		return pos.ToHash() * 31 + lod;
	}
};

typedef Urho3D::HashMap<Urho3D::IntVector2, uint8_t> ViewArea;
typedef Urho3D::HashMap<uint8_t, float> TTypesByWeight;
typedef Urho3D::PODVector<uint8_t> TTypes;

struct Corner
{
	uint16_t height;
	TTypesByWeight ttypes;

	inline Corner() {}

	inline Corner(Urho3D::Deserializer& src)
	{
		height = src.ReadUShort();
		unsigned char ttypes_size = src.ReadUByte();
		for (unsigned i = 0; i < ttypes_size; ++ i) {
			uint8_t ttype = src.ReadUByte();
			float weight = src.ReadUByte() / 255.0;
			ttypes[ttype] = weight;
		}
	}

	inline bool write(Urho3D::Serializer& dest) const
	{
		if (!dest.WriteUShort(height)) return false;
		if (!dest.WriteUByte(ttypes.Size())) return false;
		for (TTypesByWeight::ConstIterator i = ttypes.Begin(); i != ttypes.End(); ++ i) {
			if (!dest.WriteUByte(i->first_)) return false;
			if (!dest.WriteUByte(Urho3D::Clamp<int>(i->second_ * 255.0 + 0.5, 0, 255))) return false;
		}
		return true;
	}
};
typedef Urho3D::Vector<Corner> Corners;

struct LodBuildingTaskData : public Urho3D::RefCounted
{
	// Input
	Urho3D::Context* context;
	uint8_t lod;
	Corners corners;
	unsigned baseheight;
	bool calculate_ttype_image;
	// World options
	unsigned chunk_width;
	float sqr_width;
	float heightstep;
	unsigned terrain_texture_repeats;
	// Output
	Urho3D::PODVector<char> vrts_data;
	Urho3D::PODVector<Urho3D::VertexElement> vrts_elems;
	Urho3D::PODVector<uint32_t> idxs_data;
	Urho3D::BoundingBox boundingbox;
	// Outout if ttype image is calculated
	TTypes used_ttypes;
	Urho3D::SharedPtr<Urho3D::Image> ttype_image;
};

}

#endif
