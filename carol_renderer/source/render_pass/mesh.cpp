#include <render_pass/mesh.h>
#include <render_pass/global_resources.h>
#include <render_pass/shadow.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor.h>
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
	mIndirectCommandBuffer(globalResources->NumFrame),
	mMeshCB(globalResources->NumFrame),
	mSkinnedCB(globalResources->NumFrame),
	mInstanceFrustumCulledMarkBuffer(make_unique<RawBuffer>(
		2 << 16,
		mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
		mGlobalResources->DescriptorManager,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)),
	mInstanceOcclusionPassedMarkBuffer(make_unique<RawBuffer>(
		2 << 16,
		mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
		mGlobalResources->DescriptorManager,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
{
	InitCommandSignature();
	InitBuffers();
}

void Carol::MeshesPass::Draw()
{
}

void Carol::MeshesPass::Update()
{
	uint32_t currFrame = *mGlobalResources->CurrFrame;
	uint32_t meshCount[MESH_TYPE_COUNT];
	uint32_t totalMeshCount = 0;

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		meshCount[i] = mGlobalResources->Scene->GetMeshesCount(MeshType(i));
		totalMeshCount += meshCount[i];

		if (i == 0)
		{
			mMeshStartOffset[i] = 0;
		}
		else
		{
			mMeshStartOffset[i] = mMeshStartOffset[i - 1] + meshCount[i - 1];
		}
	}

	TestBufferSize(mIndirectCommandBuffer[currFrame], totalMeshCount);
	TestBufferSize(mMeshCB[currFrame], totalMeshCount);
	TestBufferSize(mSkinnedCB[currFrame], mGlobalResources->Scene->GetModelsCount());

	int modelIdx = 0;
	for (auto& modelMapPair : mGlobalResources->Scene->GetModels())
	{
		auto& model = modelMapPair.second;

		if (model->IsSkinned())
		{
			mSkinnedCB[currFrame]->CopyElements(model->GetSkinnedConstants(), modelIdx);
			model->SetSkinnedCBAddress(mSkinnedCB[currFrame]->GetElementAddress(modelIdx));
			++modelIdx;
		}
	}

	int meshIdx = 0;
	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		for (auto& meshMapPair : mGlobalResources->Scene->GetMeshes(MeshType(i)))
		{
			auto& mesh = meshMapPair.second;
			mMeshCB[currFrame]->CopyElements(mesh->GetMeshConstants(), meshIdx);
			mesh->SetMeshCBAddress(mMeshCB[currFrame]->GetElementAddress(meshIdx));

			IndirectCommand indirectCmd;
			
			indirectCmd.MeshCBAddr = mesh->GetMeshCBAddress();
			indirectCmd.SkinnedCBAddr = mesh->GetSkinnedCBAddress();

			indirectCmd.DispatchMeshArgs.ThreadGroupCountX = ceilf(mesh->GetMeshletSize() * 1.f / AS_GROUP_SIZE);
			indirectCmd.DispatchMeshArgs.ThreadGroupCountY = 1;
			indirectCmd.DispatchMeshArgs.ThreadGroupCountZ = 1;

			mIndirectCommandBuffer[currFrame]->CopyElements(&indirectCmd, meshIdx);

			++meshIdx;
		}
	}

	mMeshCB[currFrame]->CopyElements(mGlobalResources->Scene->GetSkyBox()->GetMeshConstants(), meshIdx);
	mGlobalResources->Scene->GetSkyBox()->SetMeshCBAddress(mMeshCB[currFrame]->GetElementAddress(meshIdx));
}

void Carol::MeshesPass::OnResize()
{
}

void Carol::MeshesPass::ReleaseIntermediateBuffers()
{
}

