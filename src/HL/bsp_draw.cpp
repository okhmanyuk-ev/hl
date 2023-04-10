#include "bsp_draw.h"

using namespace HL;

BspDraw::BspDraw(const BSPFile& bspfile)
{
	auto& vertices = bspfile.getVertices();
	auto& edges = bspfile.getEdges();
	auto& faces = bspfile.getFaces();
	auto& surfedges = bspfile.getSurfEdges();
	auto& texinfos = bspfile.getTexInfos();
	auto& planes = bspfile.getPlanes();
	auto& textures = bspfile.getTextures();

	std::vector<skygfx::utils::Mesh::Vertex> my_vertices;

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

		skygfx::utils::Draw(mDrawcalls[tex_id], draw_command);
	}

	mMesh.setVertices(my_vertices);

	std::vector<uint32_t> pixels = {
		Graphics::Color::ToUInt32(Graphics::Color::White),
		Graphics::Color::ToUInt32(Graphics::Color::Gray),
		Graphics::Color::ToUInt32(Graphics::Color::Gray),
		Graphics::Color::ToUInt32(Graphics::Color::White),
	};
	mDefaultTexture = std::make_shared<skygfx::Texture>(2, 2, 4, pixels.data());
}

void BspDraw::draw(std::shared_ptr<skygfx::RenderTarget> target, const glm::vec3& pos,
	float yaw, float pitch, const glm::mat4& model_matrix, const glm::vec3& world_up,
	const std::unordered_map<int, std::shared_ptr<skygfx::Texture>>& textures)
{
	auto camera = skygfx::utils::PerspectiveCamera{
		.yaw = yaw,
		.pitch = pitch,
		.position = pos,
		.world_up = world_up
	};

	RENDERER->setRenderTarget(target);
	RENDERER->setDepthMode(skygfx::ComparisonFunc::LessEqual);
	RENDERER->setCullMode(skygfx::CullMode::Back);
	if (textures.empty())
	{
		RENDERER->setSampler(skygfx::Sampler::Nearest);
	}
	else
	{
		RENDERER->setSampler(skygfx::Sampler::Linear);
	}
	RENDERER->setTextureAddressMode(skygfx::TextureAddress::Wrap);
	RENDERER->clear();

	const static std::vector<skygfx::utils::Light> DefaultLights = {
		skygfx::utils::effects::DirectionalLight{}
	};

	const auto& lights = mLights.empty() ? DefaultLights : mLights;

	RENDERER->setBlendMode(skygfx::BlendStates::NonPremultiplied);

	skygfx::utils::Commands cmds;
	skygfx::utils::SetSampler(cmds, skygfx::Sampler::Linear);
	skygfx::utils::SetMesh(cmds, &mMesh);
	skygfx::utils::SetCamera(cmds, camera);
	skygfx::utils::SetModelMatrix(cmds, model_matrix);

	for (const auto& light : lights)
	{
		std::visit(cases{
			[&](const auto& value) {
				skygfx::utils::SetEffect(cmds, value);
			}
		}, light);

		for (const auto& [tex_id, draw_commands] : mDrawcalls)
		{
			skygfx::utils::SetColorTexture(cmds, textures.contains(tex_id) ? textures.at(tex_id).get() : mDefaultTexture.get());
			skygfx::utils::InsertSubcommands(cmds, &draw_commands);
		}

		skygfx::utils::Callback(cmds, []{
			RENDERER->setBlendMode(skygfx::BlendStates::Additive);	
		});
	}

	skygfx::utils::ExecuteCommands(cmds);
}
