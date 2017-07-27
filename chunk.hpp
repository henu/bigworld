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

	Chunk(ChunkWorld* world);
	Chunk(ChunkWorld* world, Corners const& corners);
	virtual ~Chunk();

	// Starts preparing Chunk to be rendered with specific LOD. Should be called
	// multiple times until returns true to indicate that preparations are ready.
	bool prepareForLod(ChunkLod const& lod, Urho3D::IntVector2 const& pos);

	inline bool hasLod(ChunkLod const& lod) const { return lodcache.Contains(lod); }

	// Shows/hides Chunks
	void show(Urho3D::IntVector2 const& rel_pos, unsigned origin_height, ChunkLod lod);
	void hide();

	Urho3D::Node* createChildNode();

	inline unsigned getBaseHeight() const { return baseheight; }

	void copyCornerRow(Corners& result, unsigned x, unsigned y, unsigned size);

private:

	typedef Urho3D::HashMap<ChunkLod, Urho3D::SharedPtr<Urho3D::Model> > LodCache;

	ChunkWorld* world;

	Corners corners;

	unsigned baseheight;

	// Cache of Models and material. These can be
	// cleared when data in corners change.
	LodCache lodcache;
	Urho3D::SharedPtr<Urho3D::Material> matcache;

	// Scene Node, Model and LOD, if currently visible
	Urho3D::Node* node;
	Urho3D::SharedPtr<Urho3D::StaticModel> active_model;

	// Task for building LODs at background. "task_workitem"
	// tells if task is executed by being NULL or not NULL.
	Urho3D::SharedPtr<Urho3D::WorkItem> task_workitem;
	Urho3D::SharedPtr<LodBuildingTaskData> task_data;
	ChunkLod task_lod;
	Urho3D::SharedPtr<Urho3D::Material> task_mat;

	// Return true if all task results were used succesfully.
	bool storeTaskResultsToLodCache();
};

}

#endif
