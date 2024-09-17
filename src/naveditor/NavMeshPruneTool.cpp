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

#include "Shared/Include/SharedCommon.h"
#include "Detour/Include/DetourNavMesh.h"
#include "DebugUtils/Include/DetourDebugDraw.h"
#include "NavEditor/Include/NavMeshPruneTool.h"
#include "NavEditor/Include/InputGeom.h"
#include "NavEditor/Include/Editor.h"

#ifdef WIN32
#	define snprintf _snprintf
#endif

class NavmeshFlags
{
	struct TileFlags
	{
		inline void purge() { rdFree(flags); }
		unsigned char* flags;
		int nflags;
		dtPolyRef base;
	};
	
	const dtNavMesh* m_nav;
	TileFlags* m_tiles;
	int m_ntiles;

public:
	NavmeshFlags() :
		m_nav(0), m_tiles(0), m_ntiles(0)
	{
	}
	
	~NavmeshFlags()
	{
		for (int i = 0; i < m_ntiles; ++i)
			m_tiles[i].purge();
		rdFree(m_tiles);
	}
	
	bool init(const dtNavMesh* nav)
	{
		m_ntiles = nav->getMaxTiles();
		if (!m_ntiles)
			return true;
		m_tiles = (TileFlags*)rdAlloc(sizeof(TileFlags)*m_ntiles, RD_ALLOC_TEMP);
		if (!m_tiles)
		{
			return false;
		}
		memset(m_tiles, 0, sizeof(TileFlags)*m_ntiles);
		
		// Alloc flags for each tile.
		for (int i = 0; i < nav->getMaxTiles(); ++i)
		{
			const dtMeshTile* tile = nav->getTile(i);
			if (!tile->header) continue;
			TileFlags* tf = &m_tiles[i];
			tf->nflags = tile->header->polyCount;
			tf->base = nav->getPolyRefBase(tile);
			if (tf->nflags)
			{
				tf->flags = (unsigned char*)rdAlloc(tf->nflags, RD_ALLOC_TEMP);
				if (!tf->flags)
					return false;
				memset(tf->flags, 0, tf->nflags);
			}
		}
		
		m_nav = nav;
		
		return false;
	}
	
	inline void clearAllFlags()
	{
		for (int i = 0; i < m_ntiles; ++i)
		{
			TileFlags* tf = &m_tiles[i];
			if (tf->nflags)
				memset(tf->flags, 0, tf->nflags);
		}
	}
	
	inline unsigned char getFlags(dtPolyRef ref)
	{
		rdAssert(m_nav);
		rdAssert(m_ntiles);
		// Assume the ref is valid, no bounds checks.
		unsigned int salt, it, ip;
		m_nav->decodePolyId(ref, salt, it, ip);
		return m_tiles[it].flags[ip];
	}

	inline void setFlags(dtPolyRef ref, unsigned char flags)
	{
		rdAssert(m_nav);
		rdAssert(m_ntiles);
		// Assume the ref is valid, no bounds checks.
		unsigned int salt, it, ip;
		m_nav->decodePolyId(ref, salt, it, ip);
		m_tiles[it].flags[ip] = flags;
	}
	
};

static void floodNavmesh(dtNavMesh* nav, NavmeshFlags* flags, dtPolyRef start, unsigned char flag)
{
	// If already visited, skip.
	if (flags->getFlags(start))
		return;

	flags->setFlags(start, flag);
		
	rdPermVector<dtPolyRef> openList;
	openList.push_back(start);

	while (openList.size())
	{
		const dtPolyRef ref = openList.back();
		openList.pop_back();

		// Get current poly and tile.
		// The API input has been checked already, skip checking internal data.
		const dtMeshTile* tile = 0;
		const dtPoly* poly = 0;
		nav->getTileAndPolyByRefUnsafe(ref, &tile, &poly);

		// Visit linked polygons.
		for (unsigned int i = poly->firstLink; i != DT_NULL_LINK; i = tile->links[i].next)
		{
			const dtPolyRef neiRef = tile->links[i].ref;
			// Skip invalid and already visited.
			if (!neiRef || flags->getFlags(neiRef))
				continue;
			// Mark as visited
			flags->setFlags(neiRef, flag);
			// Visit neighbours
			openList.push_back(neiRef);
		}
	}
}

static void disableUnvisitedPolys(dtNavMesh* nav, NavmeshFlags* flags)
{
	for (int i = 0; i < nav->getTileCount(); ++i)
	{
		const dtMeshTile* tile = nav->getTile(i);
		dtMeshHeader* header = tile->header;

		if (!header) continue;

		const dtPolyRef base = nav->getPolyRefBase(tile);
		int numUnlinkedPolys = 0;

		for (int j = 0; j < header->polyCount; ++j)
		{
			const dtPolyRef ref = base | (unsigned int)j;
			if (!flags->getFlags(ref))
			{
				const dtMeshTile* targetTile;
				dtPoly* targetPoly;

				nav->getTileAndPolyByRefUnsafe(ref, &targetTile, (const dtPoly**)&targetPoly);

				targetPoly->groupId = DT_UNLINKED_POLY_GROUP;
				targetPoly->firstLink = DT_NULL_LINK;
				targetPoly->flags = EDITOR_POLYFLAGS_DISABLED;

				numUnlinkedPolys++;
			}
		}

		if (numUnlinkedPolys == header->polyCount)
		{
			header->userId = DT_FULL_UNLINKED_TILE_USER_ID;
			continue;
		}
	}
}

