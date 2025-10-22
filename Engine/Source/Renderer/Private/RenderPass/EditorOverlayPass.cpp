#include "pch.h"
#include "Renderer/Public/RenderPass/EditorOverlayPass.h"
#include "Editor/Public/Editor.h"
#include "Renderer/Public/RenderResourceFactory.h"

FEditorOverlayPass::FEditorOverlayPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferModels, ID3D11DepthStencilState* InDS)
	: FRenderPass(InPipeline, nullptr), ConstantBufferModels(InConstantBufferModels), DS(InDS)
{
	TArray<D3D11_INPUT_ELEMENT_DESC> DefaultLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Tangent), D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/SampleShader.hlsl", DefaultLayout, &VS, &InputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/SampleShader.hlsl", &PS);
	ConstantBufferColor = FRenderResourceFactory::CreateConstantBuffer<FVector4>();
	RS = FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid });
}

bool FEditorOverlayPass::CanRender(const FRenderingContext& Context)
{
	return !GEditor->IsPIESessionActive();
}

void FEditorOverlayPass::SetRenderTargets(class UDeviceResources* DeviceResources)
{
	ID3D11RenderTargetView* RenderTargetView[] = { DeviceResources->GetBackBufferRTV() };
	Pipeline->SetRenderTargets(1, RenderTargetView, DeviceResources->GetDepthBufferDSV());
}

void FEditorOverlayPass::Execute(FRenderingContext& Context)
{
	TArray<const FEditorPrimitive*> Primitives = GEditor->GetEditorModule()->GetEditorOverlayPrimitives();
	FPipelineInfo PipelineInfo = {InputLayout, VS, RS, DS, PS, nullptr};

	constexpr uint32 Stride = sizeof(FNormalVertex);
	for (const FEditorPrimitive* Primitive: Primitives)
	{
		if (!Primitive) { continue; }

		PipelineInfo.Topology = Primitive->Topology;
		Pipeline->UpdatePipeline(PipelineInfo);

		// Update constant buffers
		FModelConstants ModelConstants {
			FMatrix::GetModelMatrix(Primitive->Location, Primitive->Rotation, Primitive->Scale), FMatrix::Identity() };

		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModels, ModelConstants);
		Pipeline->SetConstantBuffer(0, true, ConstantBufferModels);

		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferColor, Primitive->Color);
		Pipeline->SetConstantBuffer(2, false, ConstantBufferColor);

		Pipeline->SetVertexBuffer(Primitive->VertexBuffer, Stride);
		if (Primitive->IndexBuffer && Primitive->NumIndices > 0)
		{
			Pipeline->SetIndexBuffer(Primitive->IndexBuffer, Stride);
			Pipeline->DrawIndexed(Primitive->NumIndices, 0, 0);
		}
		else
		{
			Pipeline->Draw(Primitive->NumVertices, 0);
		}
	}
}

void FEditorOverlayPass::Release()
{
	SafeRelease(VS);
	SafeRelease(PS);
	SafeRelease(InputLayout);
	SafeRelease(ConstantBufferColor);
}
