#pragma once

#include <string>
#include <string_view>
#include <fstream>
#include <vector>

#include "wadfile.h"

#include <glm/glm.hpp>

// header
#define Q1BSP_VERSION		29		// quake1 regular version (beta is 28)
#define HLBSP_VERSION		30		// half-life regular version

#define MAX_MAP_HULLS		4

#define CONTENTS_ORIGIN		-7		// removed at csg time
#define CONTENTS_CLIP		-8		// changed to contents_solid
#define CONTENTS_CURRENT_0	-9
#define CONTENTS_CURRENT_90	-10
#define CONTENTS_CURRENT_180	-11
#define CONTENTS_CURRENT_270	-12
#define CONTENTS_CURRENT_UP	-13
#define CONTENTS_CURRENT_DOWN	-14

#define CONTENTS_TRANSLUCENT	-15

#define	LUMP_ENTITIES		0
#define	LUMP_PLANES		1
#define	LUMP_TEXTURES		2
#define	LUMP_VERTEXES		3
#define	LUMP_VISIBILITY		4
#define	LUMP_NODES		5
#define	LUMP_TEXINFO		6
#define	LUMP_FACES		7
#define	LUMP_LIGHTING		8
#define	LUMP_CLIPNODES		9
#define	LUMP_LEAFS		10
#define	LUMP_MARKSURFACES	11
#define	LUMP_EDGES		12
#define	LUMP_SURFEDGES		13
#define	LUMP_MODELS		14

#define	HEADER_LUMPS		15

#define CONTENTS_EMPTY		-1
#define CONTENTS_SOLID		-2
#define CONTENTS_WATER		-3
#define CONTENTS_SLIME		-4
#define CONTENTS_LAVA		-5
#define CONTENTS_SKY		-6

typedef struct lump_s
{
	int				fileofs;
	int				filelen;
} lump_t;

typedef struct dmodel_s
{
	glm::vec3		mins;
	glm::vec3		maxs;
	glm::vec3		origin;
	int				headnode[MAX_MAP_HULLS];
	int				visleafs;		// not including the solid leaf 0
	int				firstface, numfaces;
} dmodel_t;

typedef struct dheader_s
{
	int				version;
	lump_t			lumps[15];
} dheader_t;

typedef struct dmiptexlump_s
{
	int				_nummiptex;
	int				dataofs[4];
} dmiptexlump_t;

#define MAXTEXTURENAME 16
#define MIPLEVELS 4

typedef struct miptex_s
{
	char			name[MAXTEXTURENAME];
	unsigned		width;
	unsigned		height;
	unsigned		offsets[MIPLEVELS];
} miptex_t;

typedef struct dvertex_s
{
	float			point[3];
} dvertex_t;

typedef struct dplane_s
{
	glm::vec3		normal;
	float			dist;
	int				type;
} dplane_t;

typedef struct dnode_s
{
	int				planenum;
	short			children[2];
	short			mins[3];
	short			maxs[3];
	unsigned short	firstface;
	unsigned short	numfaces;
} dnode_t;

typedef struct dclipnode_s
{
	int				planenum;
	short			children[2];	// negative numbers are contents
} dclipnode_t;

typedef struct texinfo_s
{
	float			vecs[2][4];
	int				_miptex;
	int				flags;
} texinfo_t;

typedef struct dedge_s
{
	unsigned short	v[2];
} dedge_t;

typedef struct dface_s
{
	short			planenum;
	short			side;
	int				firstedge;
	short			numedges;
	short			texinfo;
	unsigned char	styles[4];
	int				lightofs;
} dface_t;

typedef struct dleaf_s
{
	int				contents;
	int				visofs;
	short			mins[3];
	short			maxs[3];
	unsigned short	firstmarksurface;
	unsigned short	nummarksurfaces;
	unsigned char	ambient_level[4];
} dleaf_t;

// ----------------------------------------------------------------