static void removeUnlinkedTiles(dtNavMesh* nav)
{
	for (int i = nav->getTileCount(); i-- > 0;)
	{
		const dtMeshTile* tile = nav->getTile(i);
		const dtMeshHeader* header = tile->header;

		if (!header) continue;

		if (header->userId == DT_FULL_UNLINKED_TILE_USER_ID)
			nav->removeTile(nav->getTileRef(tile), 0, 0);
	};
}

NavMeshPruneTool::NavMeshPruneTool() :
	m_editor(0),
	m_flags(0),
	m_hitPosSet(false),
	m_ranPruneTool(false)
{
	m_hitPos[0] = 0.0f;
	m_hitPos[1] = 0.0f;
	m_hitPos[2] = 0.0f;
}

NavMeshPruneTool::~NavMeshPruneTool()
{
	delete m_flags;
}

void NavMeshPruneTool::init(Editor* editor)
{
	m_editor = editor;
}

void NavMeshPruneTool::reset()
{
	m_hitPosSet = false;
	m_ranPruneTool = false;
	delete m_flags;
	m_flags = 0;
}

void NavMeshPruneTool::handleMenu()
{
	dtNavMesh* nav = m_editor->getNavMesh();
	if (!nav) return;

	// todo(amos): once tile rebuilding is done, also remove unlinked polygons!
	if (m_ranPruneTool && ImGui::Button("Remove Unlinked Tiles"))
	{
		removeUnlinkedTiles(nav);
		m_ranPruneTool = false;
	}

	if (!m_flags) return;

	if (ImGui::Button("Clear Selection"))
	{
		m_flags->clearAllFlags();
	}
	
	if (ImGui::Button("Prune Unselected"))
	{
		disableUnvisitedPolys(nav, m_flags);
		dtTraverseTableCreateParams params;

		m_editor->createTraverseTableParams(&params);
		m_editor->updateStaticPathingData(&params);

		delete m_flags;
		m_flags = 0;

		m_ranPruneTool = true;
	}
}

void NavMeshPruneTool::handleClick(const float* s, const float* p, bool shift)
{
	rdIgnoreUnused(s);
	rdIgnoreUnused(shift);

	if (!m_editor) return;
	InputGeom* geom = m_editor->getInputGeom();
	if (!geom) return;
	dtNavMesh* nav = m_editor->getNavMesh();
	if (!nav) return;
	dtNavMeshQuery* query = m_editor->getNavMeshQuery();
	if (!query) return;
	
	rdVcopy(m_hitPos, p);
	m_hitPosSet = true;
	
	if (!m_flags)
	{
		m_flags = new NavmeshFlags;
		m_flags->init(nav);
	}
	
	const float halfExtents[3] = { 2, 2, 4 };
	dtQueryFilter filter;
	dtPolyRef ref = 0;
	query->findNearestPoly(p, halfExtents, &filter, &ref, 0);

	floodNavmesh(nav, m_flags, ref, 1);
}

void NavMeshPruneTool::handleToggle()
{
}

void NavMeshPruneTool::handleStep()
{
}

void NavMeshPruneTool::handleUpdate(const float /*dt*/)
{
}

void NavMeshPruneTool::handleRender()
{
	duDebugDraw& dd = m_editor->getDebugDraw();

	if (m_hitPosSet)
	{
		const float s = m_editor->getAgentRadius();
		const unsigned int col = duRGBA(255,255,255,255);
		dd.begin(DU_DRAW_LINES);
		dd.vertex(m_hitPos[0]-s,m_hitPos[1],m_hitPos[2], col);
		dd.vertex(m_hitPos[0]+s,m_hitPos[1],m_hitPos[2], col);
		dd.vertex(m_hitPos[0],m_hitPos[1]-s,m_hitPos[2], col);
		dd.vertex(m_hitPos[0],m_hitPos[1]+s,m_hitPos[2], col);
		dd.vertex(m_hitPos[0],m_hitPos[1],m_hitPos[2]-s, col);
		dd.vertex(m_hitPos[0],m_hitPos[1],m_hitPos[2]+s, col);
		dd.end();
	}

	const dtNavMesh* nav = m_editor->getNavMesh();
	const float* drawOffset = m_editor->getDetourDrawOffset();
	const unsigned int drawFlags = m_editor->getNavMeshDrawFlags();

	if (m_flags && nav)
	{
		for (int i = 0; i < nav->getMaxTiles(); ++i)
		{
			const dtMeshTile* tile = nav->getTile(i);
			if (!tile->header) continue;
			const dtPolyRef base = nav->getPolyRefBase(tile);
			for (int j = 0; j < tile->header->polyCount; ++j)
			{
				const dtPolyRef ref = base | (unsigned int)j;
				if (m_flags->getFlags(ref))
				{
					duDebugDrawNavMeshPoly(&dd, *nav, ref, drawOffset, drawFlags, duRGBA(255,255,255,128));
				}
			}
		}
	}
}

void NavMeshPruneTool::handleRenderOverlay(double* proj, double* model, int* view)
{
	rdIgnoreUnused(model);
	rdIgnoreUnused(proj);
	rdIgnoreUnused(view);

	// Tool help
	ImGui_RenderText(ImGuiTextAlign_e::kAlignLeft, ImVec2(280, 40), ImVec4(1.0f,1.0f,1.0f,0.75f), "LMB: Click fill area.");
}
