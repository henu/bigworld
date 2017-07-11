#ifndef BIGWORLD_LODBUILDER_HPP
#define BIGWORLD_LODBUILDER_HPP

#include <Urho3D/Core/WorkQueue.h>

namespace BigWorld
{

void buildLod(Urho3D::WorkItem const* item, unsigned threadIndex);

}

#endif

