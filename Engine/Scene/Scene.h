#pragma once
#include "Scene/Transform.h"
#include <string>
#include <vector>

namespace baek
{
	class Mesh;

	struct Entity
	{
		std::string name;
		Transform transform;
		const Mesh* mesh = nullptr;
		DirectX::XMFLOAT3 color{ 1, 1, 1 };		// 베이스 컬러
		bool visible = true;
	};

	struct DirectionalLight
	{
		DirectX::XMFLOAT3 direction{ 0.4f, -1.0f, 0.6f };			// 빛이 진행하는 방향
		DirectX::XMFLOAT3 color{ 1, 1, 1};
		float intensity = 1.0f;
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

		std::vector<Entity>& Entities() { return mEntities; }
		const std::vector<Entity>& Entities() const { return mEntities; }

	// Property
	public:
		DirectionalLight light;
	private:
		std::vector<Entity> mEntities;
	};
}