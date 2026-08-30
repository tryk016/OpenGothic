#include "padglyph.h"

#include <Tempest/Painter>
#include <Tempest/Color>
#include <Tempest/Brush>
#include <Tempest/Texture2d>
#include <cmath>

#include "utils/gthfont.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace Tempest;

static void fillDisc(Painter& p, float cx, float cy, float r, const Color& c) {
  p.setBrush(c);
  const int seg = 22;
  for(int i=0;i<seg;++i) {
    const float a0 = 2.f*float(M_PI)*float(i)/float(seg);
    const float a1 = 2.f*float(M_PI)*float(i+1)/float(seg);
    p.drawTriangle(cx, cy, 0.f, 0.f,
                   cx+std::cos(a0)*r, cy+std::sin(a0)*r, 0.f, 0.f,
                   cx+std::cos(a1)*r, cy+std::sin(a1)*r, 0.f, 0.f);
    }
  }

static void fillRing(Painter& p, float cx, float cy, float outer, float inner, const Color& c) {
  p.setBrush(c);
  const int seg = 32;
  for(int i=0;i<seg;++i) {
    const float a0 = 2.f*float(M_PI)*float(i)/float(seg);
    const float a1 = 2.f*float(M_PI)*float(i+1)/float(seg);
    const float ox0 = cx+std::cos(a0)*outer, oy0 = cy+std::sin(a0)*outer;
    const float ox1 = cx+std::cos(a1)*outer, oy1 = cy+std::sin(a1)*outer;
    const float ix0 = cx+std::cos(a0)*inner, iy0 = cy+std::sin(a0)*inner;
    const float ix1 = cx+std::cos(a1)*inner, iy1 = cy+std::sin(a1)*inner;
    p.drawTriangle(ox0,oy0,0.f,0.f, ox1,oy1,0.f,0.f, ix1,iy1,0.f,0.f);
    p.drawTriangle(ox0,oy0,0.f,0.f, ix1,iy1,0.f,0.f, ix0,iy0,0.f,0.f);
    }
  }

static void outlineRect(Painter& p, int x, int y, int w, int h, int line,
                        const Color& fill, const Color& stroke) {
  p.setBrush(fill);
  p.drawRect(x,y,w,h);
  p.setBrush(stroke);
  p.drawRect(x,y,w,line);
  p.drawRect(x,y+h-line,w,line);
  p.drawRect(x,y,line,h);
  p.drawRect(x+w-line,y,line,h);
  }

static void fillTri(Painter& p, float x0,float y0, float x1,float y1, float x2,float y2, const Color& c) {
  p.setBrush(c);
  p.drawTriangle(x0,y0,0.f,0.f, x1,y1,0.f,0.f, x2,y2,0.f,0.f);
  }

static void outlineTri(Painter& p,
                       float x0,float y0, float x1,float y1, float x2,float y2,
                       const Color& fill, const Color& stroke) {
  fillTri(p,x0,y0,x1,y1,x2,y2,stroke);
  const float cx = (x0+x1+x2)/3.f;
  const float cy = (y0+y1+y2)/3.f;
  constexpr float inner = 0.82f;
  fillTri(p,
          cx+(x0-cx)*inner,cy+(y0-cy)*inner,
          cx+(x1-cx)*inner,cy+(y1-cy)*inner,
          cx+(x2-cx)*inner,cy+(y2-cy)*inner,
          fill);
  }

static void centerLabel(Painter& p, const GthFont& fnt, float cx, float cy, std::string_view t) {
  const auto ts = fnt.textSize(t);
  fnt.drawText(p, int(cx)-ts.w/2, int(cy)+ts.h/2, t);
  }

