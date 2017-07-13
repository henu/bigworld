#include "chunk.hpp"

#include "chunkworld.hpp"

#include <Urho3D/Scene/Scene.h>

#include <stdexcept>

namespace BigWorld
{

Chunk::Chunk() :
Urho3D::Object(NULL),
node(NULL)
{
}

Chunk::Chunk(ChunkWorld* world) :
Urho3D::Object(world->GetContext()),
world(world),
node(NULL)
{
	// Fill chunk with default corners
	unsigned area = world->getChunkWidth() * world->getChunkWidth();
	Corner default_corner;
	default_corner.height = 0;
	default_corner.ttypes[0] = 1;
	corners.Resize(area, default_corner);

	baseheight = 0;
}

Chunk::Chunk(ChunkWorld* world, Corners const& corners) :
Urho3D::Object(world->GetContext()),
world(world),
corners(corners),
node(NULL)
{
	if (corners.Size() != world->getChunkWidth() * world->getChunkWidth()) {
		throw std::runtime_error("Array of corners has invalid size!");
	}
	// Use average height as baseheight. Also check validity of corners
	unsigned long average_height = 0;
	for (Corners::ConstIterator it = corners.Begin(); it != corners.End(); ++ it) {
		average_height += it->height;
		if (it->ttypes.Empty()) {
			throw std::runtime_error("Every corner of Chunk must have at least one terraintype!");
		}
	}
	average_height /= corners.Size();
	baseheight = average_height;
}

void Chunk::setLod(ChunkLod const& lod, Urho3D::Model* model, Urho3D::Material* mat)
{
	Lod new_lod;
	new_lod.model = model;
	new_lod.mat = mat;
	lods[lod] = new_lod;
}

void Chunk::show(Urho3D::Scene* scene, Urho3D::IntVector2 const& rel_pos, unsigned rel_height, ChunkLod lod)
{
	assert(!node);
	assert(lods.Contains(lod));

	node = scene->CreateChild();
	node->SetPosition(Urho3D::Vector3(
	  rel_pos.x_ * int(world->getChunkWidth()) * world->getSquareWidth(),
	  (int(baseheight) - int(rel_height)) * world->getHeightstep(),
	  rel_pos.y_ * int(world->getChunkWidth()) * world->getSquareWidth()
	));

	visible_model = node->CreateComponent<Urho3D::StaticModel>();
	visible_model->SetModel(lods[lod].model);
	visible_model->SetMaterial(lods[lod].mat);
}

void Chunk::hide()
{
	if (node) {
		node->Remove();
		node = NULL;
		visible_model = NULL;
	}
}

void Chunk::copyCornerRow(Corners& result, unsigned x, unsigned y, unsigned size)
{
	unsigned chunk_w = world->getChunkWidth();
	assert(size <= chunk_w - x);
	assert(y < chunk_w);
	unsigned ofs = y * chunk_w + x;
	assert(ofs + size <= corners.Size());
	result.Insert(result.End(), corners.Begin() + ofs, corners.Begin() + ofs + size);
}

}
