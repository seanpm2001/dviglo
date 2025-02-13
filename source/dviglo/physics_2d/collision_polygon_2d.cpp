// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#include "../core/context.h"
#include "../io/memory_buffer.h"
#include "../io/vector_buffer.h"
#include "collision_polygon_2d.h"
#include "physics_utils_2d.h"

#include "../common/debug_new.h"

namespace dviglo
{

extern const char* PHYSICS2D_CATEGORY;

CollisionPolygon2D::CollisionPolygon2D()
{
    fixtureDef_.shape = &polygon_shape_;
}

CollisionPolygon2D::~CollisionPolygon2D() = default;

void CollisionPolygon2D::register_object()
{
    DV_CONTEXT->RegisterFactory<CollisionPolygon2D>(PHYSICS2D_CATEGORY);

    DV_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, true, AM_DEFAULT);
    DV_COPY_BASE_ATTRIBUTES(CollisionShape2D);
    DV_ACCESSOR_ATTRIBUTE("Vertices", GetVerticesAttr, SetVerticesAttr, Variant::emptyBuffer, AM_FILE);
}

void CollisionPolygon2D::SetVertexCount(i32 count)
{
    assert(count >= 0);
    vertices_.Resize(count);
}

void CollisionPolygon2D::SetVertex(i32 index, const Vector2& vertex)
{
    assert(index >= 0);

    if (index >= vertices_.Size())
        return;

    vertices_[index] = vertex;

    if (index == vertices_.Size() - 1)
    {
        MarkNetworkUpdate();
        RecreateFixture();
    }
}

void CollisionPolygon2D::SetVertices(const Vector<Vector2>& vertices)
{
    vertices_ = vertices;

    MarkNetworkUpdate();
    RecreateFixture();
}

void CollisionPolygon2D::SetVerticesAttr(const Vector<byte>& value)
{
    if (value.Empty())
        return;

    Vector<Vector2> vertices;

    MemoryBuffer buffer(value);
    while (!buffer.IsEof())
        vertices.Push(buffer.ReadVector2());

    SetVertices(vertices);
}

Vector<byte> CollisionPolygon2D::GetVerticesAttr() const
{
    VectorBuffer ret;

    for (const Vector2& vertex : vertices_)
        ret.WriteVector2(vertex);

    return ret.GetBuffer();
}

void CollisionPolygon2D::ApplyNodeWorldScale()
{
    RecreateFixture();
}

void CollisionPolygon2D::RecreateFixture()
{
    ReleaseFixture();

    if (vertices_.Size() < 3)
        return;

    Vector<b2Vec2> b2Vertices;
    i32 count = vertices_.Size();
    b2Vertices.Resize(count);

    Vector2 worldScale(cachedWorldScale_.x, cachedWorldScale_.y);
    for (i32 i = 0; i < count; ++i)
        b2Vertices[i] = ToB2Vec2(vertices_[i] * worldScale);

    polygon_shape_.Set(&b2Vertices[0], count);

    CreateFixture();
}

}