void PadGlyph::draw(Painter& p, const GthFont& fnt, Btn b, int x, int y, int s, float a) {
  if(const Tempest::Texture2d* t = PadGlyph::texture(b)) {   // real Xelu art if bundled
    p.setBrush(Brush(*t, Color(1.f,1.f,1.f,a)));
    p.drawRect(x, y, s, s, 0, 0, int(t->w()), int(t->h()));
    return;
    }

  const float cx = float(x) + float(s)*0.5f;
  const float cy = float(y) + float(s)*0.5f;
  const float r  = float(s)*0.42f;

  auto disc = [&](float rr, float gg, float bb){
    fillDisc(p, cx, cy, r, Color(rr,gg,bb,0.95f*a));
    };
  auto pill = [&](float h){   // rounded-ish shoulder/trigger bar (approximated by a rect)
    p.setBrush(Color(0.82f,0.82f,0.86f,0.9f*a));
    p.drawRect(int(float(x)+float(s)*0.08f), int(cy-float(s)*h*0.5f),
               int(float(s)*0.84f),          int(float(s)*h));
    };
  const Color ink(0.90f,0.90f,0.90f,0.9f*a);

  switch(b) {
    case A: disc(0.30f,0.78f,0.35f); centerLabel(p,fnt,cx,cy,"A"); break;
    case B: disc(0.90f,0.28f,0.26f); centerLabel(p,fnt,cx,cy,"B"); break;
    case X: disc(0.24f,0.52f,0.94f); centerLabel(p,fnt,cx,cy,"X"); break;
    case Y: disc(0.95f,0.78f,0.20f); centerLabel(p,fnt,cx,cy,"Y"); break;

    case LB: pill(0.5f); centerLabel(p,fnt,cx,cy,"LB"); break;
    case RB: pill(0.5f); centerLabel(p,fnt,cx,cy,"RB"); break;
    case LT: pill(0.6f); centerLabel(p,fnt,cx,cy,"LT"); break;
    case RT: pill(0.6f); centerLabel(p,fnt,cx,cy,"RT"); break;

    case L3: fillDisc(p,cx,cy,r,Color(0.55f,0.55f,0.60f,0.9f*a)); centerLabel(p,fnt,cx,cy,"L3"); break;
    case R3: fillDisc(p,cx,cy,r,Color(0.55f,0.55f,0.60f,0.9f*a)); centerLabel(p,fnt,cx,cy,"R3"); break;

    case LStick: fillDisc(p,cx,cy,r,Color(0.40f,0.40f,0.46f,0.55f*a)); centerLabel(p,fnt,cx,cy,"L"); break;
    case RStick: fillDisc(p,cx,cy,r,Color(0.40f,0.40f,0.46f,0.55f*a)); centerLabel(p,fnt,cx,cy,"R"); break;

    // Keep every arrow clockwise in screen coordinates. Tempest's UI pipeline
    // culls the opposite winding, which previously made Up and Right invisible.
    case DPadUp:    fillTri(p, cx, cy-r, cx+r*0.8f, cy+r*0.4f, cx-r*0.8f, cy+r*0.4f, ink); break;
    case DPadDown:  fillTri(p, cx, cy+r, cx-r*0.8f, cy-r*0.4f, cx+r*0.8f, cy-r*0.4f, ink); break;
    case DPadLeft:  fillTri(p, cx-r, cy, cx+r*0.4f, cy-r*0.8f, cx+r*0.4f, cy+r*0.8f, ink); break;
    case DPadRight: fillTri(p, cx+r, cy, cx-r*0.4f, cy+r*0.8f, cx-r*0.4f, cy-r*0.8f, ink); break;

    case Menu: {   // three stacked lines
      p.setBrush(ink);
      const int lw = int(float(s)*0.5f), lh = std::max(2,int(float(s)*0.07f));
      for(int i=0;i<3;++i)
        p.drawRect(int(float(x)+float(s)*0.25f), int(float(y)+float(s)*(0.34f+0.16f*float(i))), lw, lh);
      break;
      }
    case View: {   // two panes
      p.setBrush(ink);
      p.drawRect(int(float(x)+float(s)*0.22f), int(float(y)+float(s)*0.34f), int(float(s)*0.24f), int(float(s)*0.32f));
      p.drawRect(int(float(x)+float(s)*0.54f), int(float(y)+float(s)*0.34f), int(float(s)*0.24f), int(float(s)*0.32f));
      break;
      }
    }
  }

