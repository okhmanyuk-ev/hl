#include "bsp_draw.h"

using namespace HL;

BspDraw::BspDraw(const BSPFile& bspfile, std::unordered_map<TexId, std::shared_ptr<skygfx::Texture>> _textures,
	const glm::mat4& model_matrix) :
	mTextures(_textures),
	mModelMatrix(model_matrix)
{
	auto& vertices = bspfile.getVertices();
	auto& edges = bspfile.getEdges();
	auto& faces = bspfile.getFaces();
	auto& surfedges = bspfile.getSurfEdges();
	auto& texinfos = bspfile.getTexInfos();
	auto& planes = bspfile.getPlanes();
	auto& textures = bspfile.getTextures();

	std::vector<skygfx::utils::Mesh::Vertex> my_vertices;

	std::unordered_map<TexId, std::vector<skygfx::utils::DrawVerticesCommand>> vertices_drawcalls;

	for (auto& face : faces)
	{
		auto texinfo = texinfos[face.texinfo];
		auto plane = planes[face.planenum];
		auto texture = textures[texinfo._miptex];

		float is = 1.0f / (float)texture.width;
		float it = 1.0f / (float)texture.height;

		uint32_t vertex_offset = static_cast<uint32_t>(my_vertices.size());
		uint32_t vertex_count = 0;

		for (int i = face.firstedge; i < face.firstedge + face.numedges; i++)
		{
			auto& surfedge = surfedges[i];
			auto& edge = edges[std::abs(surfedge)];
			auto& vertex = vertices[edge.v[surfedge < 0 ? 1 : 0]];

			auto v = skygfx::utils::Mesh::Vertex();

			v.pos = vertex;
			v.color = { 1.0f, 1.0f, 1.0f, 1.0f };

			v.normal = *(glm::vec3*)(&plane.normal);

			if (face.side)
				v.normal = -v.normal;
			
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

			v.texcoord.x = s * is;
			v.texcoord.y = t * it;
		
			if (vertex_count >= 3) // triangulation
			{
				vertex_count += 2;
				my_vertices.push_back(my_vertices[vertex_offset]);
				my_vertices.push_back(my_vertices[my_vertices.size() - 2]);
			}

			vertex_count += 1;
			my_vertices.push_back(v);
		}

		auto tex_id = texinfo._miptex;
		auto draw_command = skygfx::utils::DrawVerticesCommand{
			.vertex_count = vertex_count,
			.vertex_offset = vertex_offset
		};
		vertices_drawcalls[tex_id].push_back(draw_command);
	}

	mMesh.setVertices(my_vertices);

	std::vector<uint32_t> pixels = {
		Graphics::Color::ToUInt32(Graphics::Color::White),
		Graphics::Color::ToUInt32(Graphics::Color::Gray),
		Graphics::Color::ToUInt32(Graphics::Color::Gray),
		Graphics::Color::ToUInt32(Graphics::Color::White),
	};
	mDefaultTexture = std::make_shared<skygfx::Texture>(2, 2, skygfx::Format::Byte4, pixels.data());

	skygfx::utils::Mesh::Indices indices;

	for (const auto& [tex_id, draw_commands] : vertices_drawcalls)
	{
		skygfx::utils::DrawIndexedVerticesCommand draw_command;
		draw_command.index_offset = (uint32_t)indices.size();

		for (const auto& draw_command : draw_commands)
		{
			auto start = draw_command.vertex_offset;
			auto count = draw_command.vertex_count.value();
			for (uint32_t i = start; i < count + start; i++)
			{
				indices.push_back(i);
			}
		}

		draw_command.index_count = (uint32_t)indices.size() - draw_command.index_offset;

		auto model = skygfx::utils::Model();
		model.mesh = &mMesh;
		model.color_texture = mTextures.contains(tex_id) ? mTextures.at(tex_id).get() : mDefaultTexture.get();
		model.draw_command = draw_command;
		model.matrix = mModelMatrix;
		model.texture_address = skygfx::TextureAddress::Wrap;
		model.cull_mode = skygfx::CullMode::Back;
		model.depth_mode = skygfx::ComparisonFunc::LessEqual;
		mModels.push_back(model);
	}

	mMesh.setIndices(indices);
}

void BspDraw::draw(std::shared_ptr<skygfx::RenderTarget> target, const glm::vec3& pos,
	float yaw, float pitch, float fov, const glm::vec3& world_up)
{
	auto camera = skygfx::utils::PerspectiveCamera{
		.yaw = yaw,
		.pitch = pitch,
		.position = pos,
		.world_up = world_up,
		.fov = fov
	};

	RENDERER->setRenderTarget(target);
	skygfx::SetSampler(mTextures.empty() ? skygfx::Sampler::Nearest : skygfx::Sampler::Linear);
	RENDERER->clear();

	skygfx::utils::DrawScene(camera, mModels, mLights);
}
