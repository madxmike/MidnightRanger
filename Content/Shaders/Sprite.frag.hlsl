Texture2D<float4> Texture: register(t0, space2);
SamplerState Sampler: register(s0, space2);

struct PSInput {
    float2 UV: TEXCOORD0;
    float4 Color: COLOR0;
};

float4 Main(const PSInput input): SV_Target0 {
    float4 sample = Texture.Sample(Sampler, input.UV);
    if (sample.a <= 0.5f) {
        discard;
    }
    return sample * input.Color;
}
