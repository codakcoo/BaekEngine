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
		bool visible = true;
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

	private:
		std::vector<Entity> mEntities;
	};
}