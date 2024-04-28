#ifndef __SAMPLING_LIGHT_HLSLI__
#define __SAMPLING_LIGHT_HLSLI__

//=========================================================================
//Lighting
//=========================================================================
struct LightSample
{
    float3 directionToLight;
    float3 normal;
    float3 emission;
    float distance;
    float pdf;
};

#define DIRECTIONAL_LIGHT_SPREAD_HALF_ANGLE_RADIAN 5 * PI / 180

//ok
void sampleSphereLight(in LightGenerateParam lightGen, in float3 scatterPosition, inout LightSample lightSample)
{
    float u = rand();
    float v = rand();
    float z = -2.0f * u + 1.0f;
    float y = sqrt(max(0, 1 - z * z)) * sin(2.0f * PI * v);
    float x = sqrt(max(0, 1 - z * z)) * cos(2.0f * PI * v);

    float3 pos = lightGen.position + lightGen.sphereRadius * float3(x, y, z);
    lightSample.directionToLight = pos - scatterPosition;
    lightSample.normal = normalize(float3(x, y, z));
    lightSample.emission = lightGen.emission;
    lightSample.distance = sqrt(dot(lightSample.directionToLight, lightSample.directionToLight));
    lightSample.directionToLight /= lightSample.distance;
    lightSample.pdf = 1 / (4 * PI * lightGen.sphereRadius * lightGen.sphereRadius);
}

void sampleRectLight(in LightGenerateParam lightGen, in float3 scatterPosition, inout LightSample lightSample)
{
    float u = rand();
    float v = rand();
    float lenU = sqrt(dot(lightGen.U, lightGen.U));
    float lenV = sqrt(dot(lightGen.V, lightGen.V));

    float3 pos = lightGen.position + 2 * (u - 0.5) * lightGen.U + 2 * (v - 0.5) * lightGen.V;
    lightSample.directionToLight = pos - scatterPosition;
    lightSample.normal = normalize(cross(lightGen.U, lightGen.V));
    lightSample.emission = lightGen.emission;
    lightSample.distance = sqrt(dot(lightSample.directionToLight, lightSample.directionToLight));
    lightSample.directionToLight /= lightSample.distance;
    lightSample.pdf = 1 / (4 * lenU * lenV);
}

void sampleSpotLight(in LightGenerateParam lightGen, in float3 scatterPosition, inout LightSample lightSample)
{
    float u = rand();
    float v = rand();
    float lenU = sqrt(dot(lightGen.U, lightGen.U));
    float lenV = sqrt(dot(lightGen.V, lightGen.V));
    float r = lenU * sqrt(u);
    float theta = 2 * PI * v;
    float x = r * cos(theta);
    float y = r * sin(theta);
    y *= lenV / lenU;
    
    float3 pos = lightGen.position + x * normalize(lightGen.U) + y * normalize(lightGen.V);
    lightSample.directionToLight = pos - scatterPosition;
    lightSample.normal = normalize(cross(lightGen.U, lightGen.V));
    lightSample.emission = lightGen.emission;
    lightSample.distance = sqrt(dot(lightSample.directionToLight, lightSample.directionToLight));
    lightSample.directionToLight /= lightSample.distance;
    lightSample.pdf = 1 / (PI * lenU * lenV);
}

float3 coneSample(float3 N, float cosMax)
{
    float ct = 1 - rand() * (1 - cosMax);
    float st = sqrt(1 - ct * ct);
    float phi = 2 * PI * rand();
    float2 csp;
    sincos(phi, csp.x, csp.y);
    return tangentToWorld(N, float3(st * csp.x, st * csp.y, ct));
}

