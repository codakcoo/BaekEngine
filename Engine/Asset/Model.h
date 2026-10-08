#pragma	once
#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

namespace baek
{
	class Renderer;
	class Scene;
	class Mesh;
	class Texture;

	// glTF 파일 하나. 메시(나중에는 텍스처도)를 소융하고, 씬에 엔티티를 만들어 넣는다.
	// 씬이 이 모델의 메시를 참조하므로, 씬을 쓰는 동안 Model이 살아 있어야 한다.
	class Model
	{
	public:
		Model();
		~Model();

		void Load(Renderer& renderer, Scene& scene, const std::string& path, const DirectX::XMFLOAT3& offset = { 0, 0, 0 });
		void Shutdown();

	private:
		std::vector<std::unique_ptr<Mesh>> mMeshes;
		std::vector<std::unique_ptr<Texture>> mTextures;
	};
}