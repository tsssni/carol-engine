#include "include/common.hlsli"
#include "include/compute.hlsli"
#include "include/texture.hlsli"
#include "include/transform.hlsli"

cbuffer SsgiCB : register(b4)
{
    uint gSampleCount;
    uint gNumSteps;
}

float OcclusionFunction(float distZ)
{
    float occlusion = 0.0f;
    if (distZ > gSurfaceEplison)
    {
        float fadeLength = gOcclusionFadeEnd - gOcclusionFadeStart;
        occlusion = saturate((gOcclusionFadeEnd - distZ) / fadeLength);
    }
    
    return occlusion;
}


[numthreads(32, 32, 1)]
void main(int2 dtid : SV_DispatchThreadID)
{
    uint2 size;
    RWTexture2D<float4> ssgiMap = ResourceDescriptorHeap[gRWSsgiMapIdx];
    ssgiMap.GetDimensions(size.x, size.y);
    
    float2 texC = (dtid + 0.5f) / size;

    Texture2D sceneColorMap = ResourceDescriptorHeap[gSceneColorMapIdx];
    Texture2D depthMap = ResourceDescriptorHeap[gSsgiHiZMapIdx];
    Texture2D normalMap = ResourceDescriptorHeap[gNormalMapIdx];

    float ndcDepth = depthMap.Sample(gsamLinearClamp, texC).r;

    if (ndcDepth < 1.f && TextureBorderTest(dtid, size))
    {
        float centerViewDepth = NdcDepthToViewDepth(ndcDepth, gProj);
        float3 centerNormal = mul(normalize(normalMap.Sample(gsamLinearClamp, texC).xyz), (float3x3) gView);

        Texture2D randVecMap = ResourceDescriptorHeap[gRandVecMapIdx];
        float3 viewPos = TexPosToViewPos(texC, centerViewDepth, gInvProj);
        float3 randVec = 2.0f * randVecMap.Sample(gsamPointWrap, 4.0f * texC).xyz - 1.0f;
    
        float4 sampleColor = 0.f;
        uint sampleCount = 0;

        for (int sampleStep = 0; sampleStep < gSampleCount; ++sampleStep)
        {
            float3 traceVec = normalize(reflect(gOffsetVectors[sampleStep].xyz, randVec));
            traceVec *= sign(dot(traceVec, centerNormal)) * .1f;

            int traceStep;
            int traceSubstep;

            float mipLevel = 1.f;
            float4 sampleMip;

            bool4 multisampleHit = false;
            bool foundAnyHit = false;

            for (traceStep = 0; traceStep < gNumSteps; traceStep += 4)
            {
                float3 samplesPos[4];
                float2 samplesUV[4];
                float4 samplesZ;
                float4 sampleDepth;

                [unroll]
                for (traceSubstep = 0; traceSubstep < 4; ++traceSubstep)
                {
                    samplesPos[traceSubstep] = viewPos + (traceStep + traceSubstep + 1) * traceVec;
                }

                sampleMip.xy = mipLevel;
                mipLevel += 8.f / gNumSteps;
                sampleMip.zw = mipLevel;
                mipLevel += 8.f / gNumSteps;

                traceVec *= exp2(8.f / gNumSteps);

                [unroll]
                for (int sampleStep = 0; sampleStep < 4; ++sampleStep)
                {
                    float4 sampleProjPos = mul(float4(samplesPos[sampleStep], 1.f), gProj);
                    sampleProjPos /= sampleProjPos.w;

                    samplesUV[sampleStep] = NdcTexPosToTexPos(sampleProjPos.xy);
                    samplesZ[sampleStep] = sampleProjPos.z;
                    sampleDepth[sampleStep] = depthMap.SampleLevel(gsamLinearClamp, samplesUV[sampleStep], sampleMip[sampleStep]).r;
                }
                
                float4 depthDiff = samplesZ - sampleDepth;
                multisampleHit = depthDiff > 0.f;
                foundAnyHit = any(multisampleHit);

                if (foundAnyHit)
                {
                    break;
                }
            }

            if(foundAnyHit)
            {
                int hitStep = 0;
                int hitMip = 1.f;

                for (int hitSubstep = 0; hitSubstep < 4;++hitSubstep)
                {
                    if(multisampleHit[hitSubstep])
                    {
                        hitStep = traceStep + hitSubstep + 1;
                        hitMip = sampleMip[hitSubstep];
                        break;
                    }
                }

                float3 hitPos = viewPos + hitStep * traceVec;
                float2 uv = ProjPosToTexPos(mul(float4(hitPos, 1.f), gProj));

                sampleColor += sceneColorMap.SampleLevel(gsamLinearClamp, uv, hitMip) * dot(centerNormal, normalize(traceVec));
                ++sampleCount;

                continue;
            }
            
        }
        
        ssgiMap[dtid] = sampleColor / max(sampleCount, 1);
    }
}