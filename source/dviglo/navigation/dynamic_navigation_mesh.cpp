// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#include "../core/context.h"
#include "../core/profiler.h"
#include "../graphics/debug_renderer.h"
#include "../io/log.h"
#include "../io/memory_buffer.h"
#include "crowd_agent.h"
#include "dynamic_navigation_mesh.h"
#include "nav_area.h"
#include "nav_build_data.h"
#include "navigation_events.h"
#include "obstacle.h"
#include "off_mesh_connection.h"
#include "../scene/node.h"
#include "../scene/scene.h"
#include "../scene/scene_events.h"

#include <lz4/lz4.h>
#include <detour/DetourNavMesh.h>
#include <detour/DetourNavMeshBuilder.h>
#include <detour_tile_cache/DetourTileCache.h>
#include <detour_tile_cache/DetourTileCacheBuilder.h>
#include <recast/Recast.h>

using namespace std;

// DebugNew is deliberately not used because the macro 'free' conflicts with DetourTileCache's LinearAllocator interface
//#include "../common/debug_new.h"

static const unsigned TILECACHE_MAXLAYERS = 255;

namespace dviglo
{

extern const char* NAVIGATION_CATEGORY;

static const int DEFAULT_MAX_OBSTACLES = 1024;
static const int DEFAULT_MAX_LAYERS = 16;

struct DynamicNavigationMesh::TileCacheData
{
    unsigned char* data;
    int dataSize;
};

struct TileCompressor : public dtTileCacheCompressor
{
    int maxCompressedSize(const int bufferSize) override
    {
        return (int)(bufferSize * 1.05f);
    }

    dtStatus compress(const unsigned char* buffer, const int bufferSize,
        unsigned char* compressed, const int /*maxCompressedSize*/, int* compressedSize) override
    {
        *compressedSize = LZ4_compress_default((const char*)buffer, (char*)compressed, bufferSize, LZ4_compressBound(bufferSize));
        return DT_SUCCESS;
    }

    dtStatus decompress(const unsigned char* compressed, const int compressedSize,
        unsigned char* buffer, const int maxBufferSize, int* bufferSize) override
    {
        *bufferSize = LZ4_decompress_safe((const char*)compressed, (char*)buffer, compressedSize, maxBufferSize);
        return *bufferSize < 0 ? DT_FAILURE : DT_SUCCESS;
    }
};

struct MeshProcess : public dtTileCacheMeshProcess
{
    DynamicNavigationMesh* owner_;
    Vector<Vector3> offMeshVertices_;
    Vector<float> offMeshRadii_;
    Vector<unsigned short> offMeshFlags_;
    Vector<unsigned char> offMeshAreas_;
    Vector<unsigned char> offMeshDir_;

    inline explicit MeshProcess(DynamicNavigationMesh* owner) :
        owner_(owner)
    {
    }

    void process(struct dtNavMeshCreateParams* params, unsigned char* polyAreas, unsigned short* polyFlags) override
    {
        // Update poly flags from areas.
        // \todo Assignment of flags from areas?
        for (int i = 0; i < params->polyCount; ++i)
        {
            if (polyAreas[i] != RC_NULL_AREA)
                polyFlags[i] = RC_WALKABLE_AREA;
        }

        BoundingBox bounds;
        rcVcopy(&bounds.min_.x, params->bmin);
        rcVcopy(&bounds.max_.x, params->bmin);

        // collect off-mesh connections
        Vector<OffMeshConnection*> offMeshConnections = owner_->CollectOffMeshConnections(bounds);

        if (offMeshConnections.Size() > 0)
        {
            if (offMeshConnections.Size() != offMeshRadii_.Size())
            {
                Matrix3x4 inverse = owner_->GetNode()->GetWorldTransform().Inverse();
                ClearConnectionData();
                for (unsigned i = 0; i < offMeshConnections.Size(); ++i)
                {
                    OffMeshConnection* connection = offMeshConnections[i];
                    Vector3 start = inverse * connection->GetNode()->GetWorldPosition();
                    Vector3 end = inverse * connection->GetEndPoint()->GetWorldPosition();

                    offMeshVertices_.Push(start);
                    offMeshVertices_.Push(end);
                    offMeshRadii_.Push(connection->GetRadius());
                    offMeshFlags_.Push((unsigned short)connection->GetMask());
                    offMeshAreas_.Push((unsigned char)connection->GetAreaID());
                    offMeshDir_.Push((unsigned char)(connection->IsBidirectional() ? DT_OFFMESH_CON_BIDIR : 0));
                }
            }
            params->offMeshConCount = offMeshRadii_.Size();
            params->offMeshConVerts = &offMeshVertices_[0].x;
            params->offMeshConRad = &offMeshRadii_[0];
            params->offMeshConFlags = &offMeshFlags_[0];
            params->offMeshConAreas = &offMeshAreas_[0];
            params->offMeshConDir = &offMeshDir_[0];
        }
    }

    void ClearConnectionData()
    {
        offMeshVertices_.Clear();
        offMeshRadii_.Clear();
        offMeshFlags_.Clear();
        offMeshAreas_.Clear();
        offMeshDir_.Clear();
    }
};


// From the Detour/Recast Sample_TempObstacles.cpp
struct LinearAllocator : public dtTileCacheAlloc
{
    unsigned char* buffer;
    int capacity;
    int top;
    int high;

    explicit LinearAllocator(const int cap) :
        buffer(nullptr), capacity(0), top(0), high(0)
    {
        resize(cap);
    }

