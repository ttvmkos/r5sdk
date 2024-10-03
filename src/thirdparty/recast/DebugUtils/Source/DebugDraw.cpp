//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include "Shared/Include/SharedMath.h"
#include "Shared/Include/SharedCommon.h"
#include "DebugUtils/Include/DebugDraw.h"
#include "Detour/Include/DetourNavMesh.h"

static const unsigned char BOX_FACE_INDICES[6*4] =
{
	2, 6, 7, 3,
	0, 4, 5, 1,
	7, 6, 5, 4,
	0, 1, 2, 3,
	1, 5, 6, 2,
	3, 7, 4, 0,
};

duDebugDraw::~duDebugDraw()
{
	// Empty
}

unsigned int duDebugDraw::areaToCol(unsigned int area)
{
	if (area == 0)
	{
		// Treat zero area type as default.
		return duRGBA(0, 192, 255, 255);
	}
	else
	{
		return duIntToCol(area, 255);
	}
}

inline int bit(int a, int b)
{
	return (a & (1 << b)) >> b;
}

unsigned int duIntToCol(int i, int a)
{
	int	r = bit(i, 1) + bit(i, 3) * 2 + 1;
	int	g = bit(i, 2) + bit(i, 4) * 2 + 1;
	int	b = bit(i, 0) + bit(i, 5) * 2 + 1;
	return duRGBA(r*63,g*63,b*63,a);
}

void duIntToCol(int i, float* col)
{
	int	r = bit(i, 0) + bit(i, 3) * 2 + 1;
	int	g = bit(i, 1) + bit(i, 4) * 2 + 1;
	int	b = bit(i, 2) + bit(i, 5) * 2 + 1;
	col[0] = 1 - r*63.0f/255.0f;
	col[1] = 1 - g*63.0f/255.0f;
	col[2] = 1 - b*63.0f/255.0f;
}

const unsigned char* duSetBoxVerts(float minx, float miny, float minz, float maxx,
	float maxy, float maxz, float* verts)
{
	rdVset(&verts[0], minx, miny, minz);
	rdVset(&verts[3], maxx, miny, minz);
	rdVset(&verts[6], maxx, miny, maxz);
	rdVset(&verts[9], minx, miny, maxz);
	rdVset(&verts[12], minx, maxy, minz);
	rdVset(&verts[15], maxx, maxy, minz);
	rdVset(&verts[18], maxx, maxy, maxz);
	rdVset(&verts[21], minx, maxy, maxz);

	return BOX_FACE_INDICES;
}

void duCalcBoxColors(unsigned int* colors, unsigned int colTop, unsigned int colSide)
{
	if (!colors) return;
	
	colors[0] = duMultCol(colTop, 250);
	colors[1] = duMultCol(colSide, 140);
	colors[2] = duMultCol(colSide, 185);
	colors[3] = duMultCol(colSide, 227);
	colors[4] = duMultCol(colSide, 165);
	colors[5] = duMultCol(colSide, 207);
}

void duDebugDrawCylinderWire(struct duDebugDraw* dd, float minx, float miny, float minz,
							 float maxx, float maxy, float maxz, unsigned int col,
							 const float lineWidth, const float* offset)
{
	if (!dd) return;
	
	dd->begin(DU_DRAW_LINES, lineWidth, offset);
	duAppendCylinderWire(dd, minx,miny,minz, maxx,maxy,maxz, col);
	dd->end();
}

void duDebugDrawBoxWire(struct duDebugDraw* dd, float minx, float miny, float minz,
						float maxx, float maxy, float maxz, unsigned int col, 
						const float lineWidth, const float* offset)
{
	if (!dd) return;
	
	dd->begin(DU_DRAW_LINES, lineWidth, offset);
	duAppendBoxWire(dd, minx,miny,minz, maxx,maxy,maxz, col);
	dd->end();
}

void duDebugDrawArc(struct duDebugDraw* dd, const float x0, const float y0, const float z0,
					const float x1, const float y1, const float z1, const float h,
					const float as0, const float as1, unsigned int col,
					const float lineWidth, const float* offset)
{
	if (!dd) return;
	
	dd->begin(DU_DRAW_LINES, lineWidth, offset);
	duAppendArc(dd, x0,y0,z0, x1,y1,z1, h, as0, as1, col);
	dd->end();
}

void duDebugDrawArrow(struct duDebugDraw* dd, const float x0, const float y0, const float z0,
					  const float x1, const float y1, const float z1,
					  const float as0, const float as1, unsigned int col,
					  const float lineWidth, const float* offset)
{
	if (!dd) return;
	
	dd->begin(DU_DRAW_LINES, lineWidth, offset);
	duAppendArrow(dd, x0,y0,z0, x1,y1,z1, as0, as1, col);
	dd->end();
}

