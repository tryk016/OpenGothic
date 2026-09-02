cmake_minimum_required(VERSION 3.12)

project(GothicShaders)

set(HEADER "${PROJECT_BINARY_DIR}/sprv/shader.h")
set(CPP    "${PROJECT_BINARY_DIR}/sprv/shader.cpp")

file(GLOB_RECURSE SOURCES
  "${CMAKE_SOURCE_DIR}/shader/*.vert"
  "${CMAKE_SOURCE_DIR}/shader/*.tesc"
  "${CMAKE_SOURCE_DIR}/shader/*.tese"
  "${CMAKE_SOURCE_DIR}/shader/*.geom"
  "${CMAKE_SOURCE_DIR}/shader/*.frag"
  "${CMAKE_SOURCE_DIR}/shader/*.glsl"
  "${CMAKE_SOURCE_DIR}/shader/*.task"
  "${CMAKE_SOURCE_DIR}/shader/*.mesh"
  "${CMAKE_SOURCE_DIR}/shader/*.comp")

file(GLOB_RECURSE GLSL_SOURCES
  "${CMAKE_SOURCE_DIR}/shader/*.glsl")

# GLSL to SPIRV compiler
find_program(GLSLANGVALIDATOR glslangValidator "/opt/homebrew/bin")
if(NOT GLSLANGVALIDATOR)
  message(FATAL_ERROR "glslangValidator required")
endif()

function(add_shader OUTPUT SOURCE)
  set(OUTPUT_FILE "${PROJECT_BINARY_DIR}/sprv/${OUTPUT}.sprv")
  set(SOURCE_FILE "${CMAKE_SOURCE_DIR}/shader/${SOURCE}")
  set(OPENGOTHIC_SHADERS      ${OPENGOTHIC_SHADERS}      ${SOURCE_FILE} PARENT_SCOPE)
  set(OPENGOTHIC_SHADERS_SPRV ${OPENGOTHIC_SHADERS_SPRV} ${OUTPUT_FILE} PARENT_SCOPE)

  list(LENGTH ARGV ARGC)
  if(ARGC GREATER 2)
    list(SUBLIST ARGV 2 -1 VARS)
  endif()

  if("${OUTPUT}" MATCHES "\.(mesh)")
    list(APPEND VARS "--target-env" "spirv1.4")
  endif()

  if("${OUTPUT}" MATCHES "\.(task)")
    list(APPEND VARS "--target-env" "spirv1.4")
  endif()

  if("${OUTPUT}" MATCHES "\.(16).")
    list(APPEND VARS "-DWORKGROUP_SIZE=16")
  endif()

  if("${OUTPUT}" MATCHES "\.(32).")
    list(APPEND VARS "-DWORKGROUP_SIZE=32")
  endif()

  if("${OUTPUT}" MATCHES "\.(64).")
    list(APPEND VARS "-DWORKGROUP_SIZE=64")
  endif()

  # debug info in shaders
  if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
    list(APPEND VARS "-g")
  else()
    list(APPEND VARS "-g")
  endif()

  set(GLSL_CMD ${GLSLANGVALIDATOR})
  list(APPEND GLSL_CMD "-V")
  list(APPEND GLSL_CMD ${VARS})
  list(APPEND GLSL_CMD "--P")
  list(APPEND GLSL_CMD "#extension GL_GOOGLE_include_directive:enable")
  list(APPEND GLSL_CMD "-I${CMAKE_SOURCE_DIR}/shader")
  list(APPEND GLSL_CMD ${SOURCE_FILE})
  list(APPEND GLSL_CMD "-o")
  list(APPEND GLSL_CMD ${OUTPUT_FILE})

  add_custom_command(
    OUTPUT     ${OUTPUT_FILE}
    DEPENDS    ${SOURCE_FILE} ${GLSL_SOURCES}
    COMMAND    ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/sprv/"
    COMMAND    ${GLSL_CMD}
    VERBATIM
    )
endfunction(add_shader)

# Ubershader flags:
#   MESH_TYPE  - type of mesh enum{landscape, object, skinned, morph, pfx}
#   SHADOW_MAP - output is shadowmap
#   FORWARD    - compute forward lighting
#   GBUFFER    - emit gbuffer
#   ATEST      - use alpha test
#   WATER      - water material
#   GHOST      - ghost material

function(add_geom_shader OUTPUT)
  list(SUBLIST ARGV 1 -1 VARS)

  add_shader("${OUTPUT}_slot.vert"      materials/main.vert  -S vert ${VARS})
  add_shader("${OUTPUT}_slot.mesh"      materials/main.vert  -S mesh ${VARS})
  add_shader("${OUTPUT}_bindless.vert"  materials/main.vert  -S vert ${VARS} -DBINDLESS)
  add_shader("${OUTPUT}_bindless.mesh"  materials/main.vert  -S mesh ${VARS} -DBINDLESS)

  set(OPENGOTHIC_SHADERS      ${OPENGOTHIC_SHADERS}      PARENT_SCOPE)
  set(OPENGOTHIC_SHADERS_SPRV ${OPENGOTHIC_SHADERS_SPRV} PARENT_SCOPE)
endfunction(add_geom_shader)

