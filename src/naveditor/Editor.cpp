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
#include "Shared/Include/SharedAssert.h"
#include "Shared/Include/SharedCommon.h"
#include "Detour/Include/DetourNavMesh.h"
#include "Detour/Include/DetourNavMeshQuery.h"
#include "Detour/Include/DetourNavMeshBuilder.h"
#include "DetourCrowd/Include/DetourCrowd.h"
#include "DebugUtils/Include/RecastDebugDraw.h"
#include "DebugUtils/Include/DetourDebugDraw.h"
#include "NavEditor/Include/GameUtils.h"
#include "NavEditor/Include/InputGeom.h"
#include "NavEditor/Include/Editor.h"

#include "game/server/ai_navmesh.h"
#include "game/server/ai_hull.h"
#include "coordsize.h"

unsigned int EditorDebugDraw::areaToCol(unsigned int area)
{
	switch(area)
	{
	// Ground : light blue
	case EDITOR_POLYAREA_GROUND: return duRGBA(0, 192, 215, 255);
	// Jump : purple
	case EDITOR_POLYAREA_JUMP: return duRGBA(255, 0, 255, 255);
	// Trigger : light green
	case EDITOR_POLYAREA_TRIGGER: return duRGBA(20, 245, 0, 255);
	// Unexpected : white
	default: return duRGBA(255, 255, 255, 255);
	}
}

Editor::Editor() :
	m_geom(0),
	m_navMesh(0),
	m_navQuery(0),
	m_crowd(0),
	m_navMeshDrawFlags(
		DU_DRAWNAVMESH_OFFMESHCONS|DU_DRAWNAVMESH_WITH_CLOSED_LIST|
		DU_DRAWNAVMESH_POLY_BOUNDS_OUTER|DU_DRAWNAVMESH_ALPHA),
	m_filterLowHangingObstacles(true),
	m_filterLedgeSpans(true),
	m_filterWalkableLowHeightSpans(true),
	m_selectedNavMeshType(NAVMESH_SMALL),
	m_loadedNavMeshType(NAVMESH_SMALL),
	m_navmeshName(NavMesh_GetNameForType(NAVMESH_SMALL)),
	m_tool(0),
	m_ctx(0)
{
	resetCommonSettings();
	m_navQuery = dtAllocNavMeshQuery();
	m_crowd = dtAllocCrowd();

	for (int i = 0; i < MAX_TOOLS; i++)
		m_toolStates[i] = 0;

	rdVset(m_recastDrawOffset, 0.0f,0.0f,4.0f);
	rdVset(m_detourDrawOffset, 0.0f,0.0f,8.0f);
}

Editor::~Editor()
{
	dtFreeNavMeshQuery(m_navQuery);
	dtFreeNavMesh(m_navMesh);
	dtFreeCrowd(m_crowd);
	delete m_tool;
	for (int i = 0; i < MAX_TOOLS; i++)
		delete m_toolStates[i];
}

void Editor::setTool(EditorTool* tool)
{
	delete m_tool;
	m_tool = tool;
	if (tool)
		m_tool->init(this);
}

void Editor::handleSettings()
{
}

void Editor::handleTools()
{
}

void Editor::handleDebugMode()
{
}

void Editor::handleRender()
{
	if (!m_geom)
		return;
	
	// Draw mesh
	duDebugDrawTriMesh(&m_dd, m_geom->getMesh()->getVerts(), m_geom->getMesh()->getVertCount(),
					   m_geom->getMesh()->getTris(), m_geom->getMesh()->getNormals(), m_geom->getMesh()->getTriCount(), 0, 1.0f, nullptr);
	// Draw bounds
	const float* bmin = m_geom->getMeshBoundsMin();
	const float* bmax = m_geom->getMeshBoundsMax();
	duDebugDrawBoxWire(&m_dd, bmin[0],bmin[1],bmin[2], bmax[0],bmax[1],bmax[2], duRGBA(255,255,255,128), 1.0f, nullptr);
}

void Editor::handleRenderOverlay(double* /*proj*/, double* /*model*/, int* /*view*/)
{
}

void Editor::handleMeshChanged(InputGeom* geom)
{
	m_geom = geom;

	const BuildSettings* buildSettings = geom->getBuildSettings();
	if (buildSettings)
	{
		m_cellSize = buildSettings->cellSize;
		m_cellHeight = buildSettings->cellHeight;
		m_agentHeight = buildSettings->agentHeight;
		m_agentRadius = buildSettings->agentRadius;
		m_agentMaxClimb = buildSettings->agentMaxClimb;
		m_agentMaxSlope = buildSettings->agentMaxSlope;
		m_regionMinSize = buildSettings->regionMinSize;
		m_regionMergeSize = buildSettings->regionMergeSize;
		m_edgeMaxLen = buildSettings->edgeMaxLen;
		m_edgeMaxError = buildSettings->edgeMaxError;
		m_vertsPerPoly = buildSettings->vertsPerPoly;
		m_polyCellRes = buildSettings->polyCellRes;
		m_detailSampleDist = buildSettings->detailSampleDist;
		m_detailSampleMaxError = buildSettings->detailSampleMaxError;
		m_partitionType = buildSettings->partitionType;
	}
}

void Editor::collectSettings(BuildSettings& settings)
{
	settings.cellSize = m_cellSize;
	settings.cellHeight = m_cellHeight;
	settings.agentHeight = m_agentHeight;
	settings.agentRadius = m_agentRadius;
	settings.agentMaxClimb = m_agentMaxClimb;
	settings.agentMaxSlope = m_agentMaxSlope;
	settings.regionMinSize = m_regionMinSize;
	settings.regionMergeSize = m_regionMergeSize;
	settings.edgeMaxLen = m_edgeMaxLen;
	settings.edgeMaxError = m_edgeMaxError;
	settings.vertsPerPoly = m_vertsPerPoly;
	settings.polyCellRes = m_polyCellRes;
	settings.detailSampleDist = m_detailSampleDist;
	settings.detailSampleMaxError = m_detailSampleMaxError;
	settings.partitionType = m_partitionType;
}


