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

URHO3D_EVENT(E_VIEWAREA_ORIGIN_CHANGED, ViewareaOriginChanged)
{
}

class ChunkWorld : public Urho3D::Object
{
	URHO3D_OBJECT(ChunkWorld, Urho3D::Object)

public:

	ChunkWorld(
		Urho3D::Context* context,
		unsigned chunk_width,
		float sqr_width,
		float heightstep,
		unsigned terrain_texture_repeats,
		unsigned undergrowth_radius_chunks,
		float undergrowth_draw_distance, bool headless
	);

	void addTerrainTexture(Urho3D::String const& name);
	inline unsigned getNumOfTerrainTextures() const { return texs_names.Size(); }

	void addUndergrowthModel(unsigned terraintype, Urho3D::String const& model, Urho3D::String const& material, bool follow_ground_angle, float min_scale = 1, float max_scale = 1);
	inline UndergrowthModelsByTerraintype getUndergrowthModelsByTerraintype() const { return ugmodels; }

	inline Urho3D::Scene* getScene() const { return scene; }

	// This can be called only once.
	Camera* setUpCamera(Urho3D::IntVector2 const& chunk_pos, unsigned baseheight, Urho3D::Vector3 const& pos, float yaw = 0, float pitch = 0, float roll = 0, unsigned viewdistance_in_chunks = 8);

	void setUpWaterReflection(unsigned baseheight, float height, Urho3D::Material* water_material, float water_plane_width, unsigned water_viewmask = 0x80000000);

	inline unsigned getChunkWidth() const { return chunk_width; }
	inline float getChunkWidthFloat() const { return chunk_width * sqr_width; }
	inline float getSquareWidth() const { return sqr_width; }
	inline float getHeightstep() const { return heightstep; }
	inline unsigned getTerrainTextureRepeats() const { return terrain_texture_repeats; }
	inline float getUndergrowthDrawDistance() const { return undergrowth_draw_distance; }
	inline Urho3D::String getTerrainTextureName(uint8_t ttype) const { return texs_names[ttype]; }

	inline bool isHeadless() const { return headless; }

	float getHeightFloat(Urho3D::IntVector2 const& chunk_pos, Urho3D::Vector2 const& pos, unsigned baseheight) const;

	float getHeightFromCorners(float h_sw, float h_nw, float h_ne, float h_se, Urho3D::Vector2 const& sqr_pos) const;
	Urho3D::Vector3 getNormalFromCorners(float h_sw, float h_nw, float h_ne, float h_se, Urho3D::Vector2 const& sqr_pos) const;

	inline Urho3D::IntVector2 getOrigin() const { return origin; }
	inline unsigned getOriginHeight() const { return origin_height; }

	void addChunk(Urho3D::IntVector2 const& chunk_pos, Chunk* chunk);
	void removeChunk(Urho3D::IntVector2 const& chunk_pos);
	Chunk* getChunk(Urho3D::IntVector2 const& chunk_pos);

	// Returns height and terrain data from a specific chunk and a little bit from its
	// neighbors, so it is possible to know calculate normals and know terraintypes
	// for every corner of every square in the chunk. "result" must be empty. If there
	// is not enough Chunks loaded, then "result" is not touched.
	void extractCornersData(Corners& result, Urho3D::IntVector2 const& pos) const;

	// This is used by Chunks. Returns NULL if Material is not yet ready.
	Urho3D::Material* getSingleLayerTerrainMaterial(uint8_t ttype);

private:

	typedef Urho3D::HashMap<uint8_t, Urho3D::SharedPtr<Urho3D::Material> > SingleLayerMaterialsCache;
	typedef Urho3D::HashMap<Urho3D::IntVector2, Urho3D::SharedPtr<Chunk> > Chunks;
	typedef Urho3D::HashSet<Urho3D::IntVector2> IntVector2Set;

	Urho3D::SharedPtr<Urho3D::Scene> scene;

	// World options
	unsigned const chunk_width;
	float const sqr_width;
	float const heightstep;
	unsigned const terrain_texture_repeats;
	unsigned const undergrowth_radius_chunks; // TODO: Maybe calculate this from the undergrowth draw distance?
	float const undergrowth_draw_distance;
	Urho3D::Vector<Urho3D::String> texs_names;
	UndergrowthModelsByTerraintype ugmodels;

	bool headless;

	SingleLayerMaterialsCache mats_cache;

	Urho3D::SharedPtr<Camera> camera;

	// Water reflection
	bool water_refl;
	unsigned water_baseheight;
	float water_height;
	Urho3D::Node* water_node;
	Urho3D::Camera* water_refl_camera;

	Chunks chunks;

	// Chunks that are waiting for undergrowth to
	// load and Chunks that have undergrowth in them
	IntVector2Set chunks_missing_undergrowth;
	IntVector2Set chunks_having_undergrowth;

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

	void updateWaterReflection();

	void startCreatingUndergrowth();
	void updateUndergrowth();
};

}

#endif
