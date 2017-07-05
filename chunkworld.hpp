#ifndef BIGWORLD_CHUNKWORLD_HPP
#define BIGWORLD_CHUNKWORLD_HPP

#include "chunk.hpp"

#include <Urho3D/Container/HashMap.h>
#include <Urho3D/Container/Ptr.h>
#include <Urho3D/Container/Vector.h>
#include <Urho3D/Math/Vector2.h>

namespace BigWorld
{

class ChunkWorld : public Urho3D::Object
{
	URHO3D_OBJECT(ChunkWorld, Urho3D::Object)

public:

	ChunkWorld(Urho3D::Context* context, unsigned chunk_width, float sqr_width, float heightstep);

	void addTerrainTexture(Urho3D::String const& name);

	inline unsigned getChunkWidth() const { return chunk_width; }

	void addChunk(Urho3D::IntVector2 const& chunk_pos, Chunk* chunk);

private:

	typedef Urho3D::HashMap<Urho3D::IntVector2, Urho3D::SharedPtr<Chunk> > Chunks;

	// World options
	unsigned const chunk_width;
	float const sqr_width;
	float const heightstep;

	Urho3D::Vector<Urho3D::String> texs_names;

	Chunks chunks;
};

}

#endif