void sampleDirectionalLight(in LightGenerateParam lightGen, in float3 scatterPosition, inout LightSample lightSample)
{
    const float cosMax = cos(DIRECTIONAL_LIGHT_SPREAD_HALF_ANGLE_RADIAN);

    float3 fromLight = normalize(lightGen.position);
    float3 emit = coneSample(fromLight, cosMax);
    
    lightSample.directionToLight = -normalize(emit);
    lightSample.normal = fromLight;
    lightSample.emission = lightGen.emission;
    lightSample.distance = 10000000;
    lightSample.pdf = 1 / (2 * PI * (1 - cosMax));
    //lightSample.pdf = 1 / (2 * PI * (1 - cos(2 * DIRECTIONAL_LIGHT_SPREAD_HALF_ANGLE_RADIAN)));//sr is "2 * pi * (1 - cos(halfAngle))" but currently my conputation is failed...?
}

void sampleSphereLightEmitDirAndPosition(in LightGenerateParam lightGen, out float3 emitDir, out float3 position)
{
    emitDir = HemisphereORCosineSampling(float3(1, 0, 0), false);
    position = lightGen.position;
}

void sampleRectLightEmitDirAndPosition(in LightGenerateParam lightGen, out float3 emitDir, out float3 position)
{
    float rnd0 = (pcgHashState() - 0.5) * 2; //-1 to 1
    float rnd1 = (pcgHashState() - 0.5) * 2; //-1 to 1
    const float3 dominantDir = normalize(cross(lightGen.U, lightGen.V));
    emitDir = normalize(dominantDir * LIGHT_BASE_LENGTH + rnd0 * lightGen.U + rnd1 * lightGen.V);
    position = lightGen.position;
}

void sampleSpotLightEmitDirAndPosition(in LightGenerateParam lightGen, out float3 emitDir, out float3 position)
{
    const float3 dominantDir = normalize(cross(lightGen.U, lightGen.V));
    const float spotLightHalfAngle = atan2(length(lightGen.U), LIGHT_BASE_LENGTH);
    emitDir = getConeSample(pcgHashState(), dominantDir, spotLightHalfAngle);
    position = lightGen.position;
}

void sampleDirectionalLightEmitDirAndPosition(in LightGenerateParam lightGen, out float3 emitDir, out float3 position)
{
    const float cosMax = cos(DIRECTIONAL_LIGHT_SPREAD_HALF_ANGLE_RADIAN);
    float3 fromLight = normalize(lightGen.position);
    emitDir = coneSample(fromLight, cosMax);
    position = 100 * -emitDir;
}

void sampleLight(in float3 scatterPosition, inout LightSample lightSample, out bool isDirectionalLightSampled)
{
    const uint lightID = min(max(0, (uint) (rand() * (getLightNum()) + 0.5)), getLightNum() - 1);
    LightGenerateParam param = gLightGenerateParams[lightID];

    isDirectionalLightSampled = false;
    
    if (param.type == LIGHT_TYPE_SPHERE)
    {
        sampleSphereLight(param, scatterPosition, lightSample);
    }
    if (param.type == LIGHT_TYPE_RECT)
    {
        sampleRectLight(param, scatterPosition, lightSample);
    }
    if (param.type == LIGHT_TYPE_SPOT)
    {
        sampleSpotLight(param, scatterPosition, lightSample);
    }
    if (param.type == LIGHT_TYPE_DIRECTIONAL)
    {
        isDirectionalLightSampled = true;
        sampleDirectionalLight(param, scatterPosition, lightSample);
    }

    lightSample.pdf *= 1.0f / getLightNum();
}

