// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#pragma once

#include <dviglo/scene/logic_component.h>
#include <dviglo/common/primitive_types.h>

using namespace dviglo;

class Benchmark03_MoleculeLogic : public LogicComponent
{
public:
    DV_OBJECT(Benchmark03_MoleculeLogic);

private:
    i32 moleculeType_;
    Vector2 velocity_;
    Vector2 force_;

public:
    explicit Benchmark03_MoleculeLogic();

    void SetParameters(i32 moleculeType);

    i32 GetMoleculeType() const { return moleculeType_; }

    // Update the velocity of this molecule
    void Update(float timeStep) override;

    // Move this molecule only after updating the velocities of all molecules 
    void PostUpdate(float timeStep) override;
};
