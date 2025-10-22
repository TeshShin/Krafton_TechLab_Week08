#include "pch.h"
#include "Editor/Public/UI/Widget/Component/BillboardComponentWidget.h"
#include "Scene/Public/Level/Level.h"
#include "Manager/Public/AssetManager.h"
#include "Scene/Public/Component/BillBoardComponent.h"
#include "Asset/Public/Texture.h"
#include "Editor/Public/Editor.h"
#include <climits>


IMPLEMENT_CLASS(UBillboardComponentWidget, UComponentWidget)

void UBillboardComponentWidget::Initialize()
{
	Super::Initialize();
}

void UBillboardComponentWidget::Update()
{
	Super::Update();
}

void UBillboardComponentWidget::RenderWidget()
{
	// 먼저 UPROPERTY를 자동으로 렌더링
	Super::RenderWidget();

	// 선택된 컴포넌트 가져오기
	UActorComponent* SelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
	UBillBoardComponent* SelectedBillBoard = Cast<UBillBoardComponent>(SelectedComponent);

	if (!SelectedBillBoard) { return; }

	ImGui::Separator();
	ImGui::Text("Select Sprite");

	ImGui::Spacing();

	static int CurrentItem = 0;

	TArray<FString> Items;
	const TMap<FName, UTexture*>& TextureCache = UAssetManager::GetInstance().GetTextureCache();

	int32 Idx = 0;
	for (auto Itr = TextureCache.begin(); Itr != TextureCache.end(); ++Itr, Idx++)
	{
		if (Itr->first == SelectedBillBoard->GetSprite()->GetFilePath()) { CurrentItem = Idx; }

		Items.push_back(Itr->first.ToString());
	}

	sort(Items.begin(), Items.end());

	if (ImGui::BeginCombo("Sprite", Items[CurrentItem].c_str()))
	{
		for (int32 N = 0; N < Items.size(); N++)
		{
			bool bIsSelected = (CurrentItem == N);
			if (ImGui::Selectable(Items[N].c_str(), bIsSelected))
			{
				CurrentItem = N;
				SetSprite(Items[CurrentItem]);
			}

			if (bIsSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::Separator();

	WidgetNum = (WidgetNum + 1) % std::numeric_limits<uint32>::max();
}

void UBillboardComponentWidget::SetSprite(FString NewSprite)
{
	UActorComponent* SelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
	UBillBoardComponent* SelectedBillBoard = Cast<UBillBoardComponent>(SelectedComponent);

	if (!SelectedBillBoard)
		return;

	const TMap<FName, UTexture*>& TextureCache = UAssetManager::GetInstance().GetTextureCache();

	SelectedBillBoard->SetSprite(TextureCache.find(NewSprite)->second);
}