void Editor::resetCommonSettings()
{
	selectNavMeshType(NAVMESH_SMALL);

	m_cellSize = 16.0f;
	m_cellHeight = 5.85f;
	m_traverseLinkDrawParams.cellHeight = m_cellHeight;

	// todo(amos): check if this applies for all hulls, and check if this is the
	// actual value used by the game. This seems to generate slopes very close
	// to the walkable slopes in-game. The slopes generated for the map
	// mp_rr_canyonlands_staging.bsp where pretty much identical. If this is
	// confirmed, move this value to a game header instead and define it as a
	// constant. The value originates from here under "Player Collision Hull":
	// https://developer.valvesoftware.com/wiki/Pl/Dimensions
	m_agentMaxSlope = 45.573f;

	m_regionMinSize = 8;
	m_regionMergeSize = 20;
	m_edgeMaxLen = 12;
	m_edgeMaxError = 1.3f;
	m_vertsPerPoly = 6;
	m_detailSampleDist = 6.0f;
	m_detailSampleMaxError = 1.0f;
	m_partitionType = EDITOR_PARTITION_WATERSHED;
}
void Editor::handleCommonSettings()
{
	ImGui::Text("NavMesh Type");
	for (int i = 0; i < NAVMESH_COUNT; i++)
	{
		const NavMeshType_e navMeshType = NavMeshType_e(i);

		if (ImGui::Button(NavMesh_GetNameForType(navMeshType), ImVec2(120, 0)))
		{
			selectNavMeshType(navMeshType);
		}
	}

	ImGui::Separator();

	ImGui::PushItemWidth(180.f);
	ImGui::Text("Rasterization");

	ImGui::SliderFloat("Cell Size", &m_cellSize, 12.1f, 100.0f);
	
	if (ImGui::SliderFloat("Cell Height", &m_cellHeight, 0.4f, 100.0f))
		m_traverseLinkDrawParams.cellHeight = m_cellHeight;
	
	if (m_geom)
	{
		const float* bmin = m_geom->getNavMeshBoundsMin();
		const float* bmax = m_geom->getNavMeshBoundsMax();
		int gw = 0, gh = 0;
		rcCalcGridSize(bmin, bmax, m_cellSize, &gw, &gh);
		char text[64];
		snprintf(text, 64, "Voxels: %d x %d", gw, gh);
		ImGui::Text(text);
	}
	
	ImGui::Separator();
	ImGui::Text("Agent");
	ImGui::SliderFloat("Height", &m_agentHeight, 0.1f, 500.0f);
	ImGui::SliderFloat("Radius", &m_agentRadius, 0.0f, 500.0f);
	ImGui::SliderFloat("Max Climb", &m_agentMaxClimb, 0.1f, 250.0f);
	ImGui::SliderFloat("Max Slope", &m_agentMaxSlope, 0.0f, 90.0f);

	ImGui::PopItemWidth();
	ImGui::PushItemWidth(140.f);
	
	ImGui::Separator();
	ImGui::Text("Region");
	ImGui::SliderInt("Min Region Size", &m_regionMinSize, 0, 750); // todo(amos): increase because of larger map scale?
	ImGui::SliderInt("Merged Region Size", &m_regionMergeSize, 0, 750); // todo(amos): increase because of larger map scale?

	ImGui::PopItemWidth();

	if (m_geom)
	{
		ImGui::Separator();
		ImGui::Text("Bounding");

		float* navMeshBMin = m_geom->getNavMeshBoundsMin();
		float* navMeshBMax = m_geom->getNavMeshBoundsMax();

		const float* meshBMin = m_geom->getMeshBoundsMin();
		const float* meshBMax = m_geom->getMeshBoundsMax();

		ImGui::PushItemWidth(75);
		ImGui::SliderFloat("##BoundingMinsX", &navMeshBMin[0], meshBMin[0], rdMin(meshBMax[0], navMeshBMax[0]));
		ImGui::SameLine();
		ImGui::SliderFloat("##BoundingMinsY", &navMeshBMin[1], meshBMin[1], rdMin(meshBMax[1], navMeshBMax[1]));
		ImGui::SameLine();
		ImGui::SliderFloat("##BoundingMinsZ", &navMeshBMin[2], meshBMin[2], rdMin(meshBMax[2], navMeshBMax[2]));
		ImGui::SameLine();
		ImGui::Text("Mins");

		ImGui::SliderFloat("##BoundingMaxsX", &navMeshBMax[0], rdMax(meshBMin[0], navMeshBMin[0]), meshBMax[0]);
		ImGui::SameLine();
		ImGui::SliderFloat("##BoundingMaxsY", &navMeshBMax[1], rdMax(meshBMin[1], navMeshBMin[1]), meshBMax[1]);
		ImGui::SameLine();
		ImGui::SliderFloat("##BoundingMaxsZ", &navMeshBMax[2], rdMax(meshBMin[2], navMeshBMin[2]), meshBMax[2]);
		ImGui::SameLine();
		ImGui::Text("Maxs");
		ImGui::PopItemWidth();

		if (ImGui::Button("Reset##BoundingSettings", ImVec2(120, 0)))
		{
			rdVcopy(navMeshBMin, m_geom->getOriginalNavMeshBoundsMin());
			rdVcopy(navMeshBMax, m_geom->getOriginalNavMeshBoundsMax());
		}
	}

	ImGui::Separator();
	ImGui::Text("Partitioning");

	bool isEnabled = m_partitionType == EDITOR_PARTITION_WATERSHED;

	if (ImGui::Checkbox("Watershed", &isEnabled))
		m_partitionType = EDITOR_PARTITION_WATERSHED;

	isEnabled = m_partitionType == EDITOR_PARTITION_MONOTONE;

	if (ImGui::Checkbox("Monotone", &isEnabled))
		m_partitionType = EDITOR_PARTITION_MONOTONE;

	isEnabled = m_partitionType == EDITOR_PARTITION_LAYERS;

	if (ImGui::Checkbox("Layers", &isEnabled))
		m_partitionType = EDITOR_PARTITION_LAYERS;
	
	ImGui::Separator();
	ImGui::Text("Filtering");
	ImGui::Checkbox("Low Hanging Obstacles##FilterSettings", &m_filterLowHangingObstacles);
	ImGui::Checkbox("Ledge Spans##FilterSettings", &m_filterLedgeSpans);
	ImGui::Checkbox("Walkable Low Height Spans##FilterSettings", &m_filterWalkableLowHeightSpans);

	ImGui::PushItemWidth(145.f);
	ImGui::Separator();

	ImGui::Text("Polygonization");
	ImGui::SliderInt("Max Edge Length", &m_edgeMaxLen, 0, 50); // todo(amos): increase due to larger scale maps?
	ImGui::SliderFloat("Max Edge Error", &m_edgeMaxError, 0.1f, 3.0f);
	ImGui::SliderInt("Verts Per Poly", &m_vertsPerPoly, 3, 6);
	ImGui::SliderInt("Poly Cell Resolution", &m_polyCellRes, 1, 16);

	ImGui::Separator();
	ImGui::Text("Detail Mesh");
	ImGui::SliderFloat("Sample Distance", &m_detailSampleDist, 1.0f, 16.0f);
	ImGui::SliderFloat("Max Sample Error", &m_detailSampleMaxError, 0.0f, 16.0f);

	ImGui::PopItemWidth();
	
	ImGui::Separator();
	ImGui::Text("Traversability");

	static ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingFixedFit | 
		/*ImGuiTableFlags_ScrollX |*/ 
		ImGuiTableFlags_ScrollY | 
		ImGuiTableFlags_BordersInner | 
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_Hideable |
		/*ImGuiTableFlags_Resizable |*/
		/*ImGuiTableFlags_Reorderable |*/
		ImGuiTableFlags_HighlightHoveredColumn;

	static ImGuiTableColumnFlags columnFlags = ImGuiTableColumnFlags_AngledHeader |
		ImGuiTableColumnFlags_WidthStretch;

	static int frozenCols = 1;
	static int frozenRows = 2;
	const float textBaseHeight = ImGui::GetTextLineHeightWithSpacing();

	const char* columnNames[] = { "Type", "minElev", "maxElev", "minDist", "maxDist" };
	const int columnsCount = IM_ARRAYSIZE(columnNames);
	const int rowsCount = NUM_TRAVERSE_TYPES;

	if (ImGui::BeginTable("TraverseTableFineTuner", columnsCount, tableFlags, ImVec2(0.0f, (textBaseHeight * 12)+10.f)))
	{
		ImGui::TableSetupColumn(columnNames[0], ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder);
		for (int n = 1; n < columnsCount; n++)
			ImGui::TableSetupColumn(columnNames[n], columnFlags, 100);
		ImGui::TableSetupScrollFreeze(frozenCols, frozenRows);

		ImGui::TableAngledHeadersRow();
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin(rowsCount);

		while (clipper.Step())
		{
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
			{
				ImGui::PushID(row);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("%d", row);

				for (int column = 1; column < columnsCount; column++)
				{
					if (!ImGui::TableSetColumnIndex(column))
						continue;

					ImGui::PushID(column);
					ImGui::PushItemWidth(-FLT_MIN); // Right align cells.
					TraverseType_s& trav = s_traverseTable[row];

					switch (column)
					{
					case 1:
						trav.minElev = rdClamp(trav.minElev, 0.0f, trav.maxElev);
						ImGui::SliderFloat("", &trav.minElev, 0, trav.maxElev);
						break;
					case 2:
						ImGui::SliderFloat("", &trav.maxElev, 0, DT_TRAVERSE_DIST_MAX);
						break;
					case 3:
						trav.minDist = rdClamp(trav.minDist, 0.0f, trav.maxDist);
						ImGui::SliderFloat("", &trav.minDist, 0, trav.maxDist);
						break;
					case 4:
						ImGui::SliderFloat("", &trav.maxDist, 0, DT_TRAVERSE_DIST_MAX);
						break;
					}

					ImGui::PopItemWidth();
					ImGui::PopID();
				}
				ImGui::PopID();
			}
		}

		ImGui::EndTable();
	}
	if (ImGui::Button("Reset Traverse Table Parameters"))
		initTraverseTableParams();

	const int numTraverseTables = NavMesh_GetTraverseTableCountForNavMeshType(m_selectedNavMeshType);
	const int numColumns = numTraverseTables + 1;

	if (ImGui::BeginTable("TraverseTableMaskSelector", numColumns, tableFlags, ImVec2(0.0f, (textBaseHeight*12)+20.f)))
	{
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder);
		const bool smallNavMesh = m_selectedNavMeshType == NAVMESH_SMALL;

		for (int n = 0; n < numTraverseTables; n++)
		{
			const int i = smallNavMesh
				? NavMesh_GetTraverseTableIndexForAnimType(TraverseAnimType_e(n))
				: NavMesh_GetFirstTraverseAnimTypeForType(m_selectedNavMeshType);

			ImGui::TableSetupColumn(g_traverseAnimTypeNames[i], columnFlags);
		}

		ImGui::TableSetupScrollFreeze(frozenCols, frozenRows);

		ImGui::TableAngledHeadersRow();
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin(rowsCount);

		while (clipper.Step())
		{
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
			{
				ImGui::PushID(row);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("%d", row);

				for (int column = 0; column < numTraverseTables; column++)
				{
					if (!ImGui::TableSetColumnIndex(column + 1))
						continue;

					ImGui::PushID(column + 1);
					const int j = smallNavMesh
						? column
						: NavMesh_GetFirstTraverseAnimTypeForType(m_selectedNavMeshType);

					int* flags = &s_traverseAnimTraverseFlags[j];

					ImGui::CheckboxFlags("", flags, 1 << row);
					ImGui::PopID();
				}
				ImGui::PopID();
			}
		}

		ImGui::EndTable();
	}
	if (ImGui::Button("Reset Traverse Table Masks"))
		initTraverseMasks();

	if (ImGui::Checkbox("Dynamic Traverse Ray Offset", &m_traverseRayDynamicOffset))
		m_traverseLinkDrawParams.dynamicOffset = m_traverseRayDynamicOffset;

	if (ImGui::SliderFloat("Extra Offset", &m_traverseRayExtraOffset, 0, 128))
		m_traverseLinkDrawParams.extraOffset = m_traverseRayExtraOffset;

	ImGui::Separator();
}

