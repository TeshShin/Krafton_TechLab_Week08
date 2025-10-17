#pragma once
#include "Scene/Public/Actor/LightActor.h"

class AAmbientLightActor : public ALightActor
{
    DECLARE_CLASS(AAmbientLightActor, ALightActor)
    
protected:
    virtual UClass* GetLightClass() override;
    virtual class UTexture* GetLightBillboardTexture() override;
};
