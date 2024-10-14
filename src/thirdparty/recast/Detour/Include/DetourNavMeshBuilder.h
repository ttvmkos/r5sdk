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

#ifndef DETOURNAVMESHBUILDER_H
#define DETOURNAVMESHBUILDER_H

#include "Shared/Include/SharedAlloc.h"

/// Represents the source data used to build an navigation mesh tile.
/// @ingroup detour
struct dtNavMeshCreateParams
{

	/// @name Polygon Mesh Attributes
	/// Used to create the base navigation graph.
	/// See #rcPolyMesh for details related to these attributes.
	/// @{

	const unsigned short* verts;			///< The polygon mesh vertices. [(x, y, z) * #vertCount] [Unit: vx]
	int vertCount;							///< The number vertices in the polygon mesh. [Limit: >= 3]
	const unsigned short* polys;			///< The polygon data. [Size: #polyCount * 2 * #nvp]
	const unsigned short* polyFlags;		///< The user defined flags assigned to each polygon. [Size: #polyCount]
	const unsigned char* polyAreas;			///< The user defined area ids assigned to each polygon. [Size: #polyCount]
	const unsigned short* surfAreas;		///< The surface area amount for each polygon. [Size: #polyCount]
	int polyCount;							///< Number of polygons in the mesh. [Limit: >= 1]
	int nvp;								///< Maximum number of vertices per polygon. [Limit: >= 3]
	int cellResolution;						///< The resolution of the diamond cell grid [Limit: >= 1]

	/// @}
	/// @name Height Detail Attributes (Optional)
	/// See #rcPolyMeshDetail for details related to these attributes.
	/// @{

	const unsigned int* detailMeshes;		///< The height detail sub-mesh data. [Size: 4 * #polyCount]
	const float* detailVerts;				///< The detail mesh vertices. [Size: 3 * #detailVertsCount] [Unit: wu]
	int detailVertsCount;					///< The number of vertices in the detail mesh.
	const unsigned char* detailTris;		///< The detail mesh triangles. [Size: 4 * #detailTriCount]
	int detailTriCount;						///< The number of triangles in the detail mesh.

	/// @}
	/// @name Off-Mesh Connections Attributes (Optional)
	/// Used to define a custom point-to-point edge within the navigation graph, an 
	/// off-mesh connection is a user defined traversable connection made up to two vertices, 
	/// at least one of which resides within a navigation mesh polygon.
	/// @{

	/// Off-mesh connection vertices. [(ax, ay, az, bx, by, bz) * #offMeshConCount] [Unit: wu]
	const float* offMeshConVerts;
	/// Off-mesh connection reference positions. [(x, y, z) * #offMeshConCount] [Unit: wu]
	const float* offMeshConRefPos;
	/// Off-mesh connection radii. [Size: #offMeshConCount] [Unit: wu]
	const float* offMeshConRad;
	/// Off-mesh connection reference yaw. [Size: #offMeshConCount] [Unit: wu]
	const float* offMeshConRefYaw;
	/// User defined flags assigned to the off-mesh connections. [Size: #offMeshConCount]
	const unsigned short* offMeshConFlags;
	/// User defined area ids assigned to the off-mesh connections. [Size: #offMeshConCount]
	const unsigned char* offMeshConAreas;
	/// The permitted travel direction of the off-mesh connections. [Size: #offMeshConCount]
	///
	/// 0 = Travel only from endpoint A to endpoint B.<br/>
	/// #DT_OFFMESH_CON_BIDIR = Bidirectional travel.
	const unsigned char* offMeshConDir;
	/// The user defined jump type of the off-mesh connection. [Size: #offMeshConCount]
	const unsigned char* offMeshConJumps;
	/// The user defined lookup order of the off-mesh connection poly verts. [Size: #offMeshConCount]
	const unsigned char* offMeshConOrders;
	/// The user defined ids of the off-mesh connection. [Size: #offMeshConCount]
	const unsigned short* offMeshConUserID;
	/// The number of off-mesh connections. [Limit: >= 0]
	int offMeshConCount;

	/// @}
	/// @name Tile Attributes
	/// @note The tile grid/layer data can be left at zero if the destination is a single tile mesh.
	/// @{

	unsigned int userId;	///< The user defined id of the tile.
	int tileX;				///< The tile's x-grid location within the multi-tile destination mesh. (Along the x-axis.)
	int tileY;				///< The tile's y-grid location within the multi-tile destination mesh. (Along the z-axis.)
	int tileLayer;			///< The tile's layer within the layered destination mesh. [Limit: >= 0] (Along the y-axis.)
	float bmin[3];			///< The minimum bounds of the tile. [(x, y, z)] [Unit: wu]
	float bmax[3];			///< The maximum bounds of the tile. [(x, y, z)] [Unit: wu]

	/// @}
	/// @name General Configuration Attributes
	/// @{

	float walkableHeight;	///< The agent height. [Unit: wu]
	float walkableRadius;	///< The agent radius. [Unit: wu]
	float walkableClimb;	///< The agent maximum traversable ledge. (Up/Down) [Unit: wu]
	float cs;				///< The xy-plane cell size of the polygon mesh. [Limit: > 0] [Unit: wu]
	float ch;				///< The z-axis cell height of the polygon mesh. [Limit: > 0] [Unit: wu]

