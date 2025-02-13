// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#pragma once

#include "../core/object.h"
#include "../graphics_api/graphics_defs.h"

#include <memory>

namespace dviglo
{

class IndexBuffer;
class Ray;
class Graphics;
class VertexBuffer;

/// Defines one or more vertex buffers, an index buffer and a draw range.
class DV_API Geometry : public Object
{
    DV_OBJECT(Geometry);

public:
    /// Construct with one empty vertex buffer.
    explicit Geometry();
    /// Destruct.
    ~Geometry() override;

    /// Set number of vertex buffers.
    bool SetNumVertexBuffers(i32 num);
    /// Set a vertex buffer by index.
    bool SetVertexBuffer(i32 index, std::shared_ptr<VertexBuffer> buffer);
    /// Set the index buffer.
    void SetIndexBuffer(std::shared_ptr<IndexBuffer> buffer);
    /// Set the draw range.
    bool SetDrawRange(PrimitiveType type, i32 indexStart, i32 indexCount, bool getUsedVertexRange = true);
    /// Set the draw range.
    bool SetDrawRange(PrimitiveType type, i32 indexStart, i32 indexCount, i32 vertexStart, i32 vertexCount,
        bool checkIllegal = true);
    /// Set the LOD distance.
    void SetLodDistance(float distance);
    /// Override raw vertex data to be returned for CPU-side operations.
    void SetRawVertexData(const std::shared_ptr<byte[]>& data, const Vector<VertexElement>& elements);
    /// Override raw vertex data to be returned for CPU-side operations using a legacy vertex bitmask.
    void SetRawVertexData(const std::shared_ptr<byte[]>& data, VertexElements elementMask);
    /// Override raw index data to be returned for CPU-side operations.
    void SetRawIndexData(const std::shared_ptr<byte[]>& data, i32 indexSize);
    /// Draw.
    void Draw();

    /// Return all vertex buffers.
    const Vector<std::shared_ptr<VertexBuffer>>& GetVertexBuffers() const { return vertexBuffers_; }

    /// Return number of vertex buffers.
    i32 GetNumVertexBuffers() const { return vertexBuffers_.Size(); }

    /// Return vertex buffer by index.
    std::shared_ptr<VertexBuffer> GetVertexBuffer(i32 index) const;

    /// Return the index buffer.
    std::shared_ptr<IndexBuffer> GetIndexBuffer() const { return indexBuffer_; }

    /// Return primitive type.
    PrimitiveType GetPrimitiveType() const { return primitiveType_; }

    /// Return start index.
    i32 GetIndexStart() const { return indexStart_; }

    /// Return number of indices.
    i32 GetIndexCount() const { return indexCount_; }

    /// Return first used vertex.
    i32 GetVertexStart() const { return vertexStart_; }

    /// Return number of used vertices.
    i32 GetVertexCount() const { return vertexCount_; }

    /// Return LOD distance.
    float GetLodDistance() const { return lodDistance_; }

    /// Return buffers' combined hash value for state sorting.
    u16 GetBufferHash() const;
    /// Return raw vertex and index data for CPU operations, or null pointers if not available. Will return data of the first vertex buffer if override data not set.
    void GetRawData(const byte*& vertexData, i32& vertexSize, const byte*& indexData, i32& indexSize, const Vector<VertexElement>*& elements) const;
    /// Return raw vertex and index data for CPU operations, or null pointers if not available. Will return data of the first vertex buffer if override data not set.
    void GetRawDataShared(std::shared_ptr<byte[]>& vertexData, i32& vertexSize, std::shared_ptr<byte[]>& indexData,
        i32& indexSize, const Vector<VertexElement>*& elements) const;
    /// Return ray hit distance or infinity if no hit. Requires raw data to be set. Optionally return hit normal and hit uv coordinates at intersect point.
    float GetHitDistance(const Ray& ray, Vector3* outNormal = nullptr, Vector2* outUV = nullptr) const;
    /// Return whether or not the ray is inside geometry.
    bool IsInside(const Ray& ray) const;

    /// Return whether has empty draw range.
    bool IsEmpty() const { return indexCount_ == 0 && vertexCount_ == 0; }

private:
    /// Vertex buffers.
    Vector<std::shared_ptr<VertexBuffer>> vertexBuffers_;
    /// Index buffer.
    std::shared_ptr<IndexBuffer> indexBuffer_;
    /// Primitive type.
    PrimitiveType primitiveType_;
    /// Start index.
    i32 indexStart_;
    /// Number of indices.
    i32 indexCount_;
    /// First used vertex.
    i32 vertexStart_;
    /// Number of used vertices.
    i32 vertexCount_;
    /// LOD distance.
    float lodDistance_;
    /// Raw vertex data elements.
    Vector<VertexElement> rawElements_;
    /// Raw vertex data override.
    std::shared_ptr<byte[]> rawVertexData_;
    /// Raw index data override.
    std::shared_ptr<byte[]> rawIndexData_;
    /// Raw vertex data override size.
    i32 rawVertexSize_;
    /// Raw index data override size.
    i32 rawIndexSize_;
};

}
