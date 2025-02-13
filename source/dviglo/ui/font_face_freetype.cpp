// Copyright (c) the Dviglo project
// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "../core/context.h"
#include "../graphics/graphics.h"
#include "../graphics_api/texture_2d.h"
#include "../io/file_system.h"
#include "../io/log.h"
#include "../io/memory_buffer.h"
#include "font.h"
#include "font_face_freetype.h"
#include "free_type_lib_helper.h"
#include "ui.h"

#include <cassert>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

#include "../common/debug_new.h"

using namespace std;

namespace dviglo
{

inline float FixedToFloat(FT_Pos value)
{
    return value / 64.0f;
}

FontFaceFreeType::FontFaceFreeType(Font* font) :
    FontFace(font),
    loadMode_(FT_LOAD_DEFAULT)
{
}

FontFaceFreeType::~FontFaceFreeType()
{
    if (face_)
    {
        FT_Done_Face((FT_Face)face_);
        face_ = nullptr;
    }
}

bool FontFaceFreeType::Load(const byte* fontData, unsigned fontDataSize, float pointSize)
{
    UI* ui = DV_UI;
    const int maxTextureSize = ui->max_font_texture_size();
    const FontHintLevel hintLevel = ui->GetFontHintLevel();
    const float subpixelThreshold = ui->GetFontSubpixelThreshold();

    subpixel_ = (hintLevel <= FONT_HINT_LEVEL_LIGHT) && (pointSize <= subpixelThreshold);
    oversampling_ = subpixel_ ? ui->GetFontOversampling() : 1;

    if (pointSize <= 0)
    {
        DV_LOGERROR("Zero or negative point size");
        return false;
    }

    if (!fontDataSize)
    {
        DV_LOGERROR("Could not create font face from zero size data");
        return false;
    }

    FT_Library library = FreeTypeLibHelper::instance()->library();
    FT_Face face;
    FT_Error error = FT_New_Memory_Face(library, (FT_Byte*)fontData, fontDataSize, 0, &face);
    if (error)
    {
        DV_LOGERROR("Could not create font face");
        return false;
    }
    error = FT_Set_Char_Size(face, 0, pointSize * 64, oversampling_ * FONT_DPI, FONT_DPI);
    if (error)
    {
        FT_Done_Face(face);
        DV_LOGERROR("Could not set font point size " + String(pointSize));
        return false;
    }

    face_ = face;

    unsigned numGlyphs = (unsigned)face->num_glyphs;
    DV_LOGDEBUGF("Font face %s (%fpt) has %d glyphs", GetFileName(font_->GetName()).c_str(), pointSize, numGlyphs);

    // Load each of the glyphs to see the sizes & store other information
    loadMode_ = FT_LOAD_DEFAULT;
    if (ui->GetForceAutoHint())
    {
        loadMode_ |= FT_LOAD_FORCE_AUTOHINT;
    }
    if (ui->GetFontHintLevel() == FONT_HINT_LEVEL_NONE)
    {
        loadMode_ |= FT_LOAD_NO_HINTING;
    }
    if (ui->GetFontHintLevel() == FONT_HINT_LEVEL_LIGHT)
    {
        loadMode_ |= FT_LOAD_TARGET_LIGHT;
    }

    ascender_ = FixedToFloat(face->size->metrics.ascender);
    rowHeight_ = FixedToFloat(face->size->metrics.height);
    pointSize_ = pointSize;

    // Check if the font's OS/2 info gives different (larger) values for ascender & descender
    auto* os2Info = (TT_OS2*)FT_Get_Sfnt_Table(face, ft_sfnt_os2);
    if (os2Info)
    {
        float descender = FixedToFloat(face->size->metrics.descender);
        float unitsPerEm = face->units_per_EM;
        ascender_ = Max(ascender_, os2Info->usWinAscent * face->size->metrics.y_ppem / unitsPerEm);
        ascender_ = Max(ascender_, os2Info->sTypoAscender * face->size->metrics.y_ppem / unitsPerEm);
        descender = Max(descender, os2Info->usWinDescent * face->size->metrics.y_ppem / unitsPerEm);
        descender = Max(descender, os2Info->sTypoDescender * face->size->metrics.y_ppem / unitsPerEm);
        rowHeight_ = Max(rowHeight_, ascender_ + descender);
    }

    int textureWidth = maxTextureSize;
    int textureHeight = maxTextureSize;
    hasMutableGlyph_ = false;

    SharedPtr<Image> image(new Image());
    image->SetSize(textureWidth, textureHeight, 1);
    unsigned char* imageData = image->GetData();
    memset(imageData, 0, (size_t)image->GetWidth() * image->GetHeight());
    allocator_.Reset(FONT_TEXTURE_MIN_SIZE, FONT_TEXTURE_MIN_SIZE, textureWidth, textureHeight);

    HashMap<FT_UInt, FT_ULong> charCodes;
    FT_UInt glyphIndex;
    FT_ULong charCode = FT_Get_First_Char(face, &glyphIndex);

    while (glyphIndex != 0)
    {
        if (!LoadCharGlyph(charCode, image))
        {
            hasMutableGlyph_ = true;
            break;
        }

        // TODO: FT_Get_Next_Char can return same glyphIndex for different charCode
        charCodes[glyphIndex] = charCode;

        charCode = FT_Get_Next_Char(face, charCode, &glyphIndex);
    }

    SharedPtr<Texture2D> texture = LoadFaceTexture(image);
    if (!texture)
        return false;

    textures_.Push(texture);
    font_->SetMemoryUse(font_->GetMemoryUse() + textureWidth * textureHeight);

    // Store kerning if face has kerning information
    if (FT_HAS_KERNING(face))
    {
        // Read kerning manually to be more efficient and avoid out of memory crash when use large font file, for example there
        // are 29354 glyphs in msyh.ttf
        FT_ULong tagKern = FT_MAKE_TAG('k', 'e', 'r', 'n');
        FT_ULong kerningTableSize = 0;
        FT_Error error = FT_Load_Sfnt_Table(face, tagKern, 0, nullptr, &kerningTableSize);
        if (error)
        {
            DV_LOGERROR("Could not get kerning table length");
            return false;
        }

        unique_ptr<unsigned char[]> kerningTable(new unsigned char[kerningTableSize]);
        error = FT_Load_Sfnt_Table(face, tagKern, 0, kerningTable.get(), &kerningTableSize);
        if (error)
        {
            DV_LOGERROR("Could not load kerning table");
            return false;
        }

        // Convert big endian to little endian
        for (unsigned i = 0; i < kerningTableSize; i += 2)
            swap(kerningTable[i], kerningTable[i + 1]);
        MemoryBuffer deserializer(kerningTable.get(), (unsigned)kerningTableSize);

        unsigned short version = deserializer.ReadU16();
        if (version == 0)
        {
            unsigned numKerningTables = deserializer.ReadU16();
            for (unsigned i = 0; i < numKerningTables; ++i)
            {
                unsigned short version = deserializer.ReadU16();
                unsigned short length = deserializer.ReadU16();
                unsigned short coverage = deserializer.ReadU16();

                if (version == 0 && coverage == 1)
                {
                    unsigned numKerningPairs = deserializer.ReadU16();
                    // Skip searchRange, entrySelector and rangeShift
                    deserializer.Seek((unsigned)(deserializer.GetPosition() + 3 * sizeof(unsigned short)));

                    // x_scale is a 16.16 fixed-point value that converts font units -> 26.6 pixels (oversampled!)
                    auto xScale = (float)face->size->metrics.x_scale / (1u << 22u) / oversampling_;

                    for (unsigned j = 0; j < numKerningPairs; ++j)
                    {
                        unsigned leftIndex = deserializer.ReadU16();
                        unsigned rightIndex = deserializer.ReadU16();
                        float amount = deserializer.ReadI16() * xScale;

                        unsigned leftCharCode = charCodes[leftIndex];
                        unsigned rightCharCode = charCodes[rightIndex];
                        unsigned value = (leftCharCode << 16u) + rightCharCode;
                        // TODO: need to store kerning for glyphs but not for charCodes
                        kerningMapping_[value] = amount;
                    }
                }
                else
                {
                    // Kerning table contains information we do not support; skip and move to the next (length includes header)
                    deserializer.Seek((unsigned)(deserializer.GetPosition() + length - 3 * sizeof(unsigned short)));
                }
            }
        }
        else
            DV_LOGWARNING("Can not read kerning information: not version 0");
    }

    if (!hasMutableGlyph_)
    {
        FT_Done_Face(face);
        face_ = nullptr;
    }

    return true;
}

const FontGlyph* FontFaceFreeType::GetGlyph(c32 c)
{
    HashMap<c32, FontGlyph>::Iterator i = glyphMapping_.Find(c);
    if (i != glyphMapping_.End())
    {
        FontGlyph& glyph = i->second_;
        glyph.used = true;
        return &glyph;
    }

    if (LoadCharGlyph(c))
    {
        HashMap<c32, FontGlyph>::Iterator i = glyphMapping_.Find(c);
        if (i != glyphMapping_.End())
        {
            FontGlyph& glyph = i->second_;
            glyph.used = true;
            return &glyph;
        }
    }

    return nullptr;
}

bool FontFaceFreeType::SetupNextTexture(int textureWidth, int textureHeight)
{
    SharedPtr<Image> image(new Image());
    image->SetSize(textureWidth, textureHeight, 1);
    unsigned char* imageData = image->GetData();
    memset(imageData, 0, (size_t)image->GetWidth() * image->GetHeight());

    SharedPtr<Texture2D> texture = LoadFaceTexture(image);
    if (!texture)
        return false;

    textures_.Push(texture);
    allocator_.Reset(FONT_TEXTURE_MIN_SIZE, FONT_TEXTURE_MIN_SIZE, textureWidth, textureHeight);

    font_->SetMemoryUse(font_->GetMemoryUse() + textureWidth * textureHeight);

    return true;
}

void FontFaceFreeType::BoxFilter(unsigned char* dest, size_t destSize, const unsigned char* src, size_t srcSize)
{
    const int filterSize = oversampling_;

    assert(filterSize > 0);
    assert(destSize == srcSize + filterSize - 1);

    if (filterSize == 1)
    {
        memcpy(dest, src, srcSize);
        return;
    }

    // "accumulator" holds the total value of filterSize samples. We add one sample
    // and remove one sample per step (with special cases for left and right edges).
    int accumulator = 0;

    // The divide might make these inner loops slow. If so, some possible optimizations:
    // a) Turn it into a fixed-point multiply-and-shift rather than an integer divide;
    // b) Make this function a template, with the filter size a compile-time constant.

    int i = 0;

    if (srcSize < filterSize)
    {
        for (; i < srcSize; ++i)
        {
            accumulator += src[i];
            dest[i] = accumulator / filterSize;
        }

        for (; i < filterSize; ++i)
        {
            dest[i] = accumulator / filterSize;
        }
    }
    else
    {
        for ( ; i < filterSize; ++i)
        {
            accumulator += src[i];
            dest[i] = accumulator / filterSize;
        }

        for (; i < srcSize; ++i)
        {
            accumulator += src[i];
            accumulator -= src[i - filterSize];
            dest[i] = accumulator / filterSize;
        }
    }

    for (; i < srcSize + filterSize - 1; ++i)
    {
        accumulator -= src[i - filterSize];
        dest[i] = accumulator / filterSize;
    }
}

bool FontFaceFreeType::LoadCharGlyph(c32 charCode, Image* image)
{
    if (!face_)
        return false;

    auto face = (FT_Face)face_;
    FT_GlyphSlot slot = face->glyph;

    FontGlyph fontGlyph;
    FT_Error error = FT_Load_Char(face, charCode, loadMode_ | FT_LOAD_RENDER);
    if (error)
    {
        const char* family = face->family_name ? face->family_name : "NULL";
        DV_LOGERRORF("FT_Load_Char failed (family: %s, char code: %u)", family, charCode);
        fontGlyph.tex_width = 0;
        fontGlyph.tex_height = 0;
        fontGlyph.width = 0;
        fontGlyph.height = 0;
        fontGlyph.offset_x = 0;
        fontGlyph.offset_y = 0;
        fontGlyph.advance_x = 0;
        fontGlyph.page = 0;
    }
    else
    {
        // Note: position within texture will be filled later
        fontGlyph.tex_width = slot->bitmap.width + oversampling_ - 1;
        fontGlyph.tex_height = slot->bitmap.rows;
        fontGlyph.width = slot->bitmap.width + oversampling_ - 1;
        fontGlyph.height = slot->bitmap.rows;
        fontGlyph.offset_x = slot->bitmap_left - (oversampling_ - 1) / 2.0f;
        fontGlyph.offset_y = floorf(ascender_ + 0.5f) - slot->bitmap_top;

        if (subpixel_ && slot->linearHoriAdvance)
        {
            // linearHoriAdvance is stored in 16.16 fixed point, not the usual 26.6
            fontGlyph.advance_x = slot->linearHoriAdvance / 65536.0;
        }
        else
        {
            // Round to nearest pixel (only necessary when hinting is disabled)
            fontGlyph.advance_x = floorf(FixedToFloat(slot->metrics.horiAdvance) + 0.5f);
        }

        fontGlyph.width /= oversampling_;
        fontGlyph.offset_x /= oversampling_;
        fontGlyph.advance_x /= oversampling_;
    }

    int x = 0, y = 0;
    if (fontGlyph.tex_width > 0 && fontGlyph.tex_height > 0)
    {
        if (!allocator_.Allocate(fontGlyph.tex_width + 1, fontGlyph.tex_height + 1, x, y))
        {
            if (image)
            {
                // We're rendering into a fixed image and we ran out of room.
                return false;
            }

            int w = allocator_.GetWidth();
            int h = allocator_.GetHeight();
            if (!SetupNextTexture(w, h))
            {
                DV_LOGWARNINGF("FontFaceFreeType::LoadCharGlyph: failed to allocate new %dx%d texture", w, h);
                return false;
            }

            if (!allocator_.Allocate(fontGlyph.tex_width + 1, fontGlyph.tex_height + 1, x, y))
            {
                DV_LOGWARNINGF("FontFaceFreeType::LoadCharGlyph: failed to position char code %u in blank page", charCode);
                return false;
            }
        }

        fontGlyph.x = (short)x;
        fontGlyph.y = (short)y;

        unsigned char* dest = nullptr;
        unsigned pitch = 0;
        if (image)
        {
            fontGlyph.page = 0;
            dest = image->GetData() + fontGlyph.y * image->GetWidth() + fontGlyph.x;
            pitch = (unsigned)image->GetWidth();
        }
        else
        {
            fontGlyph.page = textures_.Size() - 1;
            dest = new unsigned char[fontGlyph.tex_width * fontGlyph.tex_height];
            pitch = (unsigned)fontGlyph.tex_width;
        }

        if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
        {
            for (unsigned y = 0; y < (unsigned)slot->bitmap.rows; ++y)
            {
                unsigned char* src = slot->bitmap.buffer + slot->bitmap.pitch * y;
                unsigned char* rowDest = dest + (oversampling_ - 1)/2 + y * pitch;

                // Don't do any oversampling, just unpack the bits directly.
                for (unsigned x = 0; x < (unsigned)slot->bitmap.width; ++x)
                    rowDest[x] = (unsigned char)((src[x >> 3u] & (0x80u >> (x & 7u))) ? 255 : 0);
            }
        }
        else
        {
            for (unsigned y = 0; y < (unsigned)slot->bitmap.rows; ++y)
            {
                unsigned char* src = slot->bitmap.buffer + slot->bitmap.pitch * y;
                unsigned char* rowDest = dest + y * pitch;
                BoxFilter(rowDest, fontGlyph.tex_width, src, slot->bitmap.width);
            }
        }

        if (!image)
        {
            textures_.Back()->SetData(0, fontGlyph.x, fontGlyph.y, fontGlyph.tex_width, fontGlyph.tex_height, dest);
            delete[] dest;
        }
    }
    else
    {
        fontGlyph.x = 0;
        fontGlyph.y = 0;
        fontGlyph.page = 0;
    }

    glyphMapping_[charCode] = fontGlyph;

    return true;
}

}