bool intersectLightWithCurrentRay(out float3 Le)
{
    const float3 rayOrigin = WorldRayOrigin();
    const float3 rayDiretion = WorldRayDirection();
    const float rayT = RayTCurrent();

    const uint lightID = min(max(0, (uint) (rand() * (getLightNum()) + 0.5)), getLightNum() - 1);
    LightGenerateParam param = gLightGenerateParams[lightID];

    if (param.type == LIGHT_TYPE_SPHERE)
    {
        const float3 shapeForwardDir = normalize(cross(param.U, param.V));
        //float2 tt = intersectEllipsoid(rayOrigin, rayDiretion, param.position,
        //shapeForwardDir, normalize(param.V), param.sphereRadius, param.sphereRadius, param.sphereRadius);
        //float frontT = tt.x >= 0 ? tt.x : tt.y;
        float t = intersectSphere(rayOrigin, rayDiretion, param.position,
        shapeForwardDir, normalize(param.V), param.sphereRadius, param.sphereRadius, param.sphereRadius);

        Le = param.emission * max(1, getLightNum() - 1);
        return (t >= 0 && t < rayT);
    }
    else if (param.type == LIGHT_TYPE_RECT)
    {
        const float3 shapeForwardDir = normalize(cross(param.U, param.V));
        float hittedT = intersectRectangle(rayOrigin, rayDiretion, param.position, param.U, param.V);
        const bool isFrontHit = (length(shapeForwardDir) > 0) && (dot(shapeForwardDir, -rayDiretion) > 0);

        Le = param.emission * max(1, getLightNum() - 1);
        return isFrontHit && (hittedT > 0 && hittedT < rayT);
    }
    else if (param.type == LIGHT_TYPE_SPOT)
    {
        const float3 shapeForwardDir = normalize(cross(param.U, param.V));
        float hittedT = intersectEllipse(rayOrigin, rayDiretion, param.position, param.U, param.V);
        const bool isFrontHit = (length(shapeForwardDir) > 0) && (dot(shapeForwardDir, -rayDiretion) > 0);

        Le = param.emission * max(1, getLightNum() - 1);
        return isFrontHit && (hittedT > 0 && hittedT < rayT);
    }

    return false;
}

bool intersectAllLightWithCurrentRay(out float3 Le)
{
    const float3 rayOrigin = WorldRayOrigin();
    const float3 rayDiretion = WorldRayDirection();
    const float rayT = RayTCurrent();

    bool isIntersect = false;

    for (int i = 0; i < getLightNum(); i++)
    {
        LightGenerateParam param = gLightGenerateParams[i];

        if (param.type == LIGHT_TYPE_SPHERE)
        {
            const float3 shapeForwardDir = normalize(cross(param.U, param.V));
        //float2 tt = intersectEllipsoid(rayOrigin, rayDiretion, param.position,
        //shapeForwardDir, normalize(param.V), param.sphereRadius, param.sphereRadius, param.sphereRadius);
        //float frontT = tt.x >= 0 ? tt.x : tt.y;
            float t = intersectSphere(rayOrigin, rayDiretion, param.position,
            shapeForwardDir, normalize(param.V), param.sphereRadius, param.sphereRadius, param.sphereRadius);

            Le = param.emission * max(1, getLightNum() - 1);
            isIntersect = (t >= 0 && t < rayT);
            if (isIntersect)
            {
                break;
            }
        }
        else if (param.type == LIGHT_TYPE_RECT)
        {
            const float3 shapeForwardDir = normalize(cross(param.U, param.V));
            float hittedT = intersectRectangle(rayOrigin, rayDiretion, param.position, param.U, param.V);
            const bool isFrontHit = (length(shapeForwardDir) > 0) && (dot(shapeForwardDir, -rayDiretion) > 0);

            Le = param.emission * max(1, getLightNum() - 1);
            isIntersect =  isFrontHit && (hittedT > 0 && hittedT < rayT);
            if (isIntersect)
            {
                break;
            }
        }
        else if (param.type == LIGHT_TYPE_SPOT)
        {
            const float3 shapeForwardDir = normalize(cross(param.U, param.V));
            float hittedT = intersectEllipse(rayOrigin, rayDiretion, param.position, param.U, param.V);
            const bool isFrontHit = (length(shapeForwardDir) > 0) && (dot(shapeForwardDir, -rayDiretion) > 0);

            Le = param.emission * max(1, getLightNum() - 1);
            isIntersect =  isFrontHit && (hittedT > 0 && hittedT < rayT);
            if (isIntersect)
            {
                break;
            }
        }
    }

    return isIntersect;
}

