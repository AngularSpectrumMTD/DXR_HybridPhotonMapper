#include "sceneCBDefinition.hlsli"
ConstantBuffer<SceneCB> gSceneParam : register(b0);
#include "sceneParamInterface.hlsli"

#include "spectralRenderingHelper.hlsli"
#include "reservoir.hlsli"

#define THREAD_NUM 16
#define REINHARD_L 1000
#define MAX_ACCUMULATION_RANGE 1000

Texture2D<float4> HistoryDIBuffer : register(t0);
Texture2D<float4> HistoryGIBuffer : register(t1);
Texture2D<float4> HistoryCausticsBuffer : register(t2);
Texture2D<float4> NormalDepthBuffer : register(t3);
Texture2D<float4> PrevNormalDepthBuffer : register(t4);
Texture2D<float2> VelocityBuffer : register(t5);
Texture2D<float2> LuminanceMomentBufferSrc : register(t6);
StructuredBuffer<DIReservoir> DIReservoirBufferSrc : register(t7);
Texture2D<float4> IDRoughnessBuffer : register(t8);
Texture2D<float4> PrevIDRoughnessBuffer : register(t9);
Texture2D<float4> PositionBuffer : register(t10);
Texture2D<float4> PrevPositionBuffer : register(t11);

RWTexture2D<float4> CurrentDIBuffer : register(u0);
RWTexture2D<float4> CurrentGIBuffer : register(u1);
RWTexture2D<float4> CurrentCausticsBuffer : register(u2);
RWTexture2D<float4> DIGIBuffer : register(u3);
RWTexture2D<uint> AccumulationCountBuffer : register(u4);
RWTexture2D<float2> LuminanceMomentBufferDst : register(u5);

//restrict
bool isWithinBounds(uint2 id, int2 size)
{
    return ((0 <= id.x) && (id.x <= (size.x - 1))) && ((0 <= id.y) && (id.y <= (size.y - 1)));
}

float computeLuminance(const float3 linearRGB)
{
    return dot(float3(0.2126, 0.7152, 0.0722), linearRGB);
}

float reinhard(float x, float L)
{
    return (x / (1 + x)) * (1 + x / L / L);
}

float3 reinhard3f(float3 v, float L)
{
    return float3(reinhard(v.x, L), reinhard(v.y, L), reinhard(v.z, L));
}

float ACESFilmicTonemapping(float x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate(x * (a * x + b) / (x * (c * x + d) + c));
}

float3 ACESFilmicTonemapping3f(float3 v)
{
    return float3(ACESFilmicTonemapping(v.x), ACESFilmicTonemapping(v.y), ACESFilmicTonemapping(v.z));
}

