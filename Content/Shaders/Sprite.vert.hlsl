cbuffer UniformBlock : register(b0, space1)
{
    float4x4 transform : packoffset(c0);
};

struct VSInput {
    float3 position: TEXCOORD0;
    float3 color: COLOR0;
    float2 uv: TEXCOORD1;
};

struct VSOutput {
    float2 uv: TEXCOORD0;
    float4 position: SV_Position;
    float4 color: COLOR0;
};

VSOutput Main(const VSInput input) {

    VSOutput output;

    output.position = mul(transform, float4(input.position, 1.0f));
    output.color = float4(input.color, 1.0f);
    output.uv = input.uv;

    return output;
}
