#include "pch.h"
#include "Renderer/Public/RenderPass/TextPass.h"
#include "Renderer/Public/Pipeline.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Scene/Public/Component/UUIDTextComponent.h"
#include "Scene/Public/Component/TextComponent.h"
#include "Editor/Public/Camera.h"
#include "Manager/Public/AssetManager.h"
#include "Editor/Public/Editor.h"
#include "Renderer/Public/Renderer.h"

FTextPass::FTextPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS, ID3D11BlendState* InBS)
    : FRenderPass(InPipeline), DS(InDS), BS(InBS)
{
    TArray<D3D11_INPUT_ELEMENT_DESC> LayoutDesc = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FFontVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FFontVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32_UINT, 0, offsetof(FFontVertex, CharIndex), D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/Primitive/FontShader.hlsl", LayoutDesc, &VS, &InputLayout);
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/Primitive/FontShader.hlsl", &PS);

    // Create dynamic vertex buffer
    D3D11_BUFFER_DESC BufferDesc = {};
    BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    BufferDesc.ByteWidth = sizeof(FFontVertex) * MAX_FONT_VERTICES;
    BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    URenderer::GetInstance().GetDevice()->CreateBuffer(&BufferDesc, nullptr, &DynamicVertexBuffer);

    // Create constant buffer
    FontDataConstantBuffer = FRenderResourceFactory::CreateConstantBuffer<FFontConstantBuffer>();

    // Load font texture
    UAssetManager& ResourceManager = UAssetManager::GetInstance();
    FontTexture = ResourceManager.LoadTexture("Data/Texture/DejaVu Sans Mono.png");
}

bool FTextPass::CanRender(const FRenderingContext& Context)
{
    return Context.ShowFlags & EEngineShowFlags::SF_Text;
}

void FTextPass::SetRenderTargets(class UDeviceResources* DeviceResources)
{
	ID3D11RenderTargetView* RTVs[] = { DeviceResources->GetDestinationRTV() };
	ID3D11DepthStencilView* DSV = DeviceResources->GetDepthBufferDSV();
	Pipeline->SetRenderTargets(1, RTVs, DSV);
}

void FTextPass::Execute(FRenderingContext& Context)
{
	FPipelineInfo PipelineInfo = {};
	PipelineInfo.InputLayout = InputLayout;
	PipelineInfo.VertexShader = VS;
	PipelineInfo.PixelShader = PS;
	PipelineInfo.RasterizerState = FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid });
	PipelineInfo.BlendState = BS;
	PipelineInfo.DepthStencilState = DS;
	Pipeline->UpdatePipeline(PipelineInfo);

	FRenderResourceFactory::UpdateConstantBufferData(FontDataConstantBuffer, ConstantBufferData);
    Pipeline->SetConstantBuffer(0, EShaderType::EST_Vertex, FontDataConstantBuffer);

    // Bind resources
    Pipeline->SetSRV(0, EShaderType::EST_Pixel, FontTexture->GetTextureSRV());
    Pipeline->SetSamplerState(0, EShaderType::EST_Pixel, FontTexture->GetTextureSampler());

    for (UTextComponent* Text : Context.Texts)
    {
		RenderTextInternal(Text->GetText(), Text->GetWorldTransformMatrix(), Context.ModelCB);
    }

    // Render UUID
    if (!(Context.ShowFlags & EEngineShowFlags::SF_Billboard)) { return; }

    for (UUUIDTextComponent* PickedBillboard : Context.UUIDs)
    {
        if (PickedBillboard->GetOwner() != GEditor->GetEditorModule()->GetSelectedActor()) { continue; }
        PickedBillboard->UpdateRotationMatrix(Context.CurrentCamera->GetForward());
        FString UUIDString = "UID: " + std::to_string(PickedBillboard->GetUUID());
        RenderTextInternal(UUIDString, PickedBillboard->GetRTMatrix(), Context.ModelCB);
    }
}

void FTextPass::RenderTextInternal(const FString& Text, const FMatrix& WorldMatrix, ID3D11Buffer* InModelCB)
{
	if (Text.empty()) return;

	const size_t TextLength = Text.length();
	const uint32 VertexCount = static_cast<uint32>(TextLength * 6);

	if (VertexCount > MAX_FONT_VERTICES) { return; }

	TArray<FFontVertex> FontVertices;
	FontVertices.resize(TextLength * 6);

	float CurrentY = 0.0f - (TextLength * 1.0f) / 2.0f;

	for (size_t Idx = 0; Idx < TextLength; ++Idx)
	{
		const char Ch = Text[Idx];
		const uint32 AsciiCode = static_cast<uint32>(Ch);

		// Simplified vertex generation, assuming fixed size
		const float Y = CurrentY + Idx * 1.0f;

		const float CharHeight = 2.0f;
		const float HalfHeight = CharHeight / 2.0f;

		const float BottomZ = -HalfHeight;
		const float TopZ = HalfHeight;     // +1.0f

		const FVector P0(0.0f, Y,        TopZ);
		const FVector P1(0.0f, Y + 1.0f, TopZ);
		const FVector P2(0.0f, Y,        BottomZ);
		const FVector P3(0.0f, Y + 1.0f, BottomZ);

		// TArray의 인덱스에 직접 접근하여 데이터 삽입
		FontVertices[Idx * 6 + 0] = { P0, FVector2(0.0f, 0.0f), AsciiCode };
		FontVertices[Idx * 6 + 1] = { P1, FVector2(1.0f, 0.0f), AsciiCode };
		FontVertices[Idx * 6 + 2] = { P2, FVector2(0.0f, 1.0f), AsciiCode };
		FontVertices[Idx * 6 + 3] = { P1, FVector2(1.0f, 0.0f), AsciiCode };
		FontVertices[Idx * 6 + 4] = { P3, FVector2(1.0f, 1.0f), AsciiCode };
		FontVertices[Idx * 6 + 5] = { P2, FVector2(0.0f, 1.0f), AsciiCode };
	}

	// 4. 데이터가 채워진 TArray를 헬퍼 함수에 전달합니다.
	FRenderResourceFactory::UpdateVertexBufferData(DynamicVertexBuffer, FontVertices);
	FRenderResourceFactory::UpdateConstantBufferData(InModelCB, WorldMatrix);
	Pipeline->SetVertexBuffer(DynamicVertexBuffer, sizeof(FFontVertex));
	Pipeline->Draw(VertexCount, 0);
}

void FTextPass::Release()
{
    SafeRelease(VS);
    SafeRelease(PS);
    SafeRelease(InputLayout);
    SafeRelease(DynamicVertexBuffer);
    SafeRelease(FontDataConstantBuffer);
}
