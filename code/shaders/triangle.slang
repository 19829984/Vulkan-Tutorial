struct VSInput
{
  float2 inPosition;
  float3 inColor;
};

struct VSOutput
{
  float4 pos : SV_Position;
  float3 color;
};

struct UniformBuffer
{
  float4x4 model;
  float4x4 view;
  float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;

[shader("vertex")]
VSOutput
vertMain(VSInput input)
{
  VSOutput output;
  float4 pos = float4(input.inPosition, 0.0, 1.0);
  pos = mul(ubo.model, pos);
  pos = mul(ubo.view, pos);
  pos = mul(ubo.proj, pos);
  output.pos = pos;
  output.color = input.inColor;
  return output;
}

[shader("fragment")]
float4
fragMain(VSOutput inVert)
  : SV_Target
{
  float3 color = inVert.color;
  return float4(color, 1.0);
}
