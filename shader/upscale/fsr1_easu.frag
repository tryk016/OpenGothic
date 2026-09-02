#version 450

#extension GL_ARB_separate_shader_objects : enable

precision mediump float;
precision highp int;

// Scalar port of AMD FidelityFX FSR 1.0.2 EASU. Relaxed float precision lets
// mobile drivers use native FP16 ALU while desktop drivers may retain FP32.
// See FSR1-LICENSE.txt and https://github.com/GPUOpen-Effects/FidelityFX-FSR.

layout(binding = 0) uniform sampler2D src;

layout(push_constant, std140) uniform PushConstant {
  highp vec2 outputSize;
  } push;

layout(location = 0) out vec4 outColor;

float safeRcp(float value) {
  return 1.0/max(value, 1.0/16384.0);
  }

vec3 loadClamped(ivec2 pixel) {
  return texelFetch(src,clamp(pixel,ivec2(0),textureSize(src,0)-1),0).rgb;
  }

void setDirection(inout vec2 dir, inout float len, float weight,
                  float lA, float lB, float lC, float lD, float lE) {
  float dc   = lD-lC;
  float cb   = lC-lB;
  float dirX = lD-lB;
  float lenX = clamp(abs(dirX)*safeRcp(max(abs(dc),abs(cb))),0.0,1.0);

  float ec   = lE-lC;
  float ca   = lC-lA;
  float dirY = lE-lA;
  float lenY = clamp(abs(dirY)*safeRcp(max(abs(ec),abs(ca))),0.0,1.0);

  dir += vec2(dirX,dirY)*weight;
  len += (lenX*lenX+lenY*lenY)*weight;
  }

void addTap(inout vec3 color, inout float weight, vec2 offset,
            vec2 dir, vec2 len, float lobe, float clipPoint, vec3 tap) {
  vec2 v = vec2(dot(offset,dir),dot(offset,vec2(-dir.y,dir.x)))*len;
  float d2 = min(dot(v,v),clipPoint);
  float wB = 0.4*d2-1.0;
  float wA = lobe*d2-1.0;
  float w  = (1.5625*wB*wB-0.5625)*wA*wA;
  color  += tap*w;
  weight += w;
  }

vec3 easu(uvec2 pixel) {
  highp vec2 inputSize = vec2(textureSize(src,0));
  highp vec2 scale = inputSize/push.outputSize;
  highp vec2 sourcePos = vec2(pixel)*scale+0.5*scale-0.5;
  highp vec2 basePos = floor(sourcePos);
  vec2 pp = vec2(sourcePos-basePos);

  ivec2 base = ivec2(basePos);
  vec3 b = loadClamped(base+ivec2( 0,-1));
  vec3 c = loadClamped(base+ivec2( 1,-1));
  vec3 e = loadClamped(base+ivec2(-1, 0));
  vec3 f = loadClamped(base);
  vec3 g = loadClamped(base+ivec2( 1, 0));
  vec3 h = loadClamped(base+ivec2( 2, 0));
  vec3 i = loadClamped(base+ivec2(-1, 1));
  vec3 j = loadClamped(base+ivec2( 0, 1));
  vec3 k = loadClamped(base+ivec2( 1, 1));
  vec3 l = loadClamped(base+ivec2( 2, 1));
  vec3 n = loadClamped(base+ivec2( 0, 2));
  vec3 o = loadClamped(base+ivec2( 1, 2));

  float bL = b.g+0.5*(b.r+b.b);
  float cL = c.g+0.5*(c.r+c.b);
  float eL = e.g+0.5*(e.r+e.b);
  float fL = f.g+0.5*(f.r+f.b);
  float gL = g.g+0.5*(g.r+g.b);
  float hL = h.g+0.5*(h.r+h.b);
  float iL = i.g+0.5*(i.r+i.b);
  float jL = j.g+0.5*(j.r+j.b);
  float kL = k.g+0.5*(k.r+k.b);
  float lL = l.g+0.5*(l.r+l.b);
  float nL = n.g+0.5*(n.r+n.b);
  float oL = o.g+0.5*(o.r+o.b);

  float s = (1.0-pp.x)*(1.0-pp.y);
  float t = pp.x*(1.0-pp.y);
  float u = (1.0-pp.x)*pp.y;
  float v = pp.x*pp.y;

  vec2 dir = vec2(0.0);
  float len = 0.0;
  setDirection(dir,len,s,bL,eL,fL,gL,jL);
  setDirection(dir,len,t,cL,fL,gL,hL,kL);
  setDirection(dir,len,u,fL,iL,jL,kL,nL);
  setDirection(dir,len,v,gL,jL,kL,lL,oL);

  float dirSq = dot(dir,dir);
  if(dirSq<1.0/32768.0)
    dir = vec2(1.0,0.0);
  else
    dir *= inversesqrt(dirSq);

  len = 0.25*len*len;
  float stretch = safeRcp(max(abs(dir.x),abs(dir.y)));
  vec2 len2 = vec2(1.0+(stretch-1.0)*len,1.0-0.5*len);
  float lobe = 0.5-0.29*len;
  float clipPoint = safeRcp(lobe);

  vec3 min4 = min(min(f,g),min(j,k));
  vec3 max4 = max(max(f,g),max(j,k));

  vec3 color = vec3(0.0);
  float weight = 0.0;
  addTap(color,weight,vec2( 0.0,-1.0)-pp,dir,len2,lobe,clipPoint,b);
  addTap(color,weight,vec2( 1.0,-1.0)-pp,dir,len2,lobe,clipPoint,c);
  addTap(color,weight,vec2(-1.0, 1.0)-pp,dir,len2,lobe,clipPoint,i);
  addTap(color,weight,vec2( 0.0, 1.0)-pp,dir,len2,lobe,clipPoint,j);
  addTap(color,weight,vec2( 0.0, 0.0)-pp,dir,len2,lobe,clipPoint,f);
  addTap(color,weight,vec2(-1.0, 0.0)-pp,dir,len2,lobe,clipPoint,e);
  addTap(color,weight,vec2( 1.0, 1.0)-pp,dir,len2,lobe,clipPoint,k);
  addTap(color,weight,vec2( 2.0, 1.0)-pp,dir,len2,lobe,clipPoint,l);
  addTap(color,weight,vec2( 2.0, 0.0)-pp,dir,len2,lobe,clipPoint,h);
  addTap(color,weight,vec2( 1.0, 0.0)-pp,dir,len2,lobe,clipPoint,g);
  addTap(color,weight,vec2( 1.0, 2.0)-pp,dir,len2,lobe,clipPoint,o);
  addTap(color,weight,vec2( 0.0, 2.0)-pp,dir,len2,lobe,clipPoint,n);
  return clamp(color/weight,min4,max4);
  }

void main() {
  outColor = vec4(easu(uvec2(gl_FragCoord.xy)),1.0);
  }