void duDebugDrawCircle(struct duDebugDraw* dd, const float x, const float y, const float z,
					   const float r, unsigned int col, const float lineWidth, const float* offset)
{
	if (!dd) return;
	
	dd->begin(DU_DRAW_LINES, lineWidth, offset);
	duAppendCircle(dd, x,y,z, r, col);
	dd->end();
}

void duDebugDrawCross(struct duDebugDraw* dd, const float x, const float y, const float z,
					  const float size, unsigned int col, const float lineWidth, const float* offset)
{
	if (!dd) return;
	
	dd->begin(DU_DRAW_LINES, lineWidth, offset);
	duAppendCross(dd, x,y,z, size, col);
	dd->end();
}

void duDebugDrawBox(struct duDebugDraw* dd, float minx, float miny, float minz,
					float maxx, float maxy, float maxz, const unsigned int* fcol, const float* offset)
{
	if (!dd) return;
	
	dd->begin(DU_DRAW_QUADS, 1.0f, offset);
	duAppendBox(dd, minx,miny,minz, maxx,maxy,maxz, fcol);
	dd->end();
}

void duDebugDrawCylinder(struct duDebugDraw* dd, float minx, float miny, float minz,
						 float maxx, float maxy, float maxz, unsigned int col, const float* offset)
{
	if (!dd) return;
	
	dd->begin(DU_DRAW_TRIS, 1.0f, offset);
	duAppendCylinder(dd, minx,miny,minz, maxx,maxy,maxz, col);
	dd->end();
}

void duDebugDrawGridXZ(struct duDebugDraw* dd, const float ox, const float oy, const float oz,
					   const int w, const int h, const float size,
					   const unsigned int col, const float lineWidth, const float* offset)
{
	if (!dd) return;

	dd->begin(DU_DRAW_LINES, lineWidth, offset);
	for (int i = 0; i <= h; ++i)
	{
		dd->vertex(ox,oy,oz+i*size, col);
		dd->vertex(ox+w*size,oy,oz+i*size, col);
	}
	for (int i = 0; i <= w; ++i)
	{
		dd->vertex(ox+i*size,oy,oz, col);
		dd->vertex(ox+i*size,oy,oz+h*size, col);
	}
	dd->end();
}
		 
void duDebugDrawGridXY(struct duDebugDraw* dd, const float ox, const float oy, const float oz,
	const int w, const int h, const float size,
	const unsigned int col, const float lineWidth, const float* offset)
{
	if (!dd) return;

	dd->begin(DU_DRAW_LINES, lineWidth, offset);
	for (int i = 0; i <= h; ++i)
	{
		dd->vertex(ox,oy+i*size,oz, col);
		dd->vertex(ox-w*size,oy+i*size,oz, col);
	}
	for (int i = 0; i <= w; ++i)
	{
		dd->vertex(ox-i*size,oy,oz, col);
		dd->vertex(ox-i*size,oy+h*size,oz, col);
	}
	dd->end();
}

void duAppendCylinderWire(struct duDebugDraw* dd, float minx, float miny, float minz,
						  float maxx, float maxy, float maxz, unsigned int col)
{
	if (!dd) return;

	static const int NUM_SEG = 16;
	static float dir[NUM_SEG*2];
	static bool init = false;
	if (!init)
	{
		init = true;
		for (int i = 0; i < NUM_SEG; ++i)
		{
			const float a = (float)i/(float)NUM_SEG*DU_PI*2;
			dir[i*2] = rdMathCosf(a);
			dir[i*2+1] = rdMathSinf(a);
		}
	}
	
	const float cx = (maxx + minx)/2;
	const float cy = (maxy + miny)/2;
	const float rx = (maxx - minx)/2;
	const float ry = (maxy - miny)/2;
	
	for (int i = 0, j = NUM_SEG-1; i < NUM_SEG; j = i++)
	{
		dd->vertex(cx+dir[j*2+0]*rx, cy+dir[j*2+1]*ry, minz, col);
		dd->vertex(cx+dir[i*2+0]*rx, cy+dir[i*2+1]*ry, minz, col);
		dd->vertex(cx+dir[j*2+0]*rx, cy+dir[j*2+1]*ry, maxz, col);
		dd->vertex(cx+dir[i*2+0]*rx, cy+dir[i*2+1]*ry, maxz, col);
	}
	for (int i = 0; i < NUM_SEG; i += NUM_SEG/4)
	{
		dd->vertex(cx+dir[i*2+0]*rx, cy+dir[i*2+1]*ry, minz, col);
		dd->vertex(cx+dir[i*2+0]*rx, cy+dir[i*2+1]*ry, maxz, col);
	}
}

