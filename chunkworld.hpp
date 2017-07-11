#ifndef BIGWORLD_CHUNKWORLD_HPP
#define BIGWORLD_CHUNKWORLD_HPP

#include "chunk.hpp"
#include "types.hpp"

#include <Urho3D/Container/HashMap.h>
#include <Urho3D/Container/Ptr.h>
#include <Urho3D/Container/Vector.h>
#include <Urho3D/Core/WorkQueue.h>
#include <Urho3D/Math/Vector2.h>

namespace BigWorld
{

class ChunkWorld : public Urho3D::Object
{
	URHO3D_OBJECT(ChunkWorld, Urho3D::Object)

public:

	ChunkWorld(Urho3D::Context* context, unsigned chunk_width, float sqr_width, float heightstep);

	void addTerrainTexture(Urho3D::String const& name);

	void setScene(Urho3D::Scene* scene);

	inline unsigned getChunkWidth() const { return chunk_width; }
	inline float getSquareWidth() const { return sqr_width; }
	inline float getHeightstep() const { return heightstep; }

	void addChunk(Urho3D::IntVector2 const& chunk_pos, Chunk* chunk);

private:

	typedef Urho3D::HashMap<Urho3D::IntVector2, Urho3D::SharedPtr<Chunk> > Chunks;

	struct Task
	{
		Urho3D::SharedPtr<LodBuildingTaskData> data;
		Urho3D::SharedPtr<Urho3D::WorkItem> workitem;
	};
	typedef Urho3D::HashMap<ChunkPosAndLod, Task> Tasks;

	// World options
	unsigned const chunk_width;
	float const sqr_width;
	float const heightstep;
	Urho3D::Vector<Urho3D::String> texs_names;

	Urho3D::Scene* scene;

	Chunks chunks;

	// View details
	ViewArea va;
	Urho3D::IntVector2 origin;
	unsigned origin_height;
	unsigned view_distance_in_chunks;

	// This is enabled if viewarea changes
	bool viewarea_recalculation_required;

	// These are used when building new viewarea
	ViewArea va_being_built;
	Urho3D::IntVector2 va_being_built_origin;
	unsigned va_being_built_origin_height;
	unsigned va_being_built_view_distance_in_chunks;

	Tasks tasks;

	void handleBeginFrame(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);

	void extractCornersData(Corners& result, Urho3D::IntVector2 const& pos);
};

}

#endif
