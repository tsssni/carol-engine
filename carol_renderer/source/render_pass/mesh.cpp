#include <render_pass/mesh.h>
#include <render_pass/global_resources.h>
#include <render_pass/shadow.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/root_signature.h>
#include <dx12/shader.h>
#include <scene/scene.h>
#include <scene/model.h>
#include <scene/camera.h>
#include <utils/common.h>
#include <cmath>

#define AS_GROUP_SIZE 32

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::unique_ptr;
	using std::make_unique;
	using namespace DirectX;
	
}

Carol::MeshesPass::MeshesPass(GlobalResources* globalResources)
	:RenderPass(globalResources), 
	mIndirectCommand(MESH_TYPE_COUNT),
	mIndirectCommandBuffer(MESH_TYPE_COUNT),
	mCullMarkBuffer(MESH_TYPE_COUNT)
{
	InitCommandSignature();
	InitBuffers();
}

void Carol::MeshesPass::Draw()
{
}

void Carol::MeshesPass::Update()
{
	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		mIndirectCommand[i].clear();
		
		for (auto& meshMapPair : mGlobalResources->Scene->GetMeshes(MeshType(i)))
		{
			mIndirectCommand[i].emplace_back();
			auto& mesh = meshMapPair.second;
			auto& indirectCmd = mIndirectCommand[i].back();

			for (int i = 0; i < Mesh::MESH_IDX_COUNT; ++i)
			{
				indirectCmd.MeshConstants[i] = mesh->GetMeshIdx(i);
			}

			indirectCmd.MeshCB = mesh->GetMeshCBGPUVirtualAddress();
			indirectCmd.SkinnedCB = mesh->GetSkinnedCBGPUVirtualAddress();
			indirectCmd.DispatchMeshArgs.ThreadGroupCountX = ceilf(mesh->GetMeshletSize() * 1.f / AS_GROUP_SIZE);
			indirectCmd.DispatchMeshArgs.ThreadGroupCountY = 1;
			indirectCmd.DispatchMeshArgs.ThreadGroupCountZ = 1;
		}

		if (mIndirectCommand[i].size() > 0) {
			uint32_t cmdBufferSize = mIndirectCommand[i].size() * sizeof(IndirectCommand);
			mIndirectCommandBuffer[i].reset();

			mIndirectCommandBuffer[i] = make_unique<StructuredBuffer>(
				mIndirectCommand[i].size(),
				sizeof(IndirectCommand),
				mGlobalResources->UploadBuffersHeap,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				mGlobalResources->CbvSrvUavAllocator);

			mIndirectCommandBuffer[i]->CopyData(mIndirectCommand[i].data(), cmdBufferSize);
		
			uint32_t cullMarkSize = std::max((uint32_t)ceilf(mIndirectCommand[i].size() / 8.f), 8u);
			mCullMarkBuffer[i].reset();
			mCullMarkBuffer[i] = make_unique<RawBuffer>(
				cullMarkSize,
				mGlobalResources->DefaultBuffersHeap,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				mGlobalResources->CbvSrvUavAllocator,
				D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		}
	}
}

void Carol::MeshesPass::OnResize()
{
}

void Carol::MeshesPass::ReleaseIntermediateBuffers()
{
}

void Carol::MeshesPass::ClearCullMark()
{
	uint32_t currFrame = *mGlobalResources->CurrFrame;
	static const uint32_t cullMarkClearValue = 0;

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		mGlobalResources->CommandList->ClearUnorderedAccessViewUint(mCullMarkBuffer[i]->GetGpuUav(), mCullMarkBuffer[i]->GetCpuUav(), mCullMarkBuffer[i]->Get(), &cullMarkClearValue, 0, nullptr);

		for (auto& meshMapPair : mGlobalResources->Scene->GetMeshes((MeshType)i))
		{
			auto& mesh = meshMapPair.second;
			mesh->ClearCullMark(mGlobalResources->CommandList);
		}
	}
}

ID3D12CommandSignature* Carol::MeshesPass::GetCommandSignature()
{
	return mCommandSignature.Get();
}

uint32_t Carol::MeshesPass::GetCullMarkIdx(MeshType type)
{
	return mCullMarkBuffer[type]->GetGpuUavIdx();
}

uint32_t Carol::MeshesPass::GetCommandBufferIdx(MeshType type)
{
	return mIndirectCommandBuffer[type]->GetGpuUavIdx();
}

void Carol::MeshesPass::DrawMeshes(const std::vector<ID3D12PipelineState*>& pso)
{
	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		if (pso[i])
		{
			mGlobalResources->CommandList->SetPipelineState(pso[i]);
			for (auto& meshMapPair : mGlobalResources->Scene->GetMeshes((MeshType)i))
			{
				auto& mesh = meshMapPair.second;
				Draw(mesh);
			}
		}
	}
}

void Carol::MeshesPass::DrawSkyBox(ID3D12PipelineState* skyBoxPSO)
{
	mGlobalResources->CommandList->SetPipelineState(skyBoxPSO);
	Draw(mGlobalResources->Scene->GetSkyBox());
}

void Carol::MeshesPass::InitShaders()
{
}

void Carol::MeshesPass::InitPSOs()
{
}

void Carol::MeshesPass::InitBuffers()
{
}

void Carol::MeshesPass::InitCommandSignature()
{
	D3D12_INDIRECT_ARGUMENT_DESC argDesc[4];

	argDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
	argDesc[0].ConstantBufferView.RootParameterIndex = RootSignature::MESH_CB;

	argDesc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
	argDesc[1].Constant.RootParameterIndex = RootSignature::MESH_CONSTANTS;
	argDesc[1].Constant.Num32BitValuesToSet = Mesh::MESH_IDX_COUNT;
	argDesc[1].Constant.DestOffsetIn32BitValues = 0;

	argDesc[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
	argDesc[2].ConstantBufferView.RootParameterIndex = RootSignature::SKINNED_CB;

	argDesc[3].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;

	D3D12_COMMAND_SIGNATURE_DESC cmdSigDesc = {};
	cmdSigDesc.NumArgumentDescs = _countof(argDesc);
	cmdSigDesc.pArgumentDescs = argDesc;
	cmdSigDesc.ByteStride = sizeof(IndirectCommand);
	cmdSigDesc.NodeMask = 0;

	ThrowIfFailed(mGlobalResources->Device->CreateCommandSignature(&cmdSigDesc, mGlobalResources->RootSignature->Get(), IID_PPV_ARGS(mCommandSignature.GetAddressOf())));
}

void Carol::MeshesPass::Draw(Mesh* mesh)
{
	mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(RootSignature::MESH_CB, mesh->GetMeshCBGPUVirtualAddress());
	mGlobalResources->CommandList->SetGraphicsRoot32BitConstants(RootSignature::MESH_CONSTANTS, Mesh::MESH_IDX_COUNT, mesh->GetMeshIdxData(), 0);

	if (mesh->IsSkinned())
	{
		mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(RootSignature::SKINNED_CB, mesh->GetSkinnedCBGPUVirtualAddress());
	}

	mGlobalResources->CommandList->DispatchMesh(ceilf(mesh->GetMeshletSize() * 1.f / AS_GROUP_SIZE), 1, 1);
}
