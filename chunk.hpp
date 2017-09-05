#ifndef BIGWORLD_CHUNK_HPP
#define BIGWORLD_CHUNK_HPP

#include "types.hpp"

#include <Urho3D/Container/HashMap.h>
#include <Urho3D/Core/Object.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/IO/Serializer.h>

namespace BigWorld
{
class ChunkWorld;

class Chunk : public Urho3D::Object
{
	URHO3D_OBJECT(Chunk, Urho3D::Object)

public:

	// Please note, that the content of "corners" will be cleared.
	Chunk(ChunkWorld* world, Urho3D::IntVector2 const& pos, Corners& corners);
	virtual ~Chunk();

	bool write(Urho3D::Serializer& dest) const;

	// Starts preparing Chunk to be rendered with specific LOD. Should be called
	// multiple times until returns true to indicate that preparations are ready.
	bool prepareForLod(uint8_t lod, Urho3D::IntVector2 const& pos);

	inline bool hasLod(uint8_t lod) const { return lodcache.Contains(lod); }

	// Shows/hides Chunks
	void show(Urho3D::IntVector2 const& rel_pos, unsigned origin_height, uint8_t lod);
	void hide();

	// Removes Chunk from World
// TODO: This feels kind of hacky...
	void removeFromWorld(void);

	Urho3D::Node* createChildNode();

	inline Urho3D::IntVector2 getPosition() const { return pos; }

	inline unsigned getBaseHeight() const { return baseheight; }

	inline uint16_t getHeight(unsigned x, unsigned y, unsigned chunk_w) const { return corners[x + y * chunk_w].height; }

	void copyCornerRow(Corners& result, unsigned x, unsigned y, unsigned size);

private:

	typedef Urho3D::HashMap<uint8_t, Urho3D::SharedPtr<Urho3D::Model> > LodCache;

	ChunkWorld* world;
	Urho3D::IntVector2 pos;

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
	uint8_t task_lod;
	Urho3D::SharedPtr<Urho3D::Material> task_mat;

	// Return true if all task results were used succesfully.
	bool storeTaskResultsToLodCache();
};

}

#endif