void Editor::handleClick(const float* s, const float* p, bool shift)
{
	if (m_tool)
		m_tool->handleClick(s, p, shift);
}

void Editor::handleToggle()
{
	if (m_tool)
		m_tool->handleToggle();
}

void Editor::handleStep()
{
	if (m_tool)
		m_tool->handleStep();
}

bool Editor::handleBuild()
{
	return true;
}

void Editor::handleUpdate(const float dt)
{
	if (m_tool)
		m_tool->handleUpdate(dt);
	updateToolStates(dt);
}

enum TraverseType_e // todo(amos): move elsewhere
{
	TRAVERSE_UNUSED_0 = 0,

	TRAVERSE_CROSS_GAP_SMALL,
	TRAVERSE_CLIMB_OBJECT_SMALL,
	TRAVERSE_CROSS_GAP_MEDIUM,

	TRAVERSE_UNUSED_4,
	TRAVERSE_UNUSED_5,
	TRAVERSE_UNUSED_6,

	TRAVERSE_CROSS_GAP_LARGE,

	TRAVERSE_CLIMB_WALL_MEDIUM,
	TRAVERSE_CLIMB_WALL_TALL,
	TRAVERSE_CLIMB_BUILDING,

	TRAVERSE_JUMP_SHORT,
	TRAVERSE_JUMP_MEDIUM,
	TRAVERSE_JUMP_LARGE,

	TRAVERSE_UNUSED_14,
	TRAVERSE_UNUSED_15,

	TRAVERSE_UNKNOWN_16, // USED!!!
	TRAVERSE_UNKNOWN_17, // USED!!!

	TRAVERSE_UNKNOWN_18,
	TRAVERSE_UNKNOWN_19, // NOTE: does not exists in MSET5!!!

	TRAVERSE_CLIMB_TARGET_SMALL,
	TRAVERSE_CLIMB_TARGET_LARGE,

	TRAVERSE_UNUSED_22,
	TRAVERSE_UNUSED_23,

	TRAVERSE_UNKNOWN_24,

	TRAVERSE_UNUSED_25,
	TRAVERSE_UNUSED_26,
	TRAVERSE_UNUSED_27,
	TRAVERSE_UNUSED_28,
	TRAVERSE_UNUSED_29,
	TRAVERSE_UNUSED_30,
	TRAVERSE_UNUSED_31,

	// These aren't traverse type!
	NUM_TRAVERSE_TYPES,
	INVALID_TRAVERSE_TYPE = DT_NULL_TRAVERSE_TYPE
};

struct TraverseType_s // todo(amos): move elsewhere
{
	float minElevation;
	float maxElevation;

	unsigned char minDist;
	unsigned char maxDist;

	bool forceSamePolyGroup;
	bool forceDifferentPolyGroup;
};