function(add_frag_shader OUTPUT)
  list(SUBLIST ARGV 1 -1 VARS)

  add_shader("${OUTPUT}_slot.frag"         materials/main.frag  ${VARS})
  add_shader("${OUTPUT}_bindless.frag"     materials/main.frag  ${VARS} -DBINDLESS)

  set(OPENGOTHIC_SHADERS      ${OPENGOTHIC_SHADERS}      PARENT_SCOPE)
  set(OPENGOTHIC_SHADERS_SPRV ${OPENGOTHIC_SHADERS_SPRV} PARENT_SCOPE)
endfunction(add_frag_shader)

# editor hud
add_shader(gizmo.vert hud/gizmo.vert)
add_shader(gizmo.frag hud/gizmo.frag)

add_shader(light_imposter.vert hud/light_imposter.vert)
add_shader(light_imposter.frag hud/light_imposter.frag)

# cluster culling
add_shader(cluster_init.comp            materials/cluster_init.comp)
add_shader(cluster_patch.comp           materials/cluster_patch.comp)
add_shader(visibility_pass.comp         materials/visibility_pass.comp)
add_shader(visibility_pass_hiz.comp     materials/visibility_pass.comp -DMAIN_VIEW -DHIZ)
add_shader(visibility_pass_hiz_cr.comp  materials/visibility_pass.comp -DMAIN_VIEW -DHIZ -DLARGE)

# mesh & vertex
add_geom_shader(main_lnd       "-DMESH_TYPE=0" -DFLAT_NORMAL)
add_geom_shader(main_lnd_f     "-DMESH_TYPE=0" -DFLAT_NORMAL -DFORWARD)
add_geom_shader(main_lnd_d     "-DMESH_TYPE=0" -DFLAT_NORMAL -DDEPTH_ONLY)
add_geom_shader(main_lnd_d_at  "-DMESH_TYPE=0" -DFLAT_NORMAL -DDEPTH_ONLY -DATEST)
add_geom_shader(main_lnd_v     "-DMESH_TYPE=0" -DFLAT_NORMAL -DDEPTH_ONLY -DVIRTUAL_SHADOW)
add_geom_shader(main_lnd_v_at  "-DMESH_TYPE=0" -DFLAT_NORMAL -DDEPTH_ONLY -DVIRTUAL_SHADOW -DATEST)

add_geom_shader(main_obj       "-DMESH_TYPE=1")
add_geom_shader(main_obj_f     "-DMESH_TYPE=1" -DFORWARD)
add_geom_shader(main_obj_d     "-DMESH_TYPE=1" -DDEPTH_ONLY)
add_geom_shader(main_obj_d_at  "-DMESH_TYPE=1" -DDEPTH_ONLY -DATEST)
add_geom_shader(main_obj_v     "-DMESH_TYPE=1" -DDEPTH_ONLY -DVIRTUAL_SHADOW)
add_geom_shader(main_obj_v_at  "-DMESH_TYPE=1" -DDEPTH_ONLY -DVIRTUAL_SHADOW -DATEST)

add_geom_shader(main_ani       "-DMESH_TYPE=2")
add_geom_shader(main_ani_f     "-DMESH_TYPE=2" -DFORWARD)
add_geom_shader(main_ani_d     "-DMESH_TYPE=2" -DDEPTH_ONLY)
add_geom_shader(main_ani_d_at  "-DMESH_TYPE=2" -DDEPTH_ONLY -DATEST)
add_geom_shader(main_ani_v     "-DMESH_TYPE=2" -DDEPTH_ONLY -DVIRTUAL_SHADOW)
add_geom_shader(main_ani_v_at  "-DMESH_TYPE=2" -DDEPTH_ONLY -DVIRTUAL_SHADOW -DATEST)

add_geom_shader(main_mph       "-DMESH_TYPE=3")
add_geom_shader(main_mph_f     "-DMESH_TYPE=3" -DFORWARD)
add_geom_shader(main_mph_d     "-DMESH_TYPE=3" -DDEPTH_ONLY)
add_geom_shader(main_mph_d_at  "-DMESH_TYPE=3" -DDEPTH_ONLY -DATEST)
add_geom_shader(main_mph_v     "-DMESH_TYPE=3" -DDEPTH_ONLY -DVIRTUAL_SHADOW)
add_geom_shader(main_mph_v_at  "-DMESH_TYPE=3" -DDEPTH_ONLY -DVIRTUAL_SHADOW -DATEST)

# tesselation
add_shader(main_lnd_f_slot.tesc      materials/main.tesc  "-DMESH_TYPE=0" -DWATER)
add_shader(main_lnd_f_slot.tese      materials/main.tese  "-DMESH_TYPE=0" -DWATER)
add_shader(main_lnd_f_bindless.tesc  materials/main.tesc  "-DMESH_TYPE=0" -DBINDLESS -DWATER)
add_shader(main_lnd_f_bindless.tese  materials/main.tese  "-DMESH_TYPE=0" -DBINDLESS -DWATER)

