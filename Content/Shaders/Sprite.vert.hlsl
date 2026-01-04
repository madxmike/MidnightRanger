static const uint triangleIndices[6] = {0, 1, 2, 3, 0, 2};
static const float3 vertexPositions[4] = {
    {-1.0f, -1.0f, 0.0f},
    {-1.0f, 1.0f, 0.0f},
    {1.0f, 1.0f, 0.0f},
    {1.0, -1.0f, 0.0f}
};

static const float2 uvCoordinates[4] = {
    {0.0f, 1.0f},
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {1.0f, 1.0f}
};

// TODO (Michael): Utilize an atlas texture for sprites. This will require the UV, width, and height of the sprite's texture on the atlas to be passed. For now, its not needed.
struct SpriteData {
    float4x4 Transform;
    float Width;
    float Height;
    float Scale_x;
    float Scale_y;
    float U;
    float V;
    float2 _padding;
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
    float3 vertexPosition = vertexPositions[vertexIndex];
    vertexPosition.x *= sprite.Scale_x;
    vertexPosition.y *= sprite.Scale_y;

    float4x4 MVP = mul(ViewProjectionMatrix, sprite.Transform);
    VSOutput output;
    output.Position = mul(MVP, float4(vertexPosition, 1.0f));
    output.UV = uvCoordinates[vertexIndex];
    output.Color = sprite.Color;

    return output;
}