float3 directionalLightingOnMissShader(Payload payload)
{
    float3 val = 0.xxx;

    float3 directionalDir = 0.xxx;
    bool isDirectionalLightFinded = false;
    float3 emis = 0.xxx;
    float3 dominantDir = 0.xxx;
    for (int i = 0; i < getLightNum(); i++)
    {
        LightGenerateParam lightGen = gLightGenerateParams[i];

        if (isDirectionalLightFinded == false && lightGen.type == LIGHT_TYPE_DIRECTIONAL)
        {
            isDirectionalLightFinded = true;
            emis = lightGen.emission;
            dominantDir = normalize(lightGen.position);
        }
    }
    
    const float cosMax = cos(DIRECTIONAL_LIGHT_SPREAD_HALF_ANGLE_RADIAN);
    if (payload.recursive > 0 && isDirectionalLightFinded && dot(dominantDir, -WorldRayDirection()) > cosMax)
    {
        val = payload.throughput * emis;
    }

    return val;
}

void sampleLightWithID(in float3 scatterPosition, in int ID, inout LightSample lightSample)
{
    LightGenerateParam param = gLightGenerateParams[ID];
    
    if (param.type == LIGHT_TYPE_SPHERE)
    {
        sampleSphereLight(param, scatterPosition, lightSample);
    }
    if (param.type == LIGHT_TYPE_RECT)
    {
        sampleRectLight(param, scatterPosition, lightSample);
    }
    if (param.type == LIGHT_TYPE_SPOT)
    {
        sampleSpotLight(param, scatterPosition, lightSample);
    }
    if (param.type == LIGHT_TYPE_DIRECTIONAL)
    {
        sampleDirectionalLight(param, scatterPosition, lightSample);
    }
}

bool isVisible(in float3 scatterPosition, in LightSample lightSample)
{
    Payload shadowPayload;
    shadowPayload.flags = 0;
    shadowPayload.flags |= PAYLOAD_BIT_MASK_IS_SHADOW_RAY;

    const float eps = 0.001;
    RayDesc shadowRay;
    shadowRay.TMin = eps;
    shadowRay.TMax = lightSample.distance - eps;

    shadowRay.Direction = lightSample.directionToLight;
    shadowRay.Origin = scatterPosition;

    RAY_FLAG flags = RAY_FLAG_NONE;

    uint rayMask = ~(LIGHT_INSTANCE_MASK);

    TraceRay(gRtScene, flags, rayMask, DEFAULT_RAY_ID, DEFAULT_GEOM_CONT_MUL, DEFAULT_MISS_ID, shadowRay, shadowPayload);

    return (shadowPayload.flags & PAYLOAD_BIT_MASK_IS_SHADOW_MISS);
}

void sampleLightEmitDirAndPosition(inout float3 dir, inout float3 position)
{
    const uint lightID = (uint) (rand() * (getLightNum()) + 0.5);
    LightGenerateParam param = gLightGenerateParams[0];
    //LightGenerateParam param = gLightGenerateParams[lightID];

    if (param.type == LIGHT_TYPE_SPHERE)
    {
        sampleSphereLightEmitDirAndPosition(param, dir, position);
    }
    if (param.type == LIGHT_TYPE_RECT)
    {
        sampleRectLightEmitDirAndPosition(param, dir, position);
    }
    if (param.type == LIGHT_TYPE_SPOT)
    {
        sampleSpotLightEmitDirAndPosition(param, dir, position);
    }
    if (param.type == LIGHT_TYPE_DIRECTIONAL)
    {
        sampleDirectionalLightEmitDirAndPosition(param, dir, position);
    }
}

#endif//__SAMPLING_LIGHT_HLSLI__