# materials landscape
add_frag_shader(main_lnd_g_s   -DFLAT_NORMAL -DGBUFFER -DSIMPLE_MAT)
add_frag_shader(main_lnd_g     -DFLAT_NORMAL -DGBUFFER)
add_frag_shader(main_lnd_g_at  -DFLAT_NORMAL -DGBUFFER -DATEST)
add_frag_shader(main_lnd_g_tx  -DFLAT_NORMAL -DGBUFFER)
add_frag_shader(main_lnd_f     -DFLAT_NORMAL -DFORWARD)
add_frag_shader(main_lnd_e     -DFLAT_NORMAL -DEMISSIVE)
add_frag_shader(main_lnd_x     -DFLAT_NORMAL -DGHOST)
add_frag_shader(main_lnd_w     -DFLAT_NORMAL -DWATER)
add_frag_shader(main_lnd_d     -DFLAT_NORMAL -DDEPTH_ONLY)
add_frag_shader(main_lnd_d_at  -DFLAT_NORMAL -DDEPTH_ONLY -DATEST)
add_frag_shader(main_lnd_v     -DFLAT_NORMAL -DDEPTH_ONLY -DVIRTUAL_SHADOW)
add_frag_shader(main_lnd_v_at  -DFLAT_NORMAL -DDEPTH_ONLY -DVIRTUAL_SHADOW -DATEST)

# materials objects
add_frag_shader(main_obj_g     -DGBUFFER)
add_frag_shader(main_obj_g_at  -DGBUFFER -DATEST)
add_frag_shader(main_obj_f     -DFORWARD)
add_frag_shader(main_obj_e     -DEMISSIVE)
add_frag_shader(main_obj_w     -DWATER)
add_frag_shader(main_obj_x     -DGHOST)
add_frag_shader(main_obj_d     -DDEPTH_ONLY)
add_frag_shader(main_obj_d_at  -DDEPTH_ONLY -DATEST)
add_frag_shader(main_obj_v     -DDEPTH_ONLY -DVIRTUAL_SHADOW)
add_frag_shader(main_obj_v_at  -DDEPTH_ONLY -DVIRTUAL_SHADOW -DATEST)

# pfx geometry
add_shader(main_pfx.vert       materials/pfx.vert   "-DMESH_TYPE=4" -DVT_COLOR)
add_shader(main_pfx_f.vert     materials/pfx.vert   "-DMESH_TYPE=4" -DFORWARD    -DVT_COLOR)
add_shader(main_pfx_d.vert     materials/pfx.vert   "-DMESH_TYPE=4" -DDEPTH_ONLY -DVT_COLOR)
add_shader(main_pfx_d_at.vert  materials/pfx.vert   "-DMESH_TYPE=4" -DDEPTH_ONLY -DVT_COLOR -DATEST)
add_shader(main_pfx_v.vert     materials/pfx.vert   "-DMESH_TYPE=4" -DDEPTH_ONLY -DVIRTUAL_SHADOW -DVT_COLOR)
add_shader(main_pfx_v_at.vert  materials/pfx.vert   "-DMESH_TYPE=4" -DDEPTH_ONLY -DVIRTUAL_SHADOW -DVT_COLOR -DATEST)
# materials pfx
add_shader(main_pfx_g.frag     materials/main.frag  "-DMESH_TYPE=4" -DGBUFFER    -DVT_COLOR)
add_shader(main_pfx_g_at.frag  materials/main.frag  "-DMESH_TYPE=4" -DGBUFFER    -DVT_COLOR -DATEST)
add_shader(main_pfx_f.frag     materials/main.frag  "-DMESH_TYPE=4" -DFORWARD    -DVT_COLOR)
add_shader(main_pfx_f_at.frag  materials/main.frag  "-DMESH_TYPE=4" -DFORWARD    -DVT_COLOR -DATEST)
add_shader(main_pfx_e.frag     materials/main.frag  "-DMESH_TYPE=4" -DEMISSIVE   -DVT_COLOR)
add_shader(main_pfx_e_at.frag  materials/main.frag  "-DMESH_TYPE=4" -DEMISSIVE   -DVT_COLOR -DATEST)
add_shader(main_pfx_x.frag     materials/main.frag  "-DMESH_TYPE=4" -DGHOST      -DVT_COLOR)
add_shader(main_pfx_x_at.frag  materials/main.frag  "-DMESH_TYPE=4" -DGHOST      -DVT_COLOR -DATEST)
add_shader(main_pfx_d.frag     materials/main.frag  "-DMESH_TYPE=4" -DDEPTH_ONLY -DVT_COLOR)
add_shader(main_pfx_d_at.frag  materials/main.frag  "-DMESH_TYPE=4" -DDEPTH_ONLY -DVT_COLOR -DATEST)
add_shader(main_pfx_v.frag     materials/main.frag  "-DMESH_TYPE=4" -DDEPTH_ONLY -DVIRTUAL_SHADOW -DVT_COLOR)
add_shader(main_pfx_v_at.frag  materials/main.frag  "-DMESH_TYPE=4" -DDEPTH_ONLY -DVIRTUAL_SHADOW -DVT_COLOR -DATEST)


add_shader(item.vert                 inventory/item.vert)
add_shader(item.frag                 inventory/item.frag)

add_shader(stash.frag                stash.frag)
add_shader(bink.frag                 bink/bink.frag)
add_shader(downscale.frag            downscale.frag)

add_shader(direct_light.frag         lighting/direct_light.frag)
add_shader(direct_light_sh.frag      lighting/direct_light.frag -DSHADOW_MAP)
add_shader(direct_light_rq.frag      lighting/direct_light.frag -DSHADOW_MAP -DRAY_QUERY -DRAY_QUERY_AT)
add_shader(direct_light_vsm.frag     virtual_shadow/vsm_direct_light.frag)