static const TraverseType_s s_traverseTypes[NUM_TRAVERSE_TYPES] = // todo(amos): move elsewhere
{
	{0.0f, 0.0f, 0, 0, false, false}, // Unused

	{0, 32, 2, 12, false, false }, //1
	{32, 40, 5, 16, false, false }, //2
	{0, 16, 11, 22, false, false },//3

	{0.0f, 0.0f, 0, 0, false, false}, // Unused
	{0.0f, 0.0f, 0, 0, false, false}, // Unused
	{0.0f, 0.0f, 0, 0, false, false}, // Unused

	{0, 40, 80, 107, false, true},    //7
	{40, 128, 7, 21, false, false},   //8
	{128, 256, 16, 45, false, false},  //9
	{256, 640, 33, 225, false, false}, //10
	{0, 40, 41, 79, false, false},    //11
	{128, 256, 41, 100, false, false},  //12
	{256, 512, 81, 179, false, false},  //13

	{0.0f, 0.0f, 0, 0, false, false}, // Unused
	{0.0f, 0.0f, 0, 0, false, false}, // Unused

	{0, 64, 22, 41, false, false}, //16
	{512, 1024, 21, 58, false, false}, //17

	{0.0f, 0.0f, 0, 0, false, false}, // Unused

#if DT_NAVMESH_SET_VERSION > 5
	{0.0f, 0.0f, 0, 0, false, false}, // Unused
#endif

	{256, 640, 16, 40, false, false}, // Maps to type 19 in MSET 5
	{640, 1024, 33, 199, false, false}, // Maps to type 20 in MSET 5

#if DT_NAVMESH_SET_VERSION == 5
	{0.0f, 0.0f, 0, 0, false, false}, // Unused
#endif

	{0.0f, 0.0f, 0, 0, false, false}, // Unused
	{0.0f, 0.0f, 0, 0, false, false}, // Unused

	{0, 0, 0, 0, false, false}, // Does not exist in MSET 5 ~ 8.

	{0.0f, 0.0f, 0, 0, false, false}, // Unused
	{0.0f, 0.0f, 0, 0, false, false}, // Unused
	{0.0f, 0.0f, 0, 0, false, false}, // Unused
	{0.0f, 0.0f, 0, 0, false, false}, // Unused
	{0.0f, 0.0f, 0, 0, false, false}, // Unused
	{0.0f, 0.0f, 0, 0, false, false}, // Unused
	{0.0f, 0.0f, 0, 0, false, false}, // Unused
};

TraverseType_e GetBestTraverseType(const float elevation, const unsigned char traverseDist, const bool samePolyGroup)
{
	TraverseType_e bestTraverseType = INVALID_TRAVERSE_TYPE;

	for (int i = NUM_TRAVERSE_TYPES-1; i >= 0; --i)
	{
		const TraverseType_s& traverseType = s_traverseTypes[i];

		// Skip unused types...
		if (traverseType.minElevation == 0.0f && traverseType.maxElevation == 0.0f &&
			traverseType.minDist == 0 && traverseType.maxDist == 0)
		{
			continue;
		}

		if (elevation < traverseType.minElevation ||
			elevation > traverseType.maxElevation)
		{
			continue;
		}

		if (traverseDist < traverseType.minDist ||
			traverseDist > traverseType.maxDist)
		{
			continue;
		}

		// NOTE: currently only type 7 is enforced in this check, perhaps we
		// should limit some other types on same/diff poly groups as well?
		if ((traverseType.forceSamePolyGroup && !samePolyGroup) ||
			(traverseType.forceDifferentPolyGroup && samePolyGroup))
		{
			continue;
		}

		bestTraverseType = (TraverseType_e)i;
		break;
	}

	return bestTraverseType;
}

static bool polyEdgeFaceAgainst(const float* v1, const float* v2, const float* n1, const float* n2)
{
	const float delta[2] = { v2[0] - v1[0], v2[1] - v1[1] };
	return (rdVdot2D(delta, n1) >= 0 && rdVdot2D(delta, n2) < 0);
}

static bool traverseLinkOffsetIntersectsGeom(const InputGeom* geom, const float* basePos, const float* offsetPos)
{
	// We need to fire a raycast from out initial
	// high pos to our offset position to make
	// sure we didn't clip into geometry:
	// 
	//                        object geom
	//                        ^
	//     outer navmesh      |
	//     ^                  |
	//     |          !-----------------------!
	//     |      gap !     / potential clip  !
	//     |      ^   !    /                  !
	//     |      |   !   /  inner navmesh    !
	//     |      |   !  /   ^                !
	//     |      |   ! /    |                !
	//   ++++++ <---> !/   +++++++++++++++    !
	// ========================================...
	// 
	// Otherwise we create links between a mesh
	// inside and outside an object, causing the
	// ai to traverse inside of it.
	if (geom->raycastMesh(basePos, offsetPos) ||
		geom->raycastMesh(offsetPos, basePos))
		return true;

	return false;
}

static bool traverseLinkInLOS(const InputGeom* geom, const float* lowPos, const float* highPos, const float* lowDir, const float* highDir, const float offsetAmount)
{
	float lowNormal[3];
	rdCalcEdgeNormal2D(lowDir, false, lowNormal);

	float highNormal[3];
	rdCalcEdgeNormal2D(highDir, false, highNormal);

	// Detect overhangs to avoid links like these:
	// 
	//        geom             upper navmesh
	//    gap ^                ^
	//    ^   |                |
	//    |   |                |
	//  <---> | +++++++++++++++++++++++++++++++...
	//  \======================================...
	//   \        |
	//    \       |
	//     \      |---> overhang
	//      \
	//       \
	//        \-----> link
	//     gap \               lower navmesh
	//       ^  \              ^
	//       |   \             |
	//     <----> +++++++++++++++++++++++++++++...
	//     ====================================...
	// 
	// The AI would otherwise attempt to initiate
	// the jump from the lower navmesh, causing it
	// to clip through geometry.
	if (!polyEdgeFaceAgainst(lowPos, highPos, lowNormal, highNormal))
		return false;

	const float* targetRayPos = highPos;
	const bool hasOffset = offsetAmount > 0;

	// We offset the highest point with at least the
	// walkable radius, and perform a raycast test
	// from the highest point to the lowest. The
	// offsetting is necessary to account for the
	// gap between the edge of the navmesh and the
	// edge of the geometry shown below:
	// 
	//                          geom    upper navmesh
	//                          ^       ^
	//                     gap  |       |
	//                     ^    |       |
	//                     |    |       |
	//           offset <-----> | +++++++++++++...
	//                / =======================...
	//               /
	//     ray <----/     lower navmesh
	//             /      ^
	//     geom   /       |
	//     ^     /        |
	//     |    +++++++++++++++++++++++++++++++...
	// ========================================...
	// 
	// We only want the raycast test to fail if the
	// ledge is larger than usual, when the low and
	// high positions are angled in such way no LOS
	// is possible, or when there's an actual object
	// between the 2 positions.
	float offsetRayPos[3];

	if (hasOffset)
	{
		offsetRayPos[0] = highPos[0] + highNormal[0] * offsetAmount;
		offsetRayPos[1] = highPos[1] + highNormal[1] * offsetAmount;
		offsetRayPos[2] = highPos[2];

		if (traverseLinkOffsetIntersectsGeom(geom, highPos, offsetRayPos))
			return false;

		targetRayPos = offsetRayPos;
	}

	// note(amos): perform 2 raycasts as we have to take the
	// face normal into account. Path must be clear from both
	// directions. We cast from the upper position first as
	// an optimization attempt because if there's a ledge, the
	// raycast test from the lower pos is more likely to pass
	// due to mesh normals, e.g. when the higher mesh is generated
	// on a single sided plane, and we have a ledge between our
	// lower and higher pos, the test from below will pass while
	// the test from above won't. Doing the test from below won't
	// matter besides burning CPU time as we will never get here if
	// the mesh normals of that plane were flipped as there
	// won't be any navmesh on the higher pos in the first place.
	// Its still possible there's something blocking on the lower
	// pos' side, but this is a lot less likely to happen.
	if (geom->raycastMesh(targetRayPos, lowPos) ||
		geom->raycastMesh(lowPos, targetRayPos))
		return false;

	return true;
}

