#include "bspfile.h"
#include <common/buffer_helpers.h>
#include <algorithm>
#include <sstream>
#include <console/system.h>
#include <common/bitbuffer.h>
#include "utils.h"

const std::string& BSPFile::Entity::getClassName() const
{
	return args.at("classname");
}

void BSPFile::loadFromFile(const std::string& fileName, bool loadWad)
{
	sky::BitBuffer bf;
    auto asset = Platform::Asset(fileName, HL_ASSET_STORAGE);

	bf.write(asset.getMemory(), asset.getSize());
	bf.toStart();

	// header

	auto header = bf.read<dheader_t>();

	// vertices

	mVertices.resize(header.lumps[LUMP_VERTEXES].filelen / sizeof(glm::vec3));
	bf.setPosition(header.lumps[LUMP_VERTEXES].fileofs);
	bf.read(mVertices.data(), header.lumps[LUMP_VERTEXES].filelen);

	// edges

	mEdges.resize(header.lumps[LUMP_EDGES].filelen / sizeof(dedge_t));
	bf.setPosition(header.lumps[LUMP_EDGES].fileofs);
	bf.read(mEdges.data(), header.lumps[LUMP_EDGES].filelen);

	// faces

	mFaces.resize(header.lumps[LUMP_FACES].filelen / sizeof(dface_t));
	bf.setPosition(header.lumps[LUMP_FACES].fileofs);
	bf.read(mFaces.data(), header.lumps[LUMP_FACES].filelen);

	// surfedges

	mSurfEdges.resize(header.lumps[LUMP_SURFEDGES].filelen / sizeof(int32_t));
	bf.setPosition(header.lumps[LUMP_SURFEDGES].fileofs);
	bf.read(mSurfEdges.data(), header.lumps[LUMP_SURFEDGES].filelen);

	// planes

	mPlanes.resize(header.lumps[LUMP_PLANES].filelen / sizeof(dplane_t));
	bf.setPosition(header.lumps[LUMP_PLANES].fileofs);

	//bf.read(m_Planes.data(), header.lumps[LUMP_PLANES].filelen);

	for (auto& plane : mPlanes)
	{
		auto p = bf.read<dplane_t>();

		plane.normal = p.normal;
		plane.dist = p.dist;
		plane.type = p.type;

	}

	// leafs 

	mLeafs.resize(header.lumps[LUMP_LEAFS].filelen / sizeof(dleaf_t));
	bf.setPosition(header.lumps[LUMP_LEAFS].fileofs);

	for (auto& leaf : mLeafs)
	{
		auto l = bf.read<dleaf_t>();

		for (int j = 0; j < 3; j++)
		{
			leaf.minmaxs[j] = l.mins[j];
			leaf.minmaxs[j + 3] = l.maxs[j];
		}

		leaf.contents = l.contents;
		leaf.firstmarksurface = l.firstmarksurface;
		leaf.nummarksurfaces = l.nummarksurfaces;

		auto k = l.visofs;

		if (k == -1)
		{
			leaf.compressed_vis = nullptr;
		}
		else
		{
			//leaf.compressed_vis = @visibility[k];
		}

		leaf.efrags = nullptr;

		for (int j = 0; j < 4; j++)
		{
			leaf.ambient_sound_level[j] = l.ambient_level[j];
		}
	}

	// nodes

	mNodes.resize(header.lumps[LUMP_NODES].filelen / sizeof(dnode_t));
	bf.setPosition(header.lumps[LUMP_NODES].fileofs);
	//bf.read(m_Nodes.data(), header.lumps[LUMP_NODES].filelen);

	for (auto& node : mNodes)
	{
		auto n = bf.read<dnode_t>();

		for (int j = 0; j < 3; j++)
		{
			node.minmaxs[j] = n.mins[j];
			node.minmaxs[j + 3] = n.maxs[j];
		}

		node.plane = &mPlanes[n.planenum];
		node.firstsurface = n.firstface;
		node.numsurfaces = n.numfaces;

		for (int j = 0; j < 2; j++)
		{
			short k = n.children[j];

			if (k >= 0)
			{
				node.children[j] = &mNodes[k];
			}
			else
			{
				node.children[j] = (mnode_s*)&mLeafs[(uint32_t)(-1 - k)];
			}
		}
	}

	// texinfos

	mTexInfos.resize(header.lumps[LUMP_TEXINFO].filelen / sizeof(texinfo_t));
	bf.setPosition(header.lumps[LUMP_TEXINFO].fileofs);
	bf.read(mTexInfos.data(), header.lumps[LUMP_TEXINFO].filelen);

	// lighting

	mLightData.resize(header.lumps[LUMP_LIGHTING].filelen);
	bf.setPosition(header.lumps[LUMP_LIGHTING].fileofs);
	bf.read(mLightData.data(), header.lumps[LUMP_LIGHTING].filelen);

	// models

	mModels.resize(header.lumps[LUMP_MODELS].filelen / sizeof(dmodel_t));
	bf.setPosition(header.lumps[LUMP_MODELS].fileofs);
	bf.read(mModels.data(), header.lumps[LUMP_MODELS].filelen);

	// clipnodes

	mClipNodes.resize(header.lumps[LUMP_CLIPNODES].filelen / sizeof(dclipnode_t));
	bf.setPosition(header.lumps[LUMP_CLIPNODES].fileofs);
	bf.read(mClipNodes.data(), header.lumps[LUMP_CLIPNODES].filelen);

	// textures

	bf.setPosition(header.lumps[LUMP_TEXTURES].fileofs);

	{
		auto m = (dmiptexlump_t *)(void*)((size_t)bf.getMemory() + header.lumps[LUMP_TEXTURES].fileofs);

		for (int i = 0; i < m->_nummiptex; i++)
		{
			if (m->dataofs[i] == -1)
				continue;

			auto mt = (miptex_t*)((char*)m + m->dataofs[i]);

			mTextures.push_back(*mt);

			//std::cout << "Name: " << mt->name << ", Width: " << mt->width 
			//	<< ", Height: " << mt->height << std::endl;

		}
	}

	{
		bf.setPosition(header.lumps[LUMP_ENTITIES].fileofs);

		std::string entities = "";

		for (int i = 0; i < header.lumps[LUMP_ENTITIES].filelen; i++)
			entities.push_back(bf.read<char>());

		while (entities.find("}") != std::string::npos)
		{
			entities = entities.substr(entities.find("{") + 1);

			auto s = entities.substr(0, entities.find("}"));

			Entity e;

			while (s.find("\"") != std::string::npos)
			{
				s = s.substr(s.find("\"") + 1);

				auto key = s.substr(0, s.find("\""));

				s = s.substr(s.find("\"") + 1);
				s = s.substr(s.find("\"") + 1);

				auto value = s.substr(0, s.find("\""));

				s = s.substr(s.find("\"") + 1);

				assert(!e.args.contains(key));
				e.args.insert({ key, value });
			}

			mEntities.push_back(e);

			entities = entities.substr(entities.find("}") + 1);
		}
	}

	for (const auto& entity : mEntities)
	{
		for (const auto& [key, value] : entity.args)
		{
			sky::Log("key: {}, value: {}", key, value);
		}
		sky::Log("----------------------------------------");
	}

	auto split = [](const std::string& s, char delimiter) -> std::vector<std::string>
	{
		std::vector<std::string> tokens;
		std::string token;
		std::istringstream tokenStream(s);
		while (std::getline(tokenStream, token, delimiter))
		{
			tokens.push_back(token);
		}
		return tokens;
	};

	auto str = Console::System::MakeTokensFromString(findEntity("worldspawn")[0]->args.at("wad"));

	auto wads = split(str[0], ';');

	if (loadWad)
	{
		for (auto& wad : wads)
		{
			auto filename = wad.substr(wad.find_last_of('\\') + 1);

			if (filename.size() == 0)
				continue;

			WADFile* wadfile = new WADFile();

			wadfile->loadFromFile(filename);

			mWADFiles.push_back(wadfile);
		}
	}

	makeHull0();

	// set up the submodels (FIXME: this is confusing)
	for (int i = 0; i < mModels.size(); i++)
	{
		//auto bm = &m_Models[i];

	//	m_Hulls[0].firstclipnode = bm->headnode[0];

		for (int j = 1; j < MAX_MAP_HULLS; j++)
		{
	//		m_Hulls[j].firstclipnode = bm->headnode[j];
			//m_Hulls[j].lastclipnode = m_mod->numclipnodes - 1;
		}

		/*mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		mod->maxs[0] = bm->maxs[0];
		mod->maxs[2] = bm->maxs[2];
		mod->maxs[1] = bm->maxs[1];

		mod->mins[0] = bm->mins[0];
		mod->mins[1] = bm->mins[1];
		mod->mins[2] = bm->mins[2];

		mod->radius = RadiusFromBounds(mod->mins, mod->maxs);
		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels - 1)
		{
			char name[10];
			Q_snprintf(name, ARRAYSIZE(name), "*%i", i + 1);

			loadmodel = Mod_FindName(0, name);
			*loadmodel = *mod;

			Q_strncpy(loadmodel->name, name, sizeof(loadmodel->name) - 1);
			loadmodel->name[sizeof(loadmodel->name) - 1] = 0;
			mod = loadmodel;
		}*/
		mModelsMap.insert({ fmt::format("*{}", i + 1), &mModels.at(i) });
	}
}