add_shader(ambient_light.frag        lighting/ambient_light.frag)
add_shader(ambient_light_ssao.frag   lighting/ambient_light.frag -DSSAO)
add_shader(ambient_light_surf.frag   lighting/ambient_light.frag -DSSAO -DSURFEL_GI)

add_shader(light.vert                lighting/light.vert)
add_shader(light.frag                lighting/light.frag)
add_shader(light_rq.frag             lighting/light.frag -DRAY_QUERY)
add_shader(light_rq_at.frag          lighting/light.frag -DRAY_QUERY -DRAY_QUERY_AT)
add_shader(light_vsm.frag            lighting/light.frag -DVIRTUAL_SHADOW)

add_shader(irradiance.comp           lighting/irradiance.comp)
add_shader(sky_exposure.comp         lighting/sky_exposure.comp)

add_shader(tonemapping.frag          lighting/tonemapping.frag)
add_shader(tonemapping_up.frag       lighting/tonemapping.frag -DUPSCALE)

add_shader(water_reflection.mesh     water/water_reflection.mesh)
add_shader(water_reflection.frag     water/water_reflection.frag)
add_shader(water_reflection_ssr.frag water/water_reflection.frag -DSSR)

add_shader(underwater_t.frag         water/underwater.frag "")
add_shader(underwater_s.frag         water/underwater.frag -DSCATTERING)

# Based on Scalable and Production Ready Sky and Atmosphere
# https://sebh.github.io/publications/egsr2020.pdf
# https://www.shadertoy.com/view/slSXRW
add_shader(sky_transmittance.frag    sky/sky_transmittance.frag)
add_shader(sky_multi_scattering.frag sky/sky_multi_scattering.frag)
add_shader(sky_view_lut.frag         sky/sky_view_lut.frag)
add_shader(sky_view_clouds_lut.frag  sky/sky_view_clouds_lut.frag)

add_shader(clouds_lut.comp           sky/clouds_lut.comp)
add_shader(fog_view_lut.comp         sky/fog_view_lut.comp -DSUN_SCATTERING -DMULTI_SCATTERING)
add_shader(fog_view_lut_sep.comp     sky/fog_view_lut.comp -DSUN_SCATTERING -DMULTI_SCATTERING -DSEPARABLE)

add_shader(sky.frag                  sky/sky.frag)
add_shader(sky_sep.frag              sky/sky.frag -DSEPARABLE)
add_shader(fog3d.comp                sky/fog.frag -S comp -DVOLUMETRIC)
add_shader(fog3d_hq.frag             sky/fog.frag -DVOLUMETRIC)
add_shader(fog_epipolar.frag         sky/fog_epipolar.frag)
add_shader(fog.frag                  sky/fog.frag)
#
add_shader(fog_epipolar_vsm.comp       epipolar/fog_epipolar.comp)
add_shader(fog_epipolar_occlusion.comp epipolar/fog_epipolar_occlusion.comp)

add_shader(sky_pathtrace.frag        sky/sky_pathtrace.frag)

add_shader(sun.vert                  sky/sun.vert)
add_shader(sun.frag                  sky/sun.frag)

# copy/edit
add_shader(triangle.vert             triangle.vert)
add_shader(triangle_uv.vert          triangle.vert -DHAS_UV)
add_shader(copy.frag                 copy.frag)
add_shader(copy.comp                 copy.comp)
add_shader(copy_img.comp             copy_img.comp)
add_shader(patch.comp                patch.comp)

add_shader(ssao.comp                 ssao/ssao.comp)
add_shader(ssao_blur.comp            ssao/ssao_blur.comp)

# GPU-driven: HiZ
if(IOS)
  add_shader(hiz_mip.comp            hiz/hiz_mip.comp -DIOS)
else()
  add_shader(hiz_mip.comp            hiz/hiz_mip.comp)
endif()
add_shader(hiz_pot.comp              hiz/hiz_pot.comp)

# RayQuery
add_shader(rt_dbg.frag               lighting/rt_dbg/rt_dbg.frag -DRAY_QUERY -DRAY_QUERY_AT)
add_shader(hash_dbg.frag             lighting/rt_dbg/hash_dbg.frag)

# Global illumination
add_shader(probe_dbg.vert            lighting/rt/probe_dbg.vert)
add_shader(probe_dbg.frag            lighting/rt/probe_dbg.frag)

add_shader(probe_hit_dbg.vert        lighting/rt/probe_hit_dbg.vert)
add_shader(probe_hit_dbg.frag        lighting/rt/probe_hit_dbg.frag)

add_shader(probe_init.comp           lighting/rt/probe_aux.comp        -DINIT)
add_shader(probe_clear.comp          lighting/rt/probe_aux.comp        -DCLEAR)
add_shader(probe_clear_hash.comp     lighting/rt/probe_aux.comp        -DCLEAR_HASH)
add_shader(probe_make_hash.comp      lighting/rt/probe_aux.comp        -DMAKE_HASH)
add_shader(probe_prune.comp          lighting/rt/probe_aux.comp        -DPRUNE)
add_shader(probe_vote.comp           lighting/rt/probe_allocation.comp -DVOTE)
add_shader(probe_allocation.comp     lighting/rt/probe_allocation.comp -DALLOC)
add_shader(probe_trace.comp          lighting/rt/probe_trace.comp      -DRAY_QUERY -DRAY_QUERY_AT)
add_shader(probe_lighting.comp       lighting/rt/probe_lighting.comp)

