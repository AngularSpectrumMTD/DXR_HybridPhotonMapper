#include "common.hlsli"

float2 equirecFetchUV(float3 dir)
{
    float2 uv = float2(atan2(dir.z , dir.x) / 2.0 / PI + 0.5, acos(dir.y) / PI);
    return uv;
}

[shader("miss")]
void miss(inout Payload payload) {
    depthPositionNormalStore(payload, gSceneParam.backgroundColor.rgb);
    float4 cubemap = gEquiRecEnvMap.SampleLevel(gSampler, equirecFetchUV(WorldRayDirection()), 0.0);

    //payload.color += 0.1.xxx;
    //payload.color = 0;
    
    float3 directionalLightDir = normalize(gSceneParam.directionalLightDirection.xyz);
    float3 directionalLightEnergy = (dot(directionalLightDir, WorldRayDirection()) <0) ? gSceneParam.directionalLightColor.xyz : float3(0, 0, 0);
    directionalLightEnergy *= (payload.recursive == 0) ? 0 : 1;
    float3 curEnergy = payload.energy + directionalLightEnergy;
    payload.color += curEnergy * cubemap.rgb;
    payload.energy = 0.xxx;
    //payload.color = directionalLightEnergy;
}

[shader("miss")]
void photonMiss(inout PhotonPayload payload)
{
    payload.throughput = float3(0,0,0);
}

[shader("miss")]
void dummyMiss(inout Payload payload)
{
    //no op
}
