#pragma once
#include "Scene/Transform.h"
#include "Renderer\Material.h"
#include <string>
#include <vector>
#include <memory>

namespace baek
{
	class Mesh;
	class Texture;

	struct Entity
	{
		std::string name;
		Transform transform;
		const Mesh* mesh = nullptr;
		Material* material = nullptr;		// 여러 엔티티가 공유 가능. 없으면 기본 머티리얼
		bool visible = true;
	};

	struct DirectionalLight
	{
		DirectX::XMFLOAT3 direction{ 0.4f, -1.0f, 0.6f };			// 빛이 진행하는 방향
		DirectX::XMFLOAT3 color{ 1, 1, 1};
		float intensity = 3.0f;
		float ambient = 0.15f;
	};

	class Scene
	{
	public:
		// 주의: 반환된 참조는 다음 Create 호출 전까지만 유효 (vector 재할당)
		Entity& Create(const std::string& name)
		{
			mEntities.push_back(Entity{name, });
			return mEntities.back();
		}

		Material& CreateMaterial(const std::string& name)
		{
			mMaterials.push_back(std::make_unique<Material>());
			mMaterials.back()->name = name;
			return *mMaterials.back();				// unique_ptr라서 주소가 바뀌지 않음
		}

		std::vector<Entity>& Entities() { return mEntities; }
		const std::vector<Entity>& Entities() const { return mEntities; }

	// Property
	public:
		DirectionalLight light;
	private:
		std::vector<Entity> mEntities;
		std::vector<std::unique_ptr<Material>> mMaterials;
	};
}