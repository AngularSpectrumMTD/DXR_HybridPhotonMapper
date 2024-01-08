#include "opticalFunction.hlsli"

#define SPP 1
#define REINHARD_L 1000

float reinhard(float x, float L)
{
    return (x / (1 + x)) * (1 + x / L / L);
}

float3 reinhard3f(float3 v, float L)
{
    return float3(reinhard(v.x, L), reinhard(v.y, L), reinhard(v.z, L));
}

void applyTimeDivision(inout float3 current, uint2 ID)
{
    float3 prev = gOutput1[ID].rgb;
    
    float currentDepth = gDepthBuffer[ID];
    float prevDepth = gPrevDepthBuffer[ID];
    uint accCount = gAccumulationCountBuffer[ID];

    float2 prevLuminanceMoment = gLuminanceMomentBufferSrc[ID];
    float luminance = luminanceFromRGB(current);
    float2 curremtLuminanceMoment = float2(luminance, luminance * luminance);

    if (currentDepth == 0 || prevDepth == 0)
    {
        gOutput1[ID].rgb = current;
        gLuminanceMomentBufferDst[ID] = curremtLuminanceMoment;
        gAccumulationCountBuffer[ID] = 1;
        return;
    }
    
    if (isAccumulationApply())
    {
        accCount++;
    }
    else
    {
        accCount = 1;
    }
    accCount = min(accCount, 3000);
    gAccumulationCountBuffer[ID] = accCount;
    const float tmpAccmuRatio = 1.f / accCount;
    current = lerp(prev, current, tmpAccmuRatio);
    curremtLuminanceMoment.x = lerp(prevLuminanceMoment.x, curremtLuminanceMoment.x, tmpAccmuRatio);
    curremtLuminanceMoment.y = lerp(prevLuminanceMoment.y, curremtLuminanceMoment.y, tmpAccmuRatio);

    gOutput1[ID].rgb = current;
    gLuminanceMomentBufferDst[ID] = curremtLuminanceMoment;
}

//
//DispatchRays By Screen Size2D
//
[shader("raygeneration")]
void rayGen() {
    uint2 launchIndex = DispatchRaysIndex().xy;
    gDepthBuffer[launchIndex] = 0;
    float2 dims = float2(DispatchRaysDimensions().xy);

    float3 accumColor = 0.xxx;

    //random
    float LightSeed = getLightRandomSeed();
    uint seed = (launchIndex.x + (DispatchRaysDimensions().x + 10 * (uint) LightSeed.x) * launchIndex.y);
    randGenState = uint(pcgHash(seed));
    rseed = LightSeed.x;

    const float energyBoost = 1.5f;

    for(int i = 0; i < SPP ; i++)
    {
        float2 IJ = int2(i / (SPP / 2.f), i % (SPP / 2.f)) - 0.5.xx;

        float2 d = (launchIndex.xy + 0.5) / dims.xy * 2.0 - 1.0 + IJ / dims.xy;
        RayDesc rayDesc;
        rayDesc.Origin = mul(gSceneParam.mtxViewInv, float4(0, 0, 0, 1)).xyz;

        float4 target = mul(gSceneParam.mtxProjInv, float4(d.x, -d.y, 1, 1));
        rayDesc.Direction = normalize(mul(gSceneParam.mtxViewInv, float4(target.xyz, 0)).xyz);

        rayDesc.TMin = 0;
        rayDesc.TMax = 100000;

        Payload payload;
        payload.energy = energyBoost * float3(1, 1, 1);
        payload.color = float3(0, 0, 0);
        payload.recursive = 0;
        payload.storeIndexXY = launchIndex;
        payload.stored = 0;//empty
        payload.eyeDir = rayDesc.Direction;
        payload.isShadowRay = 0;
        payload.isShadowMiss = 0;

        RAY_FLAG flags = RAY_FLAG_NONE;

        uint rayMask = 0xFF;

        TraceRay(
            gRtScene, 
            flags,
            rayMask,
            0, // ray index
            1, // MultiplierForGeometryContrib
            0, // miss index
            rayDesc,
            payload);

        accumColor += payload.color;
    }
    float3 finalCol = accumColor / SPP;
    //float3 finalCol = reinhard3f((accumColor + accumPhotonColor) / SPP, REINHARD_L);
    applyTimeDivision(finalCol, launchIndex);
    gOutput[launchIndex.xy] = float4(finalCol, 1);
}

//
//DispatchRays By Photon Size2D
//
[shader("raygeneration")]
void photonEmitting()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 dispatchDimensions = DispatchRaysDimensions();
    
    //random
    float LightSeed = getLightRandomSeed();
    uint seed = (launchIndex.x + (DispatchRaysDimensions().x + 100000 * (uint)LightSeed.x) * launchIndex.y);
    randGenState = uint(pcgHash(seed));

    PhotonInfo photon;
    photon.throughput = float3(0,0,0);
    photon.position = float3(0,0,0);

    int serialIndex = SerialRaysIndex(launchIndex, dispatchDimensions);
    const int COLOR_ID = serialIndex % getLightLambdaNum();

    gPhotonMap[serialIndex] = photon;//initialize

    float3 emitOrigin = 0.xxx;
    float3 emitDir = 0.xxx;

    SampleLightEmitDirAndPosition(emitDir, emitOrigin);
    
    float LAMBDA_NM = LAMBDA_VIO_NM + LAMBDA_STEP * (randGenState % LAMBDA_NUM);

    RayDesc rayDesc;
    rayDesc.Origin = emitOrigin;
    rayDesc.Direction = emitDir;
    rayDesc.TMin = 0;
    rayDesc.TMax = 100000;

    PhotonPayload payload;
    float emitIntensity = length(gSceneParam.lightColor.xyz);
    payload.throughput = emitIntensity * getBaseLightXYZ(LAMBDA_NM);
    payload.recursive = 0;
    payload.storeIndex = serialIndex;
    payload.stored = 0;//empty
    payload.lambdaNM = LAMBDA_NM;

    RAY_FLAG flags = RAY_FLAG_NONE;

    uint rayMask = ~(LIGHT_INSTANCE_MASK); //ignore your self!! lightsource model

    TraceRay(
        gRtScene,
        flags,
        rayMask,
        0, // ray index
        1, // MultiplierForGeometryContrib
        0, // miss index
        rayDesc,
        payload);
}