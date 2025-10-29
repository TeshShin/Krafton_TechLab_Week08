#include "pch.h"
#include "Scene/Public/Component/LightComponentBase.h"
#include "Asset/Public/JsonSerializer.h"
#include "Editor/Public/Line/BatchLineManager.h"
#include "Editor/Public/UI/Widget/Component/LightComponentWidget.h"
#include "Scene/Public/Actor/Actor.h"
#include "Scene/Public/Component/BillBoardComponent.h"

IMPLEMENT_ABSTRACT_CLASS(ULightComponentBase, USceneComponent)

ULightComponentBase::~ULightComponentBase()
{
}

void ULightComponentBase::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "Intensity", Intensity);
		FJsonSerializer::ReadVector(InOutHandle, "LightColor", LightColor);
		FJsonSerializer::ReadBool(InOutHandle, "bVisible", bVisible, true);
		FJsonSerializer::ReadBool(InOutHandle, "bCastShadows", bCastShadows, true);
		FJsonSerializer::ReadFloat(InOutHandle, "ShadowSharpen", ShadowSharpen, 1.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "ShadowBias", ShadowBias, 0.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "ShadowSlopeBias", ShadowSlopeBias, 0.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "ShadowResolutionScale", ShadowResolutionScale, 1.0f);
		SetLightColor(LightColor);
		int32 ShadowModeInt = 0;
		FJsonSerializer::ReadInt32(InOutHandle, "ShadowProjectionMode", ShadowModeInt, 0);
		ShadowProjectionMode = static_cast<EShadowProjectionMode>(ShadowModeInt);

		// CSM parameters
		int32 NumCascadeInt = static_cast<int32>(NumOfCascade);
		FJsonSerializer::ReadInt32(InOutHandle, "NumOfCascade", NumCascadeInt, 12);
		NumOfCascade = static_cast<uint32>(std::max(1, NumCascadeInt));
		FJsonSerializer::ReadFloat(InOutHandle, "CascadeSplitLambda", CascadeSpitLambda, 0.5f);
	}
	else
	{
		InOutHandle["Intensity"] = Intensity;
		InOutHandle["LightColor"] = FJsonSerializer::VectorToJson(LightColor);
		InOutHandle["bVisible"] = bVisible;
		InOutHandle["bCastShadows"] = bCastShadows;
		InOutHandle["ShadowSharpen"] = ShadowSharpen;
		InOutHandle["ShadowBias"] = ShadowBias;
		InOutHandle["ShadowSlopeBias"] = ShadowSlopeBias;
		InOutHandle["ShadowResolutionScale"] = ShadowResolutionScale;
		InOutHandle["ShadowProjectionMode"] = static_cast<int>(ShadowProjectionMode);
		InOutHandle["NumOfCascade"] = static_cast<int>(NumOfCascade);
		InOutHandle["CascadeSplitLambda"] = CascadeSpitLambda;
	}
}

UObject* ULightComponentBase::Duplicate()
{
	ULightComponentBase* LightComponent = Cast<ULightComponentBase>(Super::Duplicate());
	LightComponent->Intensity = Intensity;
	LightComponent->LightColor = LightColor;
	LightComponent->bVisible = bVisible;
	LightComponent->bCastShadows = bCastShadows;
	LightComponent->ShadowSharpen = ShadowSharpen;
	LightComponent->ShadowBias = ShadowBias;
	LightComponent->ShadowSlopeBias = ShadowSlopeBias;
	LightComponent->ShadowResolutionScale = ShadowResolutionScale;
	LightComponent->ShadowProjectionMode = ShadowProjectionMode;
	LightComponent->NumOfCascade = NumOfCascade;
	LightComponent->CascadeSpitLambda = CascadeSpitLambda;

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

void ULightComponentBase::BeginPlay()
{
	Super::BeginPlay();

	if (!GEditor->IsPIESessionActive())
	{
		IconBillboard = nullptr;

		// 1) 이미 로드된 빌보드(시각화 컴포넌트) 재사용 시도
		if (AActor* OwnerActor = GetOwner())
		{
			TArray<UActorComponent*>& Components = OwnerActor->GetOwnedComponents();
			for (UActorComponent* Component : Components)
			{
				UBillBoardComponent* BillboardCandidate = Cast<UBillBoardComponent>(Component);
				if (BillboardCandidate)
				{
					USceneComponent* Parent = BillboardCandidate->GetAttachParent();
					if (Parent == this && BillboardCandidate->IsVisualizationComponent())
					{
						IconBillboard = BillboardCandidate;
						break;
					}
				}
			}
		}

		// 2) 없으면 새로 생성
		if (!IconBillboard)
		{
			CreateIconChild();
		}

		// 3) 색상 동기화 보장
		if (IconBillboard)
		{
			IconBillboard->SetColor(FVector4(LightColor.X, LightColor.Y, LightColor.Z, 1.0f));
		}

	}
}

void ULightComponentBase::OnSelected()
{
	Super::OnSelected();
	ClearDebugLines();
	DrawDebugLines();
}

void ULightComponentBase::OnDeselected()
{
	Super::OnDeselected();
	ClearDebugLines();
}

void ULightComponentBase::MarkAsDirty()
{
	USceneComponent::MarkAsDirty();
	bIsLightVPDirty = true;
	if (bIsSelected)
	{
		ClearDebugLines();
		DrawDebugLines();
	}
}

void ULightComponentBase::SetLightColor(const FVector& InLightColor)
{
	LightColor = InLightColor;
	if (IconBillboard)
	{
		IconBillboard->SetColor(FVector4(InLightColor.X, InLightColor.Y, InLightColor.Z, 1.0f));
	}
}

void ULightComponentBase::ClearDebugLines()
{
	if (!DebugLineLabels.empty())
	{
		auto& LineManager = UBatchLineManager::GetInstance();
		for (const FName& Label : DebugLineLabels)
		{
			LineManager.RemoveDebugLine(Label);
		}
		DebugLineLabels.clear();
	}
}

void ULightComponentBase::CreateIconChild()
{
	if (AActor* Owner = GetOwner())
	{
		IconBillboard = Owner->AddComponent<UBillBoardComponent>();
		IconBillboard->AttachToComponent(this);
		IconBillboard->SetIsVisualizationComponent(true);
		IconBillboard->SetSprite(GetLightBillboardTexture());
		IconBillboard->SetScreenSizeScaled(true);
		IconBillboard->SetColor(FVector4(LightColor.X, LightColor.Y, LightColor.Z, 1.0f));
	}
}

const TArray<FMatrix>& ULightComponentBase::GetLightViewMatrices(const FCameraConstants& InCameraInvConstants) const
{
	UpdateLightMatricesInternal(InCameraInvConstants);
	return CachedLightViewMatrices;
}

const FMatrix& ULightComponentBase::GetLightProjectionMatrix(const FCameraConstants& InCameraInvConstants) const
{
	UpdateLightMatricesInternal(InCameraInvConstants);
	return CachedLightProjectionMatrix;
}

const TArray<FMatrix>& ULightComponentBase::GetLightViewProjectionMatrices(const FCameraConstants& InCameraInvConstants) const
{
	UpdateLightMatricesInternal(InCameraInvConstants);
	return CachedLightViewProjection;
}