void duAppendBoxWire(struct duDebugDraw* dd, float minx, float miny, float minz,
					 float maxx, float maxy, float maxz, unsigned int col)
{
	if (!dd) return;
	// Top
	dd->vertex(minx, miny, minz, col);
	dd->vertex(maxx, miny, minz, col);
	dd->vertex(maxx, miny, minz, col);
	dd->vertex(maxx, miny, maxz, col);
	dd->vertex(maxx, miny, maxz, col);
	dd->vertex(minx, miny, maxz, col);
	dd->vertex(minx, miny, maxz, col);
	dd->vertex(minx, miny, minz, col);
	
	// bottom
	dd->vertex(minx, maxy, minz, col);
	dd->vertex(maxx, maxy, minz, col);
	dd->vertex(maxx, maxy, minz, col);
	dd->vertex(maxx, maxy, maxz, col);
	dd->vertex(maxx, maxy, maxz, col);
	dd->vertex(minx, maxy, maxz, col);
	dd->vertex(minx, maxy, maxz, col);
	dd->vertex(minx, maxy, minz, col);
	
	// Sides
	dd->vertex(minx, miny, minz, col);
	dd->vertex(minx, maxy, minz, col);
	dd->vertex(maxx, miny, minz, col);
	dd->vertex(maxx, maxy, minz, col);
	dd->vertex(maxx, miny, maxz, col);
	dd->vertex(maxx, maxy, maxz, col);
	dd->vertex(minx, miny, maxz, col);
	dd->vertex(minx, maxy, maxz, col);
}

void duAppendBoxPoints(struct duDebugDraw* dd, float minx, float miny, float minz,
					   float maxx, float maxy, float maxz, unsigned int col)
{
	if (!dd) return;
	// Top
	dd->vertex(minx, miny, minz, col);
	dd->vertex(maxx, miny, minz, col);
	dd->vertex(maxx, miny, minz, col);
	dd->vertex(maxx, miny, maxz, col);
	dd->vertex(maxx, miny, maxz, col);
	dd->vertex(minx, miny, maxz, col);
	dd->vertex(minx, miny, maxz, col);
	dd->vertex(minx, miny, minz, col);
	
	// bottom
	dd->vertex(minx, maxy, minz, col);
	dd->vertex(maxx, maxy, minz, col);
	dd->vertex(maxx, maxy, minz, col);
	dd->vertex(maxx, maxy, maxz, col);
	dd->vertex(maxx, maxy, maxz, col);
	dd->vertex(minx, maxy, maxz, col);
	dd->vertex(minx, maxy, maxz, col);
	dd->vertex(minx, maxy, minz, col);
}

void duAppendBox(struct duDebugDraw* dd, float minx, float miny, float minz,
				 float maxx, float maxy, float maxz, const unsigned int* fcol)
{
	if (!dd) return;

	float verts[8*3];
	const unsigned char* in = duSetBoxVerts(minx, miny, minz, maxx, maxy, maxz, verts);

	for (int i = 0; i < 6; ++i)
	{
		dd->vertex(&verts[*in*3], fcol[i]); in++;
		dd->vertex(&verts[*in*3], fcol[i]); in++;
		dd->vertex(&verts[*in*3], fcol[i]); in++;
		dd->vertex(&verts[*in*3], fcol[i]); in++;
	}
}

