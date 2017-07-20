#include "camera.hpp"

#include "chunkworld.hpp"

namespace BigWorld
{

Camera::Camera(ChunkWorld* world, Urho3D::IntVector2 const& chunk_pos, unsigned baseheight, Urho3D::Vector3 const& pos, float yaw, float pitch, float roll, unsigned viewdistance_in_chunks) :
world(world),
chunk_pos(chunk_pos),
baseheight(baseheight),
pos(pos),
yaw(yaw),
pitch(pitch),
roll(roll),
viewdistance_in_chunks(viewdistance_in_chunks)
{
	node = world->getScene()->CreateChild();
}

void Camera::updateNodeTransform()
{
	float const CHUNK_W_F = world->getChunkWidth() * world->getSquareWidth();
	float const HEIGHTSTEP = world->getHeightstep();

	Urho3D::IntVector2 origin = world->getOrigin();
	unsigned origin_height = world->getOriginHeight();

	Urho3D::IntVector2 diff_xz = chunk_pos - origin;
	unsigned diff_y = baseheight - origin_height;

	Urho3D::Vector3 final_pos = pos;
	final_pos.x_ += diff_xz.x_ * CHUNK_W_F;
	final_pos.y_ += diff_y * HEIGHTSTEP;
	final_pos.z_ += diff_xz.y_ * CHUNK_W_F;

	node->SetPosition(final_pos);

	Urho3D::Quaternion rot(roll, Urho3D::Vector3::FORWARD);
	rot = Urho3D::Quaternion(pitch, Urho3D::Vector3::RIGHT) * rot;
	rot = Urho3D::Quaternion(yaw, Urho3D::Vector3::UP) * rot;
	node->SetRotation(rot);
}

}