	/// True if a bounding volume tree should be built for the tile.
	/// @note The BVTree is not normally needed for layered navigation meshes.
	bool buildBvTree;

	/// @}
};

/// Disjoint set algorithm used to build the static pathing data for the navmesh.
/// @ingroup detour
class dtDisjointSet
{
public:
	dtDisjointSet() = default;
	dtDisjointSet(const int size)
	{
		init(size);
	}

	void copy(dtDisjointSet& other)
	{
		other.rank.resize(rank.size());

		for (int i = 0; i < other.rank.size(); i++)
			other.rank[i] =  rank[i];

		other.parent.resize(parent.size());

		for (int i = 0; i < other.parent.size(); i++)
			other.parent[i] = parent[i];
	}

	void init(const int size)
	{
		rank.resize(size);
		parent.resize(size);

		for (int i = 0; i < parent.size(); i++)
			parent[i] = i;
	}
	int insertNew()
	{
		rank.push(0);

		const int newId = parent.size();
		parent.push(newId);

		return newId;
	}
	int find(const int id) const
	{
		int& parentRef = parent[id];

		if (parentRef != id)
			parentRef = find(parentRef);

		return parentRef;
	}
	void setUnion(const int x, const int y)
	{
		const int sx = find(x);
		const int sy = find(y);

		if (sx == sy) // Same set already.
			return;

		int& rankSx = rank[sx];
		int& rankSy = rank[sy];

		if (rankSx < rankSy)
			parent[sx] = sy;
		else if (rankSx > rankSy)
			parent[sy] = sx;
		else
		{
			parent[sy] = sx;
			rankSx += 1;
		}
	}

	inline int getSetCount() const { return parent.size(); }

private:
	rdIntArray rank;
	mutable rdIntArray parent;
};

struct dtLink;

/// Parameters used to build traverse links.
/// @ingroup detour
struct dtTraverseTableCreateParams
{
	dtNavMesh* nav;				///< The navmesh.
	dtDisjointSet* sets;		///< The disjoint polygroup sets.
	int tableCount;				///< The number of traverse tables this navmesh should contain.
	int navMeshType;			///< The navmesh type [_small, _extra_large].

	///< The user installed callback which is used to determine if an animType
	/// can use this traverse link.
	bool (*canTraverse)(const dtTraverseTableCreateParams* params, const dtLink* link, const int tableIndex);

	///< Collapses all unique linked poly groups into #DT_FIRST_USABLE_POLY_GROUP.
	/// Must be set if there are more than UINT16_MAX polygon islands.
	bool collapseGroups;
};

/// Builds navigation mesh disjoint poly groups from the provided parameters.
/// @ingroup detour
///  @param[in]		params		The build parameters.
/// @return True if the disjoint set data was successfully created.
bool dtCreateDisjointPolyGroups(const dtTraverseTableCreateParams* params);

/// Updates navigation mesh disjoint poly groups from the provided parameters.
/// @ingroup detour
///  @param[in]		params		The build parameters.
/// @return True if the disjoint set data was successfully updated.
bool dtUpdateDisjointPolyGroups(const dtTraverseTableCreateParams* params);

/// Builds navigation mesh static traverse table from the provided parameters.
/// @ingroup detour
///  @param[in]		params		The build parameters.
/// @return True if the static traverse table was successfully created.
bool dtCreateTraverseTableData(const dtTraverseTableCreateParams* params);

/// Builds navigation mesh tile data from the provided tile creation data.
/// @ingroup detour
///  @param[in]		params		Tile creation data.
///  @param[out]	outData		The resulting tile data.
///  @param[out]	outDataSize	The size of the tile data array.
/// @return True if the tile data was successfully created.
bool dtCreateNavMeshData(dtNavMeshCreateParams* params, unsigned char** outData, int* outDataSize);

/// Swaps the endianess of the tile data's header (#dtMeshHeader).
///  @param[in,out]	data		The tile data array.
///  @param[in]		dataSize	The size of the data array.
bool dtNavMeshHeaderSwapEndian(unsigned char* data, const int dataSize);

/// Swaps endianess of the tile data.
///  @param[in,out]	data		The tile data array.
///  @param[in]		dataSize	The size of the data array.
bool dtNavMeshDataSwapEndian(unsigned char* data, const int dataSize);

#endif // DETOURNAVMESHBUILDER_H

// This section contains detailed documentation for members that don't have
// a source file. It reduces clutter in the main section of the header.

/**

@struct dtNavMeshCreateParams
@par

This structure is used to marshal data between the Recast mesh generation pipeline and Detour navigation components.

See the rcPolyMesh and rcPolyMeshDetail documentation for detailed information related to mesh structure.

Units are usually in voxels (vx) or world units (wu). The units for voxels, grid size, and cell size 
are all based on the values of #cs and #ch.

The standard navigation mesh build process is to create tile data using dtCreateNavMeshData, then add the tile 
to a navigation mesh using either the dtNavMesh single tile <tt>init()</tt> function or the dtNavMesh::addTile()
function.

@see dtCreateNavMeshData

*/

