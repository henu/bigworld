#include "lodbuilder.hpp"

#include "types.hpp"

namespace BigWorld
{

void buildLod(Urho3D::WorkItem const* item, unsigned threadIndex)
{
	(void)threadIndex;

	LodBuildingTaskData* data = (LodBuildingTaskData*)item->aux_;

// TODO: Support other LODs too!

	// Set up elements
	data->vrts_elems.Push(Urho3D::VertexElement(Urho3D::TYPE_VECTOR3, Urho3D::SEM_POSITION));
	data->vrts_elems.Push(Urho3D::VertexElement(Urho3D::TYPE_VECTOR3, Urho3D::SEM_NORMAL));
	data->vrts_elems.Push(Urho3D::VertexElement(Urho3D::TYPE_VECTOR2, Urho3D::SEM_TEXCOORD));
// TODO: Support terrain data!

	// Precalculate some stuff
	unsigned const CHUNK_W = data->chunk_width;
	unsigned const CHUNK_W1 = data->chunk_width + 1;
	unsigned const CHUNK_W3 = data->chunk_width + 3;
	float const CHUNK_WF = data->chunk_width * data->sqr_width;
	unsigned const V2_SIZE = sizeof(float) * 2;
	unsigned const V3_SIZE = sizeof(float) * 3;
	unsigned const VRT_SIZE = Urho3D::VertexBuffer::GetVertexSize(data->vrts_elems);

	// Create map of positions and calculate boundingbox
	data->boundingbox.Clear();
	Urho3D::PODVector<Urho3D::Vector3> poss;
	unsigned ofs = 0;
	for (unsigned y = 0; y < CHUNK_W3; ++ y) {
		for (unsigned x = 0; x < CHUNK_W3; ++ x) {
			uint16_t height = data->corners[ofs].height;
			Urho3D::Vector3 pos(
				(int(x) - 1) * data->sqr_width - CHUNK_WF / 2,
				(int(height) - int(data->baseheight)) * data->heightstep,
				(int(y) - 1) * data->sqr_width - CHUNK_WF / 2
			);
			poss.Push(pos);

			// Do not include edge positions to boundingbox
			if (x >= 1 && x <= CHUNK_W1 && y >= 1 && y <= CHUNK_W1) {
				data->boundingbox.Merge(pos);
			}

			++ ofs;
		}
	}

	// Create vertex data
	data->vrts_data.Reserve(VRT_SIZE * CHUNK_W1 * CHUNK_W1);
	for (unsigned y = 0; y < CHUNK_W1; ++ y) {
		ofs = 1 + (y + 1) * (CHUNK_W3);
		for (unsigned x = 0; x < CHUNK_W1; ++ x) {
			Urho3D::Vector3 const& pos = poss[ofs];

			// Position
			data->vrts_data.Insert(data->vrts_data.End(), (char*)pos.Data(), (char*)pos.Data() + V3_SIZE);

			// Normal
			Urho3D::Vector3 const& pos_n = poss[ofs + CHUNK_W3];
			Urho3D::Vector3 const& pos_s = poss[ofs - CHUNK_W3];
			Urho3D::Vector3 const& pos_e = poss[ofs + 1];
			Urho3D::Vector3 const& pos_w = poss[ofs - 1];
			Urho3D::Vector3 diff_n = (pos_n - pos).Normalized();
			Urho3D::Vector3 diff_s = (pos_s - pos).Normalized();
			Urho3D::Vector3 diff_e = (pos_e - pos).Normalized();
			Urho3D::Vector3 diff_w = (pos_w - pos).Normalized();
			Urho3D::Vector3 normal = (diff_w.CrossProduct(diff_n) + diff_e.CrossProduct(diff_s)).Normalized();
			assert(normal.y_ > 0);
			data->vrts_data.Insert(data->vrts_data.End(), (char*)normal.Data(), (char*)normal.Data() + V3_SIZE);

			// Texture coordinates
			Urho3D::Vector2 uv(float(x) / CHUNK_W, float(y) / CHUNK_W);
			data->vrts_data.Insert(data->vrts_data.End(), (char*)uv.Data(), (char*)uv.Data() + V2_SIZE);

			++ ofs;
		}
	}
	assert(data->vrts_data.Size() == VRT_SIZE * CHUNK_W1 * CHUNK_W1);

	// Create index data
	data->idxs_data.Reserve(3 * 2 * CHUNK_W * CHUNK_W);
	for (unsigned y = 0; y < CHUNK_W; ++ y) {
		ofs = y * CHUNK_W1;
		for (unsigned x = 0; x < CHUNK_W; ++ x) {

			data->idxs_data.Push(ofs);
			data->idxs_data.Push(ofs + 1 + CHUNK_W1);
			data->idxs_data.Push(ofs + 1);

			data->idxs_data.Push(ofs);
			data->idxs_data.Push(ofs + CHUNK_W1);
			data->idxs_data.Push(ofs + 1 + CHUNK_W1);

			++ ofs;
		}
	}
	assert(data->idxs_data.Size() == 3 *  2 * CHUNK_W * CHUNK_W);
}

}