// TODO: this lookup table isn't correct, needs to be fixed.
static const int s_traverseAnimTraverseFlags[TraverseAnimType_e::ANIMTYPE_COUNT] = {
	0x0000013F, // ANIMTYPE_HUMAN
	0x0000013F, // ANIMTYPE_SPECTRE
#if DT_NAVMESH_SET_VERSION == 5
	0x001BDF7F, // ANIMTYPE_STALKER        // MSET 5 = 001BDF7F
	0x001BFFFF, // ANIMTYPE_FRAG_DRONE     // MSET 5 = 001BFFFF
#else
	0x0033DF7F, // ANIMTYPE_STALKER        // MSET 5 = 001BDF7F
	0x0033FFFF, // ANIMTYPE_FRAG_DRONE     // MSET 5 = 001BFFFF
#endif
	0x0000013F, // ANIMTYPE_PILOT          // Unknown, but most likely the same as ANIMTYPE_HUMAN, this also doesn't exist for MSET 5
	0x00033F87, // ANIMTYPE_PROWLER
	0x00033F82, // ANIMTYPE_SUPER_SPECTRE
	0000003000, // ANIMTYPE_TITAN
	0000003000, // ANIMTYPE_GOLIATH // Doesn't exist in MSET 5
};

// TODO: create lookup table and look for distance + slope to determine the
// correct jumpType.
// TODO: make sure we don't generate duplicate pairs of jump types between
// 2 polygons.
void Editor::connectTileTraverseLinks(dtMeshTile* const baseTile, const bool linkToNeighbor)
{
	// If we link to the same tile, we need at least 2 links.
	if (!baseTile->linkCountAvailable(linkToNeighbor ? 1 : 2))
		return;

	const dtMeshHeader* baseHeader = baseTile->header;
	const dtPolyRef basePolyRefBase = m_navMesh->getPolyRefBase(baseTile);

	bool firstBaseTileLinkUsed = false;

	for (int i = 0; i < baseHeader->polyCount; ++i)
	{
		dtPoly* const basePoly = &baseTile->polys[i];

		if (basePoly->groupId == DT_UNLINKED_POLY_GROUP)
			continue;

		for (int j = 0; j < basePoly->vertCount; ++j)
		{
			// Hard edges only!
			if (basePoly->neis[j] != 0)
				continue;

			// Polygon 1 edge
			const float* const basePolySpos = &baseTile->verts[basePoly->verts[j] * 3];
			const float* const basePolyEpos = &baseTile->verts[basePoly->verts[(j + 1) % basePoly->vertCount] * 3];

			float basePolyEdgeMid[3];
			rdVsad(basePolyEdgeMid, basePolySpos, basePolyEpos, 0.5f);

			unsigned char baseSide = rdClassifyPointInsideBounds(basePolyEdgeMid, baseHeader->bmin, baseHeader->bmax);
			const int MAX_NEIS = 32; // Max neighbors

			dtMeshTile* neis[MAX_NEIS];
			int nneis;

			if (linkToNeighbor) // Retrieve the neighboring tiles on the side of our base poly edge.
				nneis = m_navMesh->getNeighbourTilesAt(baseHeader->x, baseHeader->y, baseSide, neis, MAX_NEIS);
			else
			{
				// Internal links.
				nneis = 1;
				neis[0] = baseTile;
			}

			for (int k = 0; k < nneis; ++k)
			{
				dtMeshTile* landTile = neis[k];
				const bool sameTile = baseTile == landTile;

				// Don't connect to same tile edges yet, leave that for the second pass.
				if (linkToNeighbor && sameTile)
					continue;

				// Skip same polygon.
				if (sameTile && i == k)
					continue;

				if (!landTile->linkCountAvailable(1))
					continue;

				const dtMeshHeader* landHeader = landTile->header;
				const dtPolyRef landPolyRefBase = m_navMesh->getPolyRefBase(landTile);

				bool firstLandTileLinkUsed = false;

				for (int m = 0; m < landHeader->polyCount; ++m)
				{
					dtPoly* const landPoly = &landTile->polys[m];

					if (landPoly->groupId == DT_UNLINKED_POLY_GROUP)
						continue;

					for (int n = 0; n < landPoly->vertCount; ++n)
					{
						if (landPoly->neis[n] != 0)
							continue;

						// We need at least 2 links available, figure out if
						// we link to the same tile or another one.
						if (linkToNeighbor)
						{
							if (firstLandTileLinkUsed && !landTile->linkCountAvailable(1))
								continue;

							else if (firstBaseTileLinkUsed && !baseTile->linkCountAvailable(1))
								return;
						}
						else if (firstBaseTileLinkUsed && !baseTile->linkCountAvailable(2))
							return;

						// Polygon 2 edge
						const float* const landPolySpos = &landTile->verts[landPoly->verts[n] * 3];
						const float* const landPolyEpos = &landTile->verts[landPoly->verts[(n + 1) % landPoly->vertCount] * 3];

						float landPolyEdgeMid[3];
						rdVsad(landPolyEdgeMid, landPolySpos, landPolyEpos, 0.5f);

						const float dist = dtCalcLinkDistance(basePolyEdgeMid, landPolyEdgeMid);
						const unsigned char quantDist = dtQuantLinkDistance(dist);

						if (quantDist == 0)
							continue; // Link distance is greater than maximum supported.

						float baseEdgeDir[3], landEdgeDir[3];
						rdVsub(baseEdgeDir, basePolyEpos, basePolySpos);
						rdVsub(landEdgeDir, landPolyEpos, landPolySpos);

						const float dotProduct = rdVdot(baseEdgeDir, landEdgeDir);

						// Edges facing the same direction should not be linked.
						// Doing so causes links to go through from underneath
						// geometry. E.g. we have an HVAC on a roof, and we try
						// to link our roof poly edge facing north to the edge
						// of the poly on the HVAC also facing north, the link
						// will go through the HVAC and thus cause the NPC to
						// jump through it.
						// Another case where this is necessary is when having
						// a land edge that connects with the base edge, this
						// prevents the algorithm from establishing a parallel
						// traverse link.
						if (dotProduct > 0)
							continue;

						const float elevation = rdMathFabsf(basePolyEdgeMid[2]-landPolyEdgeMid[2]);
						const bool samePolyGroup = basePoly->groupId == landPoly->groupId;

						const TraverseType_e traverseType = GetBestTraverseType(elevation, quantDist, samePolyGroup);

						if (traverseType == DT_NULL_TRAVERSE_TYPE)
							continue;

						if (m_selectedNavMeshType > NavMeshType_e::NAVMESH_SMALL)
						{
							const int traverseTableIndex = NavMesh_GetFirstTraverseAnimTypeForType(m_selectedNavMeshType);
							const bool traverseTypeSupported = rdBitCellBit(traverseType) & s_traverseAnimTraverseFlags[traverseTableIndex];

							if (!traverseTypeSupported)
								continue;
						}

						const dtPolyRef basePolyRef = basePolyRefBase | i;
						const dtPolyRef landPolyRef = landPolyRefBase | l;

						const TraverseLinkPolyPair linkedPolyPair(basePolyRef, landPolyRef);
						auto linkedIt = m_traverseLinkPolyMap.find(linkedPolyPair);

						bool traverseLinkFound = false;

						if (linkedIt != m_traverseLinkPolyMap.end())
							traverseLinkFound = true;

						// These 2 polygons are already linked with the same traverse type.
						if (traverseLinkFound && (rdBitCellBit(traverseType) & linkedIt->second))
							continue;

						const bool basePolyHigher = basePolyEdgeMid[2] > landPolyEdgeMid[2];
						float* const lowerEdgeMid = basePolyHigher ? landPolyEdgeMid : basePolyEdgeMid;
						float* const higherEdgeMid = basePolyHigher ? basePolyEdgeMid : landPolyEdgeMid;
						float* const lowerEdgeDir = basePolyHigher ? landEdgeDir : baseEdgeDir;
						float* const higherEdgeDir = basePolyHigher ? baseEdgeDir : landEdgeDir;

						const float walkableRadius = basePolyHigher ? baseHeader->walkableRadius : landHeader->walkableRadius;

						const float slopeAngle = rdMathFabsf(rdCalcSlopeAngle(basePolyEdgeMid, landPolyEdgeMid));
						const float maxAngle = rdCalcMaxLOSAngle(walkableRadius, m_cellHeight);
						const float offsetAmount = rdCalcLedgeSpanOffsetAmount(walkableRadius, slopeAngle, maxAngle);

						if (!traverseLinkInLOS(m_geom, lowerEdgeMid, higherEdgeMid, lowerEdgeDir, higherEdgeDir, offsetAmount))
							continue;

						const unsigned char landSide = linkToNeighbor
							? rdClassifyPointOutsideBounds(landPolyEdgeMid, landHeader->bmin, landHeader->bmax)
							: rdClassifyPointInsideBounds(landPolyEdgeMid, landHeader->bmin, landHeader->bmax);

						const unsigned int forwardIdx = baseTile->allocLink();
						const unsigned int reverseIdx = landTile->allocLink();

						// Allocated 2 new links, need to check for enough space on subsequent runs.
						// This optimization saves a lot of time generating navmeshes for larger or
						// more complicated geometry.
						firstBaseTileLinkUsed = true;
						firstLandTileLinkUsed = true;

						dtLink* const forwardLink = &baseTile->links[forwardIdx];

						forwardLink->ref = landPolyRef;
						forwardLink->edge = (unsigned char)j;
						forwardLink->side = landSide;
						forwardLink->bmin = 0;
						forwardLink->bmax = 255;
						forwardLink->next = basePoly->firstLink;
						basePoly->firstLink = forwardIdx;
						forwardLink->traverseType = (unsigned char)traverseType;
						forwardLink->traverseDist = quantDist;
						forwardLink->reverseLink = (unsigned short)reverseIdx;

						dtLink* const reverseLink = &landTile->links[reverseIdx];

						reverseLink->ref = basePolyRef;
						reverseLink->edge = (unsigned char)m;
						reverseLink->side = baseSide;
						reverseLink->bmin = 0;
						reverseLink->bmax = 255;
						reverseLink->next = landPoly->firstLink;
						landPoly->firstLink = reverseIdx;
						reverseLink->traverseType = (unsigned char)traverseType;
						reverseLink->traverseDist = quantDist;
						reverseLink->reverseLink = (unsigned short)forwardIdx;
					}
				}
			}
		}
	}
}