void PadGlyph::drawTouch(Painter& p, const GthFont& fnt, Btn b,
                         int x, int y, int s, float a) {
  const float cx = float(x) + float(s)*0.5f;
  const float cy = float(y) + float(s)*0.5f;
  const float r  = float(s)*0.43f;
  const float ln = std::max(2.f,float(s)*0.035f);
  const Color fill(0.03f,0.04f,0.06f,0.16f*a);
  const Color ink (0.96f,0.97f,1.00f,0.88f*a);

  auto circle = [&] {
    fillDisc(p,cx,cy,r,fill);
    fillRing(p,cx,cy,r,std::max(0.f,r-ln),ink);
    };
  auto label = [&](std::string_view txt) {
    centerLabel(p,fnt,cx,cy,txt);
    };
  auto box = [&](float height) {
    const int bw = int(float(s)*0.88f);
    const int bh = int(float(s)*height);
    outlineRect(p,int(cx)-bw/2,int(cy)-bh/2,bw,bh,std::max(2,int(ln)),fill,ink);
    };

  switch(b) {
    case A: circle(); label("A"); break;
    case B: circle(); label("B"); break;
    case X: circle(); label("X"); break;
    case Y: circle(); label("Y"); break;

    case LB: box(0.48f); label("LB"); break;
    case RB: box(0.48f); label("RB"); break;
    case LT: box(0.56f); label("LT"); break;
    case RT: box(0.56f); label("RT"); break;

    case L3: circle(); label("L3"); break;
    case R3: circle(); label("R3"); break;

    case LStick:
    case RStick: {
      const float inner = r*0.42f;
      fillDisc(p,cx,cy,r,fill);
      fillRing(p,cx,cy,r,std::max(0.f,r-ln),ink);
      fillDisc(p,cx,cy,inner,Color(0.03f,0.04f,0.06f,0.22f*a));
      fillRing(p,cx,cy,inner,std::max(0.f,inner-ln),ink);
      label(b==LStick ? "L" : "R");
      break;
      }

    case DPadUp:
      outlineTri(p,cx,cy-r,cx+r*0.75f,cy+r*0.4f,cx-r*0.75f,cy+r*0.4f,fill,ink);
      break;
    case DPadDown:
      outlineTri(p,cx,cy+r,cx-r*0.75f,cy-r*0.4f,cx+r*0.75f,cy-r*0.4f,fill,ink);
      break;
    case DPadLeft:
      outlineTri(p,cx-r,cy,cx+r*0.4f,cy-r*0.75f,cx+r*0.4f,cy+r*0.75f,fill,ink);
      break;
    case DPadRight:
      outlineTri(p,cx+r,cy,cx-r*0.4f,cy+r*0.75f,cx-r*0.4f,cy-r*0.75f,fill,ink);
      break;

    case Menu: {
      circle();
      p.setBrush(ink);
      const int lw = int(float(s)*0.42f), lh = std::max(2,int(float(s)*0.045f));
      for(int i=0;i<3;++i)
        p.drawRect(int(cx)-lw/2,int(float(y)+float(s)*(0.37f+0.13f*float(i))),lw,lh);
      break;
      }
    case View: {
      circle();
      p.setBrush(ink);
      const int pw = int(float(s)*0.20f), ph = int(float(s)*0.25f);
      p.drawRect(int(cx)-pw-int(float(s)*0.04f),int(cy)-ph/2,pw,ph);
      p.drawRect(int(cx)+int(float(s)*0.04f),int(cy)-ph/2,pw,ph);
      break;
      }
    }
  }

int PadGlyph::drawLabelled(Painter& p, const GthFont& fnt, Btn b,
                           int x, int y, int s, std::string_view label, float a) {
  draw(p, fnt, b, x, y, s, a);
  const int gap = std::max(2, s/6);
  const int tx  = x + s + gap;
  fnt.drawText(p, tx, y + s - (s - fnt.textSize(label).h)/2, label);
  return s + gap + fnt.textSize(label).w + gap*2;
  }
