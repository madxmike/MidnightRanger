Texture2D<float4> Texture: register(t0, space2);
SamplerState Sampler: register(s0);

struct PSInput {
    float2 uv: TEXCOORD0;
    float4 color: COLOR0;
};

float4 Main(const PSInput input): SV_Target0 {
    float4 sample = Texture.Sample(Sampler, input.uv);
    return sample * input.color;
}