bool Editor::createTraverseLinks()
{
	rdAssert(m_navMesh);
	m_traverseLinkPolyMap.clear();

	const int maxTiles = m_navMesh->getMaxTiles();

	// First pass to connect edges between external tiles together.
	for (int i = 0; i < maxTiles; i++)
	{
		dtMeshTile* baseTile = m_navMesh->getTile(i);
		if (!baseTile || !baseTile->header)
			continue;

		connectTileTraverseLinks(baseTile, true);
	}

	// Second pass to use remaining links to connect internal edges on the same tile together.
	for (int i = 0; i < maxTiles; i++)
	{
		dtMeshTile* baseTile = m_navMesh->getTile(i);
		if (!baseTile || !baseTile->header)
			continue;

		connectTileTraverseLinks(baseTile, false);
	}

	return true;
}

bool Editor::createStaticPathingData(const dtTraverseTableCreateParams* params)
{
	if (!params->nav) return false;

	if (!dtCreateDisjointPolyGroups(params))
	{
		m_ctx->log(RC_LOG_ERROR, "createStaticPathingData: Failed to build disjoint poly groups.");
		return false;
	}

	if (!createTraverseLinks())
	{
		m_ctx->log(RC_LOG_ERROR, "createStaticPathingData: Failed to build traverse links.");
		return false;
	}

	return true;
}

bool Editor::updateStaticPathingData(const dtTraverseTableCreateParams* params)
{
	if (!params->nav) return false;

	const int numTraverseTables = NavMesh_GetTraverseTableCountForNavMeshType(m_selectedNavMeshType);

	if (!dtUpdateDisjointPolyGroups(params))
	{
		m_ctx->log(RC_LOG_ERROR, "updateStaticPathingData: Failed to update disjoint poly groups.");
		return false;
	}

	if (!dtCreateTraverseTableData(params))
	{
		m_ctx->log(RC_LOG_ERROR, "updateStaticPathingData: Failed to build traverse table data.");
		return false;
	}

	return true;
}

static bool animTypeSupportsTraverseLink(const dtTraverseTableCreateParams* params, const dtLink* link, const int tableIndex)
{
	// TODO: always link off-mesh connected polygon islands together?
	// Research needed.
	if (link->reverseLink == DT_NULL_TRAVERSE_REVERSE_LINK)
		return true;

	const NavMeshType_e navMeshType = (NavMeshType_e)params->navMeshType;

	// Only the _small NavMesh has more than 1 table.
	const int traverseAnimType = navMeshType == NAVMESH_SMALL
		? tableIndex
		: NavMesh_GetFirstTraverseAnimTypeForType(navMeshType);

	return rdBitCellBit(link->traverseType) & s_traverseAnimTraverseFlags[traverseAnimType];
}

void Editor::createTraverseTableParams(dtTraverseTableCreateParams* params)
{
	params->nav = m_navMesh;
	params->sets = m_djs;
	params->tableCount = NavMesh_GetTraverseTableCountForNavMeshType(m_selectedNavMeshType);
	params->navMeshType = m_selectedNavMeshType;
	params->canTraverse = animTypeSupportsTraverseLink;
}

