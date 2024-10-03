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

#ifndef RECASTDETOURCOMMON_H
#define RECASTDETOURCOMMON_H

#include "Shared/Include/SharedConst.h"
#include "Shared/Include/SharedDefs.h"
#include "Shared/Include/SharedMath.h"

/**
@defgroup shared Shared

Members in this module are used to create, manipulate, and query navigation 
meshes.

@note This is a summary list of members.  Use the index or search 
feature to find minor members.
*/

/// @name General helper functions
/// @{

/// Used to ignore a function parameter.  VS complains about unused parameters
/// and this silences the warning.
///  @param [in] _ Unused parameter
template<class T> void rdIgnoreUnused(const T&) { }

/// Swaps the values of the two parameters.
///  @param[in,out]	a	Value A
///  @param[in,out]	b	Value B
template<class T> inline void rdSwap(T& a, T& b) { T t = a; a = b; b = t; }

/// Returns the minimum of two values.
///  @param[in]		a	Value A
///  @param[in]		b	Value B
///  @return The minimum of the two values.
template<class T> inline T rdMin(T a, T b) { return a < b ? a : b; }

/// Returns the maximum of two values.
///  @param[in]		a	Value A
///  @param[in]		b	Value B
///  @return The maximum of the two values.
template<class T> inline T rdMax(T a, T b) { return a > b ? a : b; }

/// Clamps the value to the specified range.
///  @param[in]		v	The value to clamp.
///  @param[in]		mn	The minimum permitted return value.
///  @param[in]		mx	The maximum permitted return value.
///  @return The value, clamped to the specified range.
template<class T> inline T rdClamp(T v, T mn, T mx) { return v < mn ? mn : (v > mx ? mx : v); }

/// Returns the absolute value.
///  @param[in]		a	The value.
///  @return The absolute value of the specified value.
template<class T> inline T rdAbs(T a) { return a < 0 ? -a : a; }

/// Returns the square of the value.
///  @param[in]		a	The value.
///  @return The square of the value.
template<class T> inline T rdSqr(T a) { return a * a; }

/// Converts value from Degrees to Radians.
///  @param[in]		x	The value to convert.
///  @return The input value as Radians.
inline float rdDegToRad(const float x) { return x * (RD_PI/180.0f); }

/// Converts value from Radians to Degrees.
///  @param[in]		x	The value to convert.
///  @return The input value as Degrees.
inline float rdRadToDeg(const float x) { return x * (180.0f/RD_PI); }

/// Tests a specific bit in a bit cell
///  @param[in]		i	The bit number
///  @return The offset mask for the bit.
inline int rdBitCellBit(const int i) { return (1 << ((i) & (RD_BITS_PER_BIT_CELL-1))); }

/// @}
/// @name Vector helper functions.
/// @{

/// Derives the cross product of two vectors. (@p v1 x @p v2)
///  @param[out]	dest	The cross product. [(x, y, z)]
///  @param[in]		v1		A Vector [(x, y, z)]
///  @param[in]		v2		A vector [(x, y, z)]
inline void rdVcross(float* dest, const float* v1, const float* v2)
{
	dest[0] = v1[1]*v2[2] - v1[2]*v2[1];
	dest[1] = v1[2]*v2[0] - v1[0]*v2[2];
	dest[2] = v1[0]*v2[1] - v1[1]*v2[0]; 
}

/// Derives the xy-plane 2D perp product of the two vectors. (uy*vx - ux*vy)
///  @param[in]		u		The LHV vector [(x, y, z)]
///  @param[in]		v		The RHV vector [(x, y, z)]
/// @return The perp dot product on the xy-plane.
///
/// The vectors are projected onto the xy-plane, so the z-values are ignored.
inline float rdVperp2D(const float* u, const float* v)
{
	return u[0]*v[1] - u[1]*v[0];
}

