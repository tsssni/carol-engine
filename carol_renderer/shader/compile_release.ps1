$CheckDxilDir=(Test-Path dxil)
if("$CheckDxilDir" -eq "False")
{
    mkdir dxil
}

# cull_as.hlsl
dxc -E main -T as_6_6 -Fo dxil/cull_as.dxil -D FRUSTUM -D NORMAL_CONE -D HIZ_OCCLUSION cull_as.hlsl
dxc -E main -T as_6_6 -Fo dxil/opaque_cull_as.dxil -D FRUSTUM -D NORMAL_CONE -D HIZ_OCCLUSION -D WRITE cull_as.hlsl
dxc -E main -T as_6_6 -Fo dxil/transparent_cull_as.dxil -D FRUSTUM -D NORMAL_CONE -D HIZ_OCCLUSION -D TRANSPARENT -D WRITE cull_as.hlsl

# cull_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/cull_cs.dxil -D FRUSTUM -D HIZ_OCCLUSION cull_cs.hlsl

# depth_ms.hlsl
dxc -E main -T ms_6_6 -Fo dxil/static_cull_ms.dxil -D CULL depth_ms.hlsl
dxc -E main -T ms_6_6 -Fo dxil/skinned_cull_ms.dxil -D CULL -D SKINNED depth_ms.hlsl

# display_ps.hlsl
dxc -E main -T ps_6_6 -Fo dxil/display_ps.dxil display_ps.hlsl

# epf_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/epf_cs.dxil epf_cs.hlsl

# geometry_ps.hlsl
dxc -E main -T ps_6_6 -Fo dxil/geometry_ps.dxil geometry_ps.hlsl

# hist_hiz_cull_as.hlsl
dxc -E main -T as_6_6 -Fo dxil/opaque_hist_hiz_cull_as.dxil -D WRITE hist_hiz_cull_as.hlsl
dxc -E main -T as_6_6 -Fo dxil/transparent_hist_hiz_cull_as.dxil -D TRANSPARENT -D WRITE hist_hiz_cull_as.hlsl

# hist_hiz_cull_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/hist_hiz_cull_cs.dxil hist_hiz_cull_cs.hlsl

# mesh_ms.hlsl
dxc -E main -T ms_6_6 -Fo dxil/static_mesh_ms.dxil mesh_ms.hlsl
dxc -E main -T ms_6_6 -Fo dxil/skinned_mesh_ms.dxil -D SKINNED mesh_ms.hlsl

# oitppll_build_ps.hlsl
dxc -E main -T ps_6_6 -Fo dxil/oitppll_build_ps.dxil -D SMITH -D GGX -D HEIGHT_CORRELATED -D LAMBERTIAN oitppll_build_ps.hlsl

# oitppll_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/oitppll_cs.dxil oitppll_cs.hlsl

# root_signature.hlsl
dxc -E main -T ms_6_6 -Fo dxil/root_signature.dxil root_signature.hlsl

# scene_color_generate_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/scene_color_generate_cs.dxil scene_color_generate_cs.hlsl

# screen_ms.hlsl
dxc -E main -T ms_6_6 -Fo dxil/screen_ms.dxil screen_ms.hlsl

# shade_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/shade_cs.dxil -D SMITH -D GGX -D HEIGHT_CORRELATED -D LAMBERTIAN shade_cs.hlsl

# ssao_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/ssao_cs.dxil ssao_cs.hlsl

# ssgi_generate_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/ssgi_generate_cs.dxil ssgi_generate_cs.hlsl

# ssgi_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/ssgi_cs.dxil ssgi_cs.hlsl

# taa_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/taa_cs.dxil taa_cs.hlsl

# tone_mapping_cs.hlsl
dxc -E main -T cs_6_6 -Fo dxil/tone_mapping_cs.dxil tone_mapping_cs.hlsl