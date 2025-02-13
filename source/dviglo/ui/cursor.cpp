// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#include "../core/context.h"
#include "../graphics_api/texture_2d.h"
#include "../input/input.h"
#include "../io/log.h"
#include "../resource/resource_cache.h"
#include "ui.h"

#include <SDL3/SDL_mouse.h>

#include "../common/debug_new.h"

namespace dviglo
{

static const char* shapeNames[] =
{
    "Normal",
    "IBeam",
    "Cross",
    "ResizeVertical",
    "ResizeDiagonalTopRight",
    "ResizeHorizontal",
    "ResizeDiagonalTopLeft",
    "ResizeAll",
    "AcceptDrop",
    "RejectDrop",
    "Busy",
    "BusyArrow"
};

#if !defined(__ANDROID__) && !defined(IOS) && !defined(TVOS)
// OS cursor shape lookup table matching cursor shape enumeration
static const int osCursorLookup[CS_MAX_SHAPES] =
{
    SDL_SYSTEM_CURSOR_ARROW,    // CS_NORMAL
    SDL_SYSTEM_CURSOR_IBEAM,     // CS_IBEAM
    SDL_SYSTEM_CURSOR_CROSSHAIR, // CS_CROSS
    SDL_SYSTEM_CURSOR_SIZENS,   // CS_RESIZEVERTICAL
    SDL_SYSTEM_CURSOR_SIZENESW, // CS_RESIZEDIAGONAL_TOPRIGHT
    SDL_SYSTEM_CURSOR_SIZEWE,   // CS_RESIZEHORIZONTAL
    SDL_SYSTEM_CURSOR_SIZENWSE, // CS_RESIZEDIAGONAL_TOPLEFT
    SDL_SYSTEM_CURSOR_SIZEALL,   // CS_RESIZE_ALL
    SDL_SYSTEM_CURSOR_HAND,     // CS_ACCEPTDROP
    SDL_SYSTEM_CURSOR_NO,       // CS_REJECTDROP
    SDL_SYSTEM_CURSOR_WAIT,   // CS_BUSY
    SDL_SYSTEM_CURSOR_WAITARROW // CS_BUSY_ARROW
};
#endif

extern const char* UI_CATEGORY;

Cursor::Cursor() :
    shape_(shapeNames[CS_NORMAL]),
    useSystemShapes_(false),
    osShapeDirty_(false)
{
    // Define the defaults for system cursor usage.
    for (unsigned i = 0; i < CS_MAX_SHAPES; i++)
        shapeInfos_[shapeNames[i]] = CursorShapeInfo(i);

    // Subscribe to OS mouse cursor visibility changes to be able to reapply the cursor shape
    subscribe_to_event(E_MOUSEVISIBLECHANGED, DV_HANDLER(Cursor, HandleMouseVisibleChanged));
}

Cursor::~Cursor()
{
    for (HashMap<String, CursorShapeInfo>::Iterator i = shapeInfos_.Begin(); i != shapeInfos_.End(); ++i)
    {
        if (i->second_.osCursor_)
        {
            SDL_DestroyCursor(i->second_.osCursor_);
            i->second_.osCursor_ = nullptr;
        }
    }
}

void Cursor::register_object()
{
    DV_CONTEXT->RegisterFactory<Cursor>(UI_CATEGORY);

    DV_COPY_BASE_ATTRIBUTES(BorderImage);
    DV_UPDATE_ATTRIBUTE_DEFAULT_VALUE("Priority", M_MAX_INT);
    DV_ACCESSOR_ATTRIBUTE("Use System Shapes", GetUseSystemShapes, SetUseSystemShapes, false, AM_FILE);
    DV_ACCESSOR_ATTRIBUTE("Shapes", GetShapesAttr, SetShapesAttr, Variant::emptyVariantVector, AM_FILE);
}

void Cursor::GetBatches(Vector<UIBatch>& batches, Vector<float>& vertexData, const IntRect& currentScissor)
{
    i32 initialSize = vertexData.Size();
    const IntVector2& offset = shapeInfos_[shape_].hotSpot_;
    Vector2 floatOffset(-(float)offset.x, -(float)offset.y);
    BorderImage::GetBatches(batches, vertexData, currentScissor);

    for (i32 i = initialSize; i < vertexData.Size(); i += 6)
    {
        vertexData[i] += floatOffset.x;
        vertexData[i + 1] += floatOffset.y;
    }
}

void Cursor::DefineShape(CursorShape shape, Image* image, const IntRect& imageRect, const IntVector2& hotSpot)
{
    if (shape < CS_NORMAL || shape >= CS_MAX_SHAPES)
    {
        DV_LOGERROR("Shape index out of bounds, can not define cursor shape");
        return;
    }

    DefineShape(shapeNames[shape], image, imageRect, hotSpot);
}

void Cursor::DefineShape(const String& shape, Image* image, const IntRect& imageRect, const IntVector2& hotSpot)
{
    if (!image)
        return;

    if (!shapeInfos_.Contains(shape))
        shapeInfos_[shape] = CursorShapeInfo();

    CursorShapeInfo& info = shapeInfos_[shape];

    // Prefer to get the texture with same name from cache to prevent creating several copies of the texture
    info.texture_ = DV_RES_CACHE->GetResource<Texture2D>(image->GetName(), false);
    if (!info.texture_)
    {
        auto* texture = new Texture2D();
        texture->SetData(SharedPtr<Image>(image));
        info.texture_ = texture;
    }

    info.image_ = image;
    info.imageRect_ = imageRect;
    info.hotSpot_ = hotSpot;

    // Remove existing SDL cursor
    if (info.osCursor_)
    {
        SDL_DestroyCursor(info.osCursor_);
        info.osCursor_ = nullptr;
    }

    // Reset current shape if it was edited
    if (shape_ == shape)
    {
        shape_ = String::EMPTY;
        SetShape(shape);
    }
}


void Cursor::SetShape(const String& shape)
{
    if (shape == String::EMPTY || shape.Empty() || shape_ == shape || !shapeInfos_.Contains(shape))
        return;

    shape_ = shape;

    CursorShapeInfo& info = shapeInfos_[shape_];
    texture_ = info.texture_;
    imageRect_ = info.imageRect_;
    SetSize(info.imageRect_.Size());

    // To avoid flicker, the UI subsystem will apply the OS shape once per frame. Exception: if we are using the
    // busy shape, set it immediately as we may block before that
    osShapeDirty_ = true;
    if (shape_ == shapeNames[CS_BUSY])
        ApplyOSCursorShape();
}

void Cursor::SetShape(CursorShape shape)
{
    if (shape < CS_NORMAL || shape >= CS_MAX_SHAPES || shape_ == shapeNames[shape])
        return;

    SetShape(shapeNames[shape]);
}

void Cursor::SetUseSystemShapes(bool enable)
{
    if (enable != useSystemShapes_)
    {
        useSystemShapes_ = enable;
        // Reapply current shape
        osShapeDirty_ = true;
    }
}

void Cursor::SetShapesAttr(const VariantVector& value)
{
    if (!value.Size())
        return;

    for (VariantVector::ConstIterator i = value.Begin(); i != value.End(); ++i)
    {
        VariantVector shapeVector = i->GetVariantVector();
        if (shapeVector.Size() >= 4)
        {
            String shape = shapeVector[0].GetString();
            ResourceRef ref = shapeVector[1].GetResourceRef();
            IntRect imageRect = shapeVector[2].GetIntRect();
            IntVector2 hotSpot = shapeVector[3].GetIntVector2();

            DefineShape(shape, DV_RES_CACHE->GetResource<Image>(ref.name_), imageRect, hotSpot);
        }
    }
}

VariantVector Cursor::GetShapesAttr() const
{
    VariantVector ret;

    for (HashMap<String, CursorShapeInfo>::ConstIterator i = shapeInfos_.Begin(); i != shapeInfos_.End(); ++i)
    {
        if (i->second_.imageRect_ != IntRect::ZERO)
        {
            // Could use a map but this simplifies the UI xml.
            VariantVector shape;
            shape.Push(i->first_);
            shape.Push(GetResourceRef(i->second_.texture_, Texture2D::GetTypeStatic()));
            shape.Push(i->second_.imageRect_);
            shape.Push(i->second_.hotSpot_);
            ret.Push(shape);
        }
    }

    return ret;
}

void Cursor::ApplyOSCursorShape()
{
    // Mobile platforms do not support applying OS cursor shapes: comment out to avoid log error messages
#if !defined(__ANDROID__) && !defined(IOS) && !defined(TVOS)
    if (!osShapeDirty_ || !DV_INPUT->IsMouseVisible() || DV_UI->GetCursor() != this)
        return;

    CursorShapeInfo& info = shapeInfos_[shape_];

    // Remove existing SDL cursor if is not a system shape while we should be using those, or vice versa
    if (info.osCursor_ && info.systemDefined_ != useSystemShapes_)
    {
        SDL_DestroyCursor(info.osCursor_);
        info.osCursor_ = nullptr;
    }

    // Create SDL cursor now if necessary
    if (!info.osCursor_)
    {
        // Create a system default shape
        if (useSystemShapes_ && info.systemCursor_ >= 0 && info.systemCursor_ < CS_MAX_SHAPES)
        {
            info.osCursor_ = SDL_CreateSystemCursor((SDL_SystemCursor)osCursorLookup[info.systemCursor_]);
            info.systemDefined_ = true;
            if (!info.osCursor_)
                DV_LOGERROR("Could not create system cursor");
        }
        // Create from image
        else if (info.image_)
        {
            SDL_Surface* surface = info.image_->GetSDLSurface(info.imageRect_);

            if (surface)
            {
                info.osCursor_ = SDL_CreateColorCursor(surface, info.hotSpot_.x, info.hotSpot_.y);
                info.systemDefined_ = false;
                if (!info.osCursor_)
                    DV_LOGERROR("Could not create cursor from image " + info.image_->GetName());
                SDL_DestroySurface(surface);
            }
        }
    }

    if (info.osCursor_)
        SDL_SetCursor(info.osCursor_);

    osShapeDirty_ = false;
#endif
}

void Cursor::HandleMouseVisibleChanged(StringHash eventType, VariantMap& eventData)
{
    ApplyOSCursorShape();
}

}
