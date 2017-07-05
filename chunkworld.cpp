#include "chunkworld.hpp"

#include <stdexcept>

namespace BigWorld
{

ChunkWorld::ChunkWorld(Urho3D::Context* context, unsigned chunk_width, float sqr_width, float heightstep) :
Urho3D::Object(context),
chunk_width(chunk_width),
sqr_width(sqr_width),
heightstep(heightstep)
{
}

void ChunkWorld::addTerrainTexture(Urho3D::String const& name)
{
	texs_names.Push(name);
}

void ChunkWorld::addChunk(Urho3D::IntVector2 const& chunk_pos, Chunk* chunk)
{
	if (chunks.Contains(chunk_pos)) {
		throw std::runtime_error("Chunk at that position already exists!");
	}

	chunks[chunk_pos] = chunk;
}

}
