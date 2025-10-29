#pragma once
#include "LightComponentBase.h"

UCLASS()
class ULightComponent : public ULightComponentBase
{
	GENERATED_BODY()
	DECLARE_CLASS(ULightComponent, ULightComponentBase)
public:
	ULightComponent() = default;
	virtual ~ULightComponent() = default;

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
	void BeginPlay() override { Super::BeginPlay(); }

	void TickComponent(float DeltaTime) override { Super::TickComponent(DeltaTime); }
	void EndPlay() override { Super::EndPlay(); }
	/*-----------------------------------------------------------------------------
		ULightComponentBase Features
	 -----------------------------------------------------------------------------*/
public:
	ELightComponentType GetLightType() const override = 0;

	/**
	 * @brief Base implementation returns dummy data
	 * @note Derived classes should override this to provide specific light data
	 */
	struct FUnifiedDynamicLight GetUnifiedLightData() const override = 0; 
};
