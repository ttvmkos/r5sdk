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

#include "Shared/Include/SharedAssert.h"
#include "Shared/Include/SharedCommon.h"
#include "Recast/Include/Recast.h"
#include "Detour/Include/DetourNavMesh.h"
#include "Detour/Include/DetourNavMeshBuilder.h"
#include "DebugUtils/Include/RecastDebugDraw.h"
#include "DebugUtils/Include/DetourDebugDraw.h"
#include "DebugUtils/Include/DetourDump.h"
#include "NavEditor/Include/NavMeshTesterTool.h"
#include "NavEditor/Include/NavMeshPruneTool.h"
#include "NavEditor/Include/OffMeshConnectionTool.h"
#include "NavEditor/Include/ConvexVolumeTool.h"
#include "NavEditor/Include/CrowdTool.h"
#include "NavEditor/Include/InputGeom.h"
#include "NavEditor/Include/Editor.h"
#include "NavEditor/Include/Editor_TileMesh.h"

#include "game/server/ai_navmesh.h"
#include "game/server/ai_hull.h"
#include "coordsize.h"


#ifdef DT_POLYREF64
const static int MAX_POLYREF_CHARS = 22;
#define STR_TO_ID strtoull
#else
const static int MAX_POLYREF_CHARS = 11;
#define STR_TO_ID strtoul
#endif // DT_POLYREF64

class NavMeshTileTool : public EditorTool
{
	Editor_TileMesh* m_editor;
	dtNavMesh* m_navMesh;
	float m_hitPos[3];
	float m_nearestPos[3];
	int m_selectedSide;
	int m_selectedTraverseType;

	dtTileRef m_markedTileRef;
	dtPolyRef m_markedPolyRef;

	enum TileToolCursorMode
	{
		TT_CURSOR_MODE_DEBUG = 0,
		TT_CURSOR_MODE_BUILD
	};

	TileToolCursorMode m_cursorMode;

	enum TextOverlayDrawMode
	{
		TO_DRAW_MODE_DISABLED = -1,
		TO_DRAW_MODE_POLY_FLAGS,
		TO_DRAW_MODE_POLY_GROUPS,
		TO_DRAW_MODE_POLY_SURF_AREAS
	};

	TextOverlayDrawMode m_textOverlayDrawMode;

	enum TextOverlayDrawFlags
	{
		TO_DRAW_FLAGS_NONE = 1 << 0,
		TO_DRAW_FLAGS_INDICES = 1 << 1
	};

	int m_textOverlayDrawFlags;

	char m_tileRefTextInput[MAX_POLYREF_CHARS];
	char m_polyRefTextInput[MAX_POLYREF_CHARS];
	bool m_hitPosSet;
	
public:

	NavMeshTileTool() :
		m_editor(0),
		m_navMesh(0),
		m_selectedSide(-1),
		m_selectedTraverseType(-2),
		m_markedTileRef(0),
		m_markedPolyRef(0),
		m_cursorMode(TT_CURSOR_MODE_DEBUG),
		m_textOverlayDrawMode(TO_DRAW_MODE_DISABLED),
		m_textOverlayDrawFlags(TO_DRAW_FLAGS_NONE),
		m_hitPosSet(false)
	{
		rdVset(m_hitPos, 0.0f,0.0f,0.0f);
		rdVset(m_nearestPos, 0.0f,0.0f,0.0f);
		memset(m_tileRefTextInput, '\0', sizeof(m_tileRefTextInput));
		memset(m_polyRefTextInput, '\0', sizeof(m_polyRefTextInput));
	}

	virtual ~NavMeshTileTool()
	{
	}

	virtual int type() { return TOOL_TILE_EDIT; }

	virtual void init(Editor* editor)
	{
		m_editor = (Editor_TileMesh*)editor;
		m_navMesh = editor->getNavMesh();
	}
	
	virtual void reset() {}

