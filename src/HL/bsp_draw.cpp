#include "bsp_draw.h"

using namespace HL;

BspDraw::BspDraw(const BSPFile& bspfile)
{
	mCamera = std::make_shared<Graphics::Camera3D>();
	mCamera->setWorldUp({ 0.0f, 0.0f, 1.0f });

	auto& vertices = bspfile.getVertices();
	auto& edges = bspfile.getEdges();
	auto& faces = bspfile.getFaces();
	auto& surfedges = bspfile.getSurfEdges();
	auto& texinfos = bspfile.getTexInfos();
	auto& planes = bspfile.getPlanes();

	for (auto& face : faces)
	{
		auto texinfo = texinfos[face.texinfo];
		auto plane = planes[face.planenum];

		Face f = {};

		f.start = mVertices.size();

		for (int i = face.firstedge; i < face.firstedge + face.numedges; i++)
		{
			auto& surfedge = surfedges[i];
			auto& edge = edges[std::abs(surfedge)];
			auto& vertex = vertices[edge.v[surfedge < 0 ? 1 : 0]];

			auto v = Vertex();

			auto gray = glm::linearRand(0.5f, 1.0f);

			v.pos = vertex;
			v.color = { gray, gray, gray, 1.0f };

			//v.normal = *(glm::vec3*)(&plane.normal);

			//if (face.side)
			//	v.normal = -v.normal;
			
			glm::vec3 ti0 = {
				texinfo.vecs[0][0],
				texinfo.vecs[0][1],
				texinfo.vecs[0][2]
			};

			glm::vec3 ti1 = {
				texinfo.vecs[1][0],
				texinfo.vecs[1][1],
				texinfo.vecs[1][2]
			};

			float s = glm::dot(v.pos, ti0) + texinfo.vecs[0][3];
			float t = glm::dot(v.pos, ti1) + texinfo.vecs[1][3];

			//v.tex.x = s * is;
			//v.tex.y = t * it;
		
			if (f.count >= 3) // triangulation 
			{
				f.count += 2;
				mVertices.push_back(mVertices[f.start]);
				mVertices.push_back(mVertices[mVertices.size() - 2]);
			}

			f.count += 1;
			mVertices.push_back(v);
		}

		mFaces.push_back(f);
	}
}

void BspDraw::draw(std::shared_ptr<skygfx::RenderTarget> target, const glm::vec3& pos, float yaw, float pitch)
{
	mCamera->setPosition(pos);
	mCamera->setYaw(yaw);
	mCamera->setPitch(pitch);
	mCamera->onFrame();

	auto view = mCamera->getViewMatrix();
	auto projection = mCamera->getProjectionMatrix();
	auto prev_batching = GRAPHICS->isBatching();

	GRAPHICS->setBatching(false);
	GRAPHICS->pushCleanState();
	GRAPHICS->pushViewMatrix(view);
	GRAPHICS->pushProjectionMatrix(projection);
	GRAPHICS->pushRenderTarget(target);
	GRAPHICS->pushDepthMode(skygfx::ComparisonFunc::Less);
	GRAPHICS->clear(glm::vec4{ 0.0f, 0.0f, 0.0f, 1.0f });
	
	static auto builder = Graphics::MeshBuilder();
	builder.begin();

	for (const auto& face : mFaces)
	{
		for (auto i = face.start; i < face.start + face.count; i++)
		{
			const auto& vertex = mVertices.at(i);

			builder.color(vertex.color);
			builder.vertex(vertex.pos);
		}
	}

	auto [vertices, count] = builder.end();

	GRAPHICS->draw(skygfx::Topology::TriangleList, vertices, count);
	GRAPHICS->pop(5);
	GRAPHICS->setBatching(prev_batching);
}
