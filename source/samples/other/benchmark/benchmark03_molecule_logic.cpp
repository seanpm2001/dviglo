// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#include "benchmark03_molecule_logic.h"

#include <dviglo/scene/scene.h>

#include <dviglo/common/debug_new.h>

Benchmark03_MoleculeLogic::Benchmark03_MoleculeLogic()
    : moleculeType_(0)
{
    SetUpdateEventMask(LogicComponentEvents::Update | LogicComponentEvents::PostUpdate);
}

void Benchmark03_MoleculeLogic::SetParameters(i32 type)
{
    moleculeType_ = type;
}

static constexpr float MOLECULE_RADIUS = 1.f;
static constexpr float INTERACTION_RANGE = MOLECULE_RADIUS * 4.f;
static constexpr float CONTAINER_RADIUS = 9.f;

void Benchmark03_MoleculeLogic::Update(float timeStep)
{
    Vector<Node*> moleculeNodes;
    GetScene()->GetChildrenWithComponent<Benchmark03_MoleculeLogic>(moleculeNodes);

    Vector2 moleculePos = node_->GetPosition2D();

    for (Node* anotherMoleculeNode : moleculeNodes)
    {
        if (anotherMoleculeNode == node_)
            continue;

        Vector2 anotherMoleculePos = anotherMoleculeNode->GetPosition2D();
        i32 anotherMoleculeType = anotherMoleculeNode->GetComponent<Benchmark03_MoleculeLogic>()->GetMoleculeType();

        float distance = (anotherMoleculePos - moleculePos).Length();

        if (Equals(distance, 0.f))
        {
            // Molecules are at the same point
            velocity_ = Vector2(Random(-2.f, 2.f), Random(-2.f, 2.f));
            continue;
        }

        // Molecules are too far away and don't interact 
        if (distance >= INTERACTION_RANGE)
            continue;

        Vector2 direction = (anotherMoleculePos - moleculePos).normalized();
        float forceModulus = 1.f - distance / INTERACTION_RANGE;
        forceModulus = forceModulus * forceModulus * forceModulus;
        forceModulus = forceModulus * 25.f;

        if (moleculeType_ != anotherMoleculeType)
            forceModulus *= 1.5f;

        force_ -= forceModulus * direction;
    }

    if (moleculePos.Length() > CONTAINER_RADIUS - MOLECULE_RADIUS)
    {
        Vector2 backDirection = -moleculePos.normalized();

        float backModulus = (moleculePos.Length() - CONTAINER_RADIUS + MOLECULE_RADIUS) * 50.0f;
        force_ = force_ + backDirection * backModulus;
    }

    velocity_ = velocity_ + force_ * timeStep;
    velocity_ = velocity_.Lerp(Vector2::ZERO, timeStep * 0.5f);
}

void Benchmark03_MoleculeLogic::PostUpdate(float timeStep)
{
    node_->Translate2D(velocity_ * timeStep);
    force_ = Vector2::ZERO;
}