void duAppendCylinder(struct duDebugDraw* dd, float minx, float miny, float minz,
					  float maxx, float maxy, float maxz, unsigned int col)
{
	if (!dd) return;
	
	static const int NUM_SEG = 16;
	static float dir[NUM_SEG*2];
	static bool init = false;
	if (!init)
	{
		init = true;
		for (int i = 0; i < NUM_SEG; ++i)
		{
			const float a = (float)i/(float)NUM_SEG*DU_PI*2;
			dir[i*2] = rdMathCosf(a);
			dir[i*2+1] = rdMathSinf(a);
		}
	}
	
	unsigned int col2 = duMultCol(col, 160);
	
	const float cx = (maxx + minx)/2;
	const float cy = (maxy + miny)/2;
	const float rx = (maxx - minx)/2;
	const float ry = (maxy - miny)/2;

	for (int i = 2; i < NUM_SEG; ++i)
	{
		const int a = 0, b = i-1, c = i;
		dd->vertex(cx+dir[c*2+0]*rx, cy+dir[c*2+1]*ry, minz, col2);
		dd->vertex(cx+dir[b*2+0]*rx, cy+dir[b*2+1]*ry, minz, col2);
		dd->vertex(cx+dir[a*2+0]*rx, cy+dir[a*2+1]*ry, minz, col2);
	}
	for (int i = 2; i < NUM_SEG; ++i)
	{
		const int a = 0, b = i, c = i-1;
		dd->vertex(cx+dir[c*2+0]*rx, cy+dir[c*2+1]*ry, maxz, col);
		dd->vertex(cx+dir[b*2+0]*rx, cy+dir[b*2+1]*ry, maxz, col);
		dd->vertex(cx+dir[a*2+0]*rx, cy+dir[a*2+1]*ry, maxz, col);
	}
	for (int i = 0, j = NUM_SEG-1; i < NUM_SEG; j = i++)
	{
		dd->vertex(cx+dir[i*2+0]*rx, cy+dir[i*2+1]*ry, minz, col2);
		dd->vertex(cx+dir[j*2+0]*rx, cy+dir[j*2+1]*ry, maxz, col);
		dd->vertex(cx+dir[j*2+0]*rx, cy+dir[j*2+1]*ry, minz, col2);

		dd->vertex(cx+dir[j*2+0]*rx, cy+dir[j*2+1]*ry, maxz, col);
		dd->vertex(cx+dir[i*2+0]*rx, cy+dir[i*2+1]*ry, minz, col2);
		dd->vertex(cx+dir[i*2+0]*rx, cy+dir[i*2+1]*ry, maxz, col);
	}
}

inline void evalArc(const float x0, const float y0, const float z0,
					const float dx, const float dy, const float dz,
					const float h, const float u, float* res)
{
	res[0] = x0 + dx * u;
	res[1] = y0 + dy * u;
	res[2] = z0 + dz * u + h * (1-(u*2-1)*(u*2-1));
}

void appendArrowHead(struct duDebugDraw* dd, const float* p, const float* q,
					 const float s, unsigned int col)
{
	const float eps = 0.001f;
	if (!dd) return;
	if (rdVdistSqr(p,q) < eps*eps) return;
	float ax[3], ay[3] = {0,1,0}, az[3];
	rdVsub(az, q, p);
	rdVnormalize(az);
	rdVcross(ax, ay, az);
	rdVcross(ay, az, ax);
	rdVnormalize(ay);

	dd->vertex(p, col);
	dd->vertex(p[0]+az[0]*s+ay[0]*s/2, p[1]+az[1]*s+ay[1]*s/2, p[2]+az[2]*s+ay[2]*s/2, col);

	dd->vertex(p, col);
	dd->vertex(p[0]+az[0]*s-ay[0]*s/2, p[1]+az[1]*s-ay[1]*s/2, p[2]+az[2]*s-ay[2]*s/2, col);
}

void duAppendArc(struct duDebugDraw* dd, const float x0, const float y0, const float z0,
				 const float x1, const float y1, const float z1, const float h,
				 const float as0, const float as1, unsigned int col)
{
	if (!dd) return;
	static const int NUM_ARC_PTS = 8;
	static const float PAD = 0.05f;
	static const float ARC_PTS_SCALE = (1.0f-PAD*2) / (float)NUM_ARC_PTS;
	const float dx = x1 - x0;
	const float dy = y1 - y0;
	const float dz = z1 - z0;
	const float len = rdMathSqrtf(dx*dx + dy*dy + dz*dz);
	float prev[3];
	evalArc(x0,y0,z0, dx,dy,dz, len*h, PAD, prev);
	for (int i = 1; i <= NUM_ARC_PTS; ++i)
	{
		const float u = PAD + i * ARC_PTS_SCALE;
		float pt[3];
		evalArc(x0,y0,z0, dx,dy,dz, len*h, u, pt);
		dd->vertex(prev[0],prev[1],prev[2], col);
		dd->vertex(pt[0],pt[1],pt[2], col);
		prev[0] = pt[0]; prev[1] = pt[1]; prev[2] = pt[2];
	}
	
	// End arrows
	if (as0 > 0.001f)
	{
		float p[3], q[3];
		evalArc(x0,y0,z0, dx,dy,dz, len*h, PAD, p);
		evalArc(x0,y0,z0, dx,dy,dz, len*h, PAD+0.05f, q);
		appendArrowHead(dd, p, q, as0, col);
	}

	if (as1 > 0.001f)
	{
		float p[3], q[3];
		evalArc(x0,y0,z0, dx,dy,dz, len*h, 1-PAD, p);
		evalArc(x0,y0,z0, dx,dy,dz, len*h, 1-(PAD+0.05f), q);
		appendArrowHead(dd, p, q, as1, col);
	}
}

