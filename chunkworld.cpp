#include "chunkworld.hpp"

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Container/HashSet.h>
#include <Urho3D/Graphics/Technique.h>
#include <Urho3D/Graphics/Texture2D.h>
#include <Urho3D/Resource/ResourceCache.h>

#include <stdexcept>

namespace BigWorld
{

ChunkWorld::ChunkWorld(Urho3D::Context* context, unsigned chunk_width, float sqr_width, float heightstep, unsigned terrain_texture_repeats) :
Urho3D::Object(context),
chunk_width(chunk_width),
sqr_width(sqr_width),
heightstep(heightstep),
terrain_texture_repeats(terrain_texture_repeats),
scene(NULL),
origin(0, 0),
origin_height(0),
view_distance_in_chunks(8),
viewarea_recalculation_required(true)
{
	SubscribeToEvent(Urho3D::E_BEGINFRAME, URHO3D_HANDLER(ChunkWorld, handleBeginFrame));
}

void ChunkWorld::addTerrainTexture(Urho3D::String const& name)
{
	texs_names.Push(name);
}

void ChunkWorld::setScene(Urho3D::Scene* scene)
{
	assert(va.Empty());
	this->scene = scene;
}

void ChunkWorld::addChunk(Urho3D::IntVector2 const& chunk_pos, Chunk* chunk)
{
	if (chunks.Contains(chunk_pos)) {
		throw std::runtime_error("Chunk at that position already exists!");
	}

	chunks[chunk_pos] = chunk;

	viewarea_recalculation_required = true;
}

void ChunkWorld::extractCornersData(Corners& result, Urho3D::IntVector2 const& pos)
{
	// Get required chunks
	Chunk* chk = chunks[pos];
	Chunk* chk_s = chunks[pos + Urho3D::IntVector2(0, -1)];
	Chunk* chk_se = chunks[pos + Urho3D::IntVector2(1, -1)];
	Chunk* chk_e = chunks[pos + Urho3D::IntVector2(1, 0)];
	Chunk* chk_ne = chunks[pos + Urho3D::IntVector2(1, 1)];
	Chunk* chk_n = chunks[pos + Urho3D::IntVector2(0, 1)];
	Chunk* chk_nw = chunks[pos + Urho3D::IntVector2(-1, 1)];
	Chunk* chk_w = chunks[pos + Urho3D::IntVector2(-1, 0)];

	// One extra for position data, and two more
	// to calculate neighbor positions for normal.
	unsigned result_w = chunk_width + 3;

	// Prepare result
	assert(result.Empty());
	result.Reserve(result_w * result_w);

	// South edge
	// Southwest corner, never used
	result.Push(Corner());
	// South edge
	chk_s->copyCornerRow(result, 0, chunk_width - 1, chunk_width);
	// Southweast corner
	chk_se->copyCornerRow(result, 0, chunk_width - 1, 2);

	// Middle row
	for (unsigned y = 0; y < chunk_width; ++ y) {
		// West part
		chk_w->copyCornerRow(result, chunk_width - 1, y, 1);
		// Middle part
		chk->copyCornerRow(result, 0, y, chunk_width);
		// East part
		chk_e->copyCornerRow(result, 0, y, 2);
	}

	// Two northern rows
	for (unsigned y = 0; y < 2; ++ y) {
		// Northwest corner
		chk_nw->copyCornerRow(result, chunk_width - 1, y, 1);
		// North edge
		chk_n->copyCornerRow(result, 0, y, chunk_width);
		// Northeast corner
		chk_ne->copyCornerRow(result, 0, y, 2);
	}

	assert(result.Size() == result_w * result_w);
}

Urho3D::Material* ChunkWorld::getSingleLayerTerrainMaterial(uint8_t ttype)
{
	if (mats_cache.Contains(ttype)) {
		return mats_cache[ttype];
	}

	Urho3D::ResourceCache* resources = GetSubsystem<Urho3D::ResourceCache>();

	// Check if Texture is loaded. If not, make sure it is being loaded
	Urho3D::String const& tex_name = texs_names[ttype];
	Urho3D::SharedPtr<Urho3D::Texture2D> tex(resources->GetExistingResource<Urho3D::Texture2D>(tex_name));
	if (tex.Null()) {
		resources->BackgroundLoadResource<Urho3D::Texture2D>(tex_name);
		return NULL;
	}

	// Texture is loaded, so create new Material
	Urho3D::Technique* tech = resources->GetResource<Urho3D::Technique>("Techniques/Diff.xml");
	Urho3D::SharedPtr<Urho3D::Material> mat(new Urho3D::Material(context_));
	mat->SetTechnique(0, tech);
	mat->SetTexture(Urho3D::TU_DIFFUSE, tex);

	// Store to cache
	mats_cache[ttype] = mat;

	return mat;
}

void ChunkWorld::handleBeginFrame(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData)
{
	(void)eventType;
	(void)eventData;

	// If there is new viewarea being applied, then check if everything is ready
	if (!va_being_built.Empty()) {
		bool everything_ready = true;
		for (ViewArea::Iterator i = va_being_built.Begin(); i != va_being_built.End(); ++ i) {
			Urho3D::IntVector2 pos = i->first_;
			ChunkLod lod = i->second_;
			Chunk* chunk = chunks[pos];

			if (!chunk->prepareForLod(lod, pos)) {
				everything_ready = false;
			}
		}

		// If everything is ready, then ask all Chunks to switch to
		// new lod and then mark the viewarea update as complete.
		if (everything_ready) {
			// Some chunks might disappear from view. Because of
			// this, keep track of all that are currently visible.
			Urho3D::HashSet<Urho3D::IntVector2> old_chunks;
			for (ViewArea::Iterator i = va.Begin(); i != va.End(); ++ i) {
				old_chunks.Insert(i->first_);
			}

			// Reveal chunks
			for (ViewArea::Iterator i = va_being_built.Begin(); i != va_being_built.End(); ++ i) {
				Urho3D::IntVector2 const& pos = i->first_;
				ChunkLod lod = i->second_;
				Chunk* chunk = chunks[pos];

				chunk->show(scene, pos - va_being_built_origin, va_being_built_origin_height, lod);

				old_chunks.Erase(pos);
			}

			// Hide old chunks
			for (Urho3D::HashSet<Urho3D::IntVector2>::Iterator i = old_chunks.Begin(); i != old_chunks.End(); ++ i) {
				Urho3D::IntVector2 const& pos = *i;
				Chunk* chunk = chunks[pos];
				chunk->hide();
			}

			// Mark process complete
			va = va_being_built;
			origin = va_being_built_origin;
			origin_height = va_being_built_origin_height;
			view_distance_in_chunks = va_being_built_view_distance_in_chunks;
			va_being_built.Clear();
		}
	}

	if (!viewarea_recalculation_required) {
		return;
	}

	// Viewarea requires recalculation. Form new Viewarea object.
	va_being_built.Clear();
// TODO: Get the following two values from camera or something...
	va_being_built_origin = origin;
	va_being_built_origin_height = origin_height;
	va_being_built_view_distance_in_chunks = view_distance_in_chunks;

	// Go viewarea through
	Urho3D::IntVector2 it;
	for (it.y_ = -view_distance_in_chunks; it.y_ <= int(view_distance_in_chunks); ++ it.y_) {
		for (it.x_ = -view_distance_in_chunks; it.x_ <= int(view_distance_in_chunks); ++ it.x_) {
			// If too far away
			if (it.Length() > view_distance_in_chunks) {
				continue;
			}

			Urho3D::IntVector2 pos = origin + it;

			// If Chunk or any of it's neighbors (except southwestern) is missing, then skip this
			if (!chunks.Contains(pos) ||
			    !chunks.Contains(pos + Urho3D::IntVector2(-1, 0)) ||
			    !chunks.Contains(pos + Urho3D::IntVector2(-1, 1)) ||
			    !chunks.Contains(pos + Urho3D::IntVector2(0, 1)) ||
			    !chunks.Contains(pos + Urho3D::IntVector2(1, 1)) ||
			    !chunks.Contains(pos + Urho3D::IntVector2(1, 0)) ||
			    !chunks.Contains(pos + Urho3D::IntVector2(1, -1)) ||
			    !chunks.Contains(pos + Urho3D::IntVector2(0, -1))) {
				continue;
			}

			// Add to future ViewArea object
			ChunkLod lod(CL_CENTER, 0);
			va_being_built[pos] = lod;
		}
	}

	viewarea_recalculation_required = false;
}

}