add_shader(probe_ambient.vert        triangle.vert)
add_shader(probe_ambient.frag        lighting/rt/probe_ambient.frag)

# Surfels
add_shader(surf_dbg.vert             lighting/surfels/surf_dbg.vert)
add_shader(surf_dbg.frag             lighting/surfels/surf_dbg.frag)

add_shader(surf_update.comp          lighting/surfels/surf_update.comp)
add_shader(surf_culling.comp         lighting/surfels/surf_culling.comp)
add_shader(surf_decimate.comp        lighting/surfels/surf_decimate.comp)
add_shader(surf_compact.comp         lighting/surfels/surf_compact.comp)

add_shader(surf_alloc.comp           lighting/surfels/surf_alloc.comp)
add_shader(surf_apply.comp           lighting/surfels/surf_apply.comp)
add_shader(surf_freelist.comp        lighting/surfels/surf_freelist.comp)

add_shader(surf_bin_clear.comp       lighting/surfels/surf_bin_clear.comp)
add_shader(surf_bin_alloc.comp       lighting/surfels/surf_bin_alloc.comp)
add_shader(surf_bin_pass.comp        lighting/surfels/surf_bin_pass.comp)
add_shader(surf_bin_sort.comp        lighting/surfels/surf_bin_sort.comp)

add_shader(surf_pathtrace.comp       lighting/surfels/surf_pathtrace.comp)
add_shader(surf_raycast.comp         lighting/surfels/surf_raycast.comp)
add_shader(surf_lighting.comp        lighting/surfels/surf_lighting.comp)

# Pathtracing
add_shader(pathtrace.frag        lighting/pt/pathtrace.frag -DRAY_QUERY -DRAY_QUERY_AT)

add_shader(lights_morton.comp    lighting/lightstree/lights_morton.comp)
add_shader(lights_topology.comp  lighting/lightstree/lights_topology.comp)
add_shader(lights_tree.comp      lighting/lightstree/lights_tree.comp)
add_shader(lights_bvh.comp       lighting/lightstree/lights_bvh.comp)

add_shader(lightstree_dbg.frag   lighting/lightstree/lightstree_dbg.frag)

# CMAA2
add_shader(cmaa2_edges_color2x2_quality_0.comp     antialiasing/cmaa2/edge_color2x2.comp -DCMAA2_STATIC_QUALITY_PRESET=0)
add_shader(cmaa2_edges_color2x2_quality_1.comp     antialiasing/cmaa2/edge_color2x2.comp -DCMAA2_STATIC_QUALITY_PRESET=1)

add_shader(cmaa2_process_candidates.comp           antialiasing/cmaa2/process_candidates.comp)
add_shader(cmaa2_deferred_color_apply_2x2.vert     antialiasing/cmaa2/deferred_color_apply_2x2.vert)
add_shader(cmaa2_deferred_color_apply_2x2.frag     antialiasing/cmaa2/deferred_color_apply_2x2.frag)

# virtual shadows
add_shader(vsm_dbg.frag              virtual_shadow/vsm_direct_light.frag -DDEBUG)
add_shader(vsm_mark_pages.comp       virtual_shadow/vsm_mark_pages.comp)
add_shader(vsm_clear.comp            virtual_shadow/vsm_clear.comp)
add_shader(vsm_clear_omni.comp       virtual_shadow/vsm_clear_omni.comp)
add_shader(vsm_trim_pages.comp       virtual_shadow/vsm_trim_pages.comp)
add_shader(vsm_clump_pages.comp      virtual_shadow/vsm_clump_pages.comp)
add_shader(vsm_list_pages.comp       virtual_shadow/vsm_list_pages.comp)
add_shader(vsm_sort_pages.comp       virtual_shadow/vsm_sort_pages.comp)
add_shader(vsm_alloc_pages.comp      virtual_shadow/vsm_alloc_pages.comp)
add_shader(vsm_alloc_pages2.comp     virtual_shadow/vsm_alloc_pages2.comp)
add_shader(vsm_merge_pages.comp      virtual_shadow/vsm_merge_pages.comp)
add_shader(vsm_pack_draws0.comp      virtual_shadow/vsm_pack_draws.comp   -DPASS0)
add_shader(vsm_pack_draws1.comp      virtual_shadow/vsm_pack_draws.comp   -DPASS1)
add_shader(vsm_visibility_pass.comp  virtual_shadow/vsm_visibility_pass.comp -DVIRTUAL_SHADOW)
# virtual shadows: omni lights
add_shader(vsm_cull_lights.comp      virtual_shadow/vsm_cull_lights.comp)
add_shader(vsm_mark_omni_pages.comp  virtual_shadow/vsm_mark_omni_pages.comp)
add_shader(vsm_postprocess_omni.comp virtual_shadow/vsm_postprocess_omni.comp)
# virtual shadows: fog
add_shader(vsm_fog_shadow.comp       virtual_shadow/vsm_fog_shadow.comp)
add_shader(vsm_fog_mark_pages.comp   virtual_shadow/vsm_fog_shadow.comp -DPAGES)
add_shader(vsm_fog_trace.comp        virtual_shadow/vsm_fog_trace.comp)

