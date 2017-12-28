#include "chunk.hpp"

#include "chunkworld.hpp"
#include "lodbuilder.hpp"
#include "../urhoextras/random.hpp"
#include "../urhoextras/utils.hpp"

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
pos(pos),
undergrowth_state(UGSTATE_NOT_INITIALIZED),
undergrowth_node(NULL)
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

	updateLowestHeight();
}

Chunk::~Chunk()
{
	destroyUndergrowth();

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

void Chunk::moveChildNodeFrom(Urho3D::Node* child)
{
	child->SetParent(node);
	child->SetEnabled(node->IsEnabled());
}

void Chunk::copyCornerRow(Corners& result, unsigned x, unsigned y, unsigned size) const
{
	unsigned chunk_w = world->getChunkWidth();
	assert(size <= chunk_w - x);
	assert(y < chunk_w);
	unsigned ofs = y * chunk_w + x;
	assert(ofs + size <= corners.Size());
	result.Insert(result.End(), corners.Begin() + ofs, corners.Begin() + ofs + size);
}

void Chunk::getTriangles(UrhoExtras::Triangle& tri1, UrhoExtras::Triangle& tri2,
                         unsigned x, unsigned y,
                         Chunk const* ngb_n, Chunk const* ngb_ne, Chunk const* ngb_e) const
{
	unsigned const CHUNK_WIDTH = world->getChunkWidth();
	float const SQUARE_WIDTH = world->getSquareWidth();
	float const HEIGHTSTEP = world->getHeightstep();
	Urho3D::Vector3 const CHUNK_SIZE_HALF(CHUNK_WIDTH * SQUARE_WIDTH / 2, 0, CHUNK_WIDTH * SQUARE_WIDTH / 2);

	int h_sw = getHeight(x, y, CHUNK_WIDTH, NULL, NULL, NULL);
	int h_nw = getHeight(x, y + 1, CHUNK_WIDTH, ngb_n, NULL, NULL);
	int h_ne = getHeight(x + 1, y + 1, CHUNK_WIDTH, ngb_n, ngb_ne, ngb_e);
	int h_se = getHeight(x + 1, y, CHUNK_WIDTH, NULL, NULL, ngb_e);
	h_sw -= baseheight;
	h_nw -= baseheight;
	h_ne -= baseheight;
	h_se -= baseheight;

	float h_f_sw = h_sw * HEIGHTSTEP;
	float h_f_nw = h_nw * HEIGHTSTEP;
	float h_f_ne = h_ne * HEIGHTSTEP;
	float h_f_se = h_se * HEIGHTSTEP;

	Urho3D::Vector3 pos_sw(x * SQUARE_WIDTH, h_f_sw, y * SQUARE_WIDTH);
	Urho3D::Vector3 pos_nw(x * SQUARE_WIDTH, h_f_nw, (y + 1) * SQUARE_WIDTH);
	Urho3D::Vector3 pos_ne((x + 1) * SQUARE_WIDTH, h_f_ne, (y + 1) * SQUARE_WIDTH);
	Urho3D::Vector3 pos_se((x + 1) * SQUARE_WIDTH, h_f_se, y * SQUARE_WIDTH);
	pos_sw -= CHUNK_SIZE_HALF;
	pos_nw -= CHUNK_SIZE_HALF;
	pos_ne -= CHUNK_SIZE_HALF;
	pos_se -= CHUNK_SIZE_HALF;

	if (abs(h_sw - h_ne) < abs(h_se - h_nw)) {
		tri1.p1 = pos_sw;
		tri1.p2 = pos_ne;
		tri1.p3 = pos_se;
		tri2.p1 = pos_sw;
		tri2.p2 = pos_nw;
		tri2.p3 = pos_ne;
	} else {
		tri1.p1 = pos_sw;
		tri1.p2 = pos_nw;
		tri1.p3 = pos_se;
		tri2.p1 = pos_nw;
		tri2.p2 = pos_ne;
		tri2.p3 = pos_se;
	}
}

bool Chunk::createUndergrowth()
{
	URHO3D_PROFILE(ChunkCreateUndergrowth);

	if (undergrowth_state == UGSTATE_READY) {
		return true;
	}

	if (undergrowth_state == UGSTATE_NOT_INITIALIZED) {
		world->extractCornersData(undergrowth_corners, pos);
		if (undergrowth_corners.Empty()) {
			return false;
		}
		undergrowth_state = UGSTATE_PLACING;
		undergrowth_placer_wi = new Urho3D::WorkItem();
		undergrowth_placer_wi->aux_ = this;
		undergrowth_placer_wi->workFunction_ = undergrowthPlacer;
		Urho3D::WorkQueue* workqueue = GetSubsystem<Urho3D::WorkQueue>();
		workqueue->AddWorkItem(undergrowth_placer_wi);
		return false;
	}

	if (undergrowth_state == UGSTATE_PLACING) {
	    if (!undergrowth_placer_wi->completed_) {
			return false;
		}
		undergrowth_placer_wi = NULL;
		undergrowth_state = UGSTATE_LOADING_RESOURCES;
	}

	if (undergrowth_state == UGSTATE_LOADING_RESOURCES) {
		// Check if all models and materials are ready
		bool resources_missing = false;
		Urho3D::ResourceCache* resources = GetSubsystem<Urho3D::ResourceCache>();
		for (StrNStr model_and_mat : undergrowth_places.Keys()) {
			// Check model
			Urho3D::String const& model_path = model_and_mat.first_;
			if (!resources->GetExistingResource<Urho3D::Model>(model_path)) {
				resources->BackgroundLoadResource<Urho3D::Model>(model_path);
				resources_missing = true;
			}
			// Check material
			Urho3D::String const& mat_path = model_and_mat.second_;
			if (!resources->GetExistingResource<Urho3D::Material>(mat_path)) {
				resources->BackgroundLoadResource<Urho3D::Material>(mat_path);
				resources_missing = true;
			}
		}
		if (resources_missing) {
			return false;
		}

		// Resources are ready and models, positions and rotations are decided.
		// Start combining one Model from them.
		undergrowth_state = UGSTATE_COMBINING;
		undergrowth_node = createChildNode();
		undergrowth_combiner = new UrhoExtras::ModelCombiner(context_);
		for (UndergrowthPlacements::ConstIterator i = undergrowth_places.Begin(); i != undergrowth_places.End(); ++ i) {
			Urho3D::Model* model = resources->GetResource<Urho3D::Model>(i->first_.first_);
			Urho3D::Material* mat = resources->GetResource<Urho3D::Material>(i->first_.second_);
			for (PosNRot const& pos_n_rot : i->second_) {
				undergrowth_combiner->AddModel(model, mat, pos_n_rot.pos, pos_n_rot.rot);
			}
		}

		return false;
	}

	if (undergrowth_state == UGSTATE_COMBINING) {
		if (undergrowth_combiner->Ready()) {
			Urho3D::Model* model = undergrowth_combiner->GetModel();
			if (model) {
				Urho3D::StaticModel* smodel = undergrowth_node->CreateComponent<Urho3D::StaticModel>();
				smodel->SetModel(model);
				for (unsigned geom_i = 0; geom_i < undergrowth_combiner->GetModel()->GetNumGeometries(); ++ geom_i) {
					smodel->SetMaterial(geom_i, undergrowth_combiner->GetMaterial(geom_i));
				}
				smodel->SetCastShadows(false);
				smodel->SetDrawDistance(world->getUndergrowthDrawDistance());
			}
			undergrowth_combiner = NULL;
			undergrowth_places.Clear();
			undergrowth_state = UGSTATE_READY;
			return true;
		}
		return false;
	}

	return false;
}

bool Chunk::destroyUndergrowth()
{
	URHO3D_PROFILE(ChunkDestroyUndergrowth);
	undergrowth_state = UGSTATE_STOP_PLACING;
	if (undergrowth_placer_wi.NotNull()) {
		Urho3D::WorkQueue* workqueue = GetSubsystem<Urho3D::WorkQueue>();
		if (!workqueue->RemoveWorkItem(undergrowth_placer_wi)) {
			while (!undergrowth_placer_wi->completed_) {
			}
		}
		undergrowth_placer_wi = NULL;
	}
	undergrowth_corners.Clear();
	undergrowth_combiner = NULL;
	undergrowth_places.Clear();
	if (undergrowth_node) {
		undergrowth_node->Remove();
		undergrowth_node = NULL;
	}
	undergrowth_state = UGSTATE_NOT_INITIALIZED;
	return true;
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

void Chunk::updateLowestHeight()
{
	lowest_height = corners[0].height;
	for (unsigned i = 1; i < corners.Size(); ++ i) {
		lowest_height = Urho3D::Min(lowest_height, corners[i].height);
	}
}

void Chunk::undergrowthPlacer(Urho3D::WorkItem const* wi, unsigned thread_i)
{
	(void)thread_i;

	Chunk* chunk = (Chunk*)wi->aux_;
	UndergrowthModelsByTerraintype ugmodels = chunk->world->getUndergrowthModelsByTerraintype();

	unsigned const CHUNK_WIDTH = chunk->world->getChunkWidth();
	float const HEIGHTSTEP = chunk->world->getHeightstep();
	float const SQUARE_WIDTH = chunk->world->getSquareWidth();
	float const CHUNK_WIDTH_F_HALF = CHUNK_WIDTH * SQUARE_WIDTH / 2.0;

	for (unsigned y = 0; y < CHUNK_WIDTH; ++ y) {
		unsigned ofs_sw = (y + 1) * (CHUNK_WIDTH + 3) + 1;
		for (unsigned x = 0; x < CHUNK_WIDTH; ++ x) {
			unsigned ofs_nw = ofs_sw + CHUNK_WIDTH + 3;
			unsigned ofs_ne = ofs_nw + 1;
			unsigned ofs_se = ofs_sw + 1;

			// If cancel has been requested
			if (chunk->undergrowth_state == UGSTATE_STOP_PLACING) {
				chunk->undergrowth_places.Clear();
				return;
			}

			// To generate similar results every time, use deterministic random
// TODO: This does not work! Try something else!
/*
			UrhoExtras::Random rnd(ofs_sw);
			rnd.seedMore(chunk->chunk->getPosition().x_);
			rnd.seedMore(chunk->chunk->getPosition().y_);
*/
UrhoExtras::Random rnd(rand());

			// Randomize rotation and position on square
			float yaw_angle = 360 * rnd.randomFloat();
			Urho3D::Vector2 sqr_pos(rnd.randomFloat(), rnd.randomFloat());

			// Get average terraintypes in this square
			BigWorld::TTypesByWeight const& ttypes_sw = chunk->undergrowth_corners[ofs_sw].ttypes;
			BigWorld::TTypesByWeight const& ttypes_nw = chunk->undergrowth_corners[ofs_nw].ttypes;
			BigWorld::TTypesByWeight const& ttypes_ne = chunk->undergrowth_corners[ofs_ne].ttypes;
			BigWorld::TTypesByWeight const& ttypes_se = chunk->undergrowth_corners[ofs_se].ttypes;
			BigWorld::TTypesByWeight ttypes = ttypes_sw.averageOfTwo(ttypes_se).averageOfTwo(ttypes_nw.averageOfTwo(ttypes_ne));

			// Select one of the terrain types randomly
			unsigned ttypes_total_weight = ttypes.getTotalWeight();
			unsigned ttype_selection_weight = rnd.randomUnsigned() % ttypes_total_weight;
			unsigned ttype_selection_idx = 0;
			while (ttype_selection_weight >= ttypes.getValueByte(ttype_selection_idx)) {
				ttype_selection_weight -= ttypes.getValueByte(ttype_selection_idx);
				++ ttype_selection_idx;
				assert(ttype_selection_idx < ttypes.size());
			}
			uint8_t ttype_selection = ttypes.getKey(ttype_selection_idx);

			// Select one of the undergrowth models, based on terraintypes
			UndergrowthModelsByTerraintype::ConstIterator ugs_find = ugmodels.Find(ttype_selection);
			if (ugs_find != ugmodels.End()) {

				UndergrowthModels const& ttype_ugs = ugs_find->second_;
				UndergrowthModel const& ttype_ug = ttype_ugs[rnd.randomUnsigned() % ttype_ugs.Size()];

				// Decide position and rotation
				float c_sw = (int(chunk->undergrowth_corners[ofs_sw].height) - int(chunk->baseheight)) * HEIGHTSTEP;
				float c_nw = (int(chunk->undergrowth_corners[ofs_nw].height) - int(chunk->baseheight)) * HEIGHTSTEP;
				float c_ne = (int(chunk->undergrowth_corners[ofs_ne].height) - int(chunk->baseheight)) * HEIGHTSTEP;
				float c_se = (int(chunk->undergrowth_corners[ofs_se].height) - int(chunk->baseheight)) * HEIGHTSTEP;

				float height = chunk->world->getHeightFromCorners(c_sw, c_nw, c_ne, c_se, sqr_pos);

				PosNRot pos_n_rot;
				pos_n_rot.pos = Urho3D::Vector3(
					(x + sqr_pos.x_) * SQUARE_WIDTH - CHUNK_WIDTH_F_HALF,
					height,
					(y + sqr_pos.y_) * SQUARE_WIDTH - CHUNK_WIDTH_F_HALF
				);
				pos_n_rot.rot = Urho3D::Quaternion(yaw_angle, Urho3D::Vector3::UP);
				if (ttype_ug.follow_ground_angle) {
					Urho3D::Vector3 normal = chunk->world->getNormalFromCorners(c_sw, c_nw, c_ne, c_se, sqr_pos);
					Urho3D::Vector2 normal_xz(normal.x_, normal.z_);
					float follow_yaw = UrhoExtras::getAngle(normal_xz);
					float follow_pitch = UrhoExtras::getAngle(normal_xz.Length(), normal.y_);
					pos_n_rot.rot = Urho3D::Quaternion(follow_yaw, Urho3D::Vector3::UP) * Urho3D::Quaternion(follow_pitch, Urho3D::Vector3::RIGHT) * pos_n_rot.rot;
				}
				chunk->undergrowth_places[StrNStr(ttype_ug.model, ttype_ug.material)].Push(pos_n_rot);
			}
			++ ofs_sw;
		}
	}

}

}
