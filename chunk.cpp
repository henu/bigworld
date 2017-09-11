#include "chunk.hpp"

#include "chunkworld.hpp"
#include "lodbuilder.hpp"

#include <Urho3D/Core/Profiler.h>
#include <Urho3D/Graphics/Geometry.h>
#include <Urho3D/Graphics/IndexBuffer.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Technique.h>
#include <Urho3D/Graphics/Texture2D.h>
#include <Urho3D/Graphics/VertexBuffer.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/Scene.h>

#include <stdexcept>

namespace BigWorld
{

Chunk::Chunk(ChunkWorld* world, Urho3D::IntVector2 const& pos, Corners& corners) :
Urho3D::Object(world->GetContext()),
world(world),
pos(pos)
{
	if (corners.Size() != world->getChunkWidth() * world->getChunkWidth()) {
		throw std::runtime_error("Array of corners has invalid size!");
	}

	// Fast way to "copy" corners
	this->corners.Swap(corners);

	// Use average height as baseheight. Also check validity of corners
	unsigned long average_height = 0;
	for (Corners::ConstIterator it = this->corners.Begin(); it != this->corners.End(); ++ it) {
		average_height += it->height;
		if (it->ttypes.empty()) {
			throw std::runtime_error("Every corner of Chunk must have at least one terraintype!");
		}
	}
	average_height /= this->corners.Size();
	baseheight = average_height;

	node = world->getScene()->CreateChild();
	node->SetDeepEnabled(false);
}

Chunk::~Chunk()
{
	// If there is a preparation task, make sure it is completed.
	if (task_workitem.NotNull()) {
		// Action is required only if task is not yet ready
		if (!task_workitem->completed_) {
			// First try to simply remove task
			Urho3D::WorkQueue* workqueue = GetSubsystem<Urho3D::WorkQueue>();
			if (!workqueue->RemoveWorkItem(task_workitem)) {
				// Removing did not work. Let's just wait until workitem is ready
				while (!task_workitem->completed_) {
					// Wait, wait...
				}
			}
		}
	}
}

bool Chunk::write(Urho3D::Serializer& dest) const
{
	return writeWithoutObject(dest, corners);
}

bool Chunk::writeWithoutObject(Urho3D::Serializer& dest, Corners const& corners)
{
	for (Corners::ConstIterator i = corners.Begin(); i != corners.End(); ++ i) {
		Corner const& c = *i;
		if (!c.write(dest)) return false;
	}
	return true;
}

bool Chunk::prepareForLod(uint8_t lod, Urho3D::IntVector2 const& pos)
{
	// Preparation is ready when LOD can be found from loadcache
	if (lodcache.Contains(lod)) {
		return true;
	}

	Urho3D::WorkQueue* workqueue = GetSubsystem<Urho3D::WorkQueue>();

	// If there is an existing task
	if (task_workitem.NotNull()) {
		// If the task is building this LOD, then check if it's ready
		if (task_lod == lod) {
			// If not ready, then keep waiting
			if (!task_workitem->completed_) {
				return false;
			}

			// Try to use task results. Sometimes this needs to be called
			// multiple times because of texture and other resource loading.
			if (!storeTaskResultsToLodCache()) {
				return false;
			}

			task_workitem = NULL;
			task_data = NULL;
			task_mat = NULL;

			return true;
		}
		// If the task is building different LOD, then try remove it.
		else {
			// If already complete, then removing is easy
			if (task_workitem->completed_) {
				task_workitem = NULL;
				task_data = NULL;
				task_mat = NULL;
			}
			// Try to stop task
			else if (workqueue->RemoveWorkItem(task_workitem)) {
				task_workitem = NULL;
				task_data = NULL;
				task_mat = NULL;
			}
			// Removing old task was not possible, so try again later
			else {
				return false;
			}
		}
	}

	// There is no task running at background, so start one.
	task_lod = lod;
	// Get and set data
	task_data = new LodBuildingTaskData;
	task_data->context = context_;
	task_data->lod = lod;
	task_data->chunk_width = world->getChunkWidth();
	task_data->sqr_width = world->getSquareWidth();
	task_data->heightstep = world->getHeightstep();
	task_data->terrain_texture_repeats = world->getTerrainTextureRepeats();
	task_data->baseheight = baseheight;
	task_data->calculate_ttype_image = matcache.Null();
	world->extractCornersData(task_data->corners, pos);
	// Set up workitem
	task_workitem = new Urho3D::WorkItem();
	task_workitem->workFunction_ = buildLod;
	task_workitem->aux_ = task_data;
	// Store possible existing material, so setting
	// matcache to NULL does not cause problems.
	task_mat = matcache;

	// Start task
	workqueue->AddWorkItem(task_workitem);

	return false;
}

void Chunk::show(Urho3D::IntVector2 const& rel_pos, unsigned origin_height, uint8_t lod)
{
	assert(lodcache.Contains(lod));
	assert(!matcache.Null());

	node->SetPosition(Urho3D::Vector3(
		rel_pos.x_ * world->getChunkWidthFloat(),
		(int(baseheight) - int(origin_height)) * world->getHeightstep(),
		rel_pos.y_ * world->getChunkWidthFloat()
	));

	// If there is no active static model, then one needs to be created
	if (!active_model) {
		active_model = node->CreateComponent<Urho3D::StaticModel>();
		active_model->SetModel(lodcache[lod]);
		active_model->SetMaterial(matcache);
	}
	// If there is active static model, but it has different properties
	else if (active_model->GetModel() != lodcache[lod] || active_model->GetMaterial() != matcache) {
		active_model->SetModel(lodcache[lod]);
		active_model->SetMaterial(matcache);
	}

	node->SetDeepEnabled(true);
}

void Chunk::hide()
{
	// Remove active model. If the model stays up
	// to date, it can be still found from cache.
	if (active_model) {
		node->RemoveComponent(active_model);
		active_model = NULL;
	}

	node->SetDeepEnabled(false);
}

void Chunk::removeFromWorld(void)
{
	URHO3D_PROFILE(ChunkRemoveFromWorld);
	node->Remove();
	world = NULL;
	lodcache.Clear();
	matcache = NULL;
	node = NULL;
}

Urho3D::Node* Chunk::createChildNode()
{
	Urho3D::Node* child = node->CreateChild();
	child->SetEnabled(node->IsEnabled());
	return child;
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

bool Chunk::storeTaskResultsToLodCache()
{
	Urho3D::ResourceCache* resources = GetSubsystem<Urho3D::ResourceCache>();

	// Before constructing the Model, make sure material is loaded.
	Urho3D::SharedPtr<Urho3D::Material> mat;
	// First the easiest case, where existing material can be used.
	if (!task_data->calculate_ttype_image) {
		assert(task_mat.NotNull());
		mat = task_mat;
	}
	// Then second easiest case, where only one terraintype is used
	else if (task_data->used_ttypes.Size() == 1) {
		// These simple materials are stored to cache in
		// ChunkWorld. Try to get the material from there.
		mat = world->getSingleLayerTerrainMaterial(task_data->used_ttypes[0]);
		if (mat.Null()) {
			return false;
		}
	}
	// Most complex case. Material with multiple terraintypes
	else {
		// First make sure all textures are loaded
		Urho3D::Vector<Urho3D::SharedPtr<Urho3D::Texture2D> > texs;
		for (unsigned i = 0; i < task_data->used_ttypes.Size(); ++ i) {
			uint8_t ttype = task_data->used_ttypes[i];
			Urho3D::String const& tex_name = world->getTerrainTextureName(ttype);
			Urho3D::SharedPtr<Urho3D::Texture2D> tex(resources->GetExistingResource<Urho3D::Texture2D>(tex_name));
			if (tex.Null()) {
				// Texture was not loaded, so start loading it.
				resources->BackgroundLoadResource<Urho3D::Texture2D>(tex_name);
			} else {
				texs.Push(tex);
			}
		}
		// If some textures are missing, then give up for now
		if (texs.Size() != task_data->used_ttypes.Size()) {
			return false;
		}
		// All textures are ready. Construct new material.
		Urho3D::SharedPtr<Urho3D::Texture2D> blend_tex;
		mat = new Urho3D::Material(context_);
		if (texs.Size() == 4) {
			Urho3D::Technique* tech = resources->GetResource<Urho3D::Technique>("Techniques/TerrainBlend4.xml");
			mat->SetTechnique(0, tech);
		} else {
			Urho3D::Technique* tech = resources->GetResource<Urho3D::Technique>("Techniques/TerrainBlend.xml");
			mat->SetTechnique(0, tech);
		}
		mat->SetShaderParameter("DetailTiling", Urho3D::Variant(Urho3D::Vector2::ONE * world->getTerrainTextureRepeats()));
		blend_tex = new Urho3D::Texture2D(context_);
		blend_tex->SetAddressMode(Urho3D::COORD_U, Urho3D::ADDRESS_CLAMP);
		blend_tex->SetAddressMode(Urho3D::COORD_V, Urho3D::ADDRESS_CLAMP);
		assert(task_data->ttype_image.NotNull());
		blend_tex->SetData(task_data->ttype_image);
		mat->SetTexture(Urho3D::TU_DIFFUSE, blend_tex);
		for (unsigned layer = 0; layer < texs.Size(); ++ layer) {
			mat->SetTexture((Urho3D::TextureUnit)(layer + 1), texs[layer]);
		}
	}

	// Material is ready. Now construct model.
	// Convert raw data from task to real VertexBuffer
	Urho3D::SharedPtr<Urho3D::VertexBuffer> new_vb(new Urho3D::VertexBuffer(context_));
	if (!new_vb->SetSize(task_data->vrts_data.Size() / Urho3D::VertexBuffer::GetVertexSize(task_data->vrts_elems), task_data->vrts_elems)) {
		throw std::runtime_error("Unable to set VertexBuffer size!");
	}
	if (!new_vb->SetData((void*)task_data->vrts_data.Buffer())) {
		throw std::runtime_error("Unable to set VertexBuffer data!");
	}

	// Convert raw data from task to real IndexBuffer
	Urho3D::SharedPtr<Urho3D::IndexBuffer> new_ib(new Urho3D::IndexBuffer(context_));
	if (!new_ib->SetSize(task_data->idxs_data.Size(), true)) {
		throw std::runtime_error("Unable to set IndexBuffer size!");
	}
	if (!new_ib->SetData((void*)task_data->idxs_data.Buffer())) {
		throw std::runtime_error("Unable to set IndexBuffer data!");
	}

	// Create new geometry
	Urho3D::SharedPtr<Urho3D::Geometry> new_geom(new Urho3D::Geometry(context_));
	if (!new_geom->SetVertexBuffer(0, new_vb)) {
		throw std::runtime_error("Unable to set Geometry VertexBuffer!");
	}
	new_geom->SetIndexBuffer(new_ib);
	if (!new_geom->SetDrawRange(Urho3D::TRIANGLE_LIST, 0, task_data->idxs_data.Size(), false)) {
		throw std::runtime_error("Unable to set Geometry draw range!");
	}

	// Create model the data from task
	Urho3D::SharedPtr<Urho3D::Model> new_model(new Urho3D::Model(context_));
	new_model->SetNumGeometries(1);
	if (!new_model->SetNumGeometryLodLevels(0, 1)) {
		throw std::runtime_error("Unable to set number of lod levels of Model!");
	}
	if (!new_model->SetGeometry(0, 0, new_geom)) {
		throw std::runtime_error("Unable to set Model Geometry!");
	}
	new_model->SetBoundingBox(task_data->boundingbox);

	// Store model and material to cache
	lodcache[task_lod] = new_model;
	matcache = mat;

	// If cache grows too big, remove some elements from it.
	unsigned const LODCACHE_MAX_SIZE = 2;
	if (lodcache.Size() > LODCACHE_MAX_SIZE) {
		unsigned remove = rand() % (lodcache.Size() - 1);
		LodCache::Iterator it = lodcache.Begin();
		while (true) {
			// Skip current lod
			if (it->first_ == task_lod) {
				continue;
			}
			if (remove == 0) {
				lodcache.Erase(it);
				break;
			}
			-- remove;
		}
	}

	return true;
}

}
