#include "pch.h"
#include "Scene/Public/Component/ActorComponent.h"
#include "Asset/Public/JsonSerializer.h"
#include "Scene/Public/Actor/Actor.h"

IMPLEMENT_ABSTRACT_CLASS(UActorComponent, UObject)

UActorComponent::UActorComponent() : Owner(nullptr)
{
}

UActorComponent::~UActorComponent()
{
	SetOuter(nullptr);
}

void UActorComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		FString IsEditorOnlyString;
		FJsonSerializer::ReadString(InOutHandle, "IsEditorOnly", IsEditorOnlyString, "false");
		bIsEditorOnly = IsEditorOnlyString == "true";

		FString IsVisualizationString;
		FJsonSerializer::ReadString(InOutHandle, "IsVisualizationComponent", IsVisualizationString, "false");
		bIsVisualizationComponent =  IsVisualizationString == "true";
	}
	// 저장
	else
	{
		InOutHandle["IsEditorOnly"] = bIsEditorOnly ? "true" : "false";
		InOutHandle["IsVisualizationComponent"] = bIsVisualizationComponent ? "true" : "false";
	}
}

void UActorComponent::BeginPlay()
{

}

void UActorComponent::TickComponent(float DeltaTime)
{

}

void UActorComponent::EndPlay()
{

}


void UActorComponent::OnSelected()
{
	bIsSelected = true;
}


void UActorComponent::OnDeselected()
{
	bIsSelected = false;
}

UObject* UActorComponent::Duplicate()
{
	UActorComponent* ActorComponent = Cast<UActorComponent>(Super::Duplicate());
	ActorComponent->bCanEverTick = bCanEverTick;
	ActorComponent->bIsEditorOnly = bIsEditorOnly;
	ActorComponent->bIsVisualizationComponent = bIsVisualizationComponent;

	return ActorComponent;
}

void UActorComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

void UActorComponent::SetIsEditorOnly(bool bInIsEditorOnly)
{
	if (bInIsEditorOnly)
	{
		if (GetOwner() && GetOwner()->GetRootComponent() == this)
		{
			// RootComponent는 IsEditorOnly 불가능
			return;
		}
	}
	bIsEditorOnly = bInIsEditorOnly;
}

void UActorComponent::SetIsVisualizationComponent(bool bIsInVisualizationComponent)
{
	bIsVisualizationComponent = bIsInVisualizationComponent;
	if (bIsVisualizationComponent)
	{
		bIsEditorOnly = true;
	}
}
