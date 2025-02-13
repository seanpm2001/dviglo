// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#include "../core/context.h"
#include "../graphics/graphics.h"
#include "../graphics_api/texture_2d.h"
#include "../io/file_system.h"
#include "../io/log.h"
#include "../io/path.h"
#include "../math/area_allocator.h"
#include "../resource/resource_cache.h"
#include "../resource/xml_file.h"
#include "animation_set_2d.h"
#include "sprite_2d.h"
#include "sprite_sheet_2d.h"
#include "spriter_data_2d.h"

#include "../common/debug_new.h"

using namespace std;

#ifdef DV_SPINE
#include <spine/spine.h>
#include <spine/extension.h>

// Current animation set
static dviglo::AnimationSet2D* currentAnimationSet = 0;

void _spAtlasPage_createTexture(spAtlasPage* self, const char* path)
{
    using namespace dviglo;
    if (!currentAnimationSet)
        return;

    Sprite2D* sprite = DV_RES_CACHE->GetResource<Sprite2D>(path);
    // Add reference
    if (sprite)
        sprite->AddRef();

    self->width = sprite->GetTexture()->GetWidth();
    self->height = sprite->GetTexture()->GetHeight();

    self->rendererObject = sprite;
}

void _spAtlasPage_disposeTexture(spAtlasPage* self)
{
    using namespace dviglo;
    Sprite2D* sprite = static_cast<Sprite2D*>(self->rendererObject);
    if (sprite)
        sprite->ReleaseRef();

    self->rendererObject = 0;
}

char* _spUtil_readFile(const char* path, int* length)
{
    using namespace dviglo;

    if (!currentAnimationSet)
        return 0;

    SharedPtr<File> file = DV_RES_CACHE->GetFile(path);
    if (!file)
        return 0;

    unsigned size = file->GetSize();

    char* data = MALLOC(char, size + 1);
    file->Read(data, size);
    data[size] = '\0';

    file.Reset();
    *length = size;

    return data;
}
#endif

