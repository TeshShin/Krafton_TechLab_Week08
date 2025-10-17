#pragma once
#include "Scene/Public/Actor/LightActor.h"

class ADirectionalLightActor : ALightActor
{
    DECLARE_CLASS(ADirectionalLightActor, ALightActor)
protected:
    virtual UClass* GetLightClass() override;
    virtual class UTexture* GetLightBillboardTexture() override;
};
