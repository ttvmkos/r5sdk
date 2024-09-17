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

#include "Recast/Include/Recast.h"
#include "DebugUtils/Include/DetourDebugDraw.h"
#include "DebugUtils/Include/RecastDebugDraw.h"
#include "NavEditor/Include/ConvexVolumeTool.h"
#include "NavEditor/Include/InputGeom.h"
#include "NavEditor/Include/Editor.h"
#include "naveditor/include/GameUtils.h"

#include "coordsize.h"

// Quick and dirty convex hull.

// Returns true if 'c' is left of line 'a'-'b'.
inline bool left(const float* a, const float* b, const float* c)
{ 
	const float u1 = b[0] - a[0];
	const float v1 = b[1] - a[1];
	const float u2 = c[0] - a[0];
	const float v2 = c[1] - a[1];
	return u1 * v2 - v1 * u2 < 0;
}

// Returns true if 'a' is more lower-left than 'b'.
inline bool cmppt(const float* a, const float* b)
{
	if (a[0] < b[0]) return true;
	if (a[0] > b[0]) return false;
	if (a[1] < b[1]) return true;
	if (a[1] > b[1]) return false;
	return false;
}
// Calculates convex hull on xy-plane of points on 'pts',
// stores the indices of the resulting hull in 'out' and
// returns number of points on hull.
static int convexhull(const float* pts, int npts, int* out)
{
	// Find lower-leftmost point.
	int hull = 0;
	for (int i = 1; i < npts; ++i)
		if (cmppt(&pts[i*3], &pts[hull*3]))
			hull = i;
	// Gift wrap hull.
	int endpt = 0;
	int i = 0;
	do
	{
		out[i++] = hull;
		endpt = 0;
		for (int j = 1; j < npts; ++j)
			if (hull == endpt || left(&pts[hull*3], &pts[endpt*3], &pts[j*3]))
				endpt = j;
		hull = endpt;
	}
	while (endpt != out[0]);
	
	return i;
}


ConvexVolumeTool::ConvexVolumeTool() :
	m_editor(0),
	m_areaType(RC_NULL_AREA),
	m_polyFlags(0),
	m_polyOffset(0.0f),
	m_boxHeight(650.0f),
	m_boxDescent(150.0f),
	m_npts(0),
	m_nhull(0)
{
}

void ConvexVolumeTool::init(Editor* editor)
{
	m_editor = editor;
}

void ConvexVolumeTool::reset()
{
	m_npts = 0;
	m_nhull = 0;
}

void ConvexVolumeTool::handleMenu()
{
	ImGui::PushItemWidth(120.f);

	ImGui::SliderFloat("Shape Height", &m_boxHeight, 0.1f, MAX_COORD_FLOAT);
	ImGui::SliderFloat("Shape Descent", &m_boxDescent, 0.1f, MAX_COORD_FLOAT);
	ImGui::SliderFloat("Poly Offset", &m_polyOffset, 0.0f, MAX_COORD_FLOAT/2);

	ImGui::PopItemWidth();

	ImGui::Separator();

	ImGui::Text("Brushes");
	ImGui::Indent();

	bool isEnabled = m_areaType == RC_NULL_AREA;

	if (ImGui::Checkbox("Clip", &isEnabled))
		m_areaType = RC_NULL_AREA;

	isEnabled = m_areaType == EDITOR_POLYAREA_TRIGGER;
	if (ImGui::Checkbox("Trigger", &isEnabled))
		m_areaType = EDITOR_POLYAREA_TRIGGER; // todo(amos): also allow setting flags and store this in .gset.

	if (m_areaType == EDITOR_POLYAREA_TRIGGER)
	{
		ImGui::Text("Poly Flags");
		ImGui::Indent();

		const int numPolyFlags = V_ARRAYSIZE(g_navMeshPolyFlagNames);

		for (int i = 0; i < numPolyFlags; i++)
		{
			const char* flagName = g_navMeshPolyFlagNames[i];
			ImGui::CheckboxFlags(flagName, &m_polyFlags, i == (numPolyFlags-1) ? EDITOR_POLYFLAGS_ALL : 1<<i);
		}

		ImGui::Unindent();
	}

	ImGui::Unindent();
	ImGui::Separator();

	if (ImGui::Button("Clear Shape"))
	{
		m_npts = 0;
		m_nhull = 0;
	}
}

