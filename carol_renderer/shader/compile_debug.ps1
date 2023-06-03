$CheckDxilDir=(Test-Path dxil)
if("$CheckDxilDir" -eq "False")
{
    mkdir dxil
}

$CheckPdbDir=(Test-Path pdb)
if("$CheckPdbDir" -eq "False")
{
    mkdir pdb
}

# cull_as.hlsl
dxc -E main -T as_6_6 -Fo dxil/cull_as.dxil -Zi -Qembed_debug -Fd pdb/cull_as.pdb -D FRUSTUM -D NORMAL_CONE -D HIZ_OCCLUSION -Od cull_as.hlsl
dxc -E main -T as_6_6 -Fo dxil/opaque_cull_as.dxil -Zi -Qembed_debug -Fd pdb/opaque_cull_as.pdb -D FRUSTUM -D NORMAL_CONE -D HIZ_OCCLUSION -D WRITE -Od cull_as.hlsl
dxc -E main -T as_6_6 -Fo dxil/transparent_cull_as.dxil -Zi -Qembed_debug -Fd pdb/transparent_cull_as.pdb -D FRUSTUM -D NORMAL_CONE -D HIZ_OCCLUSION -D TRANSPARENT -D WRITE -Od cull_as.hlsl

# cull_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/cull_cs.dxil -Zi -Qembed_debug -Fd pdb/cull_cs.pdb -D FRUSTUM -D HIZ_OCCLUSION -Od cull_cs.hlsl

# depth_ms.hlsl
dxc -E main -T ms_6_6 -Fo dxil/static_cull_ms.dxil -Zi -Qembed_debug -Fd pdb/static_cull_ms.pdb -D CULL -Od depth_ms.hlsl
dxc -E main -T ms_6_6 -Fo dxil/skinned_cull_ms.dxil -Zi -Qembed_debug -Fd pdb/skinned_cull_ms.pdb -D CULL -D SKINNED -Od depth_ms.hlsl

# display_ps.hlsl
dxc -E main -T ps_6_6 -Fo dxil/display_ps.dxil -Zi -Qembed_debug -Fd pdb/display_ps.pdb -Od display_ps.hlsl

# epf_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/epf_cs.dxil -Zi -Qembed_debug -Fd pdb/epf_cs.pdb -Od epf_cs.hlsl

# geometry_ps.hlsl
dxc -E main -T ps_6_6 -Fo dxil/geometry_ps.dxil -Zi -Qembed_debug -Fd pdb/geometry_ps.pdb -Od geometry_ps.hlsl

# hist_hiz_cull_as.hlsl
dxc -E main -T as_6_6 -Fo dxil/opaque_hist_hiz_cull_as.dxil -Zi -Qembed_debug -Fd pdb/opaque_hist_hiz_cull_as.pdb -D WRITE -Od hist_hiz_cull_as.hlsl
dxc -E main -T as_6_6 -Fo dxil/transparent_hist_hiz_cull_as.dxil -Zi -Qembed_debug -Fd pdb/transparent_hist_hiz_cull_as.pdb -D TRANSPARENT -D WRITE -Od hist_hiz_cull_as.hlsl

# hist_hiz_cull_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/hist_hiz_cull_cs.dxil -Zi -Qembed_debug -Fd pdb/hist_hiz_cull_cs.pdb -Od hist_hiz_cull_cs.hlsl

# hiz_generate_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/hiz_generate_cs.dxil -Zi -Qembed_debug -Fd pdb/hiz_generate_cs.pdb -Od hiz_generate_cs.hlsl

# mesh_ms.hlsl
dxc -E main -T ms_6_6 -Fo dxil/static_mesh_ms.dxil -Zi -Qembed_debug -Fd pdb/static_mesh_ms.pdb -Od mesh_ms.hlsl
dxc -E main -T ms_6_6 -Fo dxil/skinned_mesh_ms.dxil -Zi -Qembed_debug -Fd pdb/skinned_mesh_ms.pdb -D SKINNED -Od mesh_ms.hlsl

# oitppll_build_ps.hlsl
dxc -E main -T ps_6_6 -Fo dxil/oitppll_build_ps.dxil -Zi -Qembed_debug -Fd pdb/oitppll_build_ps.pdb -D SMITH -D GGX -D HEIGHT_CORRELATED -D LAMBERTIAN -Od oitppll_build_ps.hlsl

# oitppll_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/oitppll_cs.dxil -Zi -Qembed_debug -Fd pdb/oitppll_cs.pdb -Od oitppll_cs.hlsl

# root_signature.hlsl
dxc -E main -T ms_6_6 -Fo dxil/root_signature.dxil -Zi -Qembed_debug -Fd pdb/root_signature.pdb -Od root_signature.hlsl

# screen_ms.hlsl
dxc -E main -T ms_6_6 -Fo dxil/screen_ms.dxil -Zi -Qembed_debug -Fd pdb/screen_ms.pdb -Od screen_ms.hlsl

# shade_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/shade_cs.dxil -Zi -Qembed_debug -Fd pdb/shade_cs.pdb -D SMITH -D GGX -D HEIGHT_CORRELATED -D LAMBERTIAN -Od shade_cs.hlsl

# ssao_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/ssao_cs.dxil -Zi -Qembed_debug -Fd pdb/ssao_cs.pdb -Od ssao_cs.hlsl

# ssgi_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/ssgi_cs.dxil -Zi -Qembed_debug -Fd pdb/ssgi_cs.pdb -Od ssgi_cs.hlsl

# taa_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/taa_cs.dxil -Zi -Qembed_debug -Fd pdb/taa_cs.pdb -Od taa_cs.hlsl

# tone_mapping_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/tone_mapping_cs.dxil -Zi -Qembed_debug -Fd pdb/tone_mapping_cs.pdb -Od tone_mapping_cs.hlsl