	virtual void handleMenu()
	{
		ImGui::Text("Cursor Mode");
		if (ImGui::RadioButton("Debug##TileTool", m_cursorMode == TT_CURSOR_MODE_DEBUG))
			m_cursorMode = TT_CURSOR_MODE_DEBUG;

		if (ImGui::RadioButton("Build##TileTool", m_cursorMode == TT_CURSOR_MODE_BUILD))
			m_cursorMode = TT_CURSOR_MODE_BUILD;

		ImGui::Separator();
		ImGui::Text("Create Tiles");

		if (ImGui::Button("Create All"))
		{
			if (m_editor)
				m_editor->buildAllTiles();
		}
		if (ImGui::Button("Remove All"))
		{
			if (m_editor)
				m_editor->removeAllTiles();
		}

		ImGui::Separator();
		ImGui::Text("Debug Options");

		if (ImGui::RadioButton("Show Poly Flags", m_textOverlayDrawMode == TO_DRAW_MODE_POLY_FLAGS))
			toggleTextOverlayDrawMode(TO_DRAW_MODE_POLY_FLAGS);

		if (ImGui::RadioButton("Show Poly Groups", m_textOverlayDrawMode == TO_DRAW_MODE_POLY_GROUPS))
			toggleTextOverlayDrawMode(TO_DRAW_MODE_POLY_GROUPS);

		if (ImGui::RadioButton("Show Poly Surface Areas", m_textOverlayDrawMode == TO_DRAW_MODE_POLY_SURF_AREAS))
			toggleTextOverlayDrawMode(TO_DRAW_MODE_POLY_SURF_AREAS);

		if (ImGui::RadioButton("Show Tile And Poly Indices", m_textOverlayDrawFlags & TO_DRAW_FLAGS_INDICES))
			toggleTextOverlayDrawFlags(TO_DRAW_FLAGS_INDICES);


		const bool hasMarker = m_markedTileRef || m_markedPolyRef;

		if (m_navMesh || hasMarker)
		{
			ImGui::Separator();
			ImGui::Text("Markers");
		}

		if (m_navMesh)
		{
			ImGui::PushItemWidth(83);
			if (m_navMesh && ImGui::InputText("Mark Tile By Ref", m_tileRefTextInput, sizeof(m_tileRefTextInput), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				char* pEnd = nullptr;
				m_markedTileRef = (dtPolyRef)STR_TO_ID(m_tileRefTextInput, &pEnd, 10);
			}
			if (m_navMesh && ImGui::InputText("Mark Poly By Ref", m_polyRefTextInput, sizeof(m_polyRefTextInput), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				char* pEnd = nullptr;
				m_markedPolyRef = (dtPolyRef)STR_TO_ID(m_polyRefTextInput, &pEnd, 10);
			}
			ImGui::SliderInt("Tile Side", &m_selectedSide, -1, 8, "%d", ImGuiSliderFlags_NoInput);
			ImGui::PopItemWidth();
		}

		ImGui::PushItemWidth(180);
		ImGui::SliderFloat3("Cursor", m_hitPos, MIN_COORD_FLOAT, MAX_COORD_FLOAT);
		ImGui::PopItemWidth();

		if (hasMarker && ImGui::Button("Clear Markers"))
		{
			m_markedTileRef = 0;
			m_markedPolyRef = 0;
			rdVset(m_nearestPos, 0.0f, 0.0f, 0.0f);
		}

		dtNavMeshQuery* query = m_editor->getNavMeshQuery();

		if (query && m_navMesh)
		{
			ImGui::Separator();
			ImGui::Text("Dumpers");

			ImGui::PushItemWidth(83);
			ImGui::SliderInt("Selected Traverse Type", &m_selectedTraverseType, -2, 31);
			ImGui::PopItemWidth();

			if (ImGui::Button("Dump Traverse Links"))
			{
				const char* modelName = m_editor->getModelName();
				char buf[512];

				if (m_selectedTraverseType == -2)
				{
					for (int i = -1; i < 32; i++)
					{
						snprintf(buf, sizeof(buf), "%s_%d.txt", modelName, i);
						FileIO io;

						if (io.openForWrite(buf))
							duDumpTraverseLinkDetail(*m_navMesh, query, i, &io);
					}
				}
				else
				{
					snprintf(buf, sizeof(buf), "%s_%d.txt", modelName, m_selectedTraverseType);
					FileIO io;

					if (io.openForWrite(buf))
						duDumpTraverseLinkDetail(*m_navMesh, query, m_selectedTraverseType, &io);
				}
			}
		}
	}

	virtual void handleClick(const float* /*s*/, const float* p, bool shift)
	{
		m_hitPosSet = true;
		rdVcopy(m_hitPos,p);
		if (m_editor)
		{
			if (m_cursorMode == TT_CURSOR_MODE_BUILD)
			{
				if (shift)
					m_editor->removeTile(m_hitPos);
				else
					m_editor->buildTile(m_hitPos);
			}
			else if (m_cursorMode == TT_CURSOR_MODE_DEBUG && m_navMesh)
			{
				const float halfExtents[3] = { 2, 2, 4 };
				dtQueryFilter filter;

				if (shift)
				{
					if (dtStatusFailed(m_editor->getNavMeshQuery()->findNearestPoly(m_hitPos, halfExtents, &filter, &m_markedPolyRef, m_nearestPos)))
					{
						m_markedPolyRef = 0;
						rdVset(m_nearestPos, 0.0f, 0.0f, 0.0f);
					}
				}
				else
				{
					int tx, ty;
					m_editor->getTilePos(m_hitPos, tx, ty);
					m_markedTileRef = m_navMesh->getTileRefAt(tx, ty, 0);
				}
			}
		}
	}

	virtual void handleToggle() {}

	virtual void handleStep() {}

	virtual void handleUpdate(const float /*dt*/) {}
	
	virtual void handleRender()
	{
		if (m_hitPosSet)
		{
			const float s = m_editor->getAgentRadius();
			glColor4ub(0,0,0,128);
			glLineWidth(2.0f);
			glBegin(GL_LINES);
			glVertex3f(m_hitPos[0]-s,m_hitPos[1],m_hitPos[2]+0.1f);
			glVertex3f(m_hitPos[0]+s,m_hitPos[1],m_hitPos[2]+0.1f);
			glVertex3f(m_hitPos[0],m_hitPos[1]-s,m_hitPos[2]+0.1f);
			glVertex3f(m_hitPos[0],m_hitPos[1]+s,m_hitPos[2]+0.1f);
			glVertex3f(m_hitPos[0],m_hitPos[1],m_hitPos[2]-s+0.1f);
			glVertex3f(m_hitPos[0],m_hitPos[1],m_hitPos[2]+s+0.1f);
			glEnd();
			glLineWidth(1.0f);
		}

		const float* debugDrawOffset = m_editor->getDetourDrawOffset();

		if (m_markedTileRef && m_editor && m_navMesh)
		{
			const dtMeshTile* tile = m_navMesh->getTileByRef(m_markedTileRef);

			if (tile && tile->header)
			{
				duDrawTraverseLinkParams params;
				duDebugDrawMeshTile(&m_editor->getDebugDraw(), *m_navMesh, 0, tile, debugDrawOffset, m_editor->getNavMeshDrawFlags(), params);

				const int side = (m_selectedSide != -1) 
					? m_selectedSide
					: rdClassifyPointOutsideBounds(m_hitPos, tile->header->bmin, tile->header->bmax);

				if (side != 0xff)
				{
					const int MAX_NEIS = 32; // Max neighbors
					dtMeshTile* neis[MAX_NEIS];

					const int nneis = m_navMesh->getNeighbourTilesAt(tile->header->x, tile->header->y, side, neis, MAX_NEIS);

					for (int i = 0; i < nneis; i++)
					{
						const dtMeshTile* neiTile = neis[i];
						duDebugDrawMeshTile(&m_editor->getDebugDraw(), *m_navMesh, 0, neiTile, debugDrawOffset, m_editor->getNavMeshDrawFlags(), params);
					}
				}
			}
		}

		if (m_markedPolyRef && m_editor && m_navMesh)
		{
			duDebugDrawNavMeshPoly(&m_editor->getDebugDraw(), *m_navMesh, m_markedPolyRef,
				debugDrawOffset, m_editor->getNavMeshDrawFlags(), duRGBA(255, 0, 170, 190), false);
		}

		if (m_markedTileRef || m_markedPolyRef)
			duDebugDrawCross(&m_editor->getDebugDraw(), m_nearestPos[0], m_nearestPos[1], m_nearestPos[2], 20.f, duRGBA(0, 0, 255, 255), 2, debugDrawOffset);
	}
	
	virtual void handleRenderOverlay(double* proj, double* model, int* view)
	{
		GLdouble x, y, z;
		const int h = view[3];
		const float* drawOffset = m_editor->getDetourDrawOffset();

		// NOTE: don't add the render offset here as we want to keep the overlay at the hit position, this
		// way we can have the navmesh on the side and hit a specific location on the input geometry, and
		// see which tile we build as this will be drawn on the hit position, while we can enumerate all
		// the tiles using the debug options in the NavMeshTileTool which will always be aligned with the
		// navmesh.
		if (m_hitPosSet && gluProject((GLdouble)m_hitPos[0], (GLdouble)m_hitPos[1], (GLdouble)m_hitPos[2],
									  model, proj, view, &x, &y, &z))
		{
			int tx=0, ty=0;
			m_editor->getTilePos(m_hitPos, tx, ty);

			ImGui_RenderText(ImGuiTextAlign_e::kAlignCenter, ImVec2((float)x, h-((float)y-25)), ImVec4(0,0,0,0.8f), "(%d,%d)", tx,ty);
		}

		if (m_navMesh && m_textOverlayDrawMode != TO_DRAW_MODE_DISABLED)
		{
			for (int i = 0; i < m_navMesh->getMaxTiles(); i++)
			{
				const dtMeshTile* tile = m_navMesh->getTile(i);
				if (!tile->header) continue;

				for (int j = 0; j < tile->header->polyCount; j++)
				{
					const dtPoly* poly = &tile->polys[j];
					unsigned short value = 0;

					const float* pos;
					if (poly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
					{
						const unsigned int ip = (unsigned int)(poly - tile->polys);
						dtOffMeshConnection* con = &tile->offMeshCons[ip - tile->header->offMeshBase];

						// Render on end position to prevent clutter, because
						// we already render ref positions on the start pos.
						pos = &con->pos[3];
					}
					else
					{
						pos = poly->center;
					}

					switch (m_textOverlayDrawMode)
					{
					case TO_DRAW_MODE_POLY_FLAGS:
						value = poly->flags;
						break;
					case TO_DRAW_MODE_POLY_GROUPS:
						value = poly->groupId;
						break;
					case TO_DRAW_MODE_POLY_SURF_AREAS:
						value = poly->surfaceArea;
						break;
					default:
						// Unhandled text overlay mode.
						rdAssert(0);
					}

					if (gluProject((GLdouble)pos[0]+drawOffset[0], (GLdouble)pos[1]+drawOffset[1], (GLdouble)pos[2]+drawOffset[2]+30,
						model, proj, view, &x, &y, &z))
					{
						const char* format = (m_textOverlayDrawFlags & TO_DRAW_FLAGS_INDICES)
							? "%hu (%d,%d)"
							: "%hu";

						ImGui_RenderText(ImGuiTextAlign_e::kAlignCenter,
							ImVec2((float)x, h - (float)y), ImVec4(0, 0, 0, 0.8f), format, value, i, j);
					}
				}

				//for (int j = 0; j < tile->header->maxCellCount; j++)
				//{
				//	const dtCell* cell = &tile->cells[j];

				//	if (gluProject((GLdouble)cell->pos[0]+drawOffset[0], (GLdouble)cell->pos[1]+drawOffset[1], (GLdouble)cell->pos[2]+drawOffset[2]+30,
				//		model, proj, view, &x, &y, &z))
				//	{
				//		ImGui_RenderText(ImGuiTextAlign_e::kAlignCenter,
				//			ImVec2((float)x, h - (float)y), ImVec4(0, 0.4, 0, 0.8f), "(%d,%d)", j, cell->flags);
				//	}
				//}
			}
		}
		
		// Tool help
		ImGui_RenderText(ImGuiTextAlign_e::kAlignLeft, ImVec2(280, 40), 
			ImVec4(1.0f,1.0f,1.0f,0.75f), "LMB: Rebuild hit tile.  Shift+LMB: Clear hit tile.");
	}

	void toggleTextOverlayDrawMode(const TextOverlayDrawMode drawMode)
	{
		m_textOverlayDrawMode == drawMode
			? m_textOverlayDrawMode = TO_DRAW_MODE_DISABLED
			: m_textOverlayDrawMode = drawMode;
	}

	void toggleTextOverlayDrawFlags(unsigned int flag) { m_textOverlayDrawFlags ^= flag; }
};
#undef STR_TO_ID



Editor_TileMesh::Editor_TileMesh() :
	m_buildAll(true),
	m_maxTiles(0),
	m_maxPolysPerTile(0),
	m_tileBuildTime(0),
	m_tileMemUsage(0),
	m_tileTriCount(0)
{
	memset(m_lastBuiltTileBmin, 0, sizeof(m_lastBuiltTileBmin));
	memset(m_lastBuiltTileBmax, 0, sizeof(m_lastBuiltTileBmax));
	
	setTool(new NavMeshTileTool);

	m_drawActiveTile = true;
}

Editor_TileMesh::~Editor_TileMesh()
{
	cleanup();
	dtFreeNavMesh(m_navMesh);
	m_navMesh = 0;
}

void Editor_TileMesh::handleSettings()
{
	Editor::handleCommonSettings();
	
	ImGui::Text("Tiling");
	ImGui::SliderInt("Min Tile Bits", &m_minTileBits, 14, 32);
	ImGui::SliderInt("Max Tile Bits", &m_maxTileBits, 22, 32);
	ImGui::SliderInt("Tile Size", &m_tileSize, 8, 2048);

	ImGui::Checkbox("Build All Tiles", &m_buildAll);
	ImGui::Checkbox("Keep Intermediate Results", &m_keepInterResults);
	
	EditorCommon_SetAndRenderTileProperties(m_geom, m_minTileBits, m_maxTileBits, m_tileSize, m_cellSize, m_maxTiles, m_maxPolysPerTile);
	
	ImGui::Separator();
	Editor_StaticTileMeshCommon::renderIntermediateTileMeshOptions();
}

void Editor_TileMesh::handleTools()
{
	int type = !m_tool ? TOOL_NONE : m_tool->type();
	bool isEnabled = type == TOOL_NAVMESH_TESTER;

	if (ImGui::Checkbox("Test NavMesh", &isEnabled))
	{
		setTool(new NavMeshTesterTool);
	}

	isEnabled = type == TOOL_NAVMESH_PRUNE;
	if (ImGui::Checkbox("Prune NavMesh", &isEnabled))
	{
		setTool(new NavMeshPruneTool);
	}

	isEnabled = type == TOOL_TILE_EDIT;
	if (ImGui::Checkbox("Create Tiles", &isEnabled))
	{
		setTool(new NavMeshTileTool);
	}

	isEnabled = type == TOOL_OFFMESH_CONNECTION;
	if (ImGui::Checkbox("Create Off-Mesh Links", &isEnabled))
	{
		setTool(new OffMeshConnectionTool);
	}

	isEnabled = type == TOOL_CONVEX_VOLUME;
	if (ImGui::Checkbox("Create Convex Volumes", &isEnabled))
	{
		setTool(new ConvexVolumeTool);
	}

	isEnabled = type == TOOL_CROWD;
	if (ImGui::Checkbox("Create Crowds", &isEnabled))
	{
		setTool(new CrowdTool);
	}
	
	ImGui::Separator();

	ImGui::Indent();

	if (m_tool)
		m_tool->handleMenu();

	ImGui::Unindent();
}

void Editor_TileMesh::handleDebugMode()
{
	Editor::renderMeshOffsetOptions();
	ImGui::Separator();
	Editor_StaticTileMeshCommon::renderRecastDebugMenu();
	ImGui::Separator();
	Editor::renderDetourDebugMenu();
}

void Editor_TileMesh::handleRender()
{
	Editor_StaticTileMeshCommon::renderTileMeshData();
}

void Editor_TileMesh::handleRenderOverlay(double* proj, double* model, int* view)
{
	GLdouble x, y, z;
	const int h = view[3];
	const float* drawOffset = getDetourDrawOffset();

	float projectPos[3];
	rdVset(projectPos, 
		((m_lastBuiltTileBmin[0]+m_lastBuiltTileBmax[0])/2)+drawOffset[0],
		((m_lastBuiltTileBmin[1]+m_lastBuiltTileBmax[1])/2)+drawOffset[1],
		((m_lastBuiltTileBmin[2]+m_lastBuiltTileBmax[2])/2)+drawOffset[2]);
	
	// Draw start and end point labels
	if (m_tileBuildTime > 0.0f && gluProject((GLdouble)projectPos[0], (GLdouble)projectPos[1], (GLdouble)projectPos[2],
											 model, proj, view, &x, &y, &z))
	{
		ImGui_RenderText(ImGuiTextAlign_e::kAlignCenter, ImVec2((float)x, h-(float)(y-25)),
			ImVec4(0,0,0,0.8f), "%.3fms / %dTris / %.1fkB", m_tileBuildTime, m_tileTriCount, m_tileMemUsage);
	}
	
	if (m_tool)
		m_tool->handleRenderOverlay(proj, model, view);
	renderOverlayToolStates(proj, model, view);
}

void Editor_TileMesh::handleMeshChanged(InputGeom* geom)
{
	Editor::handleMeshChanged(geom);

	const BuildSettings* buildSettings = geom->getBuildSettings();
	if (buildSettings && buildSettings->tileSize > 0)
		m_tileSize = buildSettings->tileSize;

	cleanup();

	dtFreeNavMesh(m_navMesh);
	m_navMesh = 0;

	if (m_tool)
	{
		m_tool->reset();
		m_tool->init(this);
	}
	resetToolStates();
	initToolStates(this);
}

bool Editor_TileMesh::handleBuild()
{
	if (!m_geom || !m_geom->getMesh())
	{
		m_ctx->log(RC_LOG_ERROR, "buildTiledNavigation: No vertices and triangles.");
		return false;
	}
	
	dtFreeNavMesh(m_navMesh);
	
	m_navMesh = dtAllocNavMesh();
	if (!m_navMesh)
	{
		m_ctx->log(RC_LOG_ERROR, "buildTiledNavigation: Could not allocate navmesh.");
		return false;
	}

	m_loadedNavMeshType = m_selectedNavMeshType;
	m_traverseLinkDrawParams.traverseAnimType = -2;

	dtNavMeshParams params;
	rdVcopy(params.orig, m_geom->getNavMeshBoundsMin());

	params.orig[0] = m_geom->getNavMeshBoundsMax()[0];

	params.tileWidth = m_tileSize*m_cellSize;
	params.tileHeight = m_tileSize*m_cellSize;
	params.maxTiles = m_maxTiles;
	params.maxPolys = m_maxPolysPerTile;
	params.polyGroupCount = 0;
	params.traverseTableSize = 0;
	params.traverseTableCount = 0;
#if DT_NAVMESH_SET_VERSION >= 8
	params.magicDataCount = 0;
#endif
	
	dtStatus status;
	
	status = m_navMesh->init(&params);
	if (dtStatusFailed(status))
	{
		m_ctx->log(RC_LOG_ERROR, "buildTiledNavigation: Could not init Detour navmesh.");
		return false;
	}
	
	status = m_navQuery->init(m_navMesh, 2048);
	if (dtStatusFailed(status))
	{
		m_ctx->log(RC_LOG_ERROR, "buildTiledNavigation: Could not init Detour navmesh query");
		return false;
	}
	
	if (m_buildAll)
		buildAllTiles();
	
	if (m_tool)
		m_tool->init(this);
	initToolStates(this);

	return true;
}

void Editor_TileMesh::collectSettings(BuildSettings& settings)
{
	Editor::collectSettings(settings);

	settings.tileSize = m_tileSize;
}

void Editor_TileMesh::buildTile(const float* pos)
{
	if (!m_geom) return;
	if (!m_navMesh) return;
			
	int tx, ty;
	getTilePos(pos, tx, ty);
	getTileExtents(tx, ty, m_lastBuiltTileBmin, m_lastBuiltTileBmax);
	
	m_tileCol = duRGBA(255,255,255,64);
	
	m_ctx->resetLog();
	
	int dataSize = 0;
	unsigned char* data = buildTileMesh(tx, ty, m_lastBuiltTileBmin, m_lastBuiltTileBmax, dataSize);

	// Remove any previous data (navmesh owns and deletes the data).
	m_navMesh->removeTile(m_navMesh->getTileRefAt(tx,ty,0),0,0);

	// Add tile, or leave the location empty.
	if (data)
	{
		const dtMeshHeader* header = (const dtMeshHeader*)data;

		// Let the navmesh own the data.
		dtTileRef tileRef = 0;
		dtStatus status = m_navMesh->addTile(data,dataSize,DT_TILE_FREE_DATA,0,&tileRef);
		bool failure = false;

		if (dtStatusFailed(status) || dtStatusFailed(m_navMesh->connectTile(tileRef)))
		{
			rdFree(data);
			failure = true;
		}
		else if (header->offMeshConCount)
		{
			m_navMesh->baseOffMeshLinks(tileRef);
			m_navMesh->connectExtOffMeshLinks(tileRef);
		}

		if (!failure)
		{
			// If there are external off-mesh links landing on
			// this tile, connect them.
			for (int i = 0; i < m_navMesh->getTileCount(); i++)
			{
				dtMeshTile* target = m_navMesh->getTile(i);
				const dtTileRef targetRef = m_navMesh->getTileRef(target);

				// Connection to self has already been done above.
				if (targetRef == tileRef)
					continue;

				const dtMeshHeader* targetHeader = target->header;

				if (!targetHeader)
					continue;

				for (int j = 0; j < targetHeader->offMeshConCount; j++)
				{
					const dtOffMeshConnection* con = &target->offMeshCons[j];

					int landTx, landTy;
					getTilePos(&con->pos[3], landTx, landTy);

					if (landTx == tx && landTy == ty)
						m_navMesh->connectExtOffMeshLinks(targetRef);
				}
			}

			// Reconnect the traverse links.
			dtTraverseLinkConnectParams params;
			createTraverseLinkParams(params);

			params.linkToNeighbor = false;
			m_navMesh->connectTraverseLinks(tileRef, params);
			params.linkToNeighbor = true;
			m_navMesh->connectTraverseLinks(tileRef, params);

			buildStaticPathingData();
		}
	}
	
	m_ctx->dumpLog("Build Tile (%d,%d):", tx,ty);
}

void Editor_TileMesh::getTileExtents(int tx, int ty, float* tmin, float* tmax)
{
	const float ts = m_tileSize * m_cellSize;
	const float* bmin = m_geom->getNavMeshBoundsMin();
	const float* bmax = m_geom->getNavMeshBoundsMax();
	tmin[0] = bmax[0] - (tx+1)*ts;
	tmin[1] = bmin[1] + (ty)*ts;
	tmin[2] = bmin[2];

	tmax[0] = bmax[0] - (tx)*ts;
	tmax[1] = bmin[1] + (ty+1)*ts;
	tmax[2] = bmax[2];
}
void Editor_TileMesh::getTilePos(const float* pos, int& tx, int& ty)
{
	if (!m_geom) return;
	
	const float* bmin = m_geom->getNavMeshBoundsMin();
	const float* bmax = m_geom->getNavMeshBoundsMax();

	const float ts = m_tileSize*m_cellSize;
	tx = (int)((bmax[0]- pos[0]) / ts);
	ty = (int)((pos[1] - bmin[1]) / ts);
}

void Editor_TileMesh::removeTile(const float* pos)
{
	if (!m_geom) return;
	if (!m_navMesh) return;
	
	int tx, ty;
	getTilePos(pos, tx, ty);
	getTileExtents(tx, ty, m_lastBuiltTileBmin, m_lastBuiltTileBmax);
	
	m_tileCol = duRGBA(255,0,0,180);
	const dtTileRef tileRef = m_navMesh->getTileRefAt(tx,ty, 0);

	if (dtStatusSucceed(m_navMesh->removeTile(tileRef, 0, 0)))
	{
		// Update traverse link map so the next time we rebuild this
		// tile, the polygon pairs will be marked as available.
		const unsigned int tileId = m_navMesh->decodePolyIdTile(tileRef);

		for (auto it = m_traverseLinkPolyMap.cbegin(); it != m_traverseLinkPolyMap.cend();)
		{
			const TraverseLinkPolyPair& pair = it->first;

			if (m_navMesh->decodePolyIdTile(pair.poly1) == tileId || 
				m_navMesh->decodePolyIdTile(pair.poly2) == tileId)
			{
				it = m_traverseLinkPolyMap.erase(it);
				continue;
			}

			++it;
		}

		buildStaticPathingData();
	}
}

void Editor_TileMesh::buildAllTiles()
{
	if (!m_geom) return;
	if (!m_navMesh) return;
	
	const float* bmin = m_geom->getNavMeshBoundsMin();
	const float* bmax = m_geom->getNavMeshBoundsMax();
	int gw = 0, gh = 0;
	rcCalcGridSize(bmin, bmax, m_cellSize, &gw, &gh);
	const int ts = m_tileSize;
	const int tw = (gw + ts-1) / ts;
	const int th = (gh + ts-1) / ts;
	
	// Start the build process.
	m_ctx->startTimer(RC_TIMER_TEMP);

	for (int y = 0; y < th; ++y)
	{
		for (int x = 0; x < tw; ++x)
		{
			getTileExtents(x, y, m_lastBuiltTileBmin, m_lastBuiltTileBmax);
			
			int dataSize = 0;
			unsigned char* data = buildTileMesh(x, y, m_lastBuiltTileBmin, m_lastBuiltTileBmax, dataSize);
			if (data)
			{
				// Remove any previous data (navmesh owns and deletes the data).
				m_navMesh->removeTile(m_navMesh->getTileRefAt(x,y,0),0,0);
				// Let the navmesh own the data.

				dtTileRef tileRef = 0;
				dtStatus status = m_navMesh->addTile(data,dataSize,DT_TILE_FREE_DATA,0,&tileRef);
				if (dtStatusFailed(status))
					rdFree(data);
				else
					m_navMesh->connectTile(tileRef);
			}
		}
	}

	connectOffMeshLinks();
	createTraverseLinks();

	buildStaticPathingData();
	
	// Start the build process.	
	m_ctx->stopTimer(RC_TIMER_TEMP);

	m_totalBuildTimeMs = m_ctx->getAccumulatedTime(RC_TIMER_TEMP)/1000.0f;
	m_tileCol = duRGBA(0,0,0,64);
}

void Editor_TileMesh::removeAllTiles()
{
	if (!m_geom || !m_navMesh)
		return;

	const float* bmin = m_geom->getNavMeshBoundsMin();
	const float* bmax = m_geom->getNavMeshBoundsMax();
	int gw = 0, gh = 0;
	rcCalcGridSize(bmin, bmax, m_cellSize, &gw, &gh);
	const int ts = m_tileSize;
	const int tw = (gw + ts-1) / ts;
	const int th = (gh + ts-1) / ts;
	
	for (int y = 0; y < th; ++y)
		for (int x = 0; x < tw; ++x)
			m_navMesh->removeTile(m_navMesh->getTileRefAt(x,y,0),0,0);

	m_traverseLinkPolyMap.clear();
	buildStaticPathingData();
}

void Editor_TileMesh::buildAllHulls()
{
	for (const hulldef& h : hulls)
	{
		m_agentRadius = h.radius;
		m_agentMaxClimb = h.climbHeight;
		m_agentHeight = h.height;
		m_navmeshName = h.name;
		m_tileSize = h.tileSize;

		m_ctx->resetLog();

		handleSettings();
		handleBuild();

		m_ctx->dumpLog("Build log %s:", h.name);
		Editor::saveAll(m_modelName.c_str(), m_navMesh);
	}
}

unsigned char* Editor_TileMesh::buildTileMesh(const int tx, const int ty, const float* bmin, const float* bmax, int& dataSize)
{
	if (!m_geom || !m_geom->getMesh() || !m_geom->getChunkyMesh())
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Input mesh is not specified.");
		return 0;
	}
	
	m_tileMemUsage = 0;
	m_tileBuildTime = 0;
	
	cleanup();
	
	const float* verts = m_geom->getMesh()->getVerts();
	const int nverts = m_geom->getMesh()->getVertCount();
	const int ntris = m_geom->getMesh()->getTriCount();
	const rcChunkyTriMesh* chunkyMesh = m_geom->getChunkyMesh();
		
	// Init build configuration from GUI
	memset(&m_cfg, 0, sizeof(m_cfg));
	m_cfg.cs = m_cellSize;
	m_cfg.ch = m_cellHeight;
	m_cfg.walkableSlopeAngle = m_agentMaxSlope;
	m_cfg.walkableHeight = (int)ceilf(m_agentHeight / m_cfg.ch);
	m_cfg.walkableClimb = (int)floorf(m_agentMaxClimb / m_cfg.ch);
	m_cfg.walkableRadius = (int)ceilf(m_agentRadius / m_cfg.cs);
	m_cfg.maxEdgeLen = (int)(m_edgeMaxLen / m_cellSize);
	m_cfg.maxSimplificationError = m_edgeMaxError;
	m_cfg.minRegionArea = rdSqr(m_regionMinSize);		// Note: area = size*size
	m_cfg.mergeRegionArea = rdSqr(m_regionMergeSize);	// Note: area = size*size
	m_cfg.maxVertsPerPoly = (int)m_vertsPerPoly;
	m_cfg.tileSize = m_tileSize;
	m_cfg.borderSize = m_cfg.walkableRadius + 3; // Reserve enough padding.
	m_cfg.width = m_cfg.tileSize + m_cfg.borderSize*2;
	m_cfg.height = m_cfg.tileSize + m_cfg.borderSize*2;
	m_cfg.detailSampleDist = m_detailSampleDist < 0.9f ? 0 : m_cellSize * m_detailSampleDist;
	m_cfg.detailSampleMaxError = m_cellHeight * m_detailSampleMaxError;
	
	// Expand the heighfield bounding box by border size to find the extents of geometry we need to build this tile.
	//
	// This is done in order to make sure that the navmesh tiles connect correctly at the borders,
	// and the obstacles close to the border work correctly with the dilation process.
	// No polygons (or contours) will be created on the border area.
	//
	// IMPORTANT!
	//
	//   :''''''''':
	//   : +-----+ :
	//   : |     | :
	//   : |     |<--- tile to build
	//   : |     | :  
	//   : +-----+ :<-- geometry needed
	//   :.........:
	//
	// You should use this bounding box to query your input geometry.
	//
	// For example if you build a navmesh for terrain, and want the navmesh tiles to match the terrain tile size
	// you will need to pass in data from neighbour terrain tiles too! In a simple case, just pass in all the 8 neighbours,
	// or use the bounding box below to only pass in a sliver of each of the 8 neighbours.
	rdVcopy(m_cfg.bmin, bmin);
	rdVcopy(m_cfg.bmax, bmax);
	m_cfg.bmin[0] -= m_cfg.borderSize*m_cfg.cs;
	m_cfg.bmin[1] -= m_cfg.borderSize*m_cfg.cs;
	m_cfg.bmax[0] += m_cfg.borderSize*m_cfg.cs;
	m_cfg.bmax[1] += m_cfg.borderSize*m_cfg.cs;
	
	// Reset build times gathering.
	m_ctx->resetTimers();
	
	// Start the build process.
	m_ctx->startTimer(RC_TIMER_TOTAL);
	
	m_ctx->log(RC_LOG_PROGRESS, "Building navigation:");
	m_ctx->log(RC_LOG_PROGRESS, " - %d x %d cells", m_cfg.width, m_cfg.height);
	m_ctx->log(RC_LOG_PROGRESS, " - %.1fK verts, %.1fK tris", nverts/1000.0f, ntris/1000.0f);
	
	// Allocate voxel heightfield where we rasterize our input data to.
	m_solid = rcAllocHeightfield();
	if (!m_solid)
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'solid'.");
		return 0;
	}
	if (!rcCreateHeightfield(m_ctx, *m_solid, m_cfg.width, m_cfg.height, m_cfg.bmin, m_cfg.bmax, m_cfg.cs, m_cfg.ch))
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not create solid heightfield.");
		return 0;
	}
	
