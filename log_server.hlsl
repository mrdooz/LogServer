float4x4 g_mtx_proj;

struct VS_Input {
	float4 pos : POSITION;
	float4 col : COLOR;
};

struct PS_Input {
	float4 pos : SV_POSITION;
	float4 col : COLOR;
};

PS_Input vs_main(VS_Input vtx) {

	PS_Input p = (PS_Input)0;
	p.pos = mul(vtx.pos, g_mtx_proj);
	p.col = vtx.col;
	return p;
}

float4 ps_main(PS_Input p) : SV_TARGET
{
	return float4(1,1,1,1);
	return p.col;
}
