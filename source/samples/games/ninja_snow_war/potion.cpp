// Copyright (c) the Dviglo project
// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "potion.h"

static constexpr int POTION_HEAL_AMOUNT = 5;

void Potion::register_object()
{
    DV_CONTEXT->RegisterFactory<Potion>();
}

Potion::Potion()
{
    healAmount = POTION_HEAL_AMOUNT;
}

void Potion::Start()
{
    subscribe_to_event(node_, E_NODECOLLISION, DV_HANDLER(Potion, HandleNodeCollision));
}

void Potion::ObjectCollision(GameObject& otherObject, VariantMap& eventData)
{
    if (healAmount > 0)
    {
        if (otherObject.Heal(healAmount))
        {
            // Could also remove the potion directly, but this way it gets removed on next update
            healAmount = 0;
            duration = 0;
        }
    }
}
