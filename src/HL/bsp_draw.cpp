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

		skygfx::utils::AddCommands(mDrawcalls[tex_id], {
			skygfx::utils::commands::Draw(draw_command)
		});
	}

	mMesh.setVertices(my_vertices);

	std::vector<uint32_t> pixels = {
		Graphics::Color::ToUInt32(Graphics::Color::White),
		Graphics::Color::ToUInt32(Graphics::Color::Gray),
		Graphics::Color::ToUInt32(Graphics::Color::Gray),
		Graphics::Color::ToUInt32(Graphics::Color::White),
	};
	mDefaultTexture = std::make_shared<skygfx::Texture>(2, 2, skygfx::Format::Byte4, pixels.data());
}

void BspDraw::draw(std::shared_ptr<skygfx::RenderTarget> target, const glm::vec3& pos,
	float yaw, float pitch, const glm::mat4& model_matrix, float fov, const glm::vec3& world_up,
	const std::unordered_map<int, std::shared_ptr<skygfx::Texture>>& textures)
{
	auto camera = skygfx::utils::PerspectiveCamera{
		.yaw = yaw,
		.pitch = pitch,
		.position = pos,
		.world_up = world_up,
		.fov = fov
	};

	RENDERER->setRenderTarget(target);
	skygfx::SetDepthMode(skygfx::ComparisonFunc::LessEqual);
	skygfx::SetCullMode(skygfx::CullMode::Back);
	skygfx::SetSampler(textures.empty() ? skygfx::Sampler::Nearest : skygfx::Sampler::Linear);
	skygfx::SetTextureAddress(skygfx::TextureAddress::Wrap);
	RENDERER->clear();

	const static std::vector<skygfx::utils::Light> DefaultLights = {
		skygfx::utils::effects::DirectionalLight{}
	};

	const auto& lights = mLights.empty() ? DefaultLights : mLights;

	skygfx::utils::Commands cmds = {
		skygfx::utils::commands::SetBlendMode(skygfx::BlendStates::NonPremultiplied),
		skygfx::utils::commands::SetSampler(skygfx::Sampler::Linear),
		skygfx::utils::commands::SetMesh(&mMesh),
		skygfx::utils::commands::SetCamera(camera),
		skygfx::utils::commands::SetModelMatrix(model_matrix)
	};

	for (const auto& light : lights)
	{
		std::visit(cases{
			[&](const auto& value) {
				skygfx::utils::AddCommands(cmds, {
					skygfx::utils::commands::SetEffect(value)
				});
			}
		}, light);

		for (const auto& [tex_id, draw_commands] : mDrawcalls)
		{
			skygfx::utils::AddCommands(cmds, {
				skygfx::utils::commands::SetColorTexture(textures.contains(tex_id) ? textures.at(tex_id).get() : mDefaultTexture.get()),
				skygfx::utils::commands::InsertSubcommands(&draw_commands)
			});
		}
		skygfx::utils::AddCommands(cmds, {
			skygfx::utils::commands::SetBlendMode(skygfx::BlendStates::Additive)
		});
	}

	skygfx::utils::ExecuteCommands(cmds);
}
