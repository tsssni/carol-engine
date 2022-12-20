#include <global_resources.h>
#include <manager/mesh.h>

void Carol::GlobalResources::DrawMeshes(ID3D12PipelineState* pso, bool textureDrawing)
{
	CommandList->SetPipelineState(pso);

	for (auto* mesh : *OpaqueStaticMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	for (auto* mesh : *OpaqueSkinnedMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	for (auto* mesh : *TransparentStaticMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	for (auto* mesh : *TransparentSkinnedMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}
}

void Carol::GlobalResources::DrawMeshes(ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO, ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO, bool textureDrawing)
{
	CommandList->SetPipelineState(opaqueStaticPSO);
	for (auto* mesh : *OpaqueStaticMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	CommandList->SetPipelineState(opaqueSkinnedPSO);
	for (auto* mesh : *OpaqueSkinnedMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	CommandList->SetPipelineState(transparentStaticPSO);
	for (auto* mesh : *TransparentStaticMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	CommandList->SetPipelineState(transparentSkinnedPSO);
	for (auto* mesh : *TransparentSkinnedMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}
}

void Carol::GlobalResources::DrawOpaqueMeshes(ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO, bool textureDrawing)
{
	CommandList->SetPipelineState(opaqueStaticPSO);
	for (auto* mesh : *OpaqueStaticMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	CommandList->SetPipelineState(opaqueSkinnedPSO);
	for (auto* mesh : *OpaqueSkinnedMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}
}

void Carol::GlobalResources::DrawTransparentMeshes(ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO, bool textureDrawing)
{
	CommandList->SetPipelineState(transparentStaticPSO);
	for (auto* mesh : *TransparentStaticMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	CommandList->SetPipelineState(transparentSkinnedPSO);
	for (auto* mesh : *TransparentSkinnedMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}
}

void Carol::GlobalResources::DrawSkyBox(ID3D12PipelineState* skyBoxPSO)
{
	CommandList->SetPipelineState(skyBoxPSO);
	SkyBoxMesh->Draw();
}

