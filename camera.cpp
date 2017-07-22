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

Urho3D::Quaternion Camera::getRotation() const
{
	Urho3D::Quaternion rot(roll, Urho3D::Vector3::FORWARD);
	rot = Urho3D::Quaternion(pitch, Urho3D::Vector3::RIGHT) * rot;
	rot = Urho3D::Quaternion(yaw, Urho3D::Vector3::UP) * rot;
	return rot;
}

void Camera::applyRelativeMovement(Urho3D::Vector3 const& movement)
{
	applyAbsoluteMovement(getRotation() * movement);
}

void Camera::applyAbsoluteMovement(Urho3D::Vector3 const& movement)
{
	pos += movement;
// TODO: Check if chunk_pos/baseheight should be updated!
	updateNodeTransform();
}


void Camera::updateNodeTransform()
{
	float const CHUNK_W_F = world->getChunkWidth() * world->getSquareWidth();
	float const HEIGHTSTEP = world->getHeightstep();

	Urho3D::IntVector2 origin = world->getOrigin();
	int origin_height = world->getOriginHeight();

	Urho3D::IntVector2 diff_xz = chunk_pos - origin;
	int diff_y = baseheight - origin_height;

	Urho3D::Vector3 final_pos = pos;
	final_pos.x_ += diff_xz.x_ * CHUNK_W_F;
	final_pos.y_ += diff_y * HEIGHTSTEP;
	final_pos.z_ += diff_xz.y_ * CHUNK_W_F;

	node->SetPosition(final_pos);
	node->SetRotation(getRotation());
}

bool Camera::fixIfOutsideOrigin()
{
	float const CHUNK_W_F = world->getChunkWidth() * world->getSquareWidth();
	float const CHUNK_HALF_W_F = CHUNK_W_F / 2;
	float const HEIGHTSTEP = world->getHeightstep();
	float CHUNK_THRESHOLD = 1.5;
	unsigned const HEIGHT_THRESHOLD = 500;

	bool fixed = false;

	if (pos.x_ < -CHUNK_HALF_W_F * CHUNK_THRESHOLD) {
		pos.x_ += CHUNK_W_F;
		-- chunk_pos.x_;
		fixed = true;
	} else if (pos.x_ > CHUNK_HALF_W_F * CHUNK_THRESHOLD) {
		pos.x_ -= CHUNK_W_F;
		++ chunk_pos.x_;
		fixed = true;
	}
	if (pos.z_ < -CHUNK_HALF_W_F * CHUNK_THRESHOLD) {
		pos.z_ += CHUNK_W_F;
		-- chunk_pos.y_;
		fixed = true;
	} else if (pos.z_ > CHUNK_HALF_W_F * CHUNK_THRESHOLD) {
		pos.z_ -= CHUNK_W_F;
		++ chunk_pos.y_;
		fixed = true;
	}
	if (pos.y_ > HEIGHTSTEP * HEIGHT_THRESHOLD) {
		pos.y_ -= HEIGHTSTEP * HEIGHT_THRESHOLD;
		baseheight += HEIGHT_THRESHOLD;
		fixed = true;
	} else if (pos.y_ < -HEIGHTSTEP * HEIGHT_THRESHOLD && baseheight > 0) {
		int mod = Urho3D::Min<int>(baseheight, HEIGHT_THRESHOLD);
		pos.y_ += HEIGHTSTEP * mod;
		baseheight -= mod;
		fixed = true;
	}

	return fixed;
}

}
