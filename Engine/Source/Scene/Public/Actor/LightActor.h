#pragma once
#include "Actor.h"

class ALightActor : public AActor
{
    DECLARE_CLASS(ALightActor, AActor)

public:
    ALightActor();
    virtual UClass* GetDefaultRootComponent() override;
    virtual void InitializeComponents() override;
    
protected:
    virtual UClass* GetLightClass() = 0;
    virtual class UTexture* GetLightBillboardTexture() = 0;
};
