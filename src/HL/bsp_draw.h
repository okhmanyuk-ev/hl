#pragma once

#include <graphics/all.h>
#include <HL/bspfile.h>
#include <skygfx/ext.h>

namespace HL
{
	class BspDraw
	{
	public:
		BspDraw(const BSPFile& bspfile);

		void draw(std::shared_ptr<skygfx::RenderTarget> target, const glm::vec3& pos,
			float yaw, float pitch, const glm::mat4& model_matrix = glm::mat4(1.0f),
			const glm::vec3& world_up = { 0.0f, 0.0f, 1.0f },
			const std::unordered_map<int, std::shared_ptr<skygfx::Texture>>& textures = {});

		const auto& getLights() const { return mLights; }
		void setLights(const std::vector<skygfx::ext::Light> value) { mLights = value; }

	private:
		skygfx::ext::Mesh mMesh;
		using TexId = int;
		std::unordered_map<TexId, std::vector<skygfx::ext::DrawCommand>> mDrawcalls;
		std::vector<skygfx::ext::Light> mLights;
		std::shared_ptr<skygfx::Texture> mDefaultTexture;
	};
}
