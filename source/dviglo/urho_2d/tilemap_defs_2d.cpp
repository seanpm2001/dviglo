// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#include "../resource/xml_element.h"
#include "../resource/json_file.h"
#include "tilemap_defs_2d.h"

#include "../common/debug_new.h"

namespace dviglo
{

float TileMapInfo2D::GetMapWidth() const
{
    return width_ * tileWidth_;
}

float TileMapInfo2D::GetMapHeight() const
{
    if (orientation_ == O_STAGGERED)
        return (height_ + 1) * 0.5f * tileHeight_;
    else if (orientation_ == O_HEXAGONAL)
        return (height_) * 0.5f * (tileHeight_ + tileHeight_ * 0.5f);
    else
        return height_ * tileHeight_;
}

Vector2 TileMapInfo2D::ConvertPosition(const Vector2& position) const
{
    switch (orientation_)
    {
    case O_ISOMETRIC:
        {
            Vector2 index = position * PIXEL_SIZE / tileHeight_;
            return Vector2((width_ + index.x - index.y) * tileWidth_ * 0.5f,
                (height_ * 2.0f - index.x - index.y) * tileHeight_ * 0.5f);
        }

    case O_STAGGERED:
        return Vector2(position.x * PIXEL_SIZE, GetMapHeight() - position.y * PIXEL_SIZE);

    case O_HEXAGONAL:
    case O_ORTHOGONAL:
    default:
        return Vector2(position.x * PIXEL_SIZE, GetMapHeight() - position.y * PIXEL_SIZE);
    }
}

Vector2 TileMapInfo2D::tile_index_to_position(int x, int y) const
{
    switch (orientation_)
    {
    case O_ISOMETRIC:
        return Vector2((width_ + x - y - 1) * tileWidth_ * 0.5f, (height_ * 2 - x - y - 2) * tileHeight_ * 0.5f);

    case O_STAGGERED:
        if (y % 2 == 0)
            return Vector2(x * tileWidth_, (height_ - 1 - y) * 0.5f * tileHeight_);
        else
            return Vector2((x + 0.5f) * tileWidth_, (height_ - 1 - y) * 0.5f * tileHeight_);

    case O_HEXAGONAL:
        if (y % 2 == 0)
            return Vector2(x * tileWidth_, (height_ - 1 - y) * 0.75f * tileHeight_);
        else
            return Vector2((x + 0.5f) * tileWidth_, (height_ - 1 - y)  * 0.75f * tileHeight_);

    case O_ORTHOGONAL:
    default:
        return Vector2(x * tileWidth_, (height_ - 1 - y) * tileHeight_);
    }
}

bool TileMapInfo2D::position_to_tile_index(int& x, int& y, const Vector2& position) const
{
    switch (orientation_)
    {
    case O_ISOMETRIC:
    {
        float ox = position.x / tileWidth_ - height_ * 0.5f;
        float oy = position.y / tileHeight_;

        x = (int)(width_ - oy + ox);
        y = (int)(height_ - oy - ox);
    }
        break;

    case O_STAGGERED:
        y = (int)(height_ - 1 - position.y * 2.0f / tileHeight_);
        if (y % 2 == 0)
            x = (int)(position.x / tileWidth_);
        else
            x = (int)(position.x / tileWidth_ - 0.5f);

        break;

    case O_HEXAGONAL:
        y = (int)(height_ - 1 - position.y / 0.75f / tileHeight_);
        if (y % 2 == 0)
            x = (int)(position.x / tileWidth_);
        else
            x = (int)(position.x / tileWidth_ - 0.75f);
        break;

    case O_ORTHOGONAL:
    default:
        x = (int)(position.x / tileWidth_);
        y = height_ - 1 - int(position.y / tileHeight_);
        break;

    }

    return x >= 0 && x < width_ && y >= 0 && y < height_;
}

PropertySet2D::PropertySet2D() = default;

PropertySet2D::~PropertySet2D() = default;

void PropertySet2D::Load(const XmlElement& element)
{
    assert(element.GetName() == "properties");
    for (XmlElement propertyElem = element.GetChild("property"); propertyElem; propertyElem = propertyElem.GetNext("property"))
        nameToValueMapping_[propertyElem.GetAttribute("name")] = propertyElem.GetAttribute("value");
}

bool PropertySet2D::HasProperty(const String& name) const
{
    return nameToValueMapping_.Find(name) != nameToValueMapping_.End();
}

const String& PropertySet2D::GetProperty(const String& name) const
{
    HashMap<String, String>::ConstIterator i = nameToValueMapping_.Find(name);
    if (i == nameToValueMapping_.End())
        return String::EMPTY;

    return i->second_;
}

Tile2D::Tile2D() :
    gid_(0)
{
}

Sprite2D* Tile2D::GetSprite() const
{
    return sprite_;
}

bool Tile2D::HasProperty(const String& name) const
{
    if (!propertySet_)
        return false;
    return propertySet_->HasProperty(name);
}

const String& Tile2D::GetProperty(const String& name) const
{
    if (!propertySet_)
        return String::EMPTY;

    return propertySet_->GetProperty(name);
}

TileMapObject2D::TileMapObject2D() = default;

unsigned TileMapObject2D::GetNumPoints() const
{
    return points_.Size();
}

const Vector2& TileMapObject2D::GetPoint(unsigned index) const
{
    if (index >= points_.Size())
        return Vector2::ZERO;

    return points_[index];
}

Sprite2D* TileMapObject2D::GetTileSprite() const
{
    return sprite_;
}

bool TileMapObject2D::HasProperty(const String& name) const
{
    if (!propertySet_)
        return false;
    return propertySet_->HasProperty(name);
}

const String& TileMapObject2D::GetProperty(const String& name) const
{
    if (!propertySet_)
        return String::EMPTY;
    return propertySet_->GetProperty(name);
}

}