void Editor::buildStaticPathingData()
{
	dtTraverseTableCreateParams params;
	createTraverseTableParams(&params);

	createStaticPathingData(&params);
	updateStaticPathingData(&params);
}

void Editor::connectOffMeshLinks()
{
	for (int i = 0; i < m_navMesh->getTileCount(); i++)
	{
		dtMeshTile* target = m_navMesh->getTile(i);
		const dtMeshHeader* header = target->header;

		if (!header)
			continue;

		const int offMeshConCount = header->offMeshConCount;

		if (!offMeshConCount)
			continue;

		const dtTileRef targetRef = m_navMesh->getTileRef(target);

		// Base off-mesh connections to their starting polygons 
		// and connect connections inside the tile.
		m_navMesh->baseOffMeshLinks(targetRef);

		// Connect off-mesh polygons to outer tiles.
		m_navMesh->connectExtOffMeshLinks(targetRef);
	}
}

void Editor::updateToolStates(const float dt)
{
	for (int i = 0; i < MAX_TOOLS; i++)
	{
		if (m_toolStates[i])
			m_toolStates[i]->handleUpdate(dt);
	}
}

void Editor::initToolStates(Editor* editor)
{
	for (int i = 0; i < MAX_TOOLS; i++)
	{
		if (m_toolStates[i])
			m_toolStates[i]->init(editor);
	}
}

void Editor::resetToolStates()
{
	for (int i = 0; i < MAX_TOOLS; i++)
	{
		if (m_toolStates[i])
			m_toolStates[i]->reset();
	}
}

void Editor::renderToolStates()
{
	for (int i = 0; i < MAX_TOOLS; i++)
	{
		if (m_toolStates[i])
			m_toolStates[i]->handleRender();
	}
}

void Editor::renderOverlayToolStates(double* proj, double* model, int* view)
{
	for (int i = 0; i < MAX_TOOLS; i++)
	{
		if (m_toolStates[i])
			m_toolStates[i]->handleRenderOverlay(proj, model, view);
	}
}

void Editor::renderMeshOffsetOptions()
{
	ImGui::Text("Render Offsets");

	ImGui::PushItemWidth(230);

	ImGui::SliderFloat3("Recast##RenderOffset", m_recastDrawOffset, -500, 500);
	ImGui::SliderFloat3("Detour##RenderOffset", m_detourDrawOffset, -500, 500);

	ImGui::PopItemWidth();
}

void Editor::renderDetourDebugMenu()
{
	ImGui::Text("Detour Render Options");

	bool isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_OFFMESHCONS);

	if (ImGui::Checkbox("Off-Mesh Connections", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_OFFMESHCONS);

	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_QUERY_NODES);

	if (ImGui::Checkbox("Query Nodes", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_QUERY_NODES);

	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_BVTREE);

	if (ImGui::Checkbox("BVTree", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_BVTREE);

	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_PORTALS);

	if (ImGui::Checkbox("Portals", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_PORTALS);

	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_WITH_CLOSED_LIST);

	if (ImGui::Checkbox("Closed List", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_WITH_CLOSED_LIST);

	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_TILE_COLORS);

	if (ImGui::Checkbox("Tile ID Colors", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_TILE_COLORS);

	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_TILE_BOUNDS);

	if (ImGui::Checkbox("Tile Bounds", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_TILE_BOUNDS);

#if DT_NAVMESH_SET_VERSION >= 8
	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_TILE_CELLS);

	if (ImGui::Checkbox("Tile Cells", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_TILE_CELLS);
#endif

	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_POLY_VERTS);

	if (ImGui::Checkbox("Vertex Points", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_POLY_VERTS);

	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_POLY_BOUNDS_INNER);

	if (ImGui::Checkbox("Inner Poly Boundaries", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_POLY_BOUNDS_INNER);

	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_POLY_BOUNDS_OUTER);

	if (ImGui::Checkbox("Outer Poly Boundaries", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_POLY_BOUNDS_OUTER);

	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_POLY_CENTERS);

	if (ImGui::Checkbox("Poly Centers", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_POLY_CENTERS);

	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_POLY_GROUPS);

	if (ImGui::Checkbox("Poly Group Colors", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_POLY_GROUPS);

	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_LEDGE_SPANS);

	if (ImGui::Checkbox("Ledge Spans", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_LEDGE_SPANS);

	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_DEPTH_MASK);

	if (ImGui::Checkbox("Depth Mask", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_DEPTH_MASK);

	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_ALPHA);

	if (ImGui::Checkbox("Transparency", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_ALPHA);

	isEnabled = (getNavMeshDrawFlags() & DU_DRAWNAVMESH_TRAVERSE_LINKS);

	if (ImGui::Checkbox("Traverse Links", &isEnabled))
		toggleNavMeshDrawFlag(DU_DRAWNAVMESH_TRAVERSE_LINKS);

	if (isEnabled && m_navMesh) // Supplemental options only available with a valid navmesh!
	{
		ImGui::PushItemWidth(190);
		ImGui::SliderInt("Traverse Type", &m_traverseLinkDrawParams.traverseLinkType, -1, DT_MAX_TRAVERSE_TYPES-1);
		ImGui::SliderInt("Traverse Dist", &m_traverseLinkDrawParams.traverseLinkDistance, -1, dtQuantLinkDistance(DT_TRAVERSE_DIST_MAX));
		ImGui::SliderInt("Traverse Anim", &m_traverseLinkDrawParams.traverseAnimType, -2, m_navMesh->getParams()->traverseTableCount-1);
		ImGui::PopItemWidth();
	}
}

// NOTE: the climb height should never equal or exceed the agent's height, see https://groups.google.com/g/recastnavigation/c/L5rBamxcOBk/m/5xGLj6YP25kJ
// Quote: "you will get into trouble in cases where there is an overhand which is low enough to step over and high enough for the agent to walk under."
const hulldef hulls[NAVMESH_COUNT] = {
	{ g_navMeshNames[NAVMESH_SMALL]      , NAI_Hull::Width(HULL_HUMAN)   * NAI_Hull::Scale(HULL_HUMAN)  , NAI_Hull::Height(HULL_HUMAN)  , NAI_Hull::Height(HULL_HUMAN)   * NAI_Hull::Scale(HULL_HUMAN)  , 32, 8 },
	{ g_navMeshNames[NAVMESH_MED_SHORT]  , NAI_Hull::Width(HULL_PROWLER) * NAI_Hull::Scale(HULL_PROWLER), NAI_Hull::Height(HULL_PROWLER), NAI_Hull::Height(HULL_PROWLER) * NAI_Hull::Scale(HULL_PROWLER), 32, 4 },
	{ g_navMeshNames[NAVMESH_MEDIUM]     , NAI_Hull::Width(HULL_MEDIUM)  * NAI_Hull::Scale(HULL_MEDIUM) , NAI_Hull::Height(HULL_MEDIUM) , NAI_Hull::Height(HULL_MEDIUM)  * NAI_Hull::Scale(HULL_MEDIUM) , 32, 4 },
	{ g_navMeshNames[NAVMESH_LARGE]      , NAI_Hull::Width(HULL_TITAN)   * NAI_Hull::Scale(HULL_TITAN)  , NAI_Hull::Height(HULL_TITAN)  , NAI_Hull::Height(HULL_TITAN)   * NAI_Hull::Scale(HULL_TITAN)  , 64, 2 },
	{ g_navMeshNames[NAVMESH_EXTRA_LARGE], NAI_Hull::Width(HULL_GOLIATH) * NAI_Hull::Scale(HULL_GOLIATH), NAI_Hull::Height(HULL_GOLIATH), NAI_Hull::Height(HULL_GOLIATH) * NAI_Hull::Scale(HULL_GOLIATH), 64, 2 },
};

