// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#pragma once

#include "../containers/vector.h"
#include "../math/bounding_box.h"
#include "../math/vector3.h"

class rcContext;

struct dtTileCacheContourSet;
struct dtTileCachePolyMesh;
struct dtTileCacheAlloc;
struct rcCompactHeightfield;
struct rcContourSet;
struct rcHeightfield;
struct rcHeightfieldLayerSet;
struct rcPolyMesh;
struct rcPolyMeshDetail;

namespace dviglo
{

/// Navigation area stub.
struct DV_API NavAreaStub
{
    /// Area bounding box.
    BoundingBox bounds_;
    /// Area ID.
    unsigned char areaID_;
};

/// Navigation build data.
struct DV_API NavBuildData
{
    /// Constructor.
    NavBuildData();
    /// Destructor.
    virtual ~NavBuildData();

    /// World-space bounding box of the navigation mesh tile.
    BoundingBox worldBoundingBox_;
    /// Vertices from geometries.
    Vector<Vector3> vertices_;
    /// Triangle indices from geometries.
    Vector<int> indices_;
    /// Offmesh connection vertices.
    Vector<Vector3> offMeshVertices_;
    /// Offmesh connection radii.
    Vector<float> offMeshRadii_;
    /// Offmesh connection flags.
    Vector<unsigned short> offMeshFlags_;
    /// Offmesh connection areas.
    Vector<unsigned char> offMeshAreas_;
    /// Offmesh connection direction.
    Vector<unsigned char> offMeshDir_;
    /// Recast context.
    rcContext* ctx_;
    /// Recast heightfield.
    rcHeightfield* heightField_;
    /// Recast compact heightfield.
    rcCompactHeightfield* compactHeightField_;
    /// Pretransformed navigation areas, no correlation to the geometry above.
    Vector<NavAreaStub> navAreas_;
};

struct SimpleNavBuildData : public NavBuildData
{
    /// Constructor.
    SimpleNavBuildData();
    /// Descturctor.
    ~SimpleNavBuildData() override;

    /// Recast contour set.
    rcContourSet* contourSet_;
    /// Recast poly mesh.
    rcPolyMesh* polyMesh_;
    /// Recast detail poly mesh.
    rcPolyMeshDetail* polyMeshDetail_;
};

struct DynamicNavBuildData : public NavBuildData
{
    /// Constructor.
    explicit DynamicNavBuildData(dtTileCacheAlloc* allocator);
    /// Destructor.
    ~DynamicNavBuildData() override;

    /// TileCache specific recast contour set.
    dtTileCacheContourSet* contourSet_;
    /// TileCache specific recast poly mesh.
    dtTileCachePolyMesh* polyMesh_;
    /// Recast heightfield layer set.
    rcHeightfieldLayerSet* heightFieldLayers_;
    /// Allocator from DynamicNavigationMesh instance.
    dtTileCacheAlloc* alloc_;
};

}