	// Allocate array that can hold triangle flags.
	// If you have multiple meshes you need to process, allocate
	// an array which can hold the max number of triangles you need to process.
	m_triareas = new unsigned char[chunkyMesh->maxTrisPerChunk];
	if (!m_triareas)
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'm_triareas' (%d).", chunkyMesh->maxTrisPerChunk);
		return 0;
	}
	
	float tbmin[2], tbmax[2];
	tbmin[0] = m_cfg.bmin[0];
	tbmin[1] = m_cfg.bmin[1];
	tbmax[0] = m_cfg.bmax[0];
	tbmax[1] = m_cfg.bmax[1];
#if 0 //NOTE(warmist): original algo
	int cid[2048];// TODO: Make grow when returning too many items.
	const int ncid = rcGetChunksOverlappingRect(chunkyMesh, tbmin, tbmax, cid, 2048);
	if (!ncid)
		return 0;
	
	m_tileTriCount = 0;
	
	for (int i = 0; i < ncid; ++i)
	{
		const rcChunkyTriMeshNode& node = chunkyMesh->nodes[cid[i]];
		const int* ctris = &chunkyMesh->tris[node.i*3];
		const int nctris = node.n;
		
		m_tileTriCount += nctris;
		
		memset(m_triareas, 0, nctris*sizeof(unsigned char));
		rcMarkWalkableTriangles(m_ctx, m_cfg.walkableSlopeAngle,
								verts, nverts, ctris, nctris, m_triareas);
		
		if (!rcRasterizeTriangles(m_ctx, verts, nverts, ctris, m_triareas, nctris, *m_solid, m_cfg.walkableClimb))
			return 0;
	}