[numthreads(THREAD_NUM, THREAD_NUM, 1)]
void temporalAccumulation(uint3 dtid : SV_DispatchThreadID)
{
    float2 dims;
    CurrentDIBuffer.GetDimensions(dims.x, dims.y);

    uint2 currID = dtid.xy;

    float currDepth = NormalDepthBuffer[currID].w;
    float3 currNormal = NormalDepthBuffer[currID].xyz;
    uint currInstanceIndex = IDRoughnessBuffer[currID].y;
    float currRoughness = IDRoughnessBuffer[currID].z;
    uint accCount = AccumulationCountBuffer[currID];

    float2 velocity = VelocityBuffer[currID];
    //velocity.y = 1.0f - velocity.y;
    //velocity = velocity * 2.0f;// - 1.0f;
    float2 currUV = currID / dims;
    float2 prevUV = currUV + velocity;
    uint2 prevID = prevUV * dims;

    float3 currPos = PositionBuffer[currID].xyz;

    float3 currDI = 0.xxx;
    if(isUseNEE() && isUseWRS_RIS())
    {
        const uint serialCurrID = currID.y * dims.x + currID.x;
        DIReservoir currDIReservoir = DIReservoirBufferSrc[serialCurrID];

        float3 reservoirElementRemovedDI = CurrentDIBuffer[currID].rgb;
        currDI = shadeDIReservoir(currDIReservoir) + reservoirElementRemovedDI;
    }
    else
    {
        currDI = CurrentDIBuffer[currID].rgb;
    }
    float3 currGI = CurrentGIBuffer[currID].rgb;
    float3 currCaustics = CurrentCausticsBuffer[currID].rgb;

    if(isWithinBounds(prevID, dims) && (currDepth != 0))
    {
        float3 prevDI = HistoryDIBuffer[prevID].rgb;
        float3 prevGI = HistoryGIBuffer[prevID].rgb;
        float3 prevCaustics = HistoryCausticsBuffer[prevID].rgb;
        float3 currDIGI = currDI + currGI;

        uint prevInstanceIndex = PrevIDRoughnessBuffer[prevID].y;
        float prevRoughness = PrevIDRoughnessBuffer[prevID].z;
        float prevDepth = PrevNormalDepthBuffer[prevID].w;
        float3 prevNormal = PrevNormalDepthBuffer[prevID].xyz;
        float2 prevLuminanceMoment = LuminanceMomentBufferSrc[prevID];

        float luminance = computeLuminance(currDIGI);
        float2 currLuminanceMoment = float2(luminance, luminance * luminance);

        float3 prevPos = PrevPositionBuffer[prevID].xyz;

        const bool isNearDepth = ((currDepth * 0.95 < prevDepth) && (prevDepth < currDepth * 1.05)) && (currDepth > 0) && (prevDepth > 0);
        const bool isNearNormal = dot(currNormal, prevNormal) > 0.8;
        const bool isNearPosition = (sqrt(dot(currPos - prevPos, currPos - prevPos)) < 0.3f);//30cm
        const bool isSameInstance = (currInstanceIndex == prevInstanceIndex);
        const bool isNearRoughness = (abs(currRoughness - prevRoughness) < 0.05);
        const bool isNearPositionWithNormal = (abs(dot(currNormal, currPos - prevPos)) < 0.01f);

        //const bool isAccumulationEnable = isNearPositionWithNormal && !isHistoryResetRequested();

        const bool isAccumulationEnable = isAccumulationApply();
        
        if (isAccumulationEnable)
        {
            accCount++;
        }
        else
        {
            accCount = 1;
        }
        AccumulationCountBuffer[currID] = accCount;

        if (accCount < MAX_ACCUMULATION_RANGE)
        {
            const float tmpAccmuRatio = 1.f / accCount;
            //const float DIAccumuRatio = isUseReservoirTemporalReuse() ? ((accCount > 100) ? tmpAccmuRatio : 1) : tmpAccmuRatio;
            float3 accumulatedDI = lerp(prevDI, currDI, tmpAccmuRatio);
            float3 accumulatedGI = lerp(prevGI, currGI, tmpAccmuRatio);
            float3 accumulatedDIGI = accumulatedDI + accumulatedGI;
            float3 accumulatedCaustics = lerp(prevCaustics, currCaustics, tmpAccmuRatio);

            currLuminanceMoment.x = lerp(prevLuminanceMoment.x, currLuminanceMoment.x, tmpAccmuRatio);
            currLuminanceMoment.y = lerp(prevLuminanceMoment.y, currLuminanceMoment.y, tmpAccmuRatio);

            float3 toneMappedDIGI = float3(accumulatedDIGI * reinhard(computeLuminance(accumulatedDIGI), REINHARD_L) / computeLuminance(accumulatedDIGI));//luminance based tone mapping
            DIGIBuffer[currID].rgb = toneMappedDIGI + getCausticsBoost() * mul(accumulatedCaustics, XYZtoRGB2);
            CurrentDIBuffer[currID].rgb = accumulatedDI;
            CurrentGIBuffer[currID].rgb = accumulatedGI;
            CurrentCausticsBuffer[currID].rgb = accumulatedCaustics;
            LuminanceMomentBufferDst[currID] = currLuminanceMoment;
        }
    }
    else
    {
        AccumulationCountBuffer[currID] = 1;
        float3 currDIGI = currDI + currGI;
        float3 toneMappedDIGI = float3(currDIGI * reinhard(computeLuminance(currDIGI), REINHARD_L) / computeLuminance(currDIGI));//luminance based tone mapping
        DIGIBuffer[currID].rgb = toneMappedDIGI + getCausticsBoost() * mul(currCaustics, XYZtoRGB2);
        CurrentDIBuffer[currID].rgb = currDI;
        CurrentGIBuffer[currID].rgb = currGI;
        CurrentCausticsBuffer[currID].rgb = currCaustics;
        float luminance = computeLuminance(currDIGI);
        float2 currLuminanceMoment = float2(luminance, luminance * luminance);
        LuminanceMomentBufferDst[currID] = currLuminanceMoment;
    }
}