/// Derives the dot product of two vectors. (@p v1 . @p v2)
///  @param[in]		v1	A Vector [(x, y, z)]
///  @param[in]		v2	A vector [(x, y, z)]
/// @return The dot product.
inline float rdVdot(const float* v1, const float* v2)
{
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

/// Derives the dot product of two vectors on the xy-plane. (@p u . @p v)
///  @param[in]		u		A vector [(x, y, z)]
///  @param[in]		v		A vector [(x, y, z)]
/// @return The dot product on the xy-plane.
///
/// The vectors are projected onto the xy-plane, so the z-values are ignored.
inline float rdVdot2D(const float* u, const float* v)
{
	return u[0]*v[0] + u[1]*v[1];
}

/// Performs a scaled vector addition. (@p v1 + (@p v2 * @p s))
///  @param[out]	dest	The result vector. [(x, y, z)]
///  @param[in]		v1		The base vector. [(x, y, z)]
///  @param[in]		v2		The vector to scale and add to @p v1. [(x, y, z)]
///  @param[in]		s		The amount to scale @p v2 by before adding to @p v1.
inline void rdVmad(float* dest, const float* v1, const float* v2, const float s)
{
	dest[0] = v1[0]+v2[0]*s;
	dest[1] = v1[1]+v2[1]*s;
	dest[2] = v1[2]+v2[2]*s;
}

/// Performs a scaled vector addition. ((@p v1 + @p v2) * @p s)
///  @param[out]	dest	The result vector. [(x, y, z)]
///  @param[in]		v1		The base vector. [(x, y, z)]
///  @param[in]		v2		The vector to add to @p v1. [(x, y, z)]
///  @param[in]		s		The amount to scale the addition result of @p v1 and @p v2.
inline void rdVsad(float* dest, const float* v1, const float* v2, const float s)
{
	dest[0] = (v1[0]+v2[0])*s;
	dest[1] = (v1[1]+v2[1])*s;
	dest[2] = (v1[2]+v2[2])*s;
}

/// Performs a linear interpolation between two vectors. (@p v1 toward @p v2)
///  @param[out]	dest	The result vector. [(x, y, x)]
///  @param[in]		v1		The starting vector.
///  @param[in]		v2		The destination vector.
///	 @param[in]		t		The interpolation factor. [Limits: 0 <= value <= 1.0]
inline void rdVlerp(float* dest, const float* v1, const float* v2, const float t)
{
	dest[0] = v1[0]+(v2[0]-v1[0])*t;
	dest[1] = v1[1]+(v2[1]-v1[1])*t;
	dest[2] = v1[2]+(v2[2]-v1[2])*t;
}

/// Performs a vector addition. (@p v1 + @p v2)
///  @param[out]	dest	The result vector. [(x, y, z)]
///  @param[in]		v1		The base vector. [(x, y, z)]
///  @param[in]		v2		The vector to add to @p v1. [(x, y, z)]
inline void rdVadd(float* dest, const float* v1, const float* v2)
{
	dest[0] = v1[0]+v2[0];
	dest[1] = v1[1]+v2[1];
	dest[2] = v1[2]+v2[2];
}

/// Performs a vector subtraction. (@p v1 - @p v2)
///  @param[out]	dest	The result vector. [(x, y, z)]
///  @param[in]		v1		The base vector. [(x, y, z)]
///  @param[in]		v2		The vector to subtract from @p v1. [(x, y, z)]
inline void rdVsub(float* dest, const float* v1, const float* v2)
{
	dest[0] = v1[0]-v2[0];
	dest[1] = v1[1]-v2[1];
	dest[2] = v1[2]-v2[2];
}

/// Scales the vector by the specified value. (@p v * @p t)
///  @param[out]	dest	The result vector. [(x, y, z)]
///  @param[in]		v		The vector to scale. [(x, y, z)]
///  @param[in]		t		The scaling factor.
inline void rdVscale(float* dest, const float* v, const float t)
{
	dest[0] = v[0]*t;
	dest[1] = v[1]*t;
	dest[2] = v[2]*t;
}

/// Selects the minimum value of each element from the specified vectors.
///  @param[in,out]	mn	A vector.  (Will be updated with the result.) [(x, y, z)]
///  @param[in]	v	A vector. [(x, y, z)]
inline void rdVmin(float* mn, const float* v)
{
	mn[0] = rdMin(mn[0], v[0]);
	mn[1] = rdMin(mn[1], v[1]);
	mn[2] = rdMin(mn[2], v[2]);
}

/// Selects the maximum value of each element from the specified vectors.
///  @param[in,out]	mx	A vector.  (Will be updated with the result.) [(x, y, z)]
///  @param[in]		v	A vector. [(x, y, z)]
inline void rdVmax(float* mx, const float* v)
{
	mx[0] = rdMax(mx[0], v[0]);
	mx[1] = rdMax(mx[1], v[1]);
	mx[2] = rdMax(mx[2], v[2]);
}

/// Sets the vector elements to the specified values.
///  @param[out]	dest	The result vector. [(x, y, z)]
///  @param[in]		x		The x-value of the vector.
///  @param[in]		y		The y-value of the vector.
///  @param[in]		z		The z-value of the vector.
inline void rdVset(float* dest, const float x, const float y, const float z)
{
	dest[0] = x; dest[1] = y; dest[2] = z;
}

/// Performs a vector copy.
///  @param[out]	dest	The result. [(x, y, z)]
///  @param[in]		a		The vector to copy. [(x, y, z)]
inline void rdVcopy(float* dest, const float* a)
{
	dest[0] = a[0];
	dest[1] = a[1];
	dest[2] = a[2];
}

/// Derives the scalar length of the vector.
///  @param[in]		v The vector. [(x, y, z)]
/// @return The scalar length of the vector.
inline float rdVlen(const float* v)
{
	return rdMathSqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/// Derives the square of the scalar length of the vector. (len * len)
///  @param[in]		v The vector. [(x, y, z)]
/// @return The square of the scalar length of the vector.
inline float rdVlenSqr(const float* v)
{
	return v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
}

/// Returns the distance between two points.
///  @param[in]		v1	A point. [(x, y, z)]
///  @param[in]		v2	A point. [(x, y, z)]
/// @return The distance between the two points.
inline float rdVdist(const float* v1, const float* v2)
{
	const float dx = v2[0] - v1[0];
	const float dy = v2[1] - v1[1];
	const float dz = v2[2] - v1[2];
	return rdMathSqrtf(dx*dx + dy*dy + dz*dz);
}

/// Returns the square of the distance between two points.
///  @param[in]		v1	A point. [(x, y, z)]
///  @param[in]		v2	A point. [(x, y, z)]
/// @return The square of the distance between the two points.
inline float rdVdistSqr(const float* v1, const float* v2)
{
	const float dx = v2[0] - v1[0];
	const float dy = v2[1] - v1[1];
	const float dz = v2[2] - v1[2];
	return dx*dx + dy*dy + dz*dz;
}

/// Derives the distance between the specified points on the xy-plane.
///  @param[in]		v1	A point. [(x, y, z)]
///  @param[in]		v2	A point. [(x, y, z)]
/// @return The distance between the point on the xy-plane.
///
/// The vectors are projected onto the xy-plane, so the z-values are ignored.
inline float rdVdist2D(const float* v1, const float* v2)
{
	const float dx = v2[0] - v1[0];
	const float dy = v2[1] - v1[1];
	return rdMathSqrtf(dx*dx + dy*dy);
}

/// Derives the square of the distance between the specified points on the xy-plane.
///  @param[in]		v1	A point. [(x, y, z)]
///  @param[in]		v2	A point. [(x, y, z)]
/// @return The square of the distance between the point on the xy-plane.
inline float rdVdist2DSqr(const float* v1, const float* v2)
{
	const float dx = v2[0] - v1[0];
	const float dy = v2[1] - v1[1];
	return dx*dx + dy*dy;
}

/// Normalizes the vector.
///  @param[in,out]	v	The vector to normalize. [(x, y, z)]
inline void rdVnormalize(float* v)
{
	float d = 1.0f / rdMathSqrtf(rdSqr(v[0]) + rdSqr(v[1]) + rdSqr(v[2]));
	v[0] *= d;
	v[1] *= d;
	v[2] *= d;
}

/// Normalizes the vector on the xy-plane.
///  @param[in,out]	v	The vector to normalize. [(x, y, z)]
inline void rdVnormalize2D(float* v)
{
	float d = 1.0f / rdMathSqrtf(rdSqr(v[0]) + rdSqr(v[1]));
	v[0] *= d;
	v[1] *= d;
}

/// Derives the magnitude of the vector.
///  @param[in]		v	A vector. [(x, y, z)]
/// @return The magnitude of the vector.
inline float rdVmag(const float* v)
{
	return rdMathSqrtf(rdVdot(v, v));
}

/// Derives the magnitude of the vector on the xy-plane.
///  @param[in]		v	A vector. [(x, y, z)]
/// @return The magnitude of the vector on the xy-plane.
inline float rdVmag2D(const float* v)
{
	return rdMathSqrtf(rdVdot2D(v, v));
}

/// Derives the scalar projection of the specified point into the vector.
///  @param[in]		p	A point. [(x, y, z)]
///  @param[in]		v	A vector. [(x, y, z)]
/// @return The scalar projection of the specified point into the vector.
inline float rdVproj(const float* p, const float* v)
{
	return rdVdot(p, v) / rdVmag(v);
}

/// Derives the scalar projection of the specified point into the vector on the xy-plane.
///  @param[in]		p	A point. [(x, y, z)]
///  @param[in]		v	A vector. [(x, y, z)]
/// @return The scalar projection of the specified point into the vector on the xy-plane.
inline float rdVproj2D(const float* p, const float* v)
{
	return rdVdot2D(p, v) / rdVmag2D(v);
}

/// Performs a 'sloppy' collocation check of the specified points.
///  @param[in]		p0	A point. [(x, y, z)]
///  @param[in]		p1	A point. [(x, y, z)]
/// @return True if the points are considered to be at the same location.
///
/// Basically, this function will return true if the specified points are 
/// close enough to each other to be considered collocated.
inline bool rdVequal(const float* p0, const float* p1)
{
	static const float thr = rdSqr(1.0f/16384.0f);
	const float d = rdVdistSqr(p0, p1);
	return d < thr;
}

/// Checks that the specified vector's components are all finite.
///  @param[in]		v	A point. [(x, y, z)]
/// @return True if all of the point's components are finite, i.e. not NaN
/// or any of the infinities.
inline bool rdVisfinite(const float* v)
{
	bool result =
		rdMathIsfinite(v[0]) &&
		rdMathIsfinite(v[1]) &&
		rdMathIsfinite(v[2]);

	return result;
}

/// Checks that the specified vector's 2D components are finite.
///  @param[in]		v	A point. [(x, y, z)]
inline bool rdVisfinite2D(const float* v)
{
	bool result = rdMathIsfinite(v[0]) && rdMathIsfinite(v[1]);
	return result;
}

/// @}
/// @name Computational geometry helper functions.
/// @{

/// Derives the signed xy-plane area of the triangle ABC, or the relationship of line AB to point C.
///  @param[in]		a		Vertex A. [(x, y, z)]
///  @param[in]		b		Vertex B. [(x, y, z)]
///  @param[in]		c		Vertex C. [(x, y, z)]
/// @return The signed xy-plane area of the triangle.
inline float rdTriArea2D(const float* a, const float* b, const float* c)
{
	const float abx = b[0] - a[0];
	const float aby = b[1] - a[1];
	const float acx = c[0] - a[0];
	const float acy = c[1] - a[1];
	return acx*aby - abx*acy;
}

/// Determines if two axis-aligned bounding boxes overlap.
///  @param[in]		amin	Minimum bounds of box A. [(x, y, z)]
///  @param[in]		amax	Maximum bounds of box A. [(x, y, z)]
///  @param[in]		bmin	Minimum bounds of box B. [(x, y, z)]
///  @param[in]		bmax	Maximum bounds of box B. [(x, y, z)]
/// @return True if the two AABB's overlap.
/// @see dtOverlapBounds
inline bool rdOverlapQuantBounds(const unsigned short amin[3], const unsigned short amax[3],
								 const unsigned short bmin[3], const unsigned short bmax[3])
{
	bool overlap = true;
	overlap = (amin[0] > bmax[0] || amax[0] < bmin[0]) ? false : overlap;
	overlap = (amin[1] > bmax[1] || amax[1] < bmin[1]) ? false : overlap;
	overlap = (amin[2] > bmax[2] || amax[2] < bmin[2]) ? false : overlap;
	return overlap;
}

/// Determines if two axis-aligned bounding boxes overlap.
///  @param[in]		amin	Minimum bounds of box A. [(x, y, z)]
///  @param[in]		amax	Maximum bounds of box A. [(x, y, z)]
///  @param[in]		bmin	Minimum bounds of box B. [(x, y, z)]
///  @param[in]		bmax	Maximum bounds of box B. [(x, y, z)]
/// @return True if the two AABB's overlap.
/// @see dtOverlapQuantBounds
inline bool rdOverlapBounds(const float* amin, const float* amax,
							const float* bmin, const float* bmax)
{
	bool overlap = true;
	overlap = (amin[0] > bmax[0] || amax[0] < bmin[0]) ? false : overlap;
	overlap = (amin[1] > bmax[1] || amax[1] < bmin[1]) ? false : overlap;
	overlap = (amin[2] > bmax[2] || amax[2] < bmin[2]) ? false : overlap;
	return overlap;
}

/// Derives the slope angle from 2 points.
///  @param[in]		v1	The start vector. [(x, y, z)]
///  @param[in]		v2	The end vector. [(x, y, z)]
/// @return The slope angle between the 2 points.
float rdCalcSlopeAngle(const float* v1, const float* v2);

/// Derives the closest point on a triangle from the specified reference point.
///  @param[out]	closest	The closest point on the triangle.	
///  @param[in]		p		The reference point from which to test. [(x, y, z)]
///  @param[in]		a		Vertex A of triangle ABC. [(x, y, z)]
///  @param[in]		b		Vertex B of triangle ABC. [(x, y, z)]
///  @param[in]		c		Vertex C of triangle ABC. [(x, y, z)]
void rdClosestPtPointTriangle(float* closest, const float* p,
							  const float* a, const float* b, const float* c);

/// Derives the z-axis height of the closest point on the triangle from the specified reference point.
///  @param[in]		p		The reference point from which to test. [(x, y, z)]
///  @param[in]		a		Vertex A of triangle ABC. [(x, y, z)]
///  @param[in]		b		Vertex B of triangle ABC. [(x, y, z)]
///  @param[in]		c		Vertex C of triangle ABC. [(x, y, z)]
///  @param[out]	h		The resulting height.
bool rdClosestHeightPointTriangle(const float* p, const float* a, const float* b, const float* c, float& h);

bool rdIntersectSegmentPoly2D(const float* p0, const float* p1,
							  const float* verts, int nverts,
							  float& tmin, float& tmax,
							  int& segMin, int& segMax);

bool rdIntersectSegmentAABB(const float* sp, const float* sq,
						 const float* amin, const float* amax,
						 float& tmin, float& tmax);

bool rdIntersectSegmentCylinder(const float* sp, const float* sq, const float* position,
								const float radius, const float height,
								float& tmin, float& tmax);

bool rdIntersectSegmentConvexHull(const float* sp, const float* sq, const float* verts,
								  int nverts, float hmin, float hmax,
								  float& tmin, float& tmax);

bool rdIntersectSegSeg2D(const float* ap, const float* aq,
						 const float* bp, const float* bq,
						 float& s, float& t);

float rdDistancePtLine2D(const float* pt, const float* p, const float* q);

/// Derives the normal of an edge
///  @param[in]		dir		The direction of the edge. [(x, y, z)]
///  @param[out]	out		The resulting normal. [(x, y)]
void rdCalcEdgeNormal2D(const float* dir, float* out);

/// Derives the normal of an edge
///  @param[in]		v1		First vert of the polygon edge. [(x, y, z)]
///  @param[in]		v2		Second vert of the polygon edge. [(x, y, z)]
///  @param[out]	out		The resulting normal. [(x, y)]
void rdCalcEdgeNormalPt2D(const float* v1, const float* v2, float* out);

/// Derives the sub-edge area of an edge.
///  @param[in]		edgeStart		First vert of the polygon edge. [(x, y, z)]
///  @param[in]		edgeEnd			Second vert of the polygon edge. [(x, y, z)]
///  @param[in]		subEdgeStart	First vert of the detail edge. [(x, y, z)]
///  @param[in]		subEdgeEnd		Second vert of the detail edge. [(x, y, z)]
///  @param[out]	tmin			The normalized distance ratio from polygon edge start to detail edge start.
///  @param[out]	tmax			The normalized distance ratio from polygon edge start to detail edge end.
/// @return False if tmin and tmax don't correspond to the winding order of the edge.
bool rdCalcSubEdgeArea2D(const float* edgeStart, const float* edgeEnd, const float* subEdgeStart,
	const float* subEdgeEnd, float& tmin, float& tmax);

/// Derives the overlap between 2 edges.
///  @param[in]		edge1Start		Start vert of the first edge. [(x, y, z)]
///  @param[in]		edge1End		End vert of the first edge. [(x, y, z)]
///  @param[in]		edge2Start		Start vert of the second edge. [(x, y, z)]
///  @param[in]		edge2End		End vert of the second edge. [(x, y, z)]
///  @param[in]		targetEdgeVec	The projection direction. [(x, y, z)]
/// @return The length of the overlap.
float rdCalcEdgeOverlap2D(const float* edge1Start, const float* edge1End,
	const float* edge2Start, const float* edge2End, const float* targetEdgeVec);

/// Derives the maximum angle in which an object on an elevated surface can be seen from below.
///  @param[in]		ledgeSpan		The distance between the edge of the object and the edge of the ledge.
///  @param[in]		objectHeight	The height of the object.
/// @return The maximum angle before LOS gets blocked.
float rdCalcMaxLOSAngle(const float ledgeSpan, const float objectHeight);

/// Determines the amount we need to offset an object to maintain LOS from an angle, with a maximum.
///  @param[in]		ledgeSpan	The distance between the edge of the object and the edge of the ledge.
///  @param[in]		slopeAngle	The slope angle to test.
///  @param[in]		maxAngle	The maximum angle in degrees.
/// @return The amount we need to offset to maintain LOS.
float rdCalcLedgeSpanOffsetAmount(const float ledgeSpan, const float slopeAngle, const float maxAngle);

unsigned char rdClassifyPointOutsideBounds(const float* pt, const float* bmin, const float* bmax);
unsigned char rdClassifyPointInsideBounds(const float* pt, const float* bmin, const float* bmax);
unsigned char rdClassifyDirection(const float* dir, const float* bmin, const float* bmax);

/// Determines if the specified point is inside the axis-aligned bounding box.
///  @param[in]		pt		The point to check. [(x, y, z)]
///  @param[in]		bmin	Minimum bounds of the box. [(x, y, z)]
///  @param[in]		bmax	Maximum bounds of the box. [(x, y, z)]
/// @return True if the point is inside the axis-aligned bounding box.
bool rdPointInAABB(const float* pt, const float* bmin, const float* bmax);

/// Determines if the specified point is inside the cylinder on the xy-plane.
///  @param[in]		pt		The point to check. [(x, y, z)]
///  @param[in]		pos		The position of the cylinder. [(x, y, z)]
///  @param[in]		radius	The radius of the cylinder.
///  @param[in]		height	The height of the cylinder.
/// @return True if the point is inside the cylinder.
bool rdPointInCylinder(const float* pt, const float* pos, const float radius, const float height);

/// Determines if the specified point is inside the convex polygon on the xy-plane.
///  @param[in]		pt		The point to check. [(x, y, z)]
///  @param[in]		verts	The polygon vertices. [(x, y, z) * @p nverts]
///  @param[in]		nverts	The number of vertices. [Limit: >= 3]
/// @return True if the point is inside the polygon.
bool rdPointInPolygon(const float* pt, const float* verts, const int nverts);

bool rdDistancePtPolyEdgesSqr(const float* pt, const float* verts, const int nverts,
							float* ed, float* et);

float rdDistancePtSegSqr2D(const float* pt, const float* p, const float* q, float& t);

/// Derives the centroid of a convex polygon.
///  @param[out]	tc		The centroid of the polygon. [(x, y, z)]
///  @param[in]		idx		The polygon indices. [(vertIndex) * @p nidx]
///  @param[in]		nidx	The number of indices in the polygon. [Limit: >= 3]
///  @param[in]		verts	The polygon vertices. [(x, y, z) * vertCount]
void rdCalcPolyCenter(float* tc, const unsigned short* idx, int nidx, const float* verts);

/// Determines if the two convex polygons overlap on the xy-plane.
///  @param[in]		polya		Polygon A vertices.	[(x, y, z) * @p npolya]
///  @param[in]		npolya		The number of vertices in polygon A.
///  @param[in]		polyb		Polygon B vertices.	[(x, y, z) * @p npolyb]
///  @param[in]		npolyb		The number of vertices in polygon B.
/// @return True if the two polygons overlap.
bool rdOverlapPolyPoly2D(const float* polya, const int npolya,
						 const float* polyb, const int npolyb);

/// @}
/// @name Miscellaneous functions.
/// @{

inline unsigned int rdNextPow2(unsigned int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

inline unsigned int rdIlog2(unsigned int v)
{
	unsigned int r;
	unsigned int shift;
	r = (v > 0xffff) << 4; v >>= r;
	shift = (v > 0xff) << 3; v >>= shift; r |= shift;
	shift = (v > 0xf) << 2; v >>= shift; r |= shift;
	shift = (v > 0x3) << 1; v >>= shift; r |= shift;
	r |= (v >> 1);
	return r;
}

inline int rdAlign4(int x) { return (x+3) & ~3; }

inline int rdOppositeTile(int side) { return (side+4) & 0x7; }

inline void rdSwapByte(unsigned char* a, unsigned char* b)
{
	unsigned char tmp = *a;
	*a = *b;
	*b = tmp;
}

inline void rdSwapEndian(unsigned short* v)
{
	unsigned char* x = (unsigned char*)v;
	rdSwapByte(x+0, x+1);
}

inline void rdSwapEndian(short* v)
{
	unsigned char* x = (unsigned char*)v;
	rdSwapByte(x+0, x+1);
}

inline void rdSwapEndian(unsigned int* v)
{
	unsigned char* x = (unsigned char*)v;
	rdSwapByte(x+0, x+3); rdSwapByte(x+1, x+2);
}

inline void rdSwapEndian(int* v)
{
	unsigned char* x = (unsigned char*)v;
	rdSwapByte(x+0, x+3); rdSwapByte(x+1, x+2);
}

inline void rdSwapEndian(float* v)
{
	unsigned char* x = (unsigned char*)v;
	rdSwapByte(x+0, x+3); rdSwapByte(x+1, x+2);
}

void rdRandomPointInConvexPoly(const float* pts, const int npts, float* areas,
							   const float s, const float t, float* out);

/// Counts the number of vertices in the polygon.
///  @param[in]		p	The polygon.
///  @param[in]		nvp	The total number of verts per polygon.
/// @return The number of vertices in the polygon.
inline int rdCountPolyVerts(const unsigned short* p, const int nvp)
{
	for (int i = 0; i < nvp; ++i)
		if (p[i] == RD_MESH_NULL_IDX)
			return i;
	return nvp;
}

template<typename TypeToRetrieveAs>
TypeToRetrieveAs* rdGetThenAdvanceBufferPointer(const unsigned char*& buffer, const rdSizeType distanceToAdvance)
{
	TypeToRetrieveAs* returnPointer = reinterpret_cast<TypeToRetrieveAs*>(buffer);
	buffer += distanceToAdvance;
	return returnPointer;
}

template<typename TypeToRetrieveAs>
TypeToRetrieveAs* rdGetThenAdvanceBufferPointer(unsigned char*& buffer, const rdSizeType distanceToAdvance)
{
	TypeToRetrieveAs* returnPointer = reinterpret_cast<TypeToRetrieveAs*>(buffer);
	buffer += distanceToAdvance;
	return returnPointer;
}


/// @}

#endif // RECASTDETOURCOMMON_H

///////////////////////////////////////////////////////////////////////////

// This section contains detailed documentation for members that don't have
// a source file. It reduces clutter in the main section of the header.

/**

@fn float rdTriArea2D(const float* a, const float* b, const float* c)
@par

The vertices are projected onto the xy-plane, so the z-values are ignored.

This is a low cost function than can be used for various purposes.  Its main purpose
is for point/line relationship testing.

In all cases: A value of zero indicates that all vertices are collinear or represent the same point.
(On the xy-plane.)

When used for point/line relationship tests, AB usually represents a line against which
the C point is to be tested.  In this case:

A positive value indicates that point C is to the left of line AB, looking from A toward B.<br/>
A negative value indicates that point C is to the right of lineAB, looking from A toward B.

When used for evaluating a triangle:

The absolute value of the return value is two times the area of the triangle when it is
projected onto the xy-plane.

A positive return value indicates:

<ul>
<li>The vertices are wrapped in the normal Detour wrap direction.</li>
<li>The triangle's 3D face normal is in the general up direction.</li>
</ul>

A negative return value indicates:

<ul>
<li>The vertices are reverse wrapped. (Wrapped opposite the normal Detour wrap direction.)</li>
<li>The triangle's 3D face normal is in the general down direction.</li>
</ul>

*/
