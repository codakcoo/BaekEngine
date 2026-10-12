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
		float iblIntensity = 1.0f;
		float shadowDistance = 50.0f;								// 이 거리까지만 그림자 계산
		float cascadeLambda = 0.75f;								// 구간 분할: 0 = 균등, 1 = 로그 (가까운 곳에 몰아줌)
		float shadowBias = 0.0005f;									// 깊이 비교 여유값
		float normalOffset = 0.02f;									// 단위: 섀도우 맵 텍셀 (구간 크기에 맞춰 자동으로 커짐)
		bool showCascade = false;									// 구간별 색 표시 (디버그)
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