std::vector<BSPFile::Entity*> BSPFile::findEntity(std::string_view className)
{
	std::vector<BSPFile::Entity*> result = { };

	for (auto& entity : mEntities)
	{
		if (entity.getClassName() != className)
			continue;

		result.push_back(&entity);
	}

	return result;
}

void BSPFile::makeHull0()
{
	mnode_t *in, *child;
	dclipnode_t *out;
	int i, j, count;

	auto& hull = m_Hulls[0];

	in = mNodes.data();
	count = (int)mNodes.size();
	out = (dclipnode_t *)malloc(count * sizeof(*out));//(dclipnode_t *)Mem_ZeroMalloc(count * sizeof(*out));

	hull.clipnodes = out;
	hull.firstclipnode = 0;
	hull.lastclipnode = count - 1;
	hull.planes = mPlanes.data();

	for (i = 0; i < count; i++, out++, in++)
	{
		out->planenum = in->plane - mPlanes.data();
		for (j = 0; j < 2; j++)
		{
			child = in->children[j];

			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - mNodes.data();
		}
	}
}

trace_t BSPFile::traceLine(const glm::vec3& start, const glm::vec3& end, const std::set<int>& models) const
{
	trace_t result;
	result.fraction = 1.0f;
	result.endpos = end;
	result.allsolid = true;
	result.startsolid = false;

	bool b = recursiveHullCheck(m_Hulls[0], m_Hulls[0].firstclipnode, 0.0f, 1.0f, start, end, result);

	if (!result.startsolid && !result.allsolid)
	{
		for (const auto& index : models)
		{
			auto trace = result;

			auto& model = mModels.at(index);

			auto model_start = start - model.origin;
			auto model_end = end - model.origin;

			recursiveHullCheck(m_Hulls[0], model.headnode[0], 0.0f, 1.0f, model_start, model_end, trace);

			trace.endpos += model.origin;

			if (trace.fraction < result.fraction)
				result = trace;
		}
	}

	if (result.allsolid)
		result.startsolid = true;

	if (result.startsolid)
	{
		result.fraction = 0.0f;
		result.endpos = start;
	}

	return result;
}

