#include "lodbuilder.hpp"

#include <Urho3D/Container/HashSet.h>

#include "types.hpp"

namespace BigWorld
{

Urho3D::SharedPtr<Urho3D::Image> calculateTerraintypeImage(TTypes& result_used_ttypes, Urho3D::Context* context, Corners const& corners, unsigned chunk_width)
{
	// Precalculate some stuff
	unsigned const CHUNK_W1 = chunk_width + 1;
	unsigned const CHUNK_W3 = chunk_width + 3;

	// Calculate what terrains are used and how much. If there are
	// too many of them, then the rarest ones will be ignored.
	unsigned const MAX_TERRAINTYPES_IN_MATERIAL = 4;
	Urho3D::HashMap<uint8_t, float> used_ttypes;
	for (unsigned y = 0; y < CHUNK_W1; ++ y) {
		unsigned ofs = 1 + (y + 1) * (CHUNK_W3);
		for (unsigned x = 0; x < CHUNK_W1; ++ x) {
			Corner const& corner = corners[ofs];
			for (unsigned ttypes_i = 0; ttypes_i < corner.ttypes.size(); ++ ttypes_i) {
				uint8_t ttype = corner.ttypes.getKey(ttypes_i);
				float weight = corner.ttypes.getValue(ttypes_i);
				if (weight > 0) {
					if (!used_ttypes.Contains(ttype)) {
						used_ttypes[ttype] = 0;
					}
					used_ttypes[ttype] += weight;
				}
			}
			++ ofs;
		}
	}
	// Do the possible ignoring of rarest terraintypes
	while (used_ttypes.Size() > MAX_TERRAINTYPES_IN_MATERIAL) {
		float lowest_usage = 9999999;
		unsigned lowest_usage_ttype = 0;
		for (Urho3D::HashMap<uint8_t, float>::Iterator it = used_ttypes.Begin(); it != used_ttypes.End(); ++ it) {
			if (it->second_ < lowest_usage) {
				lowest_usage = it->second_;
				lowest_usage_ttype = it->first_;
			}
		}
		used_ttypes.Erase(lowest_usage_ttype);
	}
	assert(result_used_ttypes.Empty());
	result_used_ttypes.Reserve(used_ttypes.Size());
	for (Urho3D::HashMap<uint8_t, float>::Iterator i = used_ttypes.Begin(); i != used_ttypes.End(); ++ i) {
		result_used_ttypes.Push(i->first_);
	}
	assert(!result_used_ttypes.Empty());

	// If there is only one terraintype, then image is not needed
	if (result_used_ttypes.Size() == 1) {
		return NULL;
	}

	Urho3D::SharedPtr<Urho3D::Image> img(new Urho3D::Image(context));
// TODO: Consider using POT(Power Of Two) image size!
// TODO: Use variable amount of components!
	if (result_used_ttypes.Size() == 4) {
		img->SetSize(CHUNK_W1, CHUNK_W1, 4);
	} else {
		img->SetSize(CHUNK_W1, CHUNK_W1, 3);
	}

	// Render terrain types to image
	for (unsigned y = 0; y < CHUNK_W1; ++ y) {
		unsigned ofs = 1 + (y + 1) * (CHUNK_W3);
		for (unsigned x = 0; x < CHUNK_W1; ++ x) {
			TTypesByWeight const& ttypes = corners[ofs].ttypes;
			assert(result_used_ttypes.Size() >= 2);
			assert(result_used_ttypes.Size() <= 4);

			float w0 = ttypes[result_used_ttypes[0]];
			float w1 = ttypes[result_used_ttypes[1]];
			float w2 = 0;
			float w3 = 0;
			if (result_used_ttypes.Size() >= 3) {
				w2 = ttypes[result_used_ttypes[2]];
			}
			if (result_used_ttypes.Size() >= 4) {
				w3 = ttypes[result_used_ttypes[3]];
			}
			float total = w0 + w1 + w2 + w3;
			if (total == 0) {
				w0 = 1;
				total = 1;
			}
			img->SetPixel(x, y, Urho3D::Color(w0 / total, w1 / total, w2 / total, w3 / total));

			++ ofs;
		}
	}

	return img;
}

void buildLod(Urho3D::WorkItem const* item, unsigned threadIndex)
{
	(void)threadIndex;

	LodBuildingTaskData* data = (LodBuildingTaskData*)item->aux_;

	// Check if terraintype image calculation is also needed
	if (data->calculate_ttype_image) {
		data->ttype_image = calculateTerraintypeImage(data->used_ttypes, data->context, data->corners, data->chunk_width);
	}

	// Precalculate some stuff
	unsigned const CHUNK_W = data->chunk_width;
	unsigned const CHUNK_W1 = data->chunk_width + 1;
	unsigned const CHUNK_W3 = data->chunk_width + 3;
	float const CHUNK_WF = data->chunk_width * data->sqr_width;
	unsigned const V2_SIZE = sizeof(float) * 2;
	unsigned const V3_SIZE = sizeof(float) * 3;

	// Set up elements
	data->vrts_elems.Push(Urho3D::VertexElement(Urho3D::TYPE_VECTOR3, Urho3D::SEM_POSITION));
	data->vrts_elems.Push(Urho3D::VertexElement(Urho3D::TYPE_VECTOR3, Urho3D::SEM_NORMAL));
	data->vrts_elems.Push(Urho3D::VertexElement(Urho3D::TYPE_VECTOR2, Urho3D::SEM_TEXCOORD));
	unsigned const VRT_SIZE = Urho3D::VertexBuffer::GetVertexSize(data->vrts_elems);

	// Create array of positions and calculate boundingbox
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

	// Check if there is more than one terraintype used
	Urho3D::HashSet<uint8_t> ttype_check;
	for (unsigned y = 0; y < CHUNK_W1 && ttype_check.Size() <= 1; ++ y) {
		unsigned ofs = 1 + (y + 1) * (CHUNK_W3);
		for (unsigned x = 0; x < CHUNK_W1 && ttype_check.Size() <= 1; ++ x) {
			Corner const& corner = data->corners[ofs];
			for (unsigned ttypes_i = 0; ttypes_i < corner.ttypes.size(); ++ ttypes_i) {
				uint8_t ttype = corner.ttypes.getKey(ttypes_i);
				float weight = corner.ttypes.getValue(ttypes_i);
				if (weight > 0) {
					ttype_check.Insert(ttype);
					if (ttype_check.Size() > 1) {
						break;
					}
				}
			}
			++ ofs;
		}
	}
	bool multiple_terraintypes = ttype_check.Size() > 1;

	// Create array of normals and UV coordinates
	Urho3D::PODVector<Urho3D::Vector3> nrms;
	Urho3D::PODVector<Urho3D::Vector2> uvs;
	ofs = 0;
	for (unsigned y = 0; y < CHUNK_W3; ++ y) {
		for (unsigned x = 0; x < CHUNK_W3; ++ x) {
			Urho3D::Vector3 nrm;
			Urho3D::Vector2 uv;

			if (x >= 1 && y >= 1 && x <= CHUNK_W1 && y <= CHUNK_W1) {
				// Normal
				Urho3D::Vector3 const& pos = poss[ofs];
				Urho3D::Vector3 const& pos_n = poss[ofs + CHUNK_W3];
				Urho3D::Vector3 const& pos_s = poss[ofs - CHUNK_W3];
				Urho3D::Vector3 const& pos_e = poss[ofs + 1];
				Urho3D::Vector3 const& pos_w = poss[ofs - 1];
				Urho3D::Vector3 diff_n = (pos_n - pos).Normalized();
				Urho3D::Vector3 diff_s = (pos_s - pos).Normalized();
				Urho3D::Vector3 diff_e = (pos_e - pos).Normalized();
				Urho3D::Vector3 diff_w = (pos_w - pos).Normalized();
				nrm = (diff_w.CrossProduct(diff_n) + diff_e.CrossProduct(diff_s)).Normalized();
				assert(nrm.y_ > 0);

				// Texture coordinates. If there are no multiple terraintypes,
				// then apply the repeating straight to UV coordinates.
				uv.x_ = float(x) / CHUNK_W;
				uv.y_ = float(y) / CHUNK_W;
				if (!multiple_terraintypes) {
					uv *= data->terrain_texture_repeats;
				}
			}

			nrms.Push(nrm);
			uvs.Push(uv);

			++ ofs;
		}
	}

	// LOD details determines the width of drawn elements, measured in world squares.
	unsigned step = Urho3D::Min<unsigned>(CHUNK_W, 1 << data->lod);

	// Create vertex data
	for (unsigned y = 0; y < CHUNK_W1; y += step) {
		ofs = 1 + (y + 1) * CHUNK_W3;
		for (unsigned x = 0; x < CHUNK_W1; x += step) {
			Urho3D::Vector3 const& pos = poss[ofs];
			Urho3D::Vector3 const& normal = nrms[ofs];
			Urho3D::Vector3 const& uv = uvs[ofs];
			data->vrts_data.Insert(data->vrts_data.End(), (char*)pos.Data(), (char*)pos.Data() + V3_SIZE);
			data->vrts_data.Insert(data->vrts_data.End(), (char*)normal.Data(), (char*)normal.Data() + V3_SIZE);
			data->vrts_data.Insert(data->vrts_data.End(), (char*)uv.Data(), (char*)uv.Data() + V2_SIZE);
			ofs += step;
		}
	}

	// Create index data
	for (unsigned y = 0; y < CHUNK_W / step; ++ y) {
		ofs = y * (CHUNK_W / step + 1);
		for (unsigned x = 0; x < CHUNK_W / step; ++ x) {

			data->idxs_data.Push(ofs);
			data->idxs_data.Push(ofs + 1 + CHUNK_W / step + 1);
			data->idxs_data.Push(ofs + 1);

			data->idxs_data.Push(ofs);
			data->idxs_data.Push(ofs + CHUNK_W / step + 1);
			data->idxs_data.Push(ofs + 1 + CHUNK_W / step + 1);

			++ ofs;
		}
	}

	// If not full detail LOD, then add some vertical triangles to
	// close some holes that appear between different detail chunks.
	if (data->lod > 0) {
		// South edge
		ofs = 1 + CHUNK_W3;
		for (unsigned i = 0; i < CHUNK_W / step; ++ i) {
			unsigned h_begin = data->corners[ofs].height;
			unsigned h_center = data->corners[ofs + step / 2].height;
			unsigned h_end = data->corners[ofs + step].height;
			if (h_center * 2 < h_begin + h_end) {
				unsigned i_begin = i;
				unsigned i_end = i + 1;
				unsigned i_center_ofs = 1 + CHUNK_W3 + i * step + step / 2;
				// Create new vertex
				unsigned i_center = data->vrts_data.Size() / VRT_SIZE;
				Urho3D::Vector3 const& center_pos = poss[i_center_ofs];
				Urho3D::Vector3 const& center_nrm = nrms[i_center_ofs];
				Urho3D::Vector3 const& center_uv = uvs[i_center_ofs];
				data->vrts_data.Insert(data->vrts_data.End(), (char*)center_pos.Data(), (char*)center_pos.Data() + V3_SIZE);
				data->vrts_data.Insert(data->vrts_data.End(), (char*)center_nrm.Data(), (char*)center_nrm.Data() + V3_SIZE);
				data->vrts_data.Insert(data->vrts_data.End(), (char*)center_uv.Data(), (char*)center_uv.Data() + V2_SIZE);
				// Create new triangle
				data->idxs_data.Push(i_begin);
				data->idxs_data.Push(i_end);
				data->idxs_data.Push(i_center);
			}
			ofs += step;
		}
		// East edge
		ofs = 1 + CHUNK_W3 + CHUNK_W;
		for (unsigned i = 0; i < CHUNK_W / step; ++ i) {
			unsigned h_begin = data->corners[ofs].height;
			unsigned h_center = data->corners[ofs + CHUNK_W3 * step / 2].height;
			unsigned h_end = data->corners[ofs + CHUNK_W3 * step].height;
			if (h_center * 2 < h_begin + h_end) {
				unsigned i_begin = CHUNK_W / step + i * (CHUNK_W / step + 1);
				unsigned i_end = i_begin + CHUNK_W / step + 1;
				unsigned i_center_ofs = 1 + CHUNK_W3 + CHUNK_W + i * CHUNK_W3 * step + CHUNK_W3 * step / 2;
				// Create new vertex
				unsigned i_center = data->vrts_data.Size() / VRT_SIZE;
				Urho3D::Vector3 const& center_pos = poss[i_center_ofs];
				Urho3D::Vector3 const& center_nrm = nrms[i_center_ofs];
				Urho3D::Vector3 const& center_uv = uvs[i_center_ofs];
				data->vrts_data.Insert(data->vrts_data.End(), (char*)center_pos.Data(), (char*)center_pos.Data() + V3_SIZE);
				data->vrts_data.Insert(data->vrts_data.End(), (char*)center_nrm.Data(), (char*)center_nrm.Data() + V3_SIZE);
				data->vrts_data.Insert(data->vrts_data.End(), (char*)center_uv.Data(), (char*)center_uv.Data() + V2_SIZE);
				// Create new triangle
				data->idxs_data.Push(i_begin);
				data->idxs_data.Push(i_end);
				data->idxs_data.Push(i_center);
			}
			ofs += step * CHUNK_W3;
		}
		// North edge
		ofs = 1 + CHUNK_W3 + CHUNK_W + CHUNK_W * CHUNK_W3;
		for (unsigned i = 0; i < CHUNK_W / step; ++ i) {
			unsigned h_begin = data->corners[ofs].height;
			unsigned h_center = data->corners[ofs - step / 2].height;
			unsigned h_end = data->corners[ofs - step].height;
			if (h_center * 2 < h_begin + h_end) {
				unsigned i_begin = CHUNK_W / step + CHUNK_W / step * (CHUNK_W / step + 1) - i;
				unsigned i_end = i_begin - 1;
				unsigned i_center_ofs = 1 + CHUNK_W3 + CHUNK_W + CHUNK_W * CHUNK_W3 - i * step - step / 2;
				// Create new vertex
				unsigned i_center = data->vrts_data.Size() / VRT_SIZE;
				Urho3D::Vector3 const& center_pos = poss[i_center_ofs];
				Urho3D::Vector3 const& center_nrm = nrms[i_center_ofs];
				Urho3D::Vector3 const& center_uv = uvs[i_center_ofs];
				data->vrts_data.Insert(data->vrts_data.End(), (char*)center_pos.Data(), (char*)center_pos.Data() + V3_SIZE);
				data->vrts_data.Insert(data->vrts_data.End(), (char*)center_nrm.Data(), (char*)center_nrm.Data() + V3_SIZE);
				data->vrts_data.Insert(data->vrts_data.End(), (char*)center_uv.Data(), (char*)center_uv.Data() + V2_SIZE);
				// Create new triangle
				data->idxs_data.Push(i_begin);
				data->idxs_data.Push(i_end);
				data->idxs_data.Push(i_center);
			}
			ofs -= step;
		}
		// West edge
		ofs = 1 + CHUNK_W3 + CHUNK_W * CHUNK_W3;
		for (unsigned i = 0; i < CHUNK_W / step; ++ i) {
			unsigned h_begin = data->corners[ofs].height;
			unsigned h_center = data->corners[ofs - CHUNK_W3 * step / 2].height;
			unsigned h_end = data->corners[ofs - CHUNK_W3 * step].height;
			if (h_center * 2 < h_begin + h_end) {
				unsigned i_begin = CHUNK_W / step * (CHUNK_W / step + 1) - i * (CHUNK_W / step + 1);
				unsigned i_end = i_begin - CHUNK_W / step - 1;
				unsigned i_center_ofs = 1 + CHUNK_W3 + CHUNK_W * CHUNK_W3 - i * CHUNK_W3 * step - CHUNK_W3 * step / 2;
				// Create new vertex
				unsigned i_center = data->vrts_data.Size() / VRT_SIZE;
				Urho3D::Vector3 const& center_pos = poss[i_center_ofs];
				Urho3D::Vector3 const& center_nrm = nrms[i_center_ofs];
				Urho3D::Vector3 const& center_uv = uvs[i_center_ofs];
				data->vrts_data.Insert(data->vrts_data.End(), (char*)center_pos.Data(), (char*)center_pos.Data() + V3_SIZE);
				data->vrts_data.Insert(data->vrts_data.End(), (char*)center_nrm.Data(), (char*)center_nrm.Data() + V3_SIZE);
				data->vrts_data.Insert(data->vrts_data.End(), (char*)center_uv.Data(), (char*)center_uv.Data() + V2_SIZE);
				// Create new triangle
				data->idxs_data.Push(i_begin);
				data->idxs_data.Push(i_end);
				data->idxs_data.Push(i_center);
			}
			ofs -= step * CHUNK_W3;
		}
	}
}

}