add_shader(vsm_rendering.comp        virtual_shadow/vsm_rendering.comp)

# software meshlet rt
add_shader(rtsm_clear.comp           rtsm/rtsm_clear.comp)
add_shader(rtsm_mark_pages.comp      rtsm/rtsm_mark_pages.comp)
add_shader(rtsm_mark_fog_pages.comp  rtsm/rtsm_mark_fog_pages.comp)
add_shader(rtsm_hiz_pages.comp       rtsm/rtsm_hiz_pages.comp)
add_shader(rtsm_culling.comp         rtsm/rtsm_culling.comp)
add_shader(rtsm_position.comp        rtsm/rtsm_position.comp)

add_shader(rtsm_meshlet_cull.comp    rtsm/rtsm_meshlet_cull.comp)
add_shader(rtsm_primitive_cull.comp  rtsm/rtsm_primitive_cull.comp)

add_shader(rtsm_raster.comp          rtsm/rtsm_raster.comp)
add_shader(rtsm_direct_light.frag    rtsm/rtsm_direct_light.frag)

add_shader(rtsm_rendering.comp       rtsm/rtsm_rendering.comp)
add_shader(rtsm_omni_rendering.comp  rtsm/rtsm_omni_rendering.comp)
add_shader(rtsm_dbg.frag             rtsm/rtsm_dbg.frag)

add_shader(rtsm_omni_clear.comp      rtsm/rtsm_omni_clear.comp)
add_shader(rtsm_cull_lights.comp     rtsm/rtsm_cull_lights.comp)
add_shader(rtsm_compact_lights.comp  rtsm/rtsm_compact_lights.comp)
add_shader(rtsm_omni_lights.comp     rtsm/rtsm_omni_lights.comp)
add_shader(rtsm_omni_bboxes.comp     rtsm/rtsm_omni_bboxes.comp)
add_shader(rtsm_omni_culling.comp    rtsm/rtsm_omni_culling.comp)
add_shader(rtsm_omni_position.comp   rtsm/rtsm_position.comp -DOMNI)
add_shader(rtsm_omni_meshlet.comp    rtsm/rtsm_omni_meshlet.comp)
add_shader(rtsm_omni_backface.comp   rtsm/rtsm_omni_backface.comp)
add_shader(rtsm_omni_compact.comp    rtsm/rtsm_omni_compact.comp)
add_shader(rtsm_omni_task.comp       rtsm/rtsm_omni_task.comp)
add_shader(rtsm_omni_primitive.comp  rtsm/rtsm_omni_primitive.comp)
add_shader(rtsm_omni_raster.comp     rtsm/rtsm_omni_raster.comp)

# software ray-tracing
#add_shader(sw_raytracing.comp swrt/sw_raytracing.comp -DANY_HIT -DUNIFORM_TRAVERSE "--target-env" "spirv1.3")
add_shader(sw_raytracing.comp   swrt/sw_raytracing.comp  -DANY_HIT)
add_shader(sw_raytracing8.comp  swrt/sw_raytracing8.comp)
add_shader(sw_raytracing64.comp swrt/sw_raytracing64.comp)

# software rendering
add_shader(sw_rendering_imm.comp  software_rendering/sw_rendering_imm.comp)
add_shader(sw_rendering_tbr.comp  software_rendering/sw_rendering_tbr.comp)
add_shader(sw_light.comp          software_rendering/sw_light.comp)
add_shader(vbuffer_blit.frag      software_rendering/vbuffer_blit.frag)

add_custom_command(
  OUTPUT     ${HEADER} ${CPP}
  DEPENDS    ${OPENGOTHIC_SHADERS_SPRV}
  COMMAND    ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/sprv/"
  COMMAND    ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/shader/link_shaders.cmake"
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )

add_custom_target(shader
  DEPENDS ${HEADER} ${CPP}
  SOURCES ${SOURCES} "${CMAKE_SOURCE_DIR}/shader/link_shaders.cmake")

add_library(${PROJECT_NAME} STATIC ${HEADER} ${CPP})
add_dependencies(${PROJECT_NAME} shader)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/sprv")
set_target_properties(
    ${PROJECT_NAME} PROPERTIES
    PUBLIC_HEADER ${HEADER}
    )

