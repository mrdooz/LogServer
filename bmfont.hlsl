float4x4 g_mtx_proj;
Texture2D g_font;
sampler g_sampler;

struct VS_Input {
	float4 pos : POSITION;
	float2 tex : TEXCOORD;
};

struct PS_Input {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

PS_Input vs_main(VS_Input vtx) {

	PS_Input p = (PS_Input)0;
	p.pos = mul(vtx.pos, g_mtx_proj);
	p.tex = vtx.tex;
	return p;
}

float4 ps_main(PS_Input p) : SV_TARGET
{
	return g_font.Sample(g_sampler, p.tex);
}
