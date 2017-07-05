#ifndef BIGWORLD_CHUNK_HPP
#define BIGWORLD_CHUNK_HPP

#include <Urho3D/Core/Object.h>
#include <Urho3D/Container/Vector.h>

#include <cstdint>

namespace BigWorld
{
class ChunkWorld;

class Chunk : public Urho3D::Object
{
	URHO3D_OBJECT(Chunk, Urho3D::Object)

public:

	Chunk();
	Chunk(ChunkWorld* world);

private:

	typedef Urho3D::HashMap<uint8_t, float> TTypesByWeight;

	struct Corner
	{
		uint16_t height;
		TTypesByWeight ttypes;
	};

	ChunkWorld* world;

	Urho3D::Vector<Corner> corners;
};

}

#endif

