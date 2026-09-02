#version 450

#extension GL_ARB_separate_shader_objects : enable

precision mediump float;
precision highp int;

// Scalar port of AMD FidelityFX FSR 1.0.2 RCAS. Relaxed float precision lets
// mobile drivers use native FP16 ALU while desktop drivers may retain FP32.
// See FSR1-LICENSE.txt and https://github.com/GPUOpen-Effects/FidelityFX-FSR.

layout(binding = 0) uniform sampler2D src;

layout(push_constant, std140) uniform PushConstant {
  float sharpness;
  } push;

layout(location = 0) out vec4 outColor;

vec3 loadClamped(ivec2 pixel) {
  return texelFetch(src,clamp(pixel,ivec2(0),textureSize(src,0)-1),0).rgb;
  }

void main() {
  ivec2 pixel = ivec2(gl_FragCoord.xy);
  vec3 b = loadClamped(pixel+ivec2( 0,-1));
  vec3 d = loadClamped(pixel+ivec2(-1, 0));
  vec3 e = loadClamped(pixel);
  vec3 f = loadClamped(pixel+ivec2( 1, 0));
  vec3 h = loadClamped(pixel+ivec2( 0, 1));

  vec3 minRing = min(min(b,d),min(f,h));
  vec3 maxRing = max(max(b,d),max(f,h));
  vec3 hitMin  = min(minRing,e)/max(4.0*maxRing,vec3(1.0/16384.0));
  vec3 hitMax  = (1.0-max(maxRing,e))/min(4.0*minRing-4.0,vec3(-1.0/16384.0));
  vec3 lobes   = max(-hitMin,hitMax);

  float lobe = clamp(max(lobes.r,max(lobes.g,lobes.b)),-3.0/16.0,0.0);
  float strength = exp2(-2.0*(1.0-clamp(push.sharpness,0.0,1.0)));
  lobe *= strength;

  vec3 color = (lobe*(b+d+f+h)+e)/(4.0*lobe+1.0);
  outColor = vec4(clamp(color,0.0,1.0),1.0);
  }