typedef struct mplane_s
{
	glm::vec3			normal;			// surface normal
	float			dist;			// closest appoach to origin
	uint8_t			type;			// for texture axis selection and fast side tests
	uint8_t			signbits;		// signx + signy<<1 + signz<<1
	uint8_t			pad[2];
} mplane_t;

typedef struct mnode_s
{
	// common with leaf
	int				contents;		// 0, to differentiate from leafs
	int				visframe;		// node needs to be traversed if current

	short			minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

	// node specific
	mplane_t		*plane;
	struct mnode_s	*children[2];

	unsigned short	firstsurface;
	unsigned short	numsurfaces;
} mnode_t;

typedef struct hull_s
{
	dclipnode_t		*clipnodes;
	mplane_t		*planes;
	int				firstclipnode;
	int				lastclipnode;
	glm::vec3			clip_mins, clip_maxs;
} hull_t;

typedef struct mleaf_s
{
	// common with node
	int				contents;		// wil be a negative contents number
	int				visframe;		// node needs to be traversed if current

	short			minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

	// leaf specific
	uint8_t			*compressed_vis;
	struct efrag_s	*efrags;

	uint32_t		firstmarksurface; // TODO: ????
	int				nummarksurfaces;
	int				key;			// BSP sequence number for leaf's contents
	uint8_t			ambient_sound_level[/*NUM_AMBIENTS*/3];
} mleaf_t;

// ---------------------------------------------------------------

typedef struct
{
	glm::vec3	normal;
	float	dist;
} plane_t;


typedef struct
{
	bool	allsolid;		// if true, plane is not valid
	bool	startsolid;		// if true, the initial point was in a solid area
	bool	inopen, inwater;
	float		fraction;		// time completed, 1.0 = didn't hit anything
	glm::vec3		endpos;			// final position
	plane_t		plane;			// surface normal at impact
	//edict_t	*	ent;			// entity the surface is on
	int			hitgroup;		// 0 == generic, non zero is specific body part
} trace_t;

class BSPFile
{
public:
	struct Entity
	{
		std::vector<std::pair<std::string, std::string>> args;
		std::string* getValueOf(std::string_view field);
	};

public:
	void loadFromFile(const std::string& fileName, bool loadWad);

public:
	std::vector<Entity*> findEntity(std::string_view className);
	
	auto& getVertices() { return m_Vertices; }
	auto& getEdges() { return m_Edges; }
	auto& getFaces() { return m_Faces; }
	auto& getPlanes() { return m_Planes; }
	auto& getSurfEdges() { return m_SurfEdges; }
	auto& getTextures() { return m_Textures; }
	auto& getTexInfos() { return m_TexInfos; }
	auto& getEntities() { return m_Entities; }
	auto& getWADFiles() { return m_WADFiles; }
	auto& getLightData() { return m_LightData; }
	auto& getModels() { return m_Models; }

	void makeHull0();

	trace_t traceLine(const glm::vec3& start, const glm::vec3& end) const;
	bool recursiveHullCheck(const hull_t& hull, int num, float p1f, float p2f, const glm::vec3& p1, const glm::vec3& p2, trace_t& trace) const;
	int hullPointContents(const hull_t& hull, int num, const glm::vec3& point) const;

private:
	std::vector<dvertex_t> m_Vertices;
	std::vector<dedge_t> m_Edges;
	std::vector<dface_t> m_Faces;
	std::vector<mplane_t> m_Planes;
	std::vector<int32_t> m_SurfEdges;
	std::vector<miptex_t> m_Textures;
	std::vector<texinfo_t> m_TexInfos;
	std::vector<Entity> m_Entities;
	std::vector<WADFile*> m_WADFiles;
	std::vector<uint8_t> m_LightData;
	std::vector<mnode_t> m_Nodes;
	std::vector<mleaf_t> m_Leafs;
	std::vector<dmodel_t> m_Models;

	hull_t m_Hulls[MAX_MAP_HULLS];
};