    ~LinearAllocator() override
    {
        dtFree(buffer);
    }

    void resize(const int cap)
    {
        if (buffer)
            dtFree(buffer);
        buffer = (unsigned char*)dtAlloc(cap, DT_ALLOC_PERM);
        capacity = cap;
    }

    void reset() override
    {
        high = Max(high, top);
        top = 0;
    }

    void* alloc(const size_t size) override
    {
        if (!buffer)
            return nullptr;
        if (top + size > capacity)
            return nullptr;
        unsigned char* mem = &buffer[top];
        top += size;
        return mem;
    }

    void free(void*) override
    {
    }
};


DynamicNavigationMesh::DynamicNavigationMesh() :
    maxLayers_(DEFAULT_MAX_LAYERS)
{
    // 64 is the largest tile-size that DetourTileCache will tolerate without silently failing
    tileSize_ = 64;
    partitionType_ = NAVMESH_PARTITION_MONOTONE;
    allocator_ = make_unique<LinearAllocator>(32000); //32kb to start
    compressor_ = make_unique<TileCompressor>();
    meshProcessor_ = make_unique<MeshProcess>(this);
}

DynamicNavigationMesh::~DynamicNavigationMesh()
{
    release_navigation_mesh();
}

void DynamicNavigationMesh::register_object()
{
    DV_CONTEXT->RegisterFactory<DynamicNavigationMesh>(NAVIGATION_CATEGORY);

    DV_COPY_BASE_ATTRIBUTES(NavigationMesh);
    DV_ACCESSOR_ATTRIBUTE("Max Obstacles", GetMaxObstacles, SetMaxObstacles, DEFAULT_MAX_OBSTACLES, AM_DEFAULT);
    DV_ACCESSOR_ATTRIBUTE("Max Layers", GetMaxLayers, SetMaxLayers, DEFAULT_MAX_LAYERS, AM_DEFAULT);
    DV_ACCESSOR_ATTRIBUTE("Draw Obstacles", GetDrawObstacles, SetDrawObstacles, false, AM_DEFAULT);
}

bool DynamicNavigationMesh::Allocate(const BoundingBox& boundingBox, unsigned maxTiles)
{
    // Release existing navigation data and zero the bounding box
    release_navigation_mesh();

    if (!node_)
        return false;

    if (!node_->GetWorldScale().Equals(Vector3::ONE))
        DV_LOGWARNING("Navigation mesh root node has scaling. Agent parameters may not work as intended");

    boundingBox_ = boundingBox.Transformed(node_->GetWorldTransform().Inverse());
    maxTiles = NextPowerOfTwo(maxTiles);

    // Calculate number of tiles
    int gridW = 0, gridH = 0;
    float tileEdgeLength = (float)tileSize_ * cellSize_;
    rcCalcGridSize(&boundingBox_.min_.x, &boundingBox_.max_.x, cellSize_, &gridW, &gridH);
    numTilesX_ = (gridW + tileSize_ - 1) / tileSize_;
    numTilesZ_ = (gridH + tileSize_ - 1) / tileSize_;

    // Calculate max number of polygons, 22 bits available to identify both tile & polygon within tile
    unsigned tileBits = LogBaseTwo(maxTiles);
    unsigned maxPolys = 1u << (22 - tileBits);

    dtNavMeshParams params;     // NOLINT(hicpp-member-init)
    rcVcopy(params.orig, &boundingBox_.min_.x);
    params.tileWidth = tileEdgeLength;
    params.tileHeight = tileEdgeLength;
    params.maxTiles = maxTiles;
    params.maxPolys = maxPolys;

    navMesh_ = dtAllocNavMesh();
    if (!navMesh_)
    {
        DV_LOGERROR("Could not allocate navigation mesh");
        return false;
    }

    if (dtStatusFailed(navMesh_->init(&params)))
    {
        DV_LOGERROR("Could not initialize navigation mesh");
        release_navigation_mesh();
        return false;
    }

    dtTileCacheParams tileCacheParams;      // NOLINT(hicpp-member-init)
    memset(&tileCacheParams, 0, sizeof(tileCacheParams));
    rcVcopy(tileCacheParams.orig, &boundingBox_.min_.x);
    tileCacheParams.ch = cellHeight_;
    tileCacheParams.cs = cellSize_;
    tileCacheParams.width = tileSize_;
    tileCacheParams.height = tileSize_;
    tileCacheParams.maxSimplificationError = edgeMaxError_;
    tileCacheParams.maxTiles = maxTiles * maxLayers_;
    tileCacheParams.maxObstacles = maxObstacles_;
    // Settings from NavigationMesh
    tileCacheParams.walkableClimb = agentMaxClimb_;
    tileCacheParams.walkableHeight = agentHeight_;
    tileCacheParams.walkableRadius = agentRadius_;

    tileCache_ = dtAllocTileCache();
    if (!tileCache_)
    {
        DV_LOGERROR("Could not allocate tile cache");
        release_navigation_mesh();
        return false;
    }

    if (dtStatusFailed(tileCache_->init(&tileCacheParams, allocator_.get(), compressor_.get(), meshProcessor_.get())))
    {
        DV_LOGERROR("Could not initialize tile cache");
        release_navigation_mesh();
        return false;
    }

    DV_LOGDEBUG("Allocated empty navigation mesh with max " + String(maxTiles) + " tiles");

    // Scan for obstacles to insert into us
    Vector<Node*> obstacles;
    GetScene()->GetChildrenWithComponent<Obstacle>(obstacles, true);
    for (unsigned i = 0; i < obstacles.Size(); ++i)
    {
        auto* obs = obstacles[i]->GetComponent<Obstacle>();
        if (obs && obs->IsEnabledEffective())
            AddObstacle(obs);
    }

    // Send a notification event to concerned parties that we've been fully rebuilt
    {
        using namespace NavigationMeshRebuilt;
        VariantMap& buildEventParams = DV_CONTEXT->GetEventDataMap();
        buildEventParams[P_NODE] = node_;
        buildEventParams[P_MESH] = this;
        SendEvent(E_NAVIGATION_MESH_REBUILT, buildEventParams);
    }
    return true;
}

bool DynamicNavigationMesh::Build()
{
    DV_PROFILE(BuildNavigationMesh);
    // Release existing navigation data and zero the bounding box
    release_navigation_mesh();

    if (!node_)
        return false;

    if (!node_->GetWorldScale().Equals(Vector3::ONE))
        DV_LOGWARNING("Navigation mesh root node has scaling. Agent parameters may not work as intended");

    Vector<NavigationGeometryInfo> geometryList;
    collect_geometries(geometryList);

    if (geometryList.Empty())
        return true; // Nothing to do

    // Build the combined bounding box
    for (unsigned i = 0; i < geometryList.Size(); ++i)
        boundingBox_.Merge(geometryList[i].boundingBox_);

    // Expand bounding box by padding
    boundingBox_.min_ -= padding_;
    boundingBox_.max_ += padding_;

    {
        DV_PROFILE(BuildNavigationMesh);

        // Calculate number of tiles
        int gridW = 0, gridH = 0;
        float tileEdgeLength = (float)tileSize_ * cellSize_;
        rcCalcGridSize(&boundingBox_.min_.x, &boundingBox_.max_.x, cellSize_, &gridW, &gridH);
        numTilesX_ = (gridW + tileSize_ - 1) / tileSize_;
        numTilesZ_ = (gridH + tileSize_ - 1) / tileSize_;

        // Calculate max. number of tiles and polygons, 22 bits available to identify both tile & polygon within tile
        unsigned maxTiles = NextPowerOfTwo((unsigned)(numTilesX_ * numTilesZ_)) * maxLayers_;
        unsigned tileBits = LogBaseTwo(maxTiles);
        unsigned maxPolys = 1u << (22 - tileBits);

        dtNavMeshParams params;     // NOLINT(hicpp-member-init)
        rcVcopy(params.orig, &boundingBox_.min_.x);
        params.tileWidth = tileEdgeLength;
        params.tileHeight = tileEdgeLength;
        params.maxTiles = maxTiles;
        params.maxPolys = maxPolys;

        navMesh_ = dtAllocNavMesh();
        if (!navMesh_)
        {
            DV_LOGERROR("Could not allocate navigation mesh");
            return false;
        }

        if (dtStatusFailed(navMesh_->init(&params)))
        {
            DV_LOGERROR("Could not initialize navigation mesh");
            release_navigation_mesh();
            return false;
        }

        dtTileCacheParams tileCacheParams;      // NOLINT(hicpp-member-init)
        memset(&tileCacheParams, 0, sizeof(tileCacheParams));
        rcVcopy(tileCacheParams.orig, &boundingBox_.min_.x);
        tileCacheParams.ch = cellHeight_;
        tileCacheParams.cs = cellSize_;
        tileCacheParams.width = tileSize_;
        tileCacheParams.height = tileSize_;
        tileCacheParams.maxSimplificationError = edgeMaxError_;
        tileCacheParams.maxTiles = numTilesX_ * numTilesZ_ * maxLayers_;
        tileCacheParams.maxObstacles = maxObstacles_;
        // Settings from NavigationMesh
        tileCacheParams.walkableClimb = agentMaxClimb_;
        tileCacheParams.walkableHeight = agentHeight_;
        tileCacheParams.walkableRadius = agentRadius_;

        tileCache_ = dtAllocTileCache();
        if (!tileCache_)
        {
            DV_LOGERROR("Could not allocate tile cache");
            release_navigation_mesh();
            return false;
        }

        if (dtStatusFailed(tileCache_->init(&tileCacheParams, allocator_.get(), compressor_.get(), meshProcessor_.get())))
        {
            DV_LOGERROR("Could not initialize tile cache");
            release_navigation_mesh();
            return false;
        }

        // Build each tile
        unsigned numTiles = 0;

        for (int z = 0; z < numTilesZ_; ++z)
        {
            for (int x = 0; x < numTilesX_; ++x)
            {
                TileCacheData tiles[TILECACHE_MAXLAYERS];
                int layerCt = BuildTile(geometryList, x, z, tiles);
                for (int i = 0; i < layerCt; ++i)
                {
                    dtCompressedTileRef tileRef;
                    int status = tileCache_->addTile(tiles[i].data, tiles[i].dataSize, DT_COMPRESSEDTILE_FREE_DATA, &tileRef);
                    if (dtStatusFailed((dtStatus)status))
                    {
                        dtFree(tiles[i].data);
                        tiles[i].data = nullptr;
                    }
                }
                tileCache_->buildNavMeshTilesAt(x, z, navMesh_);
                ++numTiles;
            }
        }

        // For a full build it's necessary to update the nav mesh
        // not doing so will cause dependent components to crash, like CrowdManager
        tileCache_->update(0, navMesh_);

        DV_LOGDEBUG("Built navigation mesh with " + String(numTiles) + " tiles");

        // Send a notification event to concerned parties that we've been fully rebuilt
        {
            using namespace NavigationMeshRebuilt;
            VariantMap& buildEventParams = DV_CONTEXT->GetEventDataMap();
            buildEventParams[P_NODE] = node_;
            buildEventParams[P_MESH] = this;
            SendEvent(E_NAVIGATION_MESH_REBUILT, buildEventParams);
        }

        // Scan for obstacles to insert into us
        Vector<Node*> obstacles;
        GetScene()->GetChildrenWithComponent<Obstacle>(obstacles, true);
        for (unsigned i = 0; i < obstacles.Size(); ++i)
        {
            auto* obs = obstacles[i]->GetComponent<Obstacle>();
            if (obs && obs->IsEnabledEffective())
                AddObstacle(obs);
        }

        return true;
    }
}

bool DynamicNavigationMesh::Build(const BoundingBox& boundingBox)
{
    DV_PROFILE(BuildPartialNavigationMesh);

    if (!node_)
        return false;

    if (!navMesh_)
    {
        DV_LOGERROR("Navigation mesh must first be built fully before it can be partially rebuilt");
        return false;
    }

    if (!node_->GetWorldScale().Equals(Vector3::ONE))
        DV_LOGWARNING("Navigation mesh root node has scaling. Agent parameters may not work as intended");

    BoundingBox localSpaceBox = boundingBox.Transformed(node_->GetWorldTransform().Inverse());

    float tileEdgeLength = (float)tileSize_ * cellSize_;

    Vector<NavigationGeometryInfo> geometryList;
    collect_geometries(geometryList);

    int sx = Clamp((int)((localSpaceBox.min_.x - boundingBox_.min_.x) / tileEdgeLength), 0, numTilesX_ - 1);
    int sz = Clamp((int)((localSpaceBox.min_.z - boundingBox_.min_.z) / tileEdgeLength), 0, numTilesZ_ - 1);
    int ex = Clamp((int)((localSpaceBox.max_.x - boundingBox_.min_.x) / tileEdgeLength), 0, numTilesX_ - 1);
    int ez = Clamp((int)((localSpaceBox.max_.z - boundingBox_.min_.z) / tileEdgeLength), 0, numTilesZ_ - 1);

    unsigned numTiles = build_tiles(geometryList, IntVector2(sx, sz), IntVector2(ex, ez));

    DV_LOGDEBUG("Rebuilt " + String(numTiles) + " tiles of the navigation mesh");
    return true;
}

bool DynamicNavigationMesh::Build(const IntVector2& from, const IntVector2& to)
{
    DV_PROFILE(BuildPartialNavigationMesh);

    if (!node_)
        return false;

    if (!navMesh_)
    {
        DV_LOGERROR("Navigation mesh must first be built fully before it can be partially rebuilt");
        return false;
    }

    if (!node_->GetWorldScale().Equals(Vector3::ONE))
        DV_LOGWARNING("Navigation mesh root node has scaling. Agent parameters may not work as intended");

    Vector<NavigationGeometryInfo> geometryList;
    collect_geometries(geometryList);

    unsigned numTiles = build_tiles(geometryList, from, to);

    DV_LOGDEBUG("Rebuilt " + String(numTiles) + " tiles of the navigation mesh");
    return true;
}

Vector<byte> DynamicNavigationMesh::GetTileData(const IntVector2& tile) const
{
    VectorBuffer ret;
    WriteTiles(ret, tile.x, tile.y);
    return ret.GetBuffer();
}

bool DynamicNavigationMesh::IsObstacleInTile(Obstacle* obstacle, const IntVector2& tile) const
{
    const BoundingBox tileBoundingBox = GetTileBoundingBox(tile);
    const Vector3 obstaclePosition = obstacle->GetNode()->GetWorldPosition();
    return tileBoundingBox.distance_to_point(obstaclePosition) < obstacle->GetRadius();
}

bool DynamicNavigationMesh::AddTile(const Vector<byte>& tileData)
{
    MemoryBuffer buffer(tileData);
    return ReadTiles(buffer, false);
}

void DynamicNavigationMesh::RemoveTile(const IntVector2& tile)
{
    if (!navMesh_)
        return;

    dtCompressedTileRef existing[TILECACHE_MAXLAYERS];
    const int existingCt = tileCache_->getTilesAt(tile.x, tile.y, existing, maxLayers_);
    for (int i = 0; i < existingCt; ++i)
    {
        unsigned char* data = nullptr;
        if (!dtStatusFailed(tileCache_->removeTile(existing[i], &data, nullptr)) && data != nullptr)
            dtFree(data);
    }

    NavigationMesh::RemoveTile(tile);
}

void DynamicNavigationMesh::RemoveAllTiles()
{
    int numTiles = tileCache_->getTileCount();
    for (int i = 0; i < numTiles; ++i)
    {
        const dtCompressedTile* tile = tileCache_->getTile(i);
        assert(tile);
        if (tile->header)
            tileCache_->removeTile(tileCache_->getTileRef(tile), nullptr, nullptr);
    }

    NavigationMesh::RemoveAllTiles();
}

void DynamicNavigationMesh::draw_debug_geometry(DebugRenderer* debug, bool depthTest)
{
    if (!debug || !navMesh_ || !node_)
        return;

    const Matrix3x4& worldTransform = node_->GetWorldTransform();

    const dtNavMesh* navMesh = navMesh_;

    for (int j = 0; j < navMesh->getMaxTiles(); ++j)
    {
        const dtMeshTile* tile = navMesh->getTile(j);
        assert(tile);
        if (!tile->header)
            continue;

        for (int i = 0; i < tile->header->polyCount; ++i)
        {
            dtPoly* poly = tile->polys + i;
            for (unsigned j = 0; j < poly->vertCount; ++j)
            {
                debug->AddLine(worldTransform * *reinterpret_cast<const Vector3*>(&tile->verts[poly->verts[j] * 3]),
                    worldTransform * *reinterpret_cast<const Vector3*>(&tile->verts[poly->verts[(j + 1) % poly->vertCount] * 3]),
                    Color::YELLOW, depthTest);
            }
        }
    }

    Scene* scene = GetScene();
    if (scene)
    {
        // Draw Obstacle components
        if (drawObstacles_)
        {
            Vector<Node*> obstacles;
            scene->GetChildrenWithComponent<Obstacle>(obstacles, true);
            for (unsigned i = 0; i < obstacles.Size(); ++i)
            {
                auto* obstacle = obstacles[i]->GetComponent<Obstacle>();
                if (obstacle && obstacle->IsEnabledEffective())
                    obstacle->draw_debug_geometry(debug, depthTest);
            }
        }

        // Draw OffMeshConnection components
        if (drawOffMeshConnections_)
        {
            Vector<Node*> connections;
            scene->GetChildrenWithComponent<OffMeshConnection>(connections, true);
            for (unsigned i = 0; i < connections.Size(); ++i)
            {
                auto* connection = connections[i]->GetComponent<OffMeshConnection>();
                if (connection && connection->IsEnabledEffective())
                    connection->draw_debug_geometry(debug, depthTest);
            }
        }

        // Draw NavArea components
        if (drawNavAreas_)
        {
            Vector<Node*> areas;
            scene->GetChildrenWithComponent<NavArea>(areas, true);
            for (unsigned i = 0; i < areas.Size(); ++i)
            {
                auto* area = areas[i]->GetComponent<NavArea>();
                if (area && area->IsEnabledEffective())
                    area->draw_debug_geometry(debug, depthTest);
            }
        }
    }
}

void DynamicNavigationMesh::draw_debug_geometry(bool depthTest)
{
    Scene* scene = GetScene();
    if (scene)
    {
        auto* debug = scene->GetComponent<DebugRenderer>();
        if (debug)
            draw_debug_geometry(debug, depthTest);
    }
}

void DynamicNavigationMesh::SetNavigationDataAttr(const Vector<byte>& value)
{
    release_navigation_mesh();

    if (value.Empty())
        return;

    MemoryBuffer buffer(value);
    boundingBox_ = buffer.ReadBoundingBox();
    numTilesX_ = buffer.ReadI32();
    numTilesZ_ = buffer.ReadI32();

    dtNavMeshParams params;     // NOLINT(hicpp-member-init)
    buffer.Read(&params, sizeof(dtNavMeshParams));

    navMesh_ = dtAllocNavMesh();
    if (!navMesh_)
    {
        DV_LOGERROR("Could not allocate navigation mesh");
        return;
    }

    if (dtStatusFailed(navMesh_->init(&params)))
    {
        DV_LOGERROR("Could not initialize navigation mesh");
        release_navigation_mesh();
        return;
    }

    dtTileCacheParams tcParams;     // NOLINT(hicpp-member-init)
    buffer.Read(&tcParams, sizeof(tcParams));

    tileCache_ = dtAllocTileCache();
    if (!tileCache_)
    {
        DV_LOGERROR("Could not allocate tile cache");
        release_navigation_mesh();
        return;
    }
    if (dtStatusFailed(tileCache_->init(&tcParams, allocator_.get(), compressor_.get(), meshProcessor_.get())))
    {
        DV_LOGERROR("Could not initialize tile cache");
        release_navigation_mesh();
        return;
    }

    ReadTiles(buffer, true);
    // \todo Shall we send E_NAVIGATION_MESH_REBUILT here?
}

Vector<byte> DynamicNavigationMesh::GetNavigationDataAttr() const
{
    VectorBuffer ret;
    if (navMesh_ && tileCache_)
    {
        ret.WriteBoundingBox(boundingBox_);
        ret.WriteI32(numTilesX_);
        ret.WriteI32(numTilesZ_);

        const dtNavMeshParams* params = navMesh_->getParams();
        ret.Write(params, sizeof(dtNavMeshParams));

        const dtTileCacheParams* tcParams = tileCache_->getParams();
        ret.Write(tcParams, sizeof(dtTileCacheParams));

        for (int z = 0; z < numTilesZ_; ++z)
            for (int x = 0; x < numTilesX_; ++x)
                WriteTiles(ret, x, z);
    }
    return ret.GetBuffer();
}

void DynamicNavigationMesh::SetMaxLayers(unsigned maxLayers)
{
    // Set 3 as a minimum due to the tendency of layers to be constructed inside the hollow space of stacked objects
    // That behavior is unlikely to be expected by the end user
    maxLayers_ = Max(3U, Min(maxLayers, TILECACHE_MAXLAYERS));
}

void DynamicNavigationMesh::WriteTiles(Serializer& dest, int x, int z) const
{
    dtCompressedTileRef tiles[TILECACHE_MAXLAYERS];
    const int ct = tileCache_->getTilesAt(x, z, tiles, maxLayers_);
    for (int i = 0; i < ct; ++i)
    {
        const dtCompressedTile* tile = tileCache_->getTileByRef(tiles[i]);
        if (!tile || !tile->header || !tile->dataSize)
            continue; // Don't write "void-space" tiles
                      // The header conveniently has the majority of the information required
        dest.Write(tile->header, sizeof(dtTileCacheLayerHeader));
        dest.WriteI32(tile->dataSize);
        dest.Write(tile->data, (unsigned)tile->dataSize);
    }
}

bool DynamicNavigationMesh::ReadTiles(Deserializer& source, bool silent)
{
    tileQueue_.Clear();
    while (!source.IsEof())
    {
        dtTileCacheLayerHeader header;      // NOLINT(hicpp-member-init)
        source.Read(&header, sizeof(dtTileCacheLayerHeader));
        const int dataSize = source.ReadI32();

        auto* data = (unsigned char*)dtAlloc(dataSize, DT_ALLOC_PERM);
        if (!data)
        {
            DV_LOGERROR("Could not allocate data for navigation mesh tile");
            return false;
        }

        source.Read(data, (unsigned)dataSize);
        if (dtStatusFailed(tileCache_->addTile(data, dataSize, DT_TILE_FREE_DATA, nullptr)))
        {
            DV_LOGERROR("Failed to add tile");
            dtFree(data);
            return false;
        }

        const IntVector2 tileIdx = IntVector2(header.tx, header.ty);
        if (tileQueue_.Empty() || tileQueue_.Back() != tileIdx)
            tileQueue_.Push(tileIdx);
    }

    for (unsigned i = 0; i < tileQueue_.Size(); ++i)
        tileCache_->buildNavMeshTilesAt(tileQueue_[i].x, tileQueue_[i].y, navMesh_);

    tileCache_->update(0, navMesh_);

    // Send event
    if (!silent)
    {
        for (unsigned i = 0; i < tileQueue_.Size(); ++i)
        {
            using namespace NavigationTileAdded;
            VariantMap& eventData = DV_CONTEXT->GetEventDataMap();
            eventData[P_NODE] = GetNode();
            eventData[P_MESH] = this;
            eventData[P_TILE] = tileQueue_[i];
            SendEvent(E_NAVIGATION_TILE_ADDED, eventData);
        }
    }
    return true;
}

int DynamicNavigationMesh::BuildTile(Vector<NavigationGeometryInfo>& geometryList, int x, int z, TileCacheData* tiles)
{
    DV_PROFILE(BuildNavigationMeshTile);

    tileCache_->removeTile(navMesh_->getTileRefAt(x, z, 0), nullptr, nullptr);

    const BoundingBox tileBoundingBox = GetTileBoundingBox(IntVector2(x, z));

    DynamicNavBuildData build(allocator_.get());

    rcConfig cfg;   // NOLINT(hicpp-member-init)
    memset(&cfg, 0, sizeof cfg);
    cfg.cs = cellSize_;
    cfg.ch = cellHeight_;
    cfg.walkableSlopeAngle = agentMaxSlope_;
    cfg.walkableHeight = (int)ceilf(agentHeight_ / cfg.ch);
    cfg.walkableClimb = (int)floorf(agentMaxClimb_ / cfg.ch);
    cfg.walkableRadius = (int)ceilf(agentRadius_ / cfg.cs);
    cfg.maxEdgeLen = (int)(edgeMaxLength_ / cellSize_);
    cfg.maxSimplificationError = edgeMaxError_;
    cfg.minRegionArea = (int)sqrtf(regionMinSize_);
    cfg.mergeRegionArea = (int)sqrtf(regionMergeSize_);
    cfg.maxVertsPerPoly = 6;
    cfg.tileSize = tileSize_;
    cfg.borderSize = cfg.walkableRadius + 3; // Add padding
    cfg.width = cfg.tileSize + cfg.borderSize * 2;
    cfg.height = cfg.tileSize + cfg.borderSize * 2;
    cfg.detailSampleDist = detailSampleDistance_ < 0.9f ? 0.0f : cellSize_ * detailSampleDistance_;
    cfg.detailSampleMaxError = cellHeight_ * detailSampleMaxError_;

    rcVcopy(cfg.bmin, &tileBoundingBox.min_.x);
    rcVcopy(cfg.bmax, &tileBoundingBox.max_.x);
    cfg.bmin[0] -= cfg.borderSize * cfg.cs;
    cfg.bmin[2] -= cfg.borderSize * cfg.cs;
    cfg.bmax[0] += cfg.borderSize * cfg.cs;
    cfg.bmax[2] += cfg.borderSize * cfg.cs;

    BoundingBox expandedBox(*reinterpret_cast<Vector3*>(cfg.bmin), *reinterpret_cast<Vector3*>(cfg.bmax));
    GetTileGeometry(&build, geometryList, expandedBox);

    if (build.vertices_.Empty() || build.indices_.Empty())
        return 0; // Nothing to do

    build.heightField_ = rcAllocHeightfield();
    if (!build.heightField_)
    {
        DV_LOGERROR("Could not allocate heightfield");
        return 0;
    }

    if (!rcCreateHeightfield(build.ctx_, *build.heightField_, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs,
        cfg.ch))
    {
        DV_LOGERROR("Could not create heightfield");
        return 0;
    }

    unsigned numTriangles = build.indices_.Size() / 3;
    unique_ptr<unsigned char[]> triAreas(new unsigned char[numTriangles]);
    memset(triAreas.get(), 0, numTriangles);

    rcMarkWalkableTriangles(build.ctx_, cfg.walkableSlopeAngle, &build.vertices_[0].x, build.vertices_.Size(),
        &build.indices_[0], numTriangles, triAreas.get());
    rcRasterizeTriangles(build.ctx_, &build.vertices_[0].x, build.vertices_.Size(), &build.indices_[0],
        triAreas.get(), numTriangles, *build.heightField_, cfg.walkableClimb);
    rcFilterLowHangingWalkableObstacles(build.ctx_, cfg.walkableClimb, *build.heightField_);

    rcFilterLedgeSpans(build.ctx_, cfg.walkableHeight, cfg.walkableClimb, *build.heightField_);
    rcFilterWalkableLowHeightSpans(build.ctx_, cfg.walkableHeight, *build.heightField_);

    build.compactHeightField_ = rcAllocCompactHeightfield();
    if (!build.compactHeightField_)
    {
        DV_LOGERROR("Could not allocate create compact heightfield");
        return 0;
    }
    if (!rcBuildCompactHeightfield(build.ctx_, cfg.walkableHeight, cfg.walkableClimb, *build.heightField_,
        *build.compactHeightField_))
    {
        DV_LOGERROR("Could not build compact heightfield");
        return 0;
    }
    if (!rcErodeWalkableArea(build.ctx_, cfg.walkableRadius, *build.compactHeightField_))
    {
        DV_LOGERROR("Could not erode compact heightfield");
        return 0;
    }

    // area volumes
    for (unsigned i = 0; i < build.navAreas_.Size(); ++i)
        rcMarkBoxArea(build.ctx_, &build.navAreas_[i].bounds_.min_.x, &build.navAreas_[i].bounds_.max_.x,
            build.navAreas_[i].areaID_, *build.compactHeightField_);

    if (this->partitionType_ == NAVMESH_PARTITION_WATERSHED)
    {
        if (!rcBuildDistanceField(build.ctx_, *build.compactHeightField_))
        {
            DV_LOGERROR("Could not build distance field");
            return 0;
        }
        if (!rcBuildRegions(build.ctx_, *build.compactHeightField_, cfg.borderSize, cfg.minRegionArea,
            cfg.mergeRegionArea))
        {
            DV_LOGERROR("Could not build regions");
            return 0;
        }
    }
    else
    {
        if (!rcBuildRegionsMonotone(build.ctx_, *build.compactHeightField_, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
        {
            DV_LOGERROR("Could not build monotone regions");
            return 0;
        }
    }

    build.heightFieldLayers_ = rcAllocHeightfieldLayerSet();
    if (!build.heightFieldLayers_)
    {
        DV_LOGERROR("Could not allocate height field layer set");
        return 0;
    }

    if (!rcBuildHeightfieldLayers(build.ctx_, *build.compactHeightField_, cfg.borderSize, cfg.walkableHeight,
        *build.heightFieldLayers_))
    {
        DV_LOGERROR("Could not build height field layers");
        return 0;
    }

    int retCt = 0;
    for (int i = 0; i < build.heightFieldLayers_->nlayers; ++i)
    {
        dtTileCacheLayerHeader header;      // NOLINT(hicpp-member-init)
        header.magic = DT_TILECACHE_MAGIC;
        header.version = DT_TILECACHE_VERSION;
        header.tx = x;
        header.ty = z;
        header.tlayer = i;

        rcHeightfieldLayer* layer = &build.heightFieldLayers_->layers[i];

        // Tile info.
        rcVcopy(header.bmin, layer->bmin);
        rcVcopy(header.bmax, layer->bmax);
        header.width = (unsigned char)layer->width;
        header.height = (unsigned char)layer->height;
        header.minx = (unsigned char)layer->minx;
        header.maxx = (unsigned char)layer->maxx;
        header.miny = (unsigned char)layer->miny;
        header.maxy = (unsigned char)layer->maxy;
        header.hmin = (unsigned short)layer->hmin;
        header.hmax = (unsigned short)layer->hmax;

        if (dtStatusFailed(
            dtBuildTileCacheLayer(compressor_.get(), &header, layer->heights, layer->areas, layer->cons,
                &(tiles[retCt].data), &tiles[retCt].dataSize)))
        {
            DV_LOGERROR("Failed to build tile cache layers");
            return 0;
        }
        else
            ++retCt;
    }

    // Send a notification of the rebuild of this tile to anyone interested
    {
        using namespace NavigationAreaRebuilt;
        VariantMap& eventData = DV_CONTEXT->GetEventDataMap();
        eventData[P_NODE] = GetNode();
        eventData[P_MESH] = this;
        eventData[P_BOUNDSMIN] = Variant(tileBoundingBox.min_);
        eventData[P_BOUNDSMAX] = Variant(tileBoundingBox.max_);
        SendEvent(E_NAVIGATION_AREA_REBUILT, eventData);
    }

    return retCt;
}

unsigned DynamicNavigationMesh::build_tiles(Vector<NavigationGeometryInfo>& geometryList, const IntVector2& from, const IntVector2& to)
{
    unsigned numTiles = 0;

    for (int z = from.y; z <= to.y; ++z)
    {
        for (int x = from.x; x <= to.x; ++x)
        {
            dtCompressedTileRef existing[TILECACHE_MAXLAYERS];
            const int existingCt = tileCache_->getTilesAt(x, z, existing, maxLayers_);
            for (int i = 0; i < existingCt; ++i)
            {
                unsigned char* data = nullptr;
                if (!dtStatusFailed(tileCache_->removeTile(existing[i], &data, nullptr)) && data != nullptr)
                    dtFree(data);
            }

            TileCacheData tiles[TILECACHE_MAXLAYERS];
            int layerCt = BuildTile(geometryList, x, z, tiles);
            for (int i = 0; i < layerCt; ++i)
            {
                dtCompressedTileRef tileRef;
                int status = tileCache_->addTile(tiles[i].data, tiles[i].dataSize, DT_COMPRESSEDTILE_FREE_DATA, &tileRef);
                if (dtStatusFailed((dtStatus)status))
                {
                    dtFree(tiles[i].data);
                    tiles[i].data = nullptr;
                }
                else
                {
                    tileCache_->buildNavMeshTile(tileRef, navMesh_);
                    ++numTiles;
                }
            }
        }
    }

    return numTiles;
}

Vector<OffMeshConnection*> DynamicNavigationMesh::CollectOffMeshConnections(const BoundingBox& bounds)
{
    Vector<OffMeshConnection*> connections;
    node_->GetComponents<OffMeshConnection>(connections, true);
    for (unsigned i = 0; i < connections.Size(); ++i)
    {
        OffMeshConnection* connection = connections[i];
        if (!(connection->IsEnabledEffective() && connection->GetEndPoint()))
        {
            // discard this connection
            connections.Erase(i);
            --i;
        }
    }

    return connections;
}

void DynamicNavigationMesh::release_navigation_mesh()
{
    NavigationMesh::release_navigation_mesh();
    ReleaseTileCache();
}

void DynamicNavigationMesh::ReleaseTileCache()
{
    dtFreeTileCache(tileCache_);
    tileCache_ = nullptr;
}

void DynamicNavigationMesh::OnSceneSet(Scene* scene)
{
    // Subscribe to the scene subsystem update, which will trigger the tile cache to update the nav mesh
    if (scene)
        subscribe_to_event(scene, E_SCENESUBSYSTEMUPDATE, DV_HANDLER(DynamicNavigationMesh, HandleSceneSubsystemUpdate));
    else
        unsubscribe_from_event(E_SCENESUBSYSTEMUPDATE);
}

void DynamicNavigationMesh::AddObstacle(Obstacle* obstacle, bool silent)
{
    if (tileCache_)
    {
        float pos[3];
        Vector3 obsPos = obstacle->GetNode()->GetWorldPosition();
        rcVcopy(pos, &obsPos.x);
        dtObstacleRef refHolder;

        // Because dtTileCache doesn't process obstacle requests while updating tiles
        // it's necessary update until sufficient request space is available
        while (tileCache_->isObstacleQueueFull())
            tileCache_->update(1, navMesh_);

        if (dtStatusFailed(tileCache_->addObstacle(pos, obstacle->GetRadius(), obstacle->GetHeight(), &refHolder)))
        {
            DV_LOGERROR("Failed to add obstacle");
            return;
        }
        obstacle->obstacleId_ = refHolder;
        assert(refHolder > 0);

        if (!silent)
        {
            using namespace NavigationObstacleAdded;
            VariantMap& eventData = DV_CONTEXT->GetEventDataMap();
            eventData[P_NODE] = obstacle->GetNode();
            eventData[P_OBSTACLE] = obstacle;
            eventData[P_POSITION] = obstacle->GetNode()->GetWorldPosition();
            eventData[P_RADIUS] = obstacle->GetRadius();
            eventData[P_HEIGHT] = obstacle->GetHeight();
            SendEvent(E_NAVIGATION_OBSTACLE_ADDED, eventData);
        }
    }
}

void DynamicNavigationMesh::ObstacleChanged(Obstacle* obstacle)
{
    if (tileCache_)
    {
        RemoveObstacle(obstacle, true);
        AddObstacle(obstacle, true);
    }
}

void DynamicNavigationMesh::RemoveObstacle(Obstacle* obstacle, bool silent)
{
    if (tileCache_ && obstacle->obstacleId_ > 0)
    {
        // Because dtTileCache doesn't process obstacle requests while updating tiles
        // it's necessary update until sufficient request space is available
        while (tileCache_->isObstacleQueueFull())
            tileCache_->update(1, navMesh_);

        if (dtStatusFailed(tileCache_->removeObstacle(obstacle->obstacleId_)))
        {
            DV_LOGERROR("Failed to remove obstacle");
            return;
        }
        obstacle->obstacleId_ = 0;
        // Require a node in order to send an event
        if (!silent && obstacle->GetNode())
        {
            using namespace NavigationObstacleRemoved;
            VariantMap& eventData = DV_CONTEXT->GetEventDataMap();
            eventData[P_NODE] = obstacle->GetNode();
            eventData[P_OBSTACLE] = obstacle;
            eventData[P_POSITION] = obstacle->GetNode()->GetWorldPosition();
            eventData[P_RADIUS] = obstacle->GetRadius();
            eventData[P_HEIGHT] = obstacle->GetHeight();
            SendEvent(E_NAVIGATION_OBSTACLE_REMOVED, eventData);
        }
    }
}

void DynamicNavigationMesh::HandleSceneSubsystemUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace SceneSubsystemUpdate;

    if (tileCache_ && navMesh_ && IsEnabledEffective())
        tileCache_->update(eventData[P_TIMESTEP].GetFloat(), navMesh_);
}

}
