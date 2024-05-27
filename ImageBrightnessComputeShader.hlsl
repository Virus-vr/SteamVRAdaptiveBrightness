// Compute shader to calculate brightness and reduction
Texture2D<float4> inputTexture : register(t0);
RWStructuredBuffer<uint> brightnessBuffer : register(u0);

[numthreads(16, 16, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    // Get the texture dimensions
    uint2 dimensions;
    inputTexture.GetDimensions(dimensions.x, dimensions.y);
    
    uint downSample = 4;
    float2 samplePos = id.xy * downSample; // Since we don't need to sample every pixel we can get away with sampling only every 4th.
    
    float4 color = inputTexture.Load(int3(samplePos, 0));
    float brightness = 0.299 * color.r + 0.587 * color.g + 0.114 * color.b; // Luminance calculation
    
    // Center position of the vr view (more left then the center of the image)
    float2 center = float2((float)dimensions.x / 2.5, dimensions.y / 2); // ToDo: Calculate this outside of the shader and pass it into the shader.
    
    // Calculate distance from the center
    float distanceFromCenter = length(samplePos - center) / length(dimensions);
    
    //Weight Influenz by distance to center
    float weightedBrightness = brightness * max(1 - pow(2 * distanceFromCenter, 0.3), 0);
    uint brightnessInt = (uint) (weightedBrightness * 1000); // Could multiply by smaller number, but it would be less accurate.
    InterlockedAdd(brightnessBuffer[0], brightnessInt);
}