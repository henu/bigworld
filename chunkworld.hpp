#ifndef BIGWORLD_CHUNKWORLD_HPP
#define BIGWORLD_CHUNKWORLD_HPP

#include "chunk.hpp"
#include "types.hpp"
#include "camera.hpp"

#include <Urho3D/Container/HashMap.h>
#include <Urho3D/Container/Ptr.h>
#include <Urho3D/Container/Vector.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Math/Vector2.h>

namespace BigWorld
{

class ChunkWorld : public Urho3D::Object
{
	URHO3D_OBJECT(ChunkWorld, Urho3D::Object)

public:

	ChunkWorld(Urho3D::Context* context, unsigned chunk_width, float sqr_width, float heightstep, unsigned terrain_texture_repeats, bool headless);

	void addTerrainTexture(Urho3D::String const& name);

	inline Urho3D::Scene* getScene() const { return scene; }

	// This can be called only once.
	Camera* setUpCamera(Urho3D::IntVector2 const& chunk_pos, unsigned baseheight, Urho3D::Vector3 const& pos, float yaw = 0, float pitch = 0, float roll = 0, unsigned viewdistance_in_chunks = 8);

	inline unsigned getChunkWidth() const { return chunk_width; }
	inline float getChunkWidthFloat() const { return chunk_width * sqr_width; }
	inline float getSquareWidth() const { return sqr_width; }
	inline float getHeightstep() const { return heightstep; }
	inline unsigned getTerrainTextureRepeats() const { return terrain_texture_repeats; }
	inline Urho3D::String getTerrainTextureName(uint8_t ttype) const { return texs_names[ttype]; }

	inline Urho3D::IntVector2 getOrigin() const { return origin; }
	inline unsigned getOriginHeight() const { return origin_height; }

	void addChunk(Urho3D::IntVector2 const& chunk_pos, Chunk* chunk);
	void removeChunk(Urho3D::IntVector2 const& chunk_pos);
	Chunk* getChunk(Urho3D::IntVector2 const& chunk_pos);

	void extractCornersData(Corners& result, Urho3D::IntVector2 const& pos);

	// This is used by Chunks. Returns NULL if Material is not yet ready.
	Urho3D::Material* getSingleLayerTerrainMaterial(uint8_t ttype);

private:

	typedef Urho3D::HashMap<uint8_t, Urho3D::SharedPtr<Urho3D::Material> > SingleLayerMaterialsCache;
	typedef Urho3D::HashMap<Urho3D::IntVector2, Urho3D::SharedPtr<Chunk> > Chunks;

	Urho3D::SharedPtr<Urho3D::Scene> scene;

	// World options
	unsigned const chunk_width;
	float const sqr_width;
	float const heightstep;
	unsigned const terrain_texture_repeats;
	Urho3D::Vector<Urho3D::String> texs_names;

	SingleLayerMaterialsCache mats_cache;

	Urho3D::SharedPtr<Camera> camera;

	Chunks chunks;

	// View details
	ViewArea va;
	Urho3D::IntVector2 origin;
	unsigned origin_height;

	// This is enabled if viewarea changes
	bool viewarea_recalculation_required;

	// These are used when building new viewarea
	ViewArea va_being_built;
	Urho3D::IntVector2 va_being_built_origin;
	unsigned va_being_built_origin_height;
	unsigned va_being_built_view_distance_in_chunks;

	void handleBeginFrame(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);
};

}

#endif