# The iOS start screen only needs two capability-independent shaders. Generate
# their canonical MSL with the same vendored SPIRV-Cross and options as Tempest,
# then compile one metallib per shader and supported MSL version. Keeping each
# library separate guarantees that every library has exactly one `main0` entry
# point and lets Tempest validate the source/profile key independently.
set(OPENGOTHIC_IOS_STARTUP_RESOURCES "" PARENT_SCOPE)
set(OPENGOTHIC_IOS_STARTUP_HEADER "" PARENT_SCOPE)
set(OPENGOTHIC_IOS_STARTUP_TARGET "" PARENT_SCOPE)
if(IOS AND OPENGOTHIC_IOS_PRECOMPILED_STARTUP_SHADERS)
  find_program(OPENGOTHIC_XCRUN_EXECUTABLE NAMES xcrun)

  set(IOS_STARTUP_HOST_CXX "")
  set(IOS_STARTUP_METAL_AVAILABLE FALSE)
  if(OPENGOTHIC_XCRUN_EXECUTABLE)
    execute_process(
      COMMAND "${OPENGOTHIC_XCRUN_EXECUTABLE}" -sdk macosx --find clang++
      RESULT_VARIABLE IOS_STARTUP_HOST_CXX_RESULT
      OUTPUT_VARIABLE IOS_STARTUP_HOST_CXX
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)

    string(TOLOWER "${CMAKE_OSX_SYSROOT}" IOS_STARTUP_SYSROOT_LOWER)
    if(IOS_STARTUP_SYSROOT_LOWER MATCHES "iphonesimulator")
      set(IOS_STARTUP_METAL_SDK "iphonesimulator")
      set(IOS_STARTUP_METAL_TARGET
        "air64-apple-ios${CMAKE_OSX_DEPLOYMENT_TARGET}-simulator")
    else()
      set(IOS_STARTUP_METAL_SDK "iphoneos")
      set(IOS_STARTUP_METAL_TARGET
        "air64-apple-ios${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()

    execute_process(
      COMMAND "${OPENGOTHIC_XCRUN_EXECUTABLE}" -sdk "${IOS_STARTUP_METAL_SDK}" --find metal
      RESULT_VARIABLE IOS_STARTUP_METAL_RESULT
      OUTPUT_QUIET ERROR_QUIET)
    execute_process(
      COMMAND "${OPENGOTHIC_XCRUN_EXECUTABLE}" -sdk "${IOS_STARTUP_METAL_SDK}" --find metallib
      RESULT_VARIABLE IOS_STARTUP_METALLIB_RESULT
      OUTPUT_QUIET ERROR_QUIET)
    if(IOS_STARTUP_METAL_RESULT EQUAL 0 AND IOS_STARTUP_METALLIB_RESULT EQUAL 0)
      set(IOS_STARTUP_METAL_AVAILABLE TRUE)
    endif()
  endif()

  if(OPENGOTHIC_XCRUN_EXECUTABLE AND
     IOS_STARTUP_HOST_CXX_RESULT EQUAL 0 AND
     NOT IOS_STARTUP_HOST_CXX STREQUAL "" AND
     IOS_STARTUP_METAL_AVAILABLE AND
     NOT CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
    set(IOS_STARTUP_DIR "${PROJECT_BINARY_DIR}/ios-startup")
    set(IOS_STARTUP_TRIANGLE "${PROJECT_BINARY_DIR}/sprv/triangle.vert.sprv")
    set(IOS_STARTUP_DOWNSCALE "${PROJECT_BINARY_DIR}/sprv/downscale.frag.sprv")
    set(IOS_STARTUP_SPIRV_STAMP
      "${IOS_STARTUP_DIR}/startup-spirv.stamp")

    add_custom_command(
      OUTPUT "${IOS_STARTUP_SPIRV_STAMP}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${IOS_STARTUP_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E touch "${IOS_STARTUP_SPIRV_STAMP}"
      DEPENDS "${IOS_STARTUP_TRIANGLE}" "${IOS_STARTUP_DOWNSCALE}"
      COMMENT "Preparing the two iOS startup SPIR-V shaders"
      VERBATIM)
    add_custom_target(ios-startup-spirv DEPENDS "${IOS_STARTUP_SPIRV_STAMP}")
    add_dependencies(shader ios-startup-spirv)

    set(IOS_STARTUP_TOOL_SOURCE_DIR
      "${CMAKE_SOURCE_DIR}/shader/ios_startup_tool")
    set(IOS_STARTUP_TOOL_BINARY_DIR
      "${PROJECT_BINARY_DIR}/ios-startup-tool-build")
    set(IOS_STARTUP_TOOL_EXECUTABLE
      "${IOS_STARTUP_TOOL_BINARY_DIR}/bin/opengothic-ios-startup-msl${CMAKE_EXECUTABLE_SUFFIX}")
    set(IOS_STARTUP_TOOL_STAMP
      "${IOS_STARTUP_TOOL_BINARY_DIR}/opengothic-ios-startup-msl.stamp")
    set(IOS_STARTUP_SPIRV_CROSS_DIR
      "${CMAKE_SOURCE_DIR}/lib/Tempest/Engine/thirdparty/spirv_cross")

    set(IOS_STARTUP_TOOL_SOURCES
      "${IOS_STARTUP_TOOL_SOURCE_DIR}/CMakeLists.txt"
      "${IOS_STARTUP_TOOL_SOURCE_DIR}/main.cpp"
      "${IOS_STARTUP_SPIRV_CROSS_DIR}/spirv_cross.cpp"
      "${IOS_STARTUP_SPIRV_CROSS_DIR}/spirv_parser.cpp"
      "${IOS_STARTUP_SPIRV_CROSS_DIR}/spirv_cross_parsed_ir.cpp"
      "${IOS_STARTUP_SPIRV_CROSS_DIR}/spirv_cfg.cpp"
      "${IOS_STARTUP_SPIRV_CROSS_DIR}/spirv_glsl.cpp"
      "${IOS_STARTUP_SPIRV_CROSS_DIR}/spirv_msl.cpp")
    file(GLOB IOS_STARTUP_SPIRV_CROSS_HEADERS CONFIGURE_DEPENDS
      "${IOS_STARTUP_SPIRV_CROSS_DIR}/*.h"
      "${IOS_STARTUP_SPIRV_CROSS_DIR}/*.hpp")

    add_custom_command(
      OUTPUT "${IOS_STARTUP_TOOL_STAMP}"
      BYPRODUCTS "${IOS_STARTUP_TOOL_EXECUTABLE}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${IOS_STARTUP_TOOL_BINARY_DIR}"
      COMMAND "${CMAKE_COMMAND}"
        -S "${IOS_STARTUP_TOOL_SOURCE_DIR}"
        -B "${IOS_STARTUP_TOOL_BINARY_DIR}"
        -G "${CMAKE_GENERATOR}"
        "-DSPIRV_CROSS_SOURCE_DIR=${IOS_STARTUP_SPIRV_CROSS_DIR}"
        "-DCMAKE_CXX_COMPILER=${IOS_STARTUP_HOST_CXX}"
        "-DCMAKE_OSX_SYSROOT=macosx"
        "-DCMAKE_BUILD_TYPE=Release"
      COMMAND "${CMAKE_COMMAND}" --build "${IOS_STARTUP_TOOL_BINARY_DIR}"
        --config Release --target opengothic-ios-startup-msl
      COMMAND "${CMAKE_COMMAND}" -E touch "${IOS_STARTUP_TOOL_STAMP}"
      DEPENDS ${IOS_STARTUP_TOOL_SOURCES} ${IOS_STARTUP_SPIRV_CROSS_HEADERS}
      COMMENT "Building the native iOS startup MSL generator"
      VERBATIM)

    set(IOS_STARTUP_RESOURCES)
    set(IOS_STARTUP_BYPRODUCTS)
    set(IOS_STARTUP_HASH_HEADER
      "${IOS_STARTUP_DIR}/opengothic_ios_startup_hashes.h")
    foreach(IOS_STARTUP_VERSION 20400 30000 30100)
      foreach(IOS_STARTUP_SHADER Triangle Downscale)
        list(APPEND IOS_STARTUP_RESOURCES
          "${IOS_STARTUP_DIR}/OpenGothic${IOS_STARTUP_SHADER}_${IOS_STARTUP_VERSION}.metallib"
          "${IOS_STARTUP_DIR}/OpenGothic${IOS_STARTUP_SHADER}_${IOS_STARTUP_VERSION}.mslsrc")
        list(APPEND IOS_STARTUP_BYPRODUCTS
          "${IOS_STARTUP_DIR}/OpenGothic${IOS_STARTUP_SHADER}_${IOS_STARTUP_VERSION}.air"
          "${IOS_STARTUP_DIR}/OpenGothic${IOS_STARTUP_SHADER}_${IOS_STARTUP_VERSION}.max-profile.mslsrc")
      endforeach()
    endforeach()

    add_custom_command(
      OUTPUT ${IOS_STARTUP_RESOURCES} "${IOS_STARTUP_HASH_HEADER}"
      BYPRODUCTS ${IOS_STARTUP_BYPRODUCTS}
      COMMAND "${CMAKE_COMMAND}"
        "-DSTARTUP_DIR=${IOS_STARTUP_DIR}"
        "-DTRIANGLE_SPV=${IOS_STARTUP_TRIANGLE}"
        "-DDOWNSCALE_SPV=${IOS_STARTUP_DOWNSCALE}"
        "-DSTARTUP_MSL_TOOL=${IOS_STARTUP_TOOL_EXECUTABLE}"
        "-DXCRUN=${OPENGOTHIC_XCRUN_EXECUTABLE}"
        "-DMETAL_SDK=${IOS_STARTUP_METAL_SDK}"
        "-DMETAL_TARGET=${IOS_STARTUP_METAL_TARGET}"
        -P "${CMAKE_SOURCE_DIR}/shader/build_ios_startup_metallib.cmake"
      DEPENDS
        "${IOS_STARTUP_TOOL_STAMP}"
        "${IOS_STARTUP_SPIRV_STAMP}"
        "${CMAKE_SOURCE_DIR}/shader/build_ios_startup_metallib.cmake"
      COMMENT "Building six capability-independent iOS startup Metal libraries"
      VERBATIM)

    add_custom_target(ios-startup-shaders DEPENDS
      ${IOS_STARTUP_RESOURCES} "${IOS_STARTUP_HASH_HEADER}")
    add_dependencies(ios-startup-shaders ios-startup-spirv)
    add_dependencies(${PROJECT_NAME} ios-startup-shaders)
    set(OPENGOTHIC_IOS_STARTUP_RESOURCES
      ${IOS_STARTUP_RESOURCES} PARENT_SCOPE)
    set(OPENGOTHIC_IOS_STARTUP_HEADER
      "${IOS_STARTUP_HASH_HEADER}" PARENT_SCOPE)
    set(OPENGOTHIC_IOS_STARTUP_TARGET
      ios-startup-shaders PARENT_SCOPE)
  else()
    message(WARNING
      "Native clang++, xcrun Metal tools, or the iOS deployment target is unavailable; precompiled startup shaders are disabled and Tempest will compile SPIR-V at runtime")
  endif()
endif()
