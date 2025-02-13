// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#pragma once

#include "drawable.h"

namespace dviglo
{

class Model;

/// Static model per-geometry extra data.
struct StaticModelGeometryData
{
    /// Geometry center.
    Vector3 center_;
    /// Current LOD level.
    i32 lodLevel_;
};

/// Static model component.
class DV_API StaticModel : public Drawable
{
    DV_OBJECT(StaticModel);

public:
    /// Construct.
    explicit StaticModel();
    /// Destruct.
    ~StaticModel() override;
    /// Register object factory. Drawable must be registered first.
    static void register_object();

    /// Process octree raycast. May be called from a worker thread.
    void ProcessRayQuery(const RayOctreeQuery& query, Vector<RayQueryResult>& results) override;
    /// Calculate distance and prepare batches for rendering. May be called from worker thread(s), possibly re-entrantly.
    void update_batches(const FrameInfo& frame) override;
    /// Return the geometry for a specific LOD level.
    Geometry* GetLodGeometry(i32 batchIndex, i32 level) override;
    /// Return number of occlusion geometry triangles.
    i32 GetNumOccluderTriangles() override;
    /// Draw to occlusion buffer. Return true if did not run out of triangles.
    bool DrawOcclusion(OcclusionBuffer* buffer) override;

    /// Set model.
    virtual void SetModel(Model* model);
    /// Set material on all geometries.
    virtual void SetMaterial(Material* material);
    /// Set material on one geometry. Return true if successful.
    virtual bool SetMaterial(unsigned index, Material* material);
    /// Set occlusion LOD level. By default (NINDEX) same as visible.
    void SetOcclusionLodLevel(i32 level);
    /// Apply default materials from a material list file. If filename is empty (default), the model's resource name with extension .txt will be used.
    void ApplyMaterialList(const String& fileName = String::EMPTY);

    /// Return model.
    Model* GetModel() const { return model_; }

    /// Return number of geometries.
    unsigned GetNumGeometries() const { return geometries_.Size(); }

    /// Return material from the first geometry, assuming all the geometries use the same material.
    virtual Material* GetMaterial() const { return GetMaterial(0); }
    /// Return material by geometry index.
    virtual Material* GetMaterial(unsigned index) const;

    /// Return occlusion LOD level.
    i32 GetOcclusionLodLevel() const { return occlusionLodLevel_; }

    /// Determines if the given world space point is within the model geometry.
    bool IsInside(const Vector3& point) const;
    /// Determines if the given local space point is within the model geometry.
    bool IsInsideLocal(const Vector3& point) const;

    /// Set model attribute.
    void SetModelAttr(const ResourceRef& value);
    /// Set materials attribute.
    void SetMaterialsAttr(const ResourceRefList& value);
    /// Return model attribute.
    ResourceRef GetModelAttr() const;
    /// Return materials attribute.
    const ResourceRefList& GetMaterialsAttr() const;

protected:
    /// Recalculate the world-space bounding box.
    void OnWorldBoundingBoxUpdate() override;
    /// Set local-space bounding box.
    void SetBoundingBox(const BoundingBox& box);
    /// Set number of geometries.
    void SetNumGeometries(unsigned num);
    /// Reset LOD levels.
    void ResetLodLevels();
    /// Choose LOD levels based on distance.
    void CalculateLodLevels();

    /// Extra per-geometry data.
    Vector<StaticModelGeometryData> geometryData_;
    /// All geometries.
    Vector<Vector<std::shared_ptr<Geometry>>> geometries_;
    /// Model.
    SharedPtr<Model> model_;
    /// Occlusion LOD level.
    i32 occlusionLodLevel_;
    /// Material list attribute.
    mutable ResourceRefList materialsAttr_;

private:
    /// Handle model reload finished.
    void HandleModelReloadFinished(StringHash eventType, VariantMap& eventData);
};

} // namespace dviglo