void Carol::MeshesPass::ClearCullMark()
{
	static const uint32_t cullMarkClearValue = 0;
	mGlobalResources->CommandList->ClearUnorderedAccessViewUint(mInstanceFrustumCulledMarkBuffer->GetGpuUav(), mInstanceFrustumCulledMarkBuffer->GetCpuUav(), mInstanceFrustumCulledMarkBuffer->Get(), &cullMarkClearValue, 0, nullptr);
	mGlobalResources->CommandList->ClearUnorderedAccessViewUint(mInstanceOcclusionPassedMarkBuffer->GetGpuUav(), mInstanceOcclusionPassedMarkBuffer->GetCpuUav(), mInstanceOcclusionPassedMarkBuffer->Get(), &cullMarkClearValue, 0, nullptr);

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
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

uint32_t Carol::MeshesPass::GetCommandBufferIdx()
{
	return mIndirectCommandBuffer[*mGlobalResources->CurrFrame]->GetGpuSrvIdx();
}

uint32_t Carol::MeshesPass::GetInstanceFrustumCulledMarkBufferIdx()
{
	return mInstanceFrustumCulledMarkBuffer->GetGpuUavIdx();
}

uint32_t Carol::MeshesPass::GetInstanceOcclusionPassedMarkBufferIdx()
{
	return mInstanceOcclusionPassedMarkBuffer->GetGpuUavIdx();
}

uint32_t Carol::MeshesPass::GetMeshCBIdx()
{
	return mMeshCB[*mGlobalResources->CurrFrame]->GetGpuSrvIdx();
}

uint32_t Carol::MeshesPass::GetMeshCBStartOffet(MeshType type)
{
	return mMeshStartOffset[type];
}

void Carol::MeshesPass::ExecuteIndirect(StructuredBuffer* indirectCmdBuffer)
{
	mGlobalResources->CommandList->ExecuteIndirect(
		mCommandSignature.Get(),
		indirectCmdBuffer->GetNumElements(),
		indirectCmdBuffer->Get(),
		0,
		indirectCmdBuffer->Get(),
		indirectCmdBuffer->GetCounterOffset());
}

void Carol::MeshesPass::DrawSkyBox(ID3D12PipelineState* skyBoxPSO)
{
	mGlobalResources->CommandList->SetPipelineState(skyBoxPSO);
	mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(MESH_CB, mGlobalResources->Scene->GetSkyBox()->GetMeshCBAddress());
	mGlobalResources->CommandList->DispatchMesh(1, 1, 1);
}

void Carol::MeshesPass::InitShaders()
{
}

void Carol::MeshesPass::InitPSOs()
{
}

void Carol::MeshesPass::InitBuffers()
{
	for (int i = 0; i < mGlobalResources->NumFrame; ++i)
	{
		ResizeBuffer(mIndirectCommandBuffer[i], 1024, sizeof(IndirectCommand), false);
		ResizeBuffer(mMeshCB[i], 1024, sizeof(MeshConstants), true);
		ResizeBuffer(mSkinnedCB[i], 1024, sizeof(SkinnedConstants), true);
	}
}

void Carol::MeshesPass::InitCommandSignature()
{
	D3D12_INDIRECT_ARGUMENT_DESC argDesc[3];

	argDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
	argDesc[0].ConstantBufferView.RootParameterIndex = MESH_CB;

	argDesc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
	argDesc[1].ConstantBufferView.RootParameterIndex = SKINNED_CB;

	argDesc[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;

	D3D12_COMMAND_SIGNATURE_DESC cmdSigDesc = {};
	cmdSigDesc.NumArgumentDescs = _countof(argDesc);
	cmdSigDesc.pArgumentDescs = argDesc;
	cmdSigDesc.ByteStride = sizeof(IndirectCommand);
	cmdSigDesc.NodeMask = 0;

	ThrowIfFailed(mGlobalResources->Device->CreateCommandSignature(&cmdSigDesc, mGlobalResources->RootSignature->Get(), IID_PPV_ARGS(mCommandSignature.GetAddressOf())));
}

void Carol::MeshesPass::TestBufferSize(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements)
{
	if (buffer->GetNumElements() < numElements)
	{
		ResizeBuffer(buffer, numElements, buffer->GetElementSize(), buffer->IsConstant());
	}
}

void Carol::MeshesPass::ResizeBuffer(unique_ptr<StructuredBuffer>& buffer, uint32_t numElements, uint32_t elementSize, bool isConstant)
{
	buffer = make_unique<StructuredBuffer>(
		numElements,
		elementSize,
		mGlobalResources->HeapManager->GetUploadBuffersHeap(),
		mGlobalResources->DescriptorManager,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_FLAG_NONE,
		isConstant);
}
