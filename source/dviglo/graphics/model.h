// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#pragma once

#include "../containers/ptr.h"
#include "../graphics_api/graphics_defs.h"
#include "../math/bounding_box.h"
#include "../resource/resource.h"
#include "skeleton.h"

namespace dviglo
{

class Geometry;
class IndexBuffer;
class Graphics;
class VertexBuffer;

/// Vertex buffer morph data.
struct VertexBufferMorph
{
    /// Vertex elements.
    VertexElements elementMask_;
    /// Number of vertices.
    i32 vertexCount_;
    /// Morphed vertices data size as bytes.
    i32 dataSize_;
    /// Morphed vertices. Stored packed as <index, data> pairs.
    std::shared_ptr<byte[]> morphData_;
};

/// Definition of a model's vertex morph.
struct ModelMorph
{
    /// Morph name.
    String name_;
    /// Morph name hash.
    StringHash nameHash_;
    /// Current morph weight.
    float weight_;
    /// Morph data per vertex buffer.
    HashMap<i32, VertexBufferMorph> buffers_;
};

/// Description of vertex buffer data for asynchronous loading.
struct VertexBufferDesc
{
    /// Vertex count.
    i32 vertexCount_;
    /// Vertex declaration.
    Vector<VertexElement> vertexElements_;
    /// Vertex data size.
    i32 dataSize_;
    /// Vertex data.
    std::unique_ptr<byte[]> data_; // TODO: std::shared_ptr<byte[]> ?
};

/// Description of index buffer data for asynchronous loading.
struct IndexBufferDesc
{
    /// Index count.
    i32 indexCount_;
    /// Index size.
    i32 indexSize_;
    /// Index data size.
    i32 dataSize_;
    /// Index data.
    std::unique_ptr<byte[]> data_; // TODO: std::shared_ptr<byte[]> ?
};

/// Description of a geometry for asynchronous loading.
struct GeometryDesc
{
    /// Primitive type.
    PrimitiveType type_;
    /// Vertex buffer ref.
    i32 vbRef_;
    /// Index buffer ref.
    i32 ibRef_;
    /// Index start.
    i32 indexStart_;
    /// Index count.
    i32 indexCount_;
};

/// 3D model resource.
class DV_API Model : public ResourceWithMetadata
{
    DV_OBJECT(Model);

public:
    /// Construct.
    explicit Model();
    /// Destruct.
    ~Model() override;
    /// Register object factory.
    static void register_object();

    /// Load resource from stream. May be called from a worker thread. Return true if successful.
    bool begin_load(Deserializer& source) override;
    /// Finish resource loading. Always called from the main thread. Return true if successful.
    bool end_load() override;
    /// Save resource. Return true if successful.
    bool Save(Serializer& dest) const override;

    /// Set local-space bounding box.
    void SetBoundingBox(const BoundingBox& box);
    /// Set vertex buffers and their morph ranges.
    bool SetVertexBuffers(const Vector<std::shared_ptr<VertexBuffer>>& buffers, const Vector<i32>& morphRangeStarts,
        const Vector<i32>& morphRangeCounts);
    /// Set index buffers.
    bool SetIndexBuffers(const Vector<std::shared_ptr<IndexBuffer>>& buffers);
    /// Set number of geometries.
    void SetNumGeometries(i32 num);
    /// Set number of LOD levels in a geometry.
    bool SetNumGeometryLodLevels(i32 index, i32 num);
    /// Set geometry.
    bool SetGeometry(i32 index, i32 lodLevel, std::shared_ptr<Geometry>& geometry);
    /// Set geometry center.
    bool SetGeometryCenter(i32 index, const Vector3& center);
    /// Set skeleton.
    void SetSkeleton(const Skeleton& skeleton);
    /// Set bone mappings when model has more bones than the skinning shader can handle.
    void SetGeometryBoneMappings(const Vector<Vector<i32>>& geometryBoneMappings);
    /// Set vertex morphs.
    void SetMorphs(const Vector<ModelMorph>& morphs);
    /// Clone the model. The geometry data is deep-copied and can be modified in the clone without affecting the original.
    SharedPtr<Model> Clone(const String& cloneName = String::EMPTY) const;

    /// Return bounding box.
    const BoundingBox& GetBoundingBox() const { return boundingBox_; }

    /// Return skeleton.
    Skeleton& GetSkeleton() { return skeleton_; }

    /// Return vertex buffers.
    const Vector<std::shared_ptr<VertexBuffer>>& GetVertexBuffers() const { return vertexBuffers_; }

    /// Return index buffers.
    const Vector<std::shared_ptr<IndexBuffer>>& GetIndexBuffers() const { return indexBuffers_; }

    /// Return number of geometries.
    i32 GetNumGeometries() const { return geometries_.Size(); }

    /// Return number of LOD levels in geometry.
    i32 GetNumGeometryLodLevels(i32 index) const;

    /// Return geometry pointers.
    const Vector<Vector<std::shared_ptr<Geometry>>>& GetGeometries() const { return geometries_; }

    /// Return geometry center points.
    const Vector<Vector3>& GetGeometryCenters() const { return geometryCenters_; }

    /// Return geometry by index and LOD level. The LOD level is clamped if out of range.
    Geometry* GetGeometry(i32 index, i32 lodLevel) const;

    /// Return geometry center by index.
    const Vector3& GetGeometryCenter(i32 index) const
    {
        assert(index >= 0);
        return index < geometryCenters_.Size() ? geometryCenters_[index] : Vector3::ZERO;
    }

    /// Return geometery bone mappings.
    const Vector<Vector<i32>>& GetGeometryBoneMappings() const { return geometryBoneMappings_; }

    /// Return vertex morphs.
    const Vector<ModelMorph>& GetMorphs() const { return morphs_; }

    /// Return number of vertex morphs.
    i32 GetNumMorphs() const { return morphs_.Size(); }

    /// Return vertex morph by index.
    const ModelMorph* GetMorph(i32 index) const;
    /// Return vertex morph by name.
    const ModelMorph* GetMorph(const String& name) const;
    /// Return vertex morph by name hash.
    const ModelMorph* GetMorph(StringHash nameHash) const;
    /// Return vertex buffer morph range start.
    i32 GetMorphRangeStart(i32 bufferIndex) const;
    /// Return vertex buffer morph range vertex count.
    i32 GetMorphRangeCount(i32 bufferIndex) const;

private:
    /// Bounding box.
    BoundingBox boundingBox_;
    /// Skeleton.
    Skeleton skeleton_;
    /// Vertex buffers.
    Vector<std::shared_ptr<VertexBuffer>> vertexBuffers_;
    /// Index buffers.
    Vector<std::shared_ptr<IndexBuffer>> indexBuffers_;
    /// Geometries.
    Vector<Vector<std::shared_ptr<Geometry>>> geometries_;
    /// Geometry bone mappings.
    Vector<Vector<i32>> geometryBoneMappings_;
    /// Geometry centers.
    Vector<Vector3> geometryCenters_;
    /// Vertex morphs.
    Vector<ModelMorph> morphs_;
    /// Vertex buffer morph range start.
    Vector<i32> morphRangeStarts_;
    /// Vertex buffer morph range vertex count.
    Vector<i32> morphRangeCounts_;
    /// Vertex buffer data for asynchronous loading.
    Vector<VertexBufferDesc> loadVBData_;
    /// Index buffer data for asynchronous loading.
    Vector<IndexBufferDesc> loadIBData_;
    /// Geometry definitions for asynchronous loading.
    Vector<Vector<GeometryDesc>> loadGeometries_;
};

} // namespace dviglo
