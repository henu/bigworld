#include "chunkworld.hpp"

#include "lodbuilder.hpp"

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Container/HashSet.h>
#include <Urho3D/Graphics/Geometry.h>
#include <Urho3D/Graphics/IndexBuffer.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/VertexBuffer.h>

#include <stdexcept>

namespace BigWorld
{

ChunkWorld::ChunkWorld(Urho3D::Context* context, unsigned chunk_width, float sqr_width, float heightstep) :
Urho3D::Object(context),
chunk_width(chunk_width),
sqr_width(sqr_width),
heightstep(heightstep),
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

void ChunkWorld::handleBeginFrame(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData)
{
	(void)eventType;
	(void)eventData;

	// If there are tasks, check if they have become ready
	for (Tasks::Iterator tasks_it = tasks.Begin(); tasks_it != tasks.End();) {
		Task& task = tasks_it->second_;
		if (task.workitem->completed_) {

			// Convert raw data from task to real VertexBuffer
			Urho3D::SharedPtr<Urho3D::VertexBuffer> new_vb(new Urho3D::VertexBuffer(context_));
			if (!new_vb->SetSize(task.data->vrts_data.Size() / Urho3D::VertexBuffer::GetVertexSize(task.data->vrts_elems), task.data->vrts_elems)) {
				throw std::runtime_error("Unable to set VertexBuffer size!");
			}
			if (!new_vb->SetData((void*)task.data->vrts_data.Buffer())) {
				throw std::runtime_error("Unable to set VertexBuffer data!");
			}

			// Convert raw data from task to real IndexBuffer
			Urho3D::SharedPtr<Urho3D::IndexBuffer> new_ib(new Urho3D::IndexBuffer(context_));
			if (!new_ib->SetSize(task.data->idxs_data.Size(), true)) {
				throw std::runtime_error("Unable to set IndexBuffer size!");
			}
			if (!new_ib->SetData((void*)task.data->idxs_data.Buffer())) {
				throw std::runtime_error("Unable to set IndexBuffer data!");
			}

			// Create new geometry
			Urho3D::SharedPtr<Urho3D::Geometry> new_geom(new Urho3D::Geometry(context_));
			if (!new_geom->SetVertexBuffer(0, new_vb)) {
				throw std::runtime_error("Unable to set Geometry VertexBuffer!");
			}
			new_geom->SetIndexBuffer(new_ib);
			if (!new_geom->SetDrawRange(Urho3D::TRIANGLE_LIST, 0, task.data->idxs_data.Size(), false)) {
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
			new_model->SetBoundingBox(task.data->boundingbox);

			Chunk* chunk = chunks[tasks_it->first_.pos];
			chunk->setLod(task.data->lod, new_model);

			tasks_it = tasks.Erase(tasks_it);
		} else {
			++ tasks_it;
		}
	}

	// If viewarea is being built, and there are no tasks
	// left, then it means viewarea building is ready.
	if (!va_being_built.Empty() && tasks.Empty()) {
		// Go old viewarea through, and hide currently visible Chunks
		for (ViewArea::Iterator va_it = va.Begin(); va_it != va.End(); ++ va_it) {
			Urho3D::IntVector2 const& pos = va_it->first_;
			Chunk* chunk = chunks[pos];
			chunk->hide();
		}

		va = va_being_built;
		origin = va_being_built_origin;
		origin_height = va_being_built_origin_height;
		view_distance_in_chunks = va_being_built_view_distance_in_chunks;

		// Reveal new Chunks
		for (ViewArea::Iterator va_it = va.Begin(); va_it != va.End(); ++ va_it) {
			Urho3D::IntVector2 const& pos = va_it->first_;
			ChunkLod lod = va_it->second_;
			Chunk* chunk = chunks[pos];
			chunk->show(scene, pos - origin, origin_height, lod);
		}

		va_being_built.Clear();
	}

	if (!viewarea_recalculation_required) {
		return;
	}

	Urho3D::WorkQueue* workqueue = GetSubsystem<Urho3D::WorkQueue>();

	// Viewarea requires recalculation. Form new Viewarea object.
	va_being_built.Clear();
// TODO: Get the following two values from camera or something...
	va_being_built_origin = origin;
	va_being_built_origin_height = origin_height;
	va_being_built_view_distance_in_chunks = view_distance_in_chunks;
	// Keep track of tasks that are no longer needed
	Urho3D::HashSet<ChunkPosAndLod> old_tasks;
	for (Tasks::Iterator tasks_it = tasks.Begin(); tasks_it != tasks.End(); ++ tasks_it) {
		old_tasks.Insert(tasks_it->first_);
	}
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

			// Now check if new Task needs to be started. First check the
			// best situation, where Chunk already contains required LOD.
			Chunk* chunk = chunks[pos];
			ChunkPosAndLod pos_and_lod(pos, lod);
			if (chunk->hasLod(lod)) {
				// Nothing needs to be done
			}
			// Then check if task already exists
			else if (tasks.Contains(pos_and_lod)) {
				old_tasks.Erase(pos_and_lod);
			}
			// Finally the worst scenario: We need to start a new task.
			else {
				Task new_task;
				// Get and set data
				new_task.data = new LodBuildingTaskData();
				new_task.data->pos = pos;
				new_task.data->lod = lod;
				new_task.data->chunk_width = chunk_width;
				new_task.data->sqr_width = sqr_width;
				new_task.data->heightstep = heightstep;
				new_task.data->baseheight = chunk->getBaseheight();
				extractCornersData(new_task.data->corners, pos);
				// Set up workitem
				new_task.workitem = new Urho3D::WorkItem();
				new_task.workitem->workFunction_ = buildLod;
				new_task.workitem->aux_ = new_task.data;
				// Start task
				workqueue->AddWorkItem(new_task.workitem);
				// Store
				tasks[pos_and_lod] = new_task;
			}
		}
	}
	// Clean those Tasks that are not needed anymore
	for (Urho3D::HashSet<ChunkPosAndLod>::Iterator old_tasks_it = old_tasks.Begin(); old_tasks_it != old_tasks.End(); ++ old_tasks_it) {
		Task& task = tasks[*old_tasks_it];
		if (workqueue->RemoveWorkItem(task.workitem)) {
			tasks.Erase(*old_tasks_it);
		}
	}

	viewarea_recalculation_required = false;
}

void ChunkWorld::extractCornersData(Corners& result, Urho3D::IntVector2 const& pos)
{
	// Get required chunks
	Chunk* chk = chunks[pos];
	Chunk* chk_s = chunks[pos + Urho3D::IntVector2(0, -1)];
	Chunk* chk_se = chunks[pos + Urho3D::IntVector2(1, -1)];
	Chunk* chk_e = chunks[pos + Urho3D::IntVector2(1, 1)];
	Chunk* chk_ne = chunks[pos + Urho3D::IntVector2(0, 1)];
	Chunk* chk_n = chunks[pos + Urho3D::IntVector2(-1, 1)];
	Chunk* chk_nw = chunks[pos + Urho3D::IntVector2(-1, 0)];
	Chunk* chk_w = chunks[pos + Urho3D::IntVector2(-1, -1)];

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

}