#else //NOTE(warmist): algo with limited return but can be reinvoked to continue the query
	int cid[1024];//NOTE: we don't grow it but we reuse it (e.g. like a yieldable function or iterator or sth)
	int currentNode = 0;

	bool done = false;
	m_tileTriCount = 0;
	do{
		int currentCount = 0;
		done=rcGetChunksOverlappingRect(chunkyMesh, tbmin, tbmax, cid, 1024,currentCount,currentNode);
		for (int i = 0; i < currentCount; ++i)
		{
			const rcChunkyTriMeshNode& node = chunkyMesh->nodes[cid[i]];
			const int* ctris = &chunkyMesh->tris[node.i*3];
			const int nctris = node.n;

			m_tileTriCount += nctris;

			memset(m_triareas, 0, nctris * sizeof(unsigned char));
			rcMarkWalkableTriangles(m_ctx, m_cfg.walkableSlopeAngle,
				verts, nverts, ctris, nctris, m_triareas);

			if (!rcRasterizeTriangles(m_ctx, verts, nverts, ctris, m_triareas, nctris, *m_solid, m_cfg.walkableClimb))
				return 0;
		}
	} while (!done);

	if (m_tileTriCount == 0)
		return 0;
#endif
	if (!m_keepInterResults)
	{
		delete [] m_triareas;
		m_triareas = 0;
	}
	
	// Once all geometry is rasterized, we do initial pass of filtering to
	// remove unwanted overhangs caused by the conservative rasterization
	// as well as filter spans where the character cannot possibly stand.
	if (m_filterLowHangingObstacles)
		rcFilterLowHangingWalkableObstacles(m_ctx, m_cfg.walkableClimb, *m_solid);
	if (m_filterLedgeSpans)
		rcFilterLedgeSpans(m_ctx, m_cfg.walkableHeight, m_cfg.walkableClimb, *m_solid);
	if (m_filterWalkableLowHeightSpans)
		rcFilterWalkableLowHeightSpans(m_ctx, m_cfg.walkableHeight, *m_solid);
	
	// Compact the heightfield so that it is faster to handle from now on.
	// This will result more cache coherent data as well as the neighbours
	// between walkable cells will be calculated.
	m_chf = rcAllocCompactHeightfield();
	if (!m_chf)
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'chf'.");
		return 0;
	}
	if (!rcBuildCompactHeightfield(m_ctx, m_cfg.walkableHeight, m_cfg.walkableClimb, *m_solid, *m_chf))
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build compact data.");
		return 0;
	}
	
	if (!m_keepInterResults)
	{
		rcFreeHeightField(m_solid);
		m_solid = 0;
	}

	// Erode the walkable area by agent radius.
	if (!rcErodeWalkableArea(m_ctx, m_cfg.walkableRadius, *m_chf))
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not erode.");
		return 0;
	}

	// (Optional) Mark areas.
	const ConvexVolume* vols = m_geom->getConvexVolumes();
	for (int i  = 0; i < m_geom->getConvexVolumeCount(); ++i)
		rcMarkConvexPolyArea(m_ctx, vols[i].verts, vols[i].nverts, vols[i].hmin, vols[i].hmax, (unsigned short)vols[i].flags, (unsigned char)vols[i].area, *m_chf);
	
	
	// Partition the heightfield so that we can use simple algorithm later to triangulate the walkable areas.
	// There are 3 partitioning methods, each with some pros and cons:
	// 1) Watershed partitioning
	//   - the classic Recast partitioning
	//   - creates the nicest tessellation
	//   - usually slowest
	//   - partitions the heightfield into nice regions without holes or overlaps
	//   - the are some corner cases where this method creates produces holes and overlaps
	//      - holes may appear when a small obstacles is close to large open area (triangulation can handle this)
	//      - overlaps may occur if you have narrow spiral corridors (i.e stairs), this make triangulation to fail
	//   * generally the best choice if you precompute the navmesh, use this if you have large open areas
	// 2) Monotone partitioning
	//   - fastest
	//   - partitions the heightfield into regions without holes and overlaps (guaranteed)
	//   - creates long thin polygons, which sometimes causes paths with detours
	//   * use this if you want fast navmesh generation
	// 3) Layer partitioning
	//   - quite fast
	//   - partitions the heighfield into non-overlapping regions
	//   - relies on the triangulation code to cope with holes (thus slower than monotone partitioning)
	//   - produces better triangles than monotone partitioning
	//   - does not have the corner cases of watershed partitioning
	//   - can be slow and create a bit ugly tessellation (still better than monotone)
	//     if you have large open areas with small obstacles (not a problem if you use tiles)
	//   * good choice to use for tiled navmesh with medium and small sized tiles
	
	if (m_partitionType == EDITOR_PARTITION_WATERSHED)
	{
		// Prepare for region partitioning, by calculating distance field along the walkable surface.
		if (!rcBuildDistanceField(m_ctx, *m_chf))
		{
			m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build distance field.");
			return 0;
		}
		
		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildRegions(m_ctx, *m_chf, m_cfg.borderSize, m_cfg.minRegionArea, m_cfg.mergeRegionArea))
		{
			m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build watershed regions.");
			return 0;
		}
	}
	else if (m_partitionType == EDITOR_PARTITION_MONOTONE)
	{
		// Partition the walkable surface into simple regions without holes.
		// Monotone partitioning does not need distancefield.
		if (!rcBuildRegionsMonotone(m_ctx, *m_chf, m_cfg.borderSize, m_cfg.minRegionArea, m_cfg.mergeRegionArea))
		{
			m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build monotone regions.");
			return 0;
		}
	}
	else // EDITOR_PARTITION_LAYERS
	{
		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildLayerRegions(m_ctx, *m_chf, m_cfg.borderSize, m_cfg.minRegionArea))
		{
			m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build layer regions.");
			return 0;
		}
	}
	 	
	// Create contours.
	m_cset = rcAllocContourSet();
	if (!m_cset)
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'cset'.");
		return 0;
	}
	if (!rcBuildContours(m_ctx, *m_chf, m_cfg.maxSimplificationError, m_cfg.maxEdgeLen, *m_cset))
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not create contours.");
		return 0;
	}

	if (m_cset->nconts == 0)
	{
		return 0;
	}
	
	// Build polygon navmesh from the contours.
	m_pmesh = rcAllocPolyMesh();
	if (!m_pmesh)
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'pmesh'.");
		return 0;
	}
	if (!rcBuildPolyMesh(m_ctx, *m_cset, m_cfg.maxVertsPerPoly, *m_pmesh))
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not triangulate contours.");
		return 0;
	}
	
	// Build detail mesh.
	m_dmesh = rcAllocPolyMeshDetail();
	if (!m_dmesh)
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'dmesh'.");
		return 0;
	}
	
	//rcFlipPolyMesh(*m_pmesh);
	if (!rcBuildPolyMeshDetail(m_ctx, *m_pmesh, *m_chf,
							   m_cfg.detailSampleDist, m_cfg.detailSampleMaxError,
							   *m_dmesh))
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build polymesh detail.");
		return 0;
	}
	
	//rcFlipPolyMeshDetail(*m_dmesh,m_pmesh->nverts);
	if (!m_keepInterResults)
	{
		rcFreeCompactHeightfield(m_chf);
		m_chf = 0;
		rcFreeContourSet(m_cset);
		m_cset = 0;
	}
	
	unsigned char* navData = 0;
	int navDataSize = 0;
	if (m_cfg.maxVertsPerPoly <= DT_VERTS_PER_POLYGON)
	{
		if (m_pmesh->nverts >= 0xffff)
		{
			// The vertex indices are ushorts, and cannot point to more than 0xffff vertices.
			m_ctx->log(RC_LOG_ERROR, "Too many vertices per tile %d (max: %d).", m_pmesh->nverts, 0xffff);
			return 0;
		}
		
		// Update poly flags from areas.
		for (int i = 0; i < m_pmesh->npolys; ++i)
		{
			if (m_pmesh->areas[i] == RC_WALKABLE_AREA)
				m_pmesh->areas[i] = EDITOR_POLYAREA_GROUND;
			
			if (m_pmesh->areas[i] == EDITOR_POLYAREA_GROUND
				//||
				//m_pmesh->areas[i] == EDITOR_POLYAREA_GRASS ||
				//m_pmesh->areas[i] == EDITOR_POLYAREA_ROAD
				)
			{
				m_pmesh->flags[i] |= EDITOR_POLYFLAGS_WALK;
			}
			//else if (m_pmesh->areas[i] == EDITOR_POLYAREA_WATER)
			//{
			//	m_pmesh->flags[i] = EDITOR_POLYFLAGS_SWIM;
			//}
			else if (m_pmesh->areas[i] == EDITOR_POLYAREA_TRIGGER)
			{
				m_pmesh->flags[i] |= EDITOR_POLYFLAGS_WALK /*| EDITOR_POLYFLAGS_DOOR*/;
			}

			if (m_pmesh->surfa[i] <= NAVMESH_SMALL_POLYGON_THRESHOLD)
				m_pmesh->flags[i] |= EDITOR_POLYFLAGS_TOO_SMALL;

			const int nvp = m_pmesh->nvp;
			const unsigned short* p = &m_pmesh->polys[i*nvp*2];

			// If polygon connects to a polygon on a neighbouring tile, flag it.
			for (int j = 0; j < nvp; ++j)
			{
				if (p[j] == RD_MESH_NULL_IDX)
					break;
				if ((p[nvp+j] & 0x8000) == 0)
					continue;
				if ((p[nvp+j] & 0xf) == 0xf)
					continue;

				m_pmesh->flags[i] |= EDITOR_POLYFLAGS_HAS_NEIGHBOUR;
			}
		}
		
		dtNavMeshCreateParams params;
		memset(&params, 0, sizeof(params));
		params.verts = m_pmesh->verts;
		params.vertCount = m_pmesh->nverts;
		params.polys = m_pmesh->polys;
		params.polyFlags = m_pmesh->flags;
		params.polyAreas = m_pmesh->areas;
		params.surfAreas = m_pmesh->surfa;
		params.polyCount = m_pmesh->npolys;
		params.nvp = m_pmesh->nvp;
		params.cellResolution = m_polyCellRes;
		params.detailMeshes = m_dmesh->meshes;
		params.detailVerts = m_dmesh->verts;
		params.detailVertsCount = m_dmesh->nverts;
		params.detailTris = m_dmesh->tris;
		params.detailTriCount = m_dmesh->ntris;
		params.offMeshConVerts = m_geom->getOffMeshConnectionVerts();
		params.offMeshConRefPos = m_geom->getOffMeshConnectionRefPos();
		params.offMeshConRad = m_geom->getOffMeshConnectionRads();
		params.offMeshConRefYaw = m_geom->getOffMeshConnectionRefYaws();
		params.offMeshConDir = m_geom->getOffMeshConnectionDirs();
		params.offMeshConJumps = m_geom->getOffMeshConnectionJumps();
		params.offMeshConOrders = m_geom->getOffMeshConnectionOrders();
		params.offMeshConAreas = m_geom->getOffMeshConnectionAreas();
		params.offMeshConFlags = m_geom->getOffMeshConnectionFlags();
		params.offMeshConUserID = m_geom->getOffMeshConnectionId();
		params.offMeshConCount = m_geom->getOffMeshConnectionCount();
		params.walkableHeight = m_agentHeight;
		params.walkableRadius = m_agentRadius;
		params.walkableClimb = m_agentMaxClimb;
		params.tileX = tx;
		params.tileY = ty;
		params.tileLayer = 0;
		rdVcopy(params.bmin, m_pmesh->bmin);
		rdVcopy(params.bmax, m_pmesh->bmax);
		params.cs = m_cfg.cs;
		params.ch = m_cfg.ch;
		params.buildBvTree = m_buildBvTree;

		const bool navMeshBuildSuccess = dtCreateNavMeshData(&params, &navData, &navDataSize);

		// Restore poly areas.
		for (int i = 0; i < m_pmesh->npolys; ++i)
		{
			// The game's poly area (ground) shares the same value as
			// RC_NULL_AREA, if we try to render the recast polymesh cache
			// without restoring this, the renderer will draw it as NULL area
			// even though it's walkable. The other values will get color ID'd
			// by the renderer so we don't need to check on those.
			if (m_pmesh->areas[i] == EDITOR_POLYAREA_GROUND)
				m_pmesh->areas[i] = RC_WALKABLE_AREA;
		}

		if (!navMeshBuildSuccess)
		{
			m_ctx->log(RC_LOG_ERROR, "Could not build Detour navmesh.");
			return 0;
		}
	}
	m_tileMemUsage = navDataSize/1024.0f;
	
	m_ctx->stopTimer(RC_TIMER_TOTAL);
	
	// Show performance stats.
	duLogBuildTimes(*m_ctx, m_ctx->getAccumulatedTime(RC_TIMER_TOTAL));
	m_ctx->log(RC_LOG_PROGRESS, ">> Polymesh: %d vertices  %d polygons", m_pmesh->nverts, m_pmesh->npolys);
	
	m_tileBuildTime = m_ctx->getAccumulatedTime(RC_TIMER_TOTAL)/1000.0f;

	dataSize = navDataSize;
	return navData;
}
