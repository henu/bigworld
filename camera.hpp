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

	void updateNodeTransform();

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
