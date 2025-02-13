// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#include "geometry.h"
#include "graphics.h"
#include "../graphics_api/index_buffer.h"
#include "../graphics_api/vertex_buffer.h"
#include "../io/log.h"
#include "../math/ray.h"

#include "../common/debug_new.h"

using namespace std;

namespace dviglo
{

Geometry::Geometry() :
    primitiveType_(TRIANGLE_LIST),
    indexStart_(0),
    indexCount_(0),
    vertexStart_(0),
    vertexCount_(0),
    rawVertexSize_(0),
    rawIndexSize_(0),
    lodDistance_(0.0f)
{
    SetNumVertexBuffers(1);
}

Geometry::~Geometry() = default;

bool Geometry::SetNumVertexBuffers(i32 num)
{
    assert(num >= 0);

    if (num >= MAX_VERTEX_STREAMS)
    {
        DV_LOGERROR("Too many vertex streams");
        return false;
    }

    i32 oldSize = vertexBuffers_.Size(); // TODO: unused
    vertexBuffers_.Resize(num);

    return true;
}

bool Geometry::SetVertexBuffer(i32 index, shared_ptr<VertexBuffer> buffer)
{
    assert(index >= 0);

    if (index >= vertexBuffers_.Size())
    {
        DV_LOGERROR("Stream index out of bounds");
        return false;
    }

    vertexBuffers_[index] = buffer;
    return true;
}

void Geometry::SetIndexBuffer(shared_ptr<IndexBuffer> buffer)
{
    indexBuffer_ = buffer;
}

bool Geometry::SetDrawRange(PrimitiveType type, i32 indexStart, i32 indexCount, bool getUsedVertexRange/* = true*/)
{
    assert(indexStart >= 0);
    assert(indexCount >= 0);

    if (!indexBuffer_ && !rawIndexData_)
    {
        DV_LOGERROR("Null index buffer and no raw index data, can not define indexed draw range");
        return false;
    }
    if (indexBuffer_ && indexStart + indexCount > indexBuffer_->GetIndexCount())
    {
        DV_LOGERROR("Illegal draw range " + String(indexStart) + " to " + String(indexStart + indexCount - 1) + ", index buffer has " +
                 String(indexBuffer_->GetIndexCount()) + " indices");
        return false;
    }

    primitiveType_ = type;
    indexStart_ = indexStart;
    indexCount_ = indexCount;

    // Get min.vertex index and num of vertices from index buffer. If it fails, use full range as fallback
    if (indexCount)
    {
        vertexStart_ = 0;
        vertexCount_ = vertexBuffers_[0] ? vertexBuffers_[0]->GetVertexCount() : 0;

        if (getUsedVertexRange && indexBuffer_)
            indexBuffer_->GetUsedVertexRange(indexStart_, indexCount_, vertexStart_, vertexCount_);
    }
    else
    {
        vertexStart_ = 0;
        vertexCount_ = 0;
    }

    return true;
}

bool Geometry::SetDrawRange(PrimitiveType type, i32 indexStart, i32 indexCount, i32 vertexStart, i32 vertexCount,
    bool checkIllegal)
{
    assert(indexStart >= 0);
    assert(indexCount >= 0);
    assert(vertexStart >= 0);
    assert(vertexCount >= 0);

    if (indexBuffer_)
    {
        // We can allow setting an illegal draw range now if the caller guarantees to resize / fill the buffer later
        if (checkIllegal && indexStart + indexCount > indexBuffer_->GetIndexCount())
        {
            DV_LOGERROR("Illegal draw range " + String(indexStart) + " to " + String(indexStart + indexCount - 1) +
                     ", index buffer has " + String(indexBuffer_->GetIndexCount()) + " indices");
            return false;
        }
    }
    else if (!rawIndexData_)
    {
        indexStart = 0;
        indexCount = 0;
    }

    primitiveType_ = type;
    indexStart_ = indexStart;
    indexCount_ = indexCount;
    vertexStart_ = vertexStart;
    vertexCount_ = vertexCount;

    return true;
}

void Geometry::SetLodDistance(float distance)
{
    if (distance < 0.0f)
        distance = 0.0f;

    lodDistance_ = distance;
}

void Geometry::SetRawVertexData(const shared_ptr<byte[]>& data, const Vector<VertexElement>& elements)
{
    rawVertexData_ = data;
    rawVertexSize_ = VertexBuffer::GetVertexSize(elements);
    rawElements_ = elements;
}

void Geometry::SetRawVertexData(const shared_ptr<byte[]>& data, VertexElements elementMask)
{
    rawVertexData_ = data;
    rawVertexSize_ = VertexBuffer::GetVertexSize(elementMask);
    rawElements_ = VertexBuffer::GetElements(elementMask);
}

void Geometry::SetRawIndexData(const shared_ptr<byte[]>& data, i32 indexSize)
{
    assert(indexSize >= 0);
    rawIndexData_ = data;
    rawIndexSize_ = indexSize;
}

void Geometry::Draw()
{
    Graphics* graphics = DV_GRAPHICS;

    if (indexBuffer_ && indexCount_ > 0)
    {
        graphics->SetIndexBuffer(indexBuffer_.get());
        graphics->SetVertexBuffers(vertexBuffers_);
        graphics->Draw(primitiveType_, indexStart_, indexCount_, vertexStart_, vertexCount_);
    }
    else if (vertexCount_ > 0)
    {
        graphics->SetVertexBuffers(vertexBuffers_);
        graphics->Draw(primitiveType_, vertexStart_, vertexCount_);
    }
}

shared_ptr<VertexBuffer> Geometry::GetVertexBuffer(i32 index) const
{
    assert(index >= 0);
    return index < vertexBuffers_.Size() ? vertexBuffers_[index] : nullptr;
}

u16 Geometry::GetBufferHash() const // TODO: Поменять типа на hash16?
{
    u16 hash = 0;

    for (i32 i = 0; i < vertexBuffers_.Size(); ++i)
    {
        VertexBuffer* vBuf = vertexBuffers_[i].get();
        hash += *((u16*)&vBuf);
    }

    IndexBuffer* iBuf = indexBuffer_.get();
    hash += *((u16*)&iBuf);

    return hash;
}

void Geometry::GetRawData(const byte*& vertexData, i32& vertexSize, const byte*& indexData,
    i32& indexSize, const Vector<VertexElement>*& elements) const
{
    if (rawVertexData_)
    {
        vertexData = rawVertexData_.get();
        vertexSize = rawVertexSize_;
        elements = &rawElements_;
    }
    else if (vertexBuffers_.Size() && vertexBuffers_[0])
    {
        vertexData = vertexBuffers_[0]->GetShadowData();
        vertexSize = vertexBuffers_[0]->GetVertexSize();
        elements = &vertexBuffers_[0]->GetElements();
    }
    else
    {
        vertexData = nullptr;
        vertexSize = 0;
        elements = nullptr;
    }

    if (rawIndexData_)
    {
        indexData = rawIndexData_.get();
        indexSize = rawIndexSize_;
    }
    else
    {
        if (indexBuffer_)
        {
            indexData = indexBuffer_->GetShadowData();
            if (indexData)
                indexSize = indexBuffer_->GetIndexSize();
            else
                indexSize = 0;
        }
        else
        {
            indexData = nullptr;
            indexSize = 0;
        }
    }
}

void Geometry::GetRawDataShared(shared_ptr<byte[]>& vertexData, i32& vertexSize,
    shared_ptr<byte[]>& indexData, i32& indexSize, const Vector<VertexElement>*& elements) const
{
    if (rawVertexData_)
    {
        vertexData = rawVertexData_;
        vertexSize = rawVertexSize_;
        elements = &rawElements_;
    }
    else if (vertexBuffers_.Size() && vertexBuffers_[0])
    {
        vertexData = vertexBuffers_[0]->GetShadowDataShared();
        vertexSize = vertexBuffers_[0]->GetVertexSize();
        elements = &vertexBuffers_[0]->GetElements();
    }
    else
    {
        vertexData = nullptr;
        vertexSize = 0;
        elements = nullptr;
    }

    if (rawIndexData_)
    {
        indexData = rawIndexData_;
        indexSize = rawIndexSize_;
    }
    else
    {
        if (indexBuffer_)
        {
            indexData = indexBuffer_->GetShadowDataShared();
            if (indexData)
                indexSize = indexBuffer_->GetIndexSize();
            else
                indexSize = 0;
        }
        else
        {
            indexData = nullptr;
            indexSize = 0;
        }
    }
}

float Geometry::GetHitDistance(const Ray& ray, Vector3* outNormal, Vector2* outUV) const
{
    const byte* vertexData;
    const byte* indexData;
    i32 vertexSize;
    i32 indexSize;
    const Vector<VertexElement>* elements;

    GetRawData(vertexData, vertexSize, indexData, indexSize, elements);

    if (!vertexData || !elements || VertexBuffer::GetElementOffset(*elements, TYPE_VECTOR3, SEM_POSITION) != 0)
        return M_INFINITY;

    i32 uvOffset = VertexBuffer::GetElementOffset(*elements, TYPE_VECTOR2, SEM_TEXCOORD);

    if (outUV && uvOffset == NINDEX)
    {
        // requested UV output, but no texture data in vertex buffer
        DV_LOGWARNING("Illegal GetHitDistance call: UV return requested on vertex buffer without UV coords");
        *outUV = Vector2::ZERO;
        outUV = nullptr;
    }

    return indexData ? ray.HitDistance(vertexData, vertexSize, indexData, indexSize, indexStart_, indexCount_, outNormal, outUV,
        uvOffset) : ray.HitDistance(vertexData, vertexSize, vertexStart_, vertexCount_, outNormal, outUV, uvOffset);
}

bool Geometry::IsInside(const Ray& ray) const
{
    const byte* vertexData;
    const byte* indexData;
    i32 vertexSize;
    i32 indexSize;
    const Vector<VertexElement>* elements;

    GetRawData(vertexData, vertexSize, indexData, indexSize, elements);

    return vertexData ? (indexData ? ray.InsideGeometry(vertexData, vertexSize, indexData, indexSize, indexStart_, indexCount_) :
                         ray.InsideGeometry(vertexData, vertexSize, vertexStart_, vertexCount_)) : false;
}

}
