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

class TTypesByWeight
{

public:

	inline TTypesByWeight() :
	buf(NULL),
	buf_size(0)
	{
	}

	inline TTypesByWeight(TTypesByWeight const& ttypes) :
	buf_size(ttypes.buf_size)
	{
		if (buf_size) {
			buf = new uint8_t[buf_size];
			memcpy(buf, ttypes.buf, buf_size);
		}
	}

	inline TTypesByWeight& operator=(TTypesByWeight const& ttypes)
	{
		if (buf_size != ttypes.buf_size) {
			buf_size = ttypes.buf_size;
			delete[] buf;
			if (buf_size) {
				buf = new uint8_t[buf_size];
			} else {
				buf = NULL;
			}
		}
		if (buf_size) {
			memcpy(buf, ttypes.buf, buf_size);
		}
		return *this;
	}

	inline ~TTypesByWeight()
	{
		if (buf_size > 0) {
			delete[] buf;
		}
	}

	inline void rawFill(Urho3D::Deserializer& src, uint8_t size)
	{
		buf_size = size * 2;
		buf = new uint8_t[buf_size];
		src.Read(buf, buf_size);
	}

	inline void initRawFill(uint8_t size)
	{
		if (size) {
			buf = new uint8_t[size * 2];
		} else {
			buf = NULL;
		}
		buf_size = 0;
	}

	inline void rawFillByte(uint8_t key, uint8_t val)
	{
		buf[buf_size ++] = key;
		buf[buf_size ++] = val;
	}

	inline void set(uint8_t key, float val)
	{
		uint8_t byte_val = Urho3D::Clamp<int>(val * 255.0 + 0.5, 0, 255);
		for (unsigned i = 0; i < buf_size; i += 2) {
			if (buf[i] == key) {
				// Default case
				if (byte_val > 0) {
					buf[i + 1] = byte_val;
				}
				// Special case: Setting to zero means clear
				else {
					if (buf_size == 2) {
						delete[] buf;
						buf = NULL;
						buf_size = 0;
					} else {
						uint8_t* new_buf = new uint8_t[buf_size - 2];
						memcpy(new_buf, buf, i);
						memcpy(new_buf + i, buf + i + 2, buf_size - 2 - i);
						delete[] buf;
						buf = new_buf;
						buf_size -= 2;
					}
				}
				return;
			}
		}
		// Special case: Do nothing if zero
		if (byte_val == 0) {
			return;
		}
		uint8_t* new_buf = new uint8_t[buf_size + 2];
		memcpy(new_buf, buf, buf_size);
		delete[] buf;
		buf = new_buf;
		new_buf[buf_size ++] = key;
		new_buf[buf_size ++] = byte_val;
	}

	inline float operator[](uint8_t key) const
	{
		for (unsigned i = 0; i < buf_size; i += 2) {
			if (buf[i] == key) {
				return buf[i + 1] / 255.0;
			}
		}
		return 0;
	}

	inline uint8_t size() const
	{
		return buf_size / 2;
	}

	inline bool empty() const
	{
		return buf_size == 0;
	}

	inline uint8_t getKey(uint8_t idx) const
	{
		return buf[idx * 2];
	}

	inline float getValue(uint8_t idx) const
	{
		return getValueByte(idx) / 255.0;
	}

	inline uint8_t getValueByte(uint8_t idx) const
	{
		return buf[idx * 2 + 1];
	}

private:

	uint8_t* buf;
	uint8_t buf_size;
};

typedef Urho3D::HashMap<Urho3D::IntVector2, uint8_t> ViewArea;
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
		ttypes.rawFill(src, ttypes_size);
	}

	inline bool write(Urho3D::Serializer& dest) const
	{
		if (!dest.WriteUShort(height)) return false;
		if (!dest.WriteUByte(ttypes.size())) return false;
		for (unsigned i = 0; i < ttypes.size(); ++ i) {
			if (!dest.WriteUByte(ttypes.getKey(i))) return false;
			if (!dest.WriteUByte(ttypes.getValueByte(i))) return false;
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