bool BSPFile::recursiveHullCheck(const hull_t& hull, int num, float p1f, float p2f, const glm::vec3& p1, const glm::vec3& p2, trace_t& trace) const
{
	const float Epsilon = 0.03125f;

	if (num >= 0)
	{
		assert(num >= hull.firstclipnode);
		assert(num <= hull.lastclipnode);
		assert(hull.planes);

		const auto& node = hull.clipnodes[num];
		const auto& plane = hull.planes[node.planenum];

		float t1 = 0.0f;
		float t2 = 0.0f;

		if (plane.type >= 3)
		{
			t1 = glm::dot(p1, plane.normal) - plane.dist;
			t2 = glm::dot(p2, plane.normal) - plane.dist;
		}
		else
		{
			t1 = p1[plane.type] - plane.dist;
			t2 = p2[plane.type] - plane.dist;
		}

		if (t1 >= 0.0f && t2 >= 0.0f)
			return recursiveHullCheck(hull, node.children[0], p1f, p2f, p1, p2, trace);

		float midf = 0.0;

		if (t1 >= 0.0f)
			midf = t1 - Epsilon;
		else if (t2 < 0.0f)
			return recursiveHullCheck(hull, node.children[1], p1f, p2f, p1, p2, trace);
		else
			midf = t1 + Epsilon;

		midf = midf / (t1 - t2);

		if (std::isnan(midf))
			return false;

		midf = std::clamp(midf, 0.0f, 1.0f);

		auto frac = (p2f - p1f) * midf + p1f;
		auto mid = (p2 - p1) * midf + p1;
		auto side = t1 < 0.0f ? 1 : 0;

		if (!recursiveHullCheck(hull, node.children[side], p1f, frac, p1, mid, trace))
			return false;

		if (hullPointContents(hull, node.children[side ^ 1], mid) != CONTENTS_SOLID)
			return recursiveHullCheck(hull, node.children[side ^ 1], frac, p2f, mid, p2, trace);

		if (trace.allsolid)
			return false;

		if (side)
		{
			trace.plane.normal = -plane.normal;
			trace.plane.dist = -plane.dist;
		}
		else
		{
			trace.plane.normal = plane.normal;
			trace.plane.dist = plane.dist;
		}

		while (hullPointContents(hull, hull.firstclipnode, mid) == CONTENTS_SOLID)
		{
			midf -= 0.1f;

			if (midf < 0.0f)
				break;

			frac = (p2f - p1f) * midf + p1f;
			mid = (p2 - p1) * midf + p1;
		}

		trace.fraction = frac;
		trace.endpos = mid;
		return false;
	}

	if (num == CONTENTS_SOLID)
	{
		trace.startsolid = true;
	}
	else
	{
		trace.allsolid = false;

		if (num == CONTENTS_EMPTY)
			trace.inopen = true;
		else if (num != CONTENTS_TRANSLUCENT)
			trace.inwater = true;
	}

	return true;
}

int BSPFile::hullPointContents(const hull_t& hull, int num, const glm::vec3& point) const
{
	while (num >= 0)
	{
		assert(num >= hull.firstclipnode);
		assert(num <= hull.lastclipnode);

		auto& node = hull.clipnodes[num];
		auto& plane = hull.planes[node.planenum];

		float d = 0.0f;

		if (plane.type >= 3)
			d = glm::dot(point, plane.normal) - plane.dist;
		else
			d = point[plane.type] - plane.dist;

		num = node.children[d < 0 ? 1 : 0];
	}

	return num;
}

void BSPFile::setModelOrigin(int index, const glm::vec3& origin)
{
	mModels[index].origin = origin;
}
