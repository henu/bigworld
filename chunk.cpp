#include "chunk.hpp"

#include "chunkworld.hpp"

namespace BigWorld
{

Chunk::Chunk() :
Urho3D::Object(NULL)
{
}

Chunk::Chunk(ChunkWorld* world) :
Urho3D::Object(world->GetContext()),
world(world)
{
	// Fill chunk with default corners
	unsigned area = world->getChunkWidth() * world->getChunkWidth();
	Corner default_corner;
	default_corner.height = 0;
	default_corner.ttypes[0] = 1;
	corners.Resize(area, default_corner);
}

}