void ConvexVolumeTool::handleClick(const float* /*s*/, const float* p, bool shift)
{
	if (!m_editor) return;
	InputGeom* geom = m_editor->getInputGeom();
	if (!geom) return;
	
	if (shift)
	{
		// Delete
		int nearestIndex = -1;
		const ConvexVolume* vols = geom->getConvexVolumes();
		for (int i = 0; i < geom->getConvexVolumeCount(); ++i)
		{
			if (rdPointInPolygon(p, vols[i].verts, vols[i].nverts) &&
							p[1] >= vols[i].hmin && p[1] <= vols[i].hmax)
			{
				nearestIndex = i;
			}
		}
		// If end point close enough, delete it.
		if (nearestIndex != -1)
		{
			geom->deleteConvexVolume(nearestIndex);
		}
	}
	else
	{
		// Create

		// If clicked on that last pt, create the shape.
		if (m_npts && rdVdistSqr(p, &m_pts[(m_npts-1)*3]) < rdSqr(0.2f))
		{
			if (m_nhull > 2)
			{
				// Create shape.
				float verts[MAX_PTS*3];
				for (int i = 0; i < m_nhull; ++i)
					rdVcopy(&verts[i*3], &m_pts[m_hull[i]*3]);
					
				float minh = FLT_MAX, maxh = 0;
				for (int i = 0; i < m_nhull; ++i)
					minh = rdMin(minh, verts[i*3+2]);
				minh -= m_boxDescent;
				maxh = minh + m_boxHeight;

				if (m_polyOffset > 0.01f)
				{
					float offset[MAX_PTS*2*3];
					int noffset = rcOffsetPoly(verts, m_nhull, m_polyOffset, offset, MAX_PTS*2);
					if (noffset > 0)
						geom->addConvexVolume(offset, noffset, minh, maxh, (unsigned short)m_polyFlags, (unsigned char)m_areaType);
				}
				else
				{
					geom->addConvexVolume(verts, m_nhull, minh, maxh, (unsigned short)m_polyFlags, (unsigned char)m_areaType);
				}
			}
			
			m_npts = 0;
			m_nhull = 0;
		}
		else
		{
			// Add new point 
			if (m_npts < MAX_PTS)
			{
				rdVcopy(&m_pts[m_npts*3], p);
				const float* f = &m_pts[m_npts * 3];

				printf("<%f, %f, %f>\n", f[0], f[1], f[2]);

				m_npts++;
				// Update hull.
				if (m_npts > 1)
					m_nhull = convexhull(m_pts, m_npts, m_hull);
				else
					m_nhull = 0;
			}
		}		
	}
	
}

void ConvexVolumeTool::handleToggle()
{
}

void ConvexVolumeTool::handleStep()
{
}

void ConvexVolumeTool::handleUpdate(const float /*dt*/)
{
}

void ConvexVolumeTool::handleRender()
{
	duDebugDraw& dd = m_editor->getDebugDraw();
	const float* drawOffset = m_editor->getDetourDrawOffset();
	
	// Find height extent of the shape.
	float minh = FLT_MAX, maxh = 0;
	for (int i = 0; i < m_npts; ++i)
		minh = rdMin(minh, m_pts[i*3+2]);
	minh -= m_boxDescent;
	maxh = minh + m_boxHeight;

	dd.begin(DU_DRAW_POINTS, 4.0f, drawOffset);
	for (int i = 0; i < m_npts; ++i)
	{
		unsigned int col = duRGBA(255,255,255,255);
		if (i == m_npts-1)
			col = duRGBA(240,32,16,255);
		dd.vertex(m_pts[i*3+0],m_pts[i*3+1],m_pts[i*3+2]+0.1f, col);//Needs to be flipped (y = z).
	}
	dd.end();

	dd.begin(DU_DRAW_LINES, 2.0f, drawOffset);
	for (int i = 0, j = m_nhull-1; i < m_nhull; j = i++)
	{
		const float* vi = &m_pts[m_hull[j]*3];
		const float* vj = &m_pts[m_hull[i]*3];
		dd.vertex(vj[0],vj[1],minh, duRGBA(255, 255, 255, 64));
		dd.vertex(vi[0],vi[1],minh, duRGBA(255, 255, 255, 64));
		dd.vertex(vj[0],vj[1],maxh, duRGBA(255, 255, 255, 64));
		dd.vertex(vi[0],vi[1],maxh, duRGBA(255, 255, 255, 64));
		dd.vertex(vj[0],vj[1],minh, duRGBA(255, 255, 255, 64));
		dd.vertex(vj[0],vj[1],maxh, duRGBA(255,255,255,64));
	}
	dd.end();	
}

void ConvexVolumeTool::handleRenderOverlay(double* /*proj*/, double* /*model*/, int* /*view*/)
{
	// Tool help
	if (!m_npts)
	{
		ImGui_RenderText(ImGuiTextAlign_e::kAlignLeft,
			ImVec2(280, 40), ImVec4(1.0f,1.0f,1.0f,0.75f), "LMB: Create new shape.  SHIFT+LMB: Delete existing shape (click inside a shape).");
	}
	else
	{
		ImGui_RenderText(ImGuiTextAlign_e::kAlignLeft,
			ImVec2(280, 60), ImVec4(1.0f,1.0f,1.0f,0.75f), "The shape will be convex hull of all added points.");
	}
}
