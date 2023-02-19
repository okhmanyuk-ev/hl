#pragma once

#include <graphics/all.h>
#include <HL/bspfile.h>

namespace HL
{
	class BspDraw
	{
	public:
		struct Face
		{
			int start;
			int count;
		};

	public:
		BspDraw(const BSPFile& bspfile);

		void draw(std::shared_ptr<skygfx::RenderTarget> target, const glm::vec3& pos, float yaw, float pitch);

	private:
		using Vertex = skygfx::Vertex::PositionColor;

		std::shared_ptr<Graphics::Camera3D> mCamera;
		std::vector<Face> mFaces;
		std::vector<Vertex> mVertices;
	};
}
