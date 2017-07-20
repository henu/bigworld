#ifndef BIGWORLD_CAMERA_HPP
#define BIGWORLD_CAMERA_HPP

#include <Urho3D/Container/RefCounted.h>
#include <Urho3D/Math/Vector2.h>
#include <Urho3D/Math/Vector3.h>
#include <Urho3D/Scene/Scene.h>

namespace BigWorld
{

class ChunkWorld;

class Camera : public Urho3D::RefCounted
{

public:

	Camera(ChunkWorld* world, Urho3D::IntVector2 const& chunk_pos, unsigned baseheight, Urho3D::Vector3 const& pos, float yaw, float pitch, float roll, unsigned viewdistance_in_chunks);

	inline Urho3D::IntVector2 getChunkPosition() const { return chunk_pos; }
	inline unsigned getBaseHeight() const { return baseheight; }
	inline unsigned getViewDistanceInChunks() const { return viewdistance_in_chunks; }

	inline Urho3D::Node* getNode() const { return node; }
	Urho3D::Quaternion getRotation() const;

	void applyRelativeMovement(Urho3D::Vector3 const& movement);
	void applyAbsoluteMovement(Urho3D::Vector3 const& movement);

	inline void addYaw(float angle) { yaw += angle; updateNodeTransform(); }
	inline void addPitch(float angle) { pitch += angle; updateNodeTransform(); }
	inline void addRoll(float angle) { roll += angle; updateNodeTransform(); }

	inline float getYaw() const { return yaw; }
	inline float getPitch() const { return pitch; }
	inline float getRoll() const { return roll; }

	inline void setYaw(float angle) { yaw = angle; updateNodeTransform(); }
	inline void setPitch(float angle) { pitch = angle; updateNodeTransform(); }
	inline void setRoll(float angle) { roll = angle; updateNodeTransform(); }

	// Called automatically
	void updateNodeTransform();

	bool fixIfOutsideOrigin();

private:

	ChunkWorld* world;

	Urho3D::IntVector2 chunk_pos;
	unsigned baseheight;
	Urho3D::Vector3 pos;
	float yaw, pitch, roll;

	unsigned viewdistance_in_chunks;

	Urho3D::Node* node;
};

}

#endif