void Editor::selectNavMeshType(const NavMeshType_e navMeshType)
{
	const hulldef& h = hulls[navMeshType];

	m_agentRadius = h.radius;
	m_agentMaxClimb = h.climbHeight;
	m_agentHeight = h.height;
	m_navmeshName = h.name;
	m_tileSize = h.tileSize;
	m_polyCellRes = h.cellResolution;

	m_selectedNavMeshType = navMeshType;
}

bool Editor::loadAll(std::string path, const bool fullPath)
{
	dtFreeNavMesh(m_navMesh);
	m_navMesh = nullptr;

	const char* navMeshPath = nullptr;
	char buffer[256];

	if (!fullPath) // Load from model name (e.g. "mp_rr_box").
	{
		fs::path p = "..\\maps\\navmesh\\";
		if (fs::is_directory(p))
		{
			path.insert(0, p.string());
		}

		sprintf(buffer, "%s_%s.nm", path.c_str(), m_navmeshName);
		navMeshPath = buffer;
	}
	else
		navMeshPath = path.c_str();

	FILE* fp = fopen(navMeshPath, "rb");
	if (!fp)
		return false;

	// Read header.
	dtNavMeshSetHeader header;
	size_t readLen = fread(&header, sizeof(dtNavMeshSetHeader), 1, fp);
	if (readLen != 1)
	{
		fclose(fp);
		return false;
	}
	if (header.magic != DT_NAVMESH_SET_MAGIC) // todo(amos) check for tool mode since tilecache uses different constants!
	{
		fclose(fp);
		return false;
	}
	if (header.version != DT_NAVMESH_SET_VERSION) // todo(amos) check for tool mode since tilecache uses different constants!
	{
		fclose(fp);
		return false;
	}

	dtNavMesh* mesh = dtAllocNavMesh();
	if (!mesh)
	{
		fclose(fp);
		return false;
	}


	dtStatus status = mesh->init(&header.params);
	if (dtStatusFailed(status))
	{
		fclose(fp);
		return false;
	}
	
	// Read tiles.
	for (int i = 0; i < header.numTiles; ++i)
	{
		dtNavMeshTileHeader tileHeader;
		readLen = fread(&tileHeader, sizeof(tileHeader), 1, fp);
		if (readLen != 1)
		{
			fclose(fp);
			return false;
		}

		if (!tileHeader.tileRef || !tileHeader.dataSize)
			break;

		unsigned char* data = (unsigned char*)rdAlloc(tileHeader.dataSize, RD_ALLOC_PERM);
		if (!data)
			break;

		memset(data, 0, tileHeader.dataSize);
		readLen = fread(data, tileHeader.dataSize, 1, fp);

		if (readLen != 1)
		{
			rdFree(data);
			fclose(fp);
			return false;
		}

		mesh->addTile(data, tileHeader.dataSize, DT_TILE_FREE_DATA, tileHeader.tileRef, NULL);
	}

	// Read read static pathing data.
	if (header.params.polyGroupCount >= DT_MIN_POLY_GROUP_COUNT)
	{
		for (int i = 0; i < header.params.traverseTableCount; i++)
		{
			int* traverseTable = (int*)rdAlloc(header.params.traverseTableSize, RD_ALLOC_PERM);
			if (!traverseTable)
				break;

			memset(traverseTable, 0, header.params.traverseTableSize);
			readLen = fread(traverseTable, header.params.traverseTableSize, 1, fp);

			if (readLen != 1)
			{
				rdFree(traverseTable);
				fclose(fp);
				return 0;
			}

			mesh->setTraverseTable(i, traverseTable);
		}
	}

	fclose(fp);
	m_navMesh = mesh;

	return true;
}

void Editor::saveAll(std::string path, const dtNavMesh* mesh)
{
	if (!mesh)
		return;

	fs::path p = "..\\maps\\navmesh\\";
	if (fs::is_directory(p))
	{
		path.insert(0, p.string());
	}

	char buffer[256];
	sprintf(buffer, "%s_%s.nm", path.c_str(), m_navmeshName);

	FILE* fp = fopen(buffer, "wb");
	if (!fp)
		return;

	// Store header.
	dtNavMeshSetHeader header;
	header.magic = DT_NAVMESH_SET_MAGIC;
	header.version = DT_NAVMESH_SET_VERSION;
	header.numTiles = 0;

	for (int i = 0; i < mesh->getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = mesh->getTile(i);
		if (!tile || !tile->header || !tile->dataSize)
			continue;

		header.numTiles++;
	}

	const dtNavMeshParams* params = mesh->getParams();

	memcpy(&header.params, params, sizeof(dtNavMeshParams));
	fwrite(&header, sizeof(dtNavMeshSetHeader), 1, fp);

	// Store tiles.
	for (int i = 0; i < mesh->getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = mesh->getTile(i);
		if (!tile || !tile->header || !tile->dataSize)
			continue;

		dtNavMeshTileHeader tileHeader;
		tileHeader.tileRef = mesh->getTileRef(tile);
		tileHeader.dataSize = tile->dataSize;

		fwrite(&tileHeader, sizeof(tileHeader), 1, fp);
		fwrite(tile->data, tile->dataSize, 1, fp);
	}

#if DT_NAVMESH_SET_VERSION == 5
	int mset5Unkown = 0;
	for (int i = 0; i < params->polyGroupCount; i++)
		fwrite(&mset5Unkown, sizeof(int), 1, fp);
#endif

	// Only store if we have 3 or more poly groups.
	if (params->polyGroupCount >= DT_MIN_POLY_GROUP_COUNT)
	{
		int** traverseTables = mesh->getTraverseTables();

		rdAssert(traverseTables);

		for (int i = 0; i < header.params.traverseTableCount; i++)
		{
			const int* const tableData = traverseTables[i];
			rdAssert(tableData);

			fwrite(tableData, sizeof(int), (header.params.traverseTableSize/4), fp);
		}
	}

	fclose(fp);
}

bool Editor::loadNavMesh(const char* path, const bool fullPath)
{
	const bool result = Editor::loadAll(path, fullPath);
	m_navQuery->init(m_navMesh, 2048);

	m_loadedNavMeshType = m_selectedNavMeshType;
	m_traverseLinkDrawParams.traverseAnimType = -2;

	if (m_tool)
	{
		m_tool->reset();
		m_tool->init(this);
	}

	resetToolStates();
	initToolStates(this);

	return result;
}
