#ifndef BIGWORLD_CHUNK_HPP
#define BIGWORLD_CHUNK_HPP

#include "types.hpp"

#include <Urho3D/Container/HashMap.h>
#include <Urho3D/Core/Object.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/StaticModel.h>

namespace BigWorld
{
class ChunkWorld;

class Chunk : public Urho3D::Object
{
	URHO3D_OBJECT(Chunk, Urho3D::Object)

public:

	Chunk();
	Chunk(ChunkWorld* world);
	Chunk(ChunkWorld* world, Corners const& corners);

	inline unsigned getBaseheight() const { return baseheight; }

	inline bool hasLod(ChunkLod const& lod) const { return lods.Contains(lod); }

	void setLod(ChunkLod const& lod, Urho3D::Model* model, Urho3D::Material* mat);

	// Shows/hides Chunks
	void show(Urho3D::Scene* scene, Urho3D::IntVector2 const& rel_pos, unsigned rel_height, ChunkLod lod);
	void hide();

	void copyCornerRow(Corners& result, unsigned x, unsigned y, unsigned size);

private:

	struct Lod
	{
		Urho3D::SharedPtr<Urho3D::Model> model;
		Urho3D::SharedPtr<Urho3D::Material> mat;
	};

	typedef Urho3D::HashMap<ChunkLod, Lod> Lods;

	ChunkWorld* world;

	Corners corners;

	unsigned baseheight;

	Lods lods;

	// Scene Node and Model, if currently visible
	Urho3D::Node* node;
	Urho3D::SharedPtr<Urho3D::StaticModel> visible_model;
};

}

#endif
