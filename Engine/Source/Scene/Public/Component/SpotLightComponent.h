#pragma once

#include "LightComponent.h"

UCLASS()
class USpotLightComponent : public ULightComponentBase
{
    GENERATED_BODY()
    DECLARE_CLASS(USpotLightComponent, ULightComponentBase)

public:
    USpotLightComponent() = default;

    virtual ~USpotLightComponent() = default;

    /*-----------------------------------------------------------------------------
        UObject Features
     -----------------------------------------------------------------------------*/
public:
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    virtual UObject* Duplicate() override;

    virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;

    /*-----------------------------------------------------------------------------
        UActorComponent Features
     -----------------------------------------------------------------------------*/
public:
    virtual void BeginPlay() override { Super::BeginPlay(); }

    virtual void TickComponent(float DeltaTime) override { Super::TickComponent(DeltaTime); }

    virtual void EndPlay() override { Super::EndPlay(); }

    virtual UClass* GetSpecificWidgetClass() const override;

    /*-----------------------------------------------------------------------------
        ULightComponentBase Features
     -----------------------------------------------------------------------------*/
public:
    virtual ELightComponentType GetLightType() const override { return ELightComponentType::LightType_Spot; }

    /*-----------------------------------------------------------------------------
        USpotLightComponent Features
     -----------------------------------------------------------------------------*/
public:
    // --- Getters & Setters ---

    // TODO: Add SpotLight specific properties (e.g., InnerConeAngle, OuterConeAngle, etc.)

private:
    // TODO: Add SpotLight specific member variables
};
