#include "pch.h"
#include "Scene/Public/Component/LightComponentBase.h"
#include "Asset/Public/JsonSerializer.h"
#include "Editor/Public/UI/Widget/Component/LightComponentWidget.h"

IMPLEMENT_ABSTRACT_CLASS(ULightComponentBase, USceneComponent)

void ULightComponentBase::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "Intensity", Intensity);
		FJsonSerializer::ReadVector(InOutHandle, "LightColor", LightColor);
		FJsonSerializer::ReadBool(InOutHandle, "bVisible", bVisible, true);
	}
	else
	{
		InOutHandle["Intensity"] = Intensity;
		InOutHandle["LightColor"] = FJsonSerializer::VectorToJson(LightColor);
		InOutHandle["bVisible"] = bVisible;
	}
}

UObject* ULightComponentBase::Duplicate()
{
	ULightComponentBase* LightComponent = Cast<ULightComponentBase>(Super::Duplicate());
	LightComponent->Intensity = Intensity;
	LightComponent->LightColor = LightColor;
	LightComponent->bVisible = bVisible;

	return LightComponent;
}

void ULightComponentBase::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

UClass* ULightComponentBase::GetSpecificWidgetClass() const
{
	return ULightComponentWidget::StaticClass();
}
