// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#include "area_allocator.h"

#include "../common/debug_new.h"

namespace dviglo
{

AreaAllocator::AreaAllocator()
{
    Reset(0, 0);
}

AreaAllocator::AreaAllocator(i32 width, i32 height, bool fastMode)
{
    Reset(width, height, width, height, fastMode);
}

AreaAllocator::AreaAllocator(i32 width, i32 height, i32 maxWidth, i32 maxHeight, bool fastMode)
{
    Reset(width, height, maxWidth, maxHeight, fastMode);
}

void AreaAllocator::Reset(i32 width, i32 height, i32 maxWidth, i32 maxHeight, bool fastMode)
{
    doubleWidth_ = true;
    size_ = IntVector2(width, height);
    maxSize_ = IntVector2(maxWidth, maxHeight);
    fastMode_ = fastMode;

    freeAreas_.Clear();
    IntRect initialArea(0, 0, width, height);
    freeAreas_.Push(initialArea);
}

bool AreaAllocator::Allocate(i32 width, i32 height, i32& x, i32& y)
{
    if (width < 0)
        width = 0;
    if (height < 0)
        height = 0;

    Vector<IntRect>::Iterator best;
    i32 bestFreeArea;

    for (;;)
    {
        best = freeAreas_.End();
        bestFreeArea = M_MAX_INT;
        for (Vector<IntRect>::Iterator i = freeAreas_.Begin(); i != freeAreas_.End(); ++i)
        {
            i32 freeWidth = i->Width();
            i32 freeHeight = i->Height();

            if (freeWidth >= width && freeHeight >= height)
            {
                // Calculate rank for free area. Lower is better
                i32 freeArea = freeWidth * freeHeight;

                if (freeArea < bestFreeArea)
                {
                    best = i;
                    bestFreeArea = freeArea;
                }
            }
        }

        if (best == freeAreas_.End())
        {
            if (doubleWidth_ && size_.x < maxSize_.x)
            {
                i32 oldWidth = size_.x;
                size_.x <<= 1;
                // If no allocations yet, simply expand the single free area
                IntRect& first = freeAreas_.Front();
                if (freeAreas_.Size() == 1 && first.left_ == 0 && first.top_ == 0 && first.right_ == oldWidth &&
                    first.bottom_ == size_.y)
                    first.right_ = size_.x;
                else
                {
                    IntRect newArea(oldWidth, 0, size_.x, size_.y);
                    freeAreas_.Push(newArea);
                }
            }
            else if (!doubleWidth_ && size_.y < maxSize_.y)
            {
                i32 oldHeight = size_.y;
                size_.y <<= 1;
                // If no allocations yet, simply expand the single free area
                IntRect& first = freeAreas_.Front();
                if (freeAreas_.Size() == 1 && first.left_ == 0 && first.top_ == 0 && first.right_ == size_.x &&
                    first.bottom_ == oldHeight)
                    first.bottom_ = size_.y;
                else
                {
                    IntRect newArea(0, oldHeight, size_.x, size_.y);
                    freeAreas_.Push(newArea);
                }
            }
            else
                return false;

            doubleWidth_ = !doubleWidth_;
        }
        else
            break;
    }

    IntRect reserved(best->left_, best->top_, best->left_ + width, best->top_ + height);
    x = best->left_;
    y = best->top_;

    if (fastMode_)
    {
        // Reserve the area by splitting up the remaining free area
        best->left_ = reserved.right_;
        if (best->Height() > 2 * height || height >= size_.y / 2)
        {
            IntRect splitArea(reserved.left_, reserved.bottom_, best->right_, best->bottom_);
            best->bottom_ = reserved.bottom_;
            freeAreas_.Push(splitArea);
        }
    }
    else
    {
        // Remove the reserved area from all free areas
        for (i32 i = 0; i < freeAreas_.Size();)
        {
            if (SplitRect(i, reserved))
                freeAreas_.Erase(i);
            else
                ++i;
        }

        Cleanup();
    }

    return true;
}

bool AreaAllocator::SplitRect(i32 freeAreaIndex, const IntRect& reserve)
{
    assert(freeAreaIndex >= 0);

    // Make a copy, as the vector will be modified
    IntRect original = freeAreas_[freeAreaIndex];

    if (reserve.right_ > original.left_ && reserve.left_ < original.right_ && reserve.bottom_ > original.top_ &&
        reserve.top_ < original.bottom_)
    {
        // Check for splitting from the right
        if (reserve.right_ < original.right_)
        {
            IntRect newRect = original;
            newRect.left_ = reserve.right_;
            freeAreas_.Push(newRect);
        }
        // Check for splitting from the left
        if (reserve.left_ > original.left_)
        {
            IntRect newRect = original;
            newRect.right_ = reserve.left_;
            freeAreas_.Push(newRect);
        }
        // Check for splitting from the bottom
        if (reserve.bottom_ < original.bottom_)
        {
            IntRect newRect = original;
            newRect.top_ = reserve.bottom_;
            freeAreas_.Push(newRect);
        }
        // Check for splitting from the top
        if (reserve.top_ > original.top_)
        {
            IntRect newRect = original;
            newRect.bottom_ = reserve.top_;
            freeAreas_.Push(newRect);
        }

        return true;
    }

    return false;
}

void AreaAllocator::Cleanup()
{
    // Remove rects which are contained within another rect
    for (i32 i = 0; i < freeAreas_.Size();)
    {
        bool erased = false;
        for (i32 j = i + 1; j < freeAreas_.Size();)
        {
            if ((freeAreas_[i].left_ >= freeAreas_[j].left_) &&
                (freeAreas_[i].top_ >= freeAreas_[j].top_) &&
                (freeAreas_[i].right_ <= freeAreas_[j].right_) &&
                (freeAreas_[i].bottom_ <= freeAreas_[j].bottom_))
            {
                freeAreas_.Erase(i);
                erased = true;
                break;
            }
            if ((freeAreas_[j].left_ >= freeAreas_[i].left_) &&
                (freeAreas_[j].top_ >= freeAreas_[i].top_) &&
                (freeAreas_[j].right_ <= freeAreas_[i].right_) &&
                (freeAreas_[j].bottom_ <= freeAreas_[i].bottom_))
                freeAreas_.Erase(j);
            else
                ++j;
        }
        if (!erased)
            ++i;
    }
}

}
