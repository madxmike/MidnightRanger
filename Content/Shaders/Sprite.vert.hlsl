static const uint triangleIndices[6] = {0, 1, 2, 3, 2, 1};
static const float2 vertexPosition[4] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {0.0f, 1.0f},
    {1.0f, 1.0f}
};


// TODO (Michael): Utilize an atlas texture for sprites. This will require the UV, width, and height of the sprite's texture on the atlas to be passed. For now, its not needed.
struct SpriteData {
    float Width;
    float Height;
    float2 _padding;
    float3 Position;
    float Rotation;
    float2 Scale;
    float U;
    float V;
    float4 Color;
};

struct VSOutput {
    float2 UV: TEXCOORD0;
    float4 Color: COLOR0;
    float4 Position: SV_Position;
};

StructuredBuffer<SpriteData> DataBuffer : register(t0, space0);

cbuffer UniformBlock : register(b0, space1)
{
    float4x4 ViewProjectionMatrix : packoffset(c0);
};

VSOutput Main(uint id: SV_VertexID) {

    uint spriteIndex = id / 6;
    SpriteData sprite = DataBuffer[spriteIndex];

    uint vertexIndex = triangleIndices[id % 6];
    float2 position2D = vertexPosition[vertexIndex];
    position2D *= sprite.Scale;

    float c = cos(sprite.Rotation);
    float s = sin(sprite.Rotation);
    float2x2 rotation = {
        c, s,
        -s, c
    };
    position2D = mul(position2D, rotation);

    float3 position3D = float3(position2D + sprite.Position.xy, sprite.Position.z);

    float2 uvCoordinates[4] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {0.0f, 1.0f},
        {1.0f, 1.0f}
    };

    VSOutput output;
    output.Position = mul(ViewProjectionMatrix, float4(position3D, 1.0f));
    output.UV = uvCoordinates[vertexIndex];
    output.Color = sprite.Color;


    return output;
}
