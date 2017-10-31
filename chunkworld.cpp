#include "chunkworld.hpp"

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/Profiler.h>
#include <Urho3D/Container/HashSet.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Technique.h>
#include <Urho3D/Graphics/Texture2D.h>
#include <Urho3D/Resource/ResourceCache.h>

#include <stdexcept>

namespace BigWorld
{

ChunkWorld::ChunkWorld(Urho3D::Context* context, unsigned chunk_width, float sqr_width, float heightstep, unsigned terrain_texture_repeats, bool headless) :
Urho3D::Object(context),
chunk_width(chunk_width),
sqr_width(sqr_width),
heightstep(heightstep),
terrain_texture_repeats(terrain_texture_repeats),
headless(headless),
water_refl(false),
water_baseheight(0),
water_height(0),
water_node(NULL),
origin(0, 0),
origin_height(0),
viewarea_recalculation_required(false)
{
	scene = new Urho3D::Scene(context);
	scene->CreateComponent<Urho3D::Octree>();

	if (!headless) {
		SubscribeToEvent(Urho3D::E_BEGINFRAME, URHO3D_HANDLER(ChunkWorld, handleBeginFrame));
	}
}

void ChunkWorld::addTerrainTexture(Urho3D::String const& name)
{
	texs_names.Push(name);
}

Camera* ChunkWorld::setUpCamera(Urho3D::IntVector2 const& chunk_pos, unsigned baseheight, Urho3D::Vector3 const& pos, float yaw, float pitch, float roll, unsigned viewdistance_in_chunks)
{
	if (camera.NotNull()) {
		throw std::runtime_error("Camera can be set up only once!");
	}

	camera = new Camera(this, chunk_pos, baseheight, pos, yaw, pitch, roll, viewdistance_in_chunks);
	camera->updateNodeTransform();

	viewarea_recalculation_required = true;

	return camera;
}

void ChunkWorld::setUpWaterReflection(unsigned baseheight, float height, Urho3D::Material* water_material, float water_plane_width, unsigned water_viewmask)
{
	if (water_refl) {
		throw std::runtime_error("Water reflection can be set up only once!");
	}
	if (!camera) {
		throw std::runtime_error("Camera must be set up before water reflection can be created!");
	}

	unsigned const REFL_TEX_SIZE = 1024;

	Urho3D::ResourceCache* resources = GetSubsystem<Urho3D::ResourceCache>();

	water_refl = true;
	water_baseheight = baseheight;
	water_height = height;

	// Water plane
	water_node = scene->CreateChild("Water");
	water_node->SetScale(Urho3D::Vector3(water_plane_width / 2.0f, 1, water_plane_width / 2.0f));
	water_node->SetPosition(Urho3D::Vector3(0, 0, 0));
	Urho3D::StaticModel* water = water_node->CreateComponent<Urho3D::StaticModel>();
	water->SetModel(resources->GetResource<Urho3D::Model>("Models/Plane.mdl"));
	water->SetMaterial(water_material);
	// Use viewmask to hide water from reflection camera
	water->SetViewMask(water_viewmask);
// TODO: What about water plane from under the water?

	// Create camera for water reflection
	// It will have the same farclip and position as the main viewport camera, but uses a reflection plane to modify
	// its position when rendering
	water_refl_camera = camera->createWaterReflectionCamera();
	water_refl_camera->SetViewMask(0xffffffff ^ water_viewmask); // Use viewmask to hide water plane
	water_refl_camera->SetAutoAspectRatio(false);
	water_refl_camera->SetUseReflection(true);
	water_refl_camera->SetUseClipping(true); // Enable clipping of geometry behind water plane
	water_refl_camera->SetViewOverrideFlags(Urho3D::VO_DISABLE_SHADOWS);

	// Create a texture and setup viewport for water reflection. Assign the
	// reflection texture to the diffuse texture unit of the water material
	Urho3D::SharedPtr<Urho3D::Texture2D> refl_render_tex(new Urho3D::Texture2D(context_));
	refl_render_tex->SetSize(REFL_TEX_SIZE, REFL_TEX_SIZE, Urho3D::Graphics::GetRGBFormat(), Urho3D::TEXTURE_RENDERTARGET);
	refl_render_tex->SetFilterMode(Urho3D::FILTER_BILINEAR);
	Urho3D::RenderSurface* surface = refl_render_tex->GetRenderSurface();
	Urho3D::SharedPtr<Urho3D::Viewport> refl_viewport(new Urho3D::Viewport(context_, scene, water_refl_camera));
	surface->SetViewport(0, refl_viewport);
	water_material->SetTexture(Urho3D::TU_DIFFUSE, refl_render_tex);

	updateWaterReflection();
}

float ChunkWorld::getHeightFloat(Urho3D::IntVector2 const& chunk_pos, Urho3D::Vector2 const& pos, unsigned baseheight) const
{
	Chunks::ConstIterator chunk_find = chunks.Find(chunk_pos);
	Chunks::ConstIterator chunk_e_find = chunks.Find(chunk_pos + Urho3D::IntVector2(1, 0));
	Chunks::ConstIterator chunk_ne_find = chunks.Find(chunk_pos + Urho3D::IntVector2(1, 1));
	Chunks::ConstIterator chunk_n_find = chunks.Find(chunk_pos + Urho3D::IntVector2(0, 1));
	if (chunk_find == chunks.End() || chunk_e_find == chunks.End() || chunk_ne_find == chunks.End() || chunk_n_find == chunks.End()) {
		throw std::runtime_error("Unable to get height becaue some of required four chunks is missing!");
	}

	Chunk const* chunk = chunk_find->second_;
	Chunk const* chunk_e = chunk_e_find->second_;
	Chunk const* chunk_ne = chunk_ne_find->second_;
	Chunk const* chunk_n = chunk_n_find->second_;

	// Convert to squares
	float pos_x_moved = pos.x_ + chunk_width * sqr_width * 0.5;
	float pos_y_moved = pos.y_ + chunk_width * sqr_width * 0.5;
	unsigned pos_i_x = Urho3D::Clamp<int>(Urho3D::FloorToInt(pos_x_moved / sqr_width), 0, chunk_width - 1);
	unsigned pos_i_y = Urho3D::Clamp<int>(Urho3D::FloorToInt(pos_y_moved / sqr_width), 0, chunk_width - 1);
	float pos_f_x = Urho3D::Clamp<float>(pos_x_moved / sqr_width - pos_i_x, 0, 1);
	float pos_f_y = Urho3D::Clamp<float>(pos_y_moved / sqr_width - pos_i_y, 0, 1);

	// Find heights of corners that surround the position
	int h_sw = chunk->getHeight(pos_i_x, pos_i_y, chunk_width);
	int h_se, h_ne, h_nw;
	if (pos_i_x < chunk_width - 1) {
		h_se = chunk->getHeight(pos_i_x + 1, pos_i_y, chunk_width);
		if (pos_i_y < chunk_width - 1) {
			h_ne = chunk->getHeight(pos_i_x + 1, pos_i_y + 1, chunk_width);
		} else {
			h_ne = chunk_n->getHeight(pos_i_x + 1, 0, chunk_width);
		}
	} else {
		h_se = chunk_e->getHeight(0, pos_i_y, chunk_width);
		if (pos_i_y < chunk_width - 1) {
			h_ne = chunk_e->getHeight(0, pos_i_y + 1, chunk_width);
		} else {
			h_ne = chunk_ne->getHeight(0, 0, chunk_width);
		}
	}
	if (pos_i_y < chunk_width - 1) {
		h_nw = chunk->getHeight(pos_i_x, pos_i_y + 1, chunk_width);
	} else {
		h_nw = chunk_n->getHeight(pos_i_x, 0, chunk_width);
	}

	// Apply baseheight
	h_sw -= baseheight;
	h_se -= baseheight;
	h_ne -= baseheight;
	h_nw -= baseheight;

	// Convert to floats
	float h_sw_f = h_sw * heightstep;
	float h_se_f = h_se * heightstep;
	float h_ne_f = h_ne * heightstep;
	float h_nw_f = h_nw * heightstep;

// TODO: In future, consider how square is really divided into two triangles
return Urho3D::Lerp(Urho3D::Lerp(h_sw_f, h_nw_f, pos_f_y), Urho3D::Lerp(h_se_f, h_ne_f, pos_f_y), pos_f_x);
}

void ChunkWorld::addChunk(Urho3D::IntVector2 const& chunk_pos, Chunk* chunk)
{
	assert(chunk);
	if (chunks.Contains(chunk_pos)) {
		throw std::runtime_error("Chunk at that position already exists!");
	}

	chunks[chunk_pos] = chunk;

	viewarea_recalculation_required = true;
}

void ChunkWorld::removeChunk(Urho3D::IntVector2 const& chunk_pos)
{
	URHO3D_PROFILE(ChunkWorldRemoveChunk);

	Chunks::Iterator chunks_find = chunks.Find(chunk_pos);
	if (chunks_find == chunks.End()) {
		throw std::runtime_error("There is no chunk to remove at that position!");
	}
	chunks_find->second_->removeFromWorld();
	chunks.Erase(chunks_find);

	viewarea_recalculation_required = true;

	// It might not be possible toi build the viewarea anymore, as one chunk might be missing.
	va_being_built.Clear();
}

Chunk* ChunkWorld::getChunk(Urho3D::IntVector2 const& chunk_pos)
{
	Chunks::Iterator chunks_find = chunks.Find(chunk_pos);
	if (chunks_find != chunks.End()) {
		return chunks_find->second_;
	}
	return NULL;
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
	URHO3D_PROFILE(ManageChunkWorldBuilding);

	(void)eventType;
	(void)eventData;

	// If there is new viewarea being applied, then check if everything is ready
	if (!va_being_built.Empty()) {
		URHO3D_PROFILE(CheckIfViewareaIsReady);

		// Sometimes preparing takes lots of time. Use timer to
		// stop preparations if too much time is being spent.
		Urho3D::Time timer(context_);
		float preparation_started = timer.GetElapsedTime();

		bool everything_ready = true;
		for (ViewArea::Iterator i = va_being_built.Begin(); i != va_being_built.End(); ++ i) {
			Urho3D::IntVector2 pos = i->first_;
			uint8_t lod = i->second_;
			assert(chunks.Contains(pos));
			Chunk* chunk = chunks[pos];

			if (!chunk->prepareForLod(lod, pos)) {
				everything_ready = false;
			}

			if (timer.GetElapsedTime() - preparation_started > 1.0 / 120) {
				everything_ready = false;
				break;
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
				uint8_t lod = i->second_;
				Chunk* chunk = chunks[pos];

				chunk->show(pos - va_being_built_origin, va_being_built_origin_height, lod);

				old_chunks.Erase(pos);
			}

			// Hide old chunks
			for (Urho3D::HashSet<Urho3D::IntVector2>::Iterator i = old_chunks.Begin(); i != old_chunks.End(); ++ i) {
				Urho3D::IntVector2 const& pos = *i;
				Chunks::Iterator chunks_find = chunks.Find(pos);
				if (chunks_find != chunks.End()) {
					chunks_find->second_->hide();
				}
			}

			// Mark process complete
			va = va_being_built;
			origin = va_being_built_origin;
			origin_height = va_being_built_origin_height;
			va_being_built.Clear();

			camera->updateNodeTransform();
		}
	}

	// If there is no camera, then do nothing
	if (camera.Null()) {
		return;
	}

	if (water_refl) {
		updateWaterReflection();
	}

	// Check if camera has moved away from origin
	if (camera->fixIfOutsideOrigin()) {
		viewarea_recalculation_required = true;
	}

	if (!viewarea_recalculation_required) {
		return;
	}

	{
		URHO3D_PROFILE(FinishViewareaRebuilding);

		// Viewarea requires recalculation. Form new Viewarea object.
		va_being_built.Clear();
		va_being_built_origin = camera->getChunkPosition();
		va_being_built_origin_height = camera->getBaseHeight();
		va_being_built_view_distance_in_chunks = camera->getViewDistanceInChunks();

		// Go viewarea through
		Urho3D::IntVector2 it;
		for (it.y_ = -va_being_built_view_distance_in_chunks; it.y_ <= int(va_being_built_view_distance_in_chunks); ++ it.y_) {
			for (it.x_ = -va_being_built_view_distance_in_chunks; it.x_ <= int(va_being_built_view_distance_in_chunks); ++ it.x_) {
				// If too far away
				float distance = it.Length();
				if (distance > va_being_built_view_distance_in_chunks) {
					continue;
				}

				Urho3D::IntVector2 pos = va_being_built_origin + it;

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
				unsigned lod_detail = distance / 12;
				va_being_built[pos] = lod_detail;
			}
		}

		viewarea_recalculation_required = false;

	}
}

void ChunkWorld::updateWaterReflection()
{
	// Update water node position
	int baseheight = int(water_baseheight) - int(origin_height);
	float height = water_height + baseheight * heightstep;
	water_node->SetPosition(Urho3D::Vector3(0, height, 0));

	// Create a mathematical plane to represent the water in calculations
	Urho3D::Plane water_refl_plane = Urho3D::Plane(water_node->GetWorldRotation() * Urho3D::Vector3::UP, water_node->GetWorldPosition());
	// Create a downward biased plane for reflection view clipping. Biasing is necessary to avoid too aggressive clipping
	Urho3D::Plane water_clip_plane = Urho3D::Plane(water_node->GetWorldRotation() * Urho3D::Vector3::UP, water_node->GetWorldPosition() + Urho3D::Vector3::DOWN);

	water_refl_camera->SetReflectionPlane(water_refl_plane);
	water_refl_camera->SetClipPlane(water_clip_plane);

	// The water reflection texture is rectangular. Set reflection camera aspect ratio to match
	water_refl_camera->SetAspectRatio(float(GetSubsystem<Urho3D::Graphics>()->GetWidth()) / float(GetSubsystem<Urho3D::Graphics>()->GetHeight()));
}

}