void duAppendArrow(struct duDebugDraw* dd, const float x0, const float y0, const float z0,
				   const float x1, const float y1, const float z1,
				   const float as0, const float as1, unsigned int col)
{
	if (!dd) return;

	dd->vertex(x0,y0,z0, col);
	dd->vertex(x1,y1,z1, col);
	
	// End arrows
	const float p[3] = {x0,y0,z0}, q[3] = {x1,y1,z1};
	if (as0 > 0.001f)
		appendArrowHead(dd, p, q, as0, col);
	if (as1 > 0.001f)
		appendArrowHead(dd, q, p, as1, col);
}

void duAppendCircle(struct duDebugDraw* dd, const float x, const float y, const float z,
					const float r, unsigned int col)
{
	if (!dd) return;
	static const int NUM_SEG = 40;
	static float dir[40*2];
	static bool init = false;
	if (!init)
	{
		init = true;
		for (int i = 0; i < NUM_SEG; ++i)
		{
			const float a = (float)i/(float)NUM_SEG*DU_PI*2;
			dir[i*2] = rdMathCosf(a);
			dir[i*2+1] = rdMathSinf(a);
		}
	}
	
	for (int i = 0, j = NUM_SEG-1; i < NUM_SEG; j = i++)
	{
		dd->vertex(x+dir[j*2+0]*r, y+dir[j*2+1]*r, z, col);
		dd->vertex(x+dir[i*2+0]*r, y+dir[i*2+1]*r, z, col);
	}
}

void duAppendCross(struct duDebugDraw* dd, const float x, const float y, const float z,
				   const float s, unsigned int col)
{
	if (!dd) return;
	dd->vertex(x-s,y,z, col);
	dd->vertex(x+s,y,z, col);
	dd->vertex(x,y-s,z, col);
	dd->vertex(x,y+s,z, col);
	dd->vertex(x,y,z-s, col);
	dd->vertex(x,y,z+s, col);
}

duDisplayList::duDisplayList(int cap) :
	m_prim(DU_DRAW_LINES),
	m_primSize(1.0f),
	m_pos(0),
	m_color(0),
	m_size(0),
	m_cap(0),
	m_depthMask(true)
{
	if (cap < 8)
		cap = 8;
	resize(cap);
	rdVset(m_drawOffset, 0.0f,0.0f,0.0f);
}

duDisplayList::~duDisplayList()
{
	delete [] m_pos;
	delete [] m_color;
}

void duDisplayList::resize(int cap)
{
	float* newPos = new float[cap*3];
	if (m_size)
		memcpy(newPos, m_pos, sizeof(float)*3*m_size);
	delete [] m_pos;
	m_pos = newPos;

	unsigned int* newColor = new unsigned int[cap];
	if (m_size)
		memcpy(newColor, m_color, sizeof(unsigned int)*m_size);
	delete [] m_color;
	m_color = newColor;
	
	m_cap = cap;
}

void duDisplayList::clear()
{
	m_size = 0;
}

void duDisplayList::depthMask(bool state)
{
	m_depthMask = state;
}

void duDisplayList::begin(const duDebugDrawPrimitives prim, const float size, const float* offset)
{
	clear();
	m_prim = prim;
	m_primSize = size;
	if (offset)
		rdVcopy(m_drawOffset, offset);
}

void duDisplayList::vertex(const float x, const float y, const float z, unsigned int color)
{
	if (m_size+1 >= m_cap)
		resize(m_cap*2);
	float* p = &m_pos[m_size*3];
	rdVset(p,x,y,z);
	rdVadd(p,p,m_drawOffset);
	m_color[m_size] = color;
	m_size++;
}

void duDisplayList::vertex(const float* pos, unsigned int color)
{
	vertex(pos[0],pos[1],pos[2],color);
}

void duDisplayList::end()
{
	rdVset(m_drawOffset, 0.0f,0.0f,0.0f);
}

void duDisplayList::draw(struct duDebugDraw* dd)
{
	if (!dd) return;
	if (!m_size) return;
	dd->depthMask(m_depthMask);
	dd->begin(m_prim, m_primSize);
	for (int i = 0; i < m_size; ++i)
		dd->vertex(&m_pos[i*3], m_color[i]);
	dd->end();
}