namespace dviglo
{

AnimationSet2D::AnimationSet2D() :
#ifdef DV_SPINE
    skeletonData_(0),
    atlas_(0),
#endif
    hasSpriteSheet_(false)
{
}

AnimationSet2D::~AnimationSet2D()
{
    Dispose();
}

void AnimationSet2D::register_object()
{
    DV_CONTEXT->RegisterFactory<AnimationSet2D>();
}

bool AnimationSet2D::begin_load(Deserializer& source)
{
    Dispose();

    if (GetName().Empty())
        SetName(source.GetName());

    String extension = GetExtension(source.GetName());
#ifdef DV_SPINE
    if (extension == ".json")
        return BeginLoadSpine(source);
#endif
    if (extension == ".scml")
        return BeginLoadSpriter(source);

    DV_LOGERROR("Unsupport animation set file: " + source.GetName());

    return false;
}

bool AnimationSet2D::end_load()
{
#ifdef DV_SPINE
    if (jsonData_)
        return EndLoadSpine();
#endif
    if (spriterData_)
        return EndLoadSpriter();

    return false;
}

unsigned AnimationSet2D::GetNumAnimations() const
{
#ifdef DV_SPINE
    if (skeletonData_)
        return (unsigned)skeletonData_->animationsCount;
#endif
    if (spriterData_ && !spriterData_->entities_.Empty())
        return spriterData_->entities_[0]->animations_.Size();
    return 0;
}

String AnimationSet2D::GetAnimation(unsigned index) const
{
    if (index >= GetNumAnimations())
        return String::EMPTY;

#ifdef DV_SPINE
    if (skeletonData_)
        return skeletonData_->animations[index]->name;
#endif
    if (spriterData_ && !spriterData_->entities_.Empty())
        return spriterData_->entities_[0]->animations_[index]->name_;

    return String::EMPTY;
}

bool AnimationSet2D::HasAnimation(const String& animationName) const
{
#ifdef DV_SPINE
    if (skeletonData_)
    {
        for (int i = 0; i < skeletonData_->animationsCount; ++i)
        {
            if (animationName == skeletonData_->animations[i]->name)
                return true;
        }
    }
#endif
    if (spriterData_ && !spriterData_->entities_.Empty())
    {
        const Vector<Spriter::Animation*>& animations = spriterData_->entities_[0]->animations_;
        for (unsigned i = 0; i < animations.Size(); ++i)
        {
            if (animationName == animations[i]->name_)
                return true;
        }
    }

    return false;
}

Sprite2D* AnimationSet2D::GetSprite() const
{
    return sprite_;
}

Sprite2D* AnimationSet2D::GetSpriterFileSprite(int folderId, int fileId) const
{
    unsigned key = folderId << 16u | fileId;
    HashMap<unsigned, SharedPtr<Sprite2D>>::ConstIterator i = spriterFileSprites_.Find(key);
    if (i != spriterFileSprites_.End())
        return i->second_;

    return nullptr;
}

#ifdef DV_SPINE
bool AnimationSet2D::BeginLoadSpine(Deserializer& source)
{
    if (GetName().Empty())
        SetName(source.GetName());

    unsigned size = source.GetSize();
    jsonData_ = new char[size + 1];
    source.Read(jsonData_, size);
    jsonData_[size] = '\0';
    SetMemoryUse(size);
    return true;
}

bool AnimationSet2D::EndLoadSpine()
{
    currentAnimationSet = this;

    String atlasFileName = replace_extension(GetName(), ".atlas");
    atlas_ = spAtlas_createFromFile(atlasFileName.c_str(), 0);
    if (!atlas_)
    {
        DV_LOGERROR("Create spine atlas failed");
        return false;
    }

    int numAtlasPages = 0;
    spAtlasPage* atlasPage = atlas_->pages;
    while (atlasPage)
    {
        ++numAtlasPages;
        atlasPage = atlasPage->next;
    }

    if (numAtlasPages > 1)
    {
        DV_LOGERROR("Only one page is supported in Urho3D");
        return false;
    }

    sprite_ = static_cast<Sprite2D*>(atlas_->pages->rendererObject);

    spSkeletonJson* skeletonJson = spSkeletonJson_create(atlas_);
    if (!skeletonJson)
    {
        DV_LOGERROR("Create skeleton Json failed");
        return false;
    }

    skeletonJson->scale = PIXEL_SIZE;
    skeletonData_ = spSkeletonJson_readSkeletonData(skeletonJson, &jsonData_[0]);

    spSkeletonJson_dispose(skeletonJson);
    jsonData_.Reset();

    currentAnimationSet = 0;

    return true;
}
#endif

bool AnimationSet2D::BeginLoadSpriter(Deserializer& source)
{
    unsigned dataSize = source.GetSize();
    if (!dataSize && !source.GetName().Empty())
    {
        DV_LOGERROR("Zero sized XML data in " + source.GetName());
        return false;
    }

    unique_ptr<char[]> buffer(new char[dataSize]);
    if (source.Read(buffer.get(), dataSize) != dataSize)
        return false;

    spriterData_ = make_unique<Spriter::SpriterData>();
    if (!spriterData_->Load(buffer.get(), dataSize))
    {
        DV_LOGERROR("Could not spriter data from " + source.GetName());
        return false;
    }

    // Check has sprite sheet
    String parentPath = get_parent(GetName());

    spriteSheetFilePath_ = parentPath + GetFileName(GetName()) + ".xml";
    hasSpriteSheet_ = DV_RES_CACHE->Exists(spriteSheetFilePath_);
    if (!hasSpriteSheet_)
    {
        spriteSheetFilePath_ = parentPath + GetFileName(GetName()) + ".plist";
        hasSpriteSheet_ = DV_RES_CACHE->Exists(spriteSheetFilePath_);
    }

    if (GetAsyncLoadState() == ASYNC_LOADING)
    {
        if (hasSpriteSheet_)
        {
            DV_RES_CACHE->background_load_resource<SpriteSheet2D>(spriteSheetFilePath_, true, this);
        }
        else
        {
            for (unsigned i = 0; i < spriterData_->folders_.Size(); ++i)
            {
                Spriter::Folder* folder = spriterData_->folders_[i];
                for (unsigned j = 0; j < folder->files_.Size(); ++j)
                {
                    Spriter::File* file = folder->files_[j];
                    String imagePath = parentPath + file->name_;
                    DV_RES_CACHE->background_load_resource<Image>(imagePath, true, this);
                }
            }
        }
    }

    // Note: this probably does not reflect internal data structure size accurately
    SetMemoryUse(dataSize);

    return true;
}

struct SpriteInfo
{
    int x{};
    int y{};
    Spriter::File* file_{};
    SharedPtr<Image> image_;
};

bool AnimationSet2D::EndLoadSpriter()
{
    if (!spriterData_)
        return false;

    if (hasSpriteSheet_)
    {
        spriteSheet_ = DV_RES_CACHE->GetResource<SpriteSheet2D>(spriteSheetFilePath_);
        if (!spriteSheet_)
            return false;

        for (unsigned i = 0; i < spriterData_->folders_.Size(); ++i)
        {
            Spriter::Folder* folder = spriterData_->folders_[i];
            for (unsigned j = 0; j < folder->files_.Size(); ++j)
            {
                Spriter::File* file = folder->files_[j];
                SharedPtr<Sprite2D> sprite(spriteSheet_->GetSprite(GetFileName(file->name_)));
                if (!sprite)
                {
                    DV_LOGERROR("Could not load sprite " + file->name_);
                    return false;
                }

                Vector2 hotSpot(file->pivotX_, file->pivotY_);

                // If sprite is trimmed, recalculate hot spot
                const IntVector2& offset = sprite->GetOffset();
                if (offset != IntVector2::ZERO)
                {
                    float pivotX = file->width_ * hotSpot.x;
                    float pivotY = file->height_ * (1.0f - hotSpot.y);

                    const IntRect& rectangle = sprite->GetRectangle();
                    hotSpot.x = (offset.x + pivotX) / rectangle.Width();
                    hotSpot.y = 1.0f - (offset.y + pivotY) / rectangle.Height();
                }

                sprite->SetHotSpot(hotSpot);

                if (!sprite_)
                    sprite_ = sprite;

                unsigned key = folder->id_ << 16u | file->id_;
                spriterFileSprites_[key] = sprite;
            }
        }
    }
    else
    {
        Vector<SpriteInfo> spriteInfos;
        String parentPath = get_parent(GetName());

        for (unsigned i = 0; i < spriterData_->folders_.Size(); ++i)
        {
            Spriter::Folder* folder = spriterData_->folders_[i];
            for (unsigned j = 0; j < folder->files_.Size(); ++j)
            {
                Spriter::File* file = folder->files_[j];
                String imagePath = parentPath + file->name_;
                SharedPtr<Image> image(DV_RES_CACHE->GetResource<Image>(imagePath));
                if (!image)
                {
                    DV_LOGERROR("Could not load image");
                    return false;
                }
                if (image->IsCompressed())
                {
                    DV_LOGERROR("Compressed image is not support");
                    return false;
                }
                if (image->GetComponents() != 4)
                {
                    DV_LOGERROR("Only support image with 4 components");
                    return false;
                }

                SpriteInfo def;
                def.x = 0;
                def.y = 0;
                def.file_ = file;
                def.image_ = image;
                spriteInfos.Push(def);
            }
        }

        if (spriteInfos.Empty())
            return false;

        if (spriteInfos.Size() > 1)
        {
            AreaAllocator allocator(128, 128, 2048, 2048);
            for (unsigned i = 0; i < spriteInfos.Size(); ++i)
            {
                SpriteInfo& info = spriteInfos[i];
                Image* image = info.image_;
                if (!allocator.Allocate(image->GetWidth() + 1, image->GetHeight() + 1, info.x, info.y))
                {
                    DV_LOGERROR("Could not allocate area");
                    return false;
                }
            }

            SharedPtr<Texture2D> texture(new Texture2D());
            texture->SetMipsToSkip(QUALITY_LOW, 0);
            texture->SetNumLevels(1);
            texture->SetSize(allocator.GetWidth(), allocator.GetHeight(), Graphics::GetRGBAFormat());

            auto textureDataSize = (unsigned)allocator.GetWidth() * allocator.GetHeight() * 4;
            unique_ptr<byte[]> textureData(new byte[textureDataSize]);
            memset(textureData.get(), 0, textureDataSize);

            sprite_ = new Sprite2D();
            sprite_->SetTexture(texture);

            for (unsigned i = 0; i < spriteInfos.Size(); ++i)
            {
                SpriteInfo& info = spriteInfos[i];
                Image* image = info.image_;

                for (int y = 0; y < image->GetHeight(); ++y)
                {
                    memcpy(textureData.get() + ((info.y + y) * allocator.GetWidth() + info.x) * 4,
                        image->GetData() + y * image->GetWidth() * 4, (size_t)image->GetWidth() * 4);
                }

                SharedPtr<Sprite2D> sprite(new Sprite2D());
                sprite->SetTexture(texture);
                sprite->SetRectangle(IntRect(info.x, info.y, info.x + image->GetWidth(), info.y + image->GetHeight()));
                sprite->SetHotSpot(Vector2(info.file_->pivotX_, info.file_->pivotY_));

                unsigned key = info.file_->folder_->id_ << 16u | info.file_->id_;
                spriterFileSprites_[key] = sprite;
            }

            texture->SetData(0, 0, 0, allocator.GetWidth(), allocator.GetHeight(), textureData.get());
        }
        else
        {
            SharedPtr<Texture2D> texture(new Texture2D());
            texture->SetMipsToSkip(QUALITY_LOW, 0);
            texture->SetNumLevels(1);

            SpriteInfo& info = spriteInfos[0];
            texture->SetData(info.image_, true);

            sprite_ = new Sprite2D();
            sprite_->SetTexture(texture);
            sprite_->SetRectangle(IntRect(info.x, info.y, info.x + info.image_->GetWidth(), info.y + info.image_->GetHeight()));
            sprite_->SetHotSpot(Vector2(info.file_->pivotX_, info.file_->pivotY_));

            unsigned key = info.file_->folder_->id_ << 16u | info.file_->id_;
            spriterFileSprites_[key] = sprite_;
        }
    }

    return true;
}

void AnimationSet2D::Dispose()
{
#ifdef DV_SPINE
    if (skeletonData_)
    {
        spSkeletonData_dispose(skeletonData_);
        skeletonData_ = 0;
    }

    if (atlas_)
    {
        spAtlas_dispose(atlas_);
        atlas_ = 0;
    }
#endif

    spriterData_.reset();

    sprite_.Reset();
    spriteSheet_.Reset();
    spriterFileSprites_.Clear();
}

}
