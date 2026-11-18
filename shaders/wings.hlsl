// edwide_screen: the side parts of the game's picture laid over side monitors set at an angle (src/wings_math.h has
// the same arithmetic on the CPU, for the test). t0 the picture as the game left it, written back whole: the middle
// monitor's part as it was (read, not filtered: the same bits), each side's pixel from where the ray from the eye
// through it, on its monitor turned towards the eye, meets the game's flat picture.
Texture2D<float4> picture : register(t0);
SamplerState linear_clamp : register(s0);

cbuffer wings : register(b0)
  {
  float4 size;    // width, height, half the middle monitor's width, the bezel (pixels of the picture)
  float4 turn;    // the picture plane's distance from the eye (pixels), cos and sin of the sides' angle, the curve 0..1
  float4 adjust;  // zoom, vertical offset (pixels), guides 0/1, unused
  };

struct vs_out
  {
  float4 pos : SV_POSITION;
  };

// one triangle over the whole picture
vs_out vs_main(uint id : SV_VertexID)
  {
  vs_out o;
  float2 uv = float2((id << 1) & 2, id & 2);
  o.pos = float4(uv * float2(2.f, -2.f) + float2(-1.f, 1.f), 0.f, 1.f);
  return o;
  }

float4 ps_main(vs_out input) : SV_TARGET
  {
  float width = size.x, height = size.y, half_middle = size.z, bezel = size.w;
  float d = turn.x, c = turn.y, s = turn.z, curve = turn.w;
  float zoom = adjust.x, offset = adjust.y;
  float u = input.pos.x - 0.5f * width;
  float v = input.pos.y - 0.5f * height;
  float side = u < 0.f ? -1.f : 1.f;
  float along = abs(u) - half_middle;  // from the hinge, along the side monitor
  if(along < 0.f)
    return picture.Load(int3(input.pos.xy, 0));
  if(along < bezel)  // behind the bezel: not seen
    return float4(0.f, 0.f, 0.f, 1.f);
  // the pixel on its monitor turned towards the eye about the hinge (flat), or on a cylinder about the eye (curved)
  float3 flat = float3(half_middle + along * c, v, d - along * s);
  float radius = sqrt(half_middle * half_middle + d * d);
  float angle = atan2(half_middle, d) + along / radius;
  float3 round = float3(radius * sin(angle), v, radius * cos(angle));
  float3 p = lerp(flat, round, curve);
  if(p.z <= 1.f)  // turned past the eye's side: nothing to show
    return float4(0.f, 0.f, 0.f, 1.f);
  // where its ray meets the game's picture plane, then the zoom about the hinge and the offset
  float x = half_middle + (p.x * d / p.z - half_middle) / zoom;
  float y = (p.y * d / p.z) / zoom + offset;
  float2 at = float2(0.5f * width + side * x, 0.5f * height + y);
  if(any(at < 0.f) || at.x >= width || at.y >= height)
    return float4(0.f, 0.f, 0.f, 1.f);
  float4 colour = picture.SampleLevel(linear_clamp, at / float2(width, height), 0.f);
  if(adjust.z > 0.5f)
    {
    // the guides: the hinge cyan, the picture's rows every eighth of the height magenta (straight on the monitor as the
    // angle is right)
    if(along - bezel < 2.f)
      colour = float4(0.f, 1.f, 1.f, 1.f);
    float step = height / 8.f;
    float off_row = abs(frac(at.y / step + 0.5f) - 0.5f) * step;  // pixels of the picture from the nearest row
    if(off_row < 1.f / zoom)
      colour = float4(1.f, 0.f, 1.f, 1.f);
    }
  return colour;
  }
