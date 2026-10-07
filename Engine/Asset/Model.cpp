#include "Model.h"
#include "Renderer\Renderer.h"
#include "Renderer\Mesh.h"
#include "Scene\Scene.h"
#include <stdexcept>
#include <utility>

#pragma warning(push, 0)					// 서드파티헤더 경고 끄기
#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"
#pragma warning(pop)

using namespace DirectX;

namespace baek
{
	namespace
	{
		// glTF 프리미티브 하나 -> Mesh. 오른손 좌표계를 왼손 좌표계로 바꾼다 (Z 반전)
		std::unique_ptr<Mesh> BuildMesh(ID3D12Device* device, const cgltf_primitive& prim)
		{
			const cgltf_accessor* posAcc = nullptr;
			const cgltf_accessor* nrmAcc = nullptr;
			const cgltf_accessor* tanAcc = nullptr;
			const cgltf_accessor* uvAcc = nullptr;

			for (cgltf_size a = 0; a < prim.attributes_count; ++a)
			{
				const cgltf_attribute& attr = prim.attributes[a];
				switch (attr.type)
				{
				case cgltf_attribute_type_position:
					posAcc = attr.data;
					break;
				case cgltf_attribute_type_normal:
					nrmAcc = attr.data;
					break;
				case cgltf_attribute_type_tangent:
					tanAcc = attr.data;
					break;
				case cgltf_attribute_type_texcoord:
					uvAcc = attr.data;
					break;
				}
			}
			if(!posAcc) return nullptr;

			std::vector<Vertex> verts(posAcc->count);
			for (cgltf_size v = 0; v < posAcc->count; ++v)
			{
				float p[3] = { 0, 0, 0 }, n[3] = { 0, 1, 0 }, t[4] = { 1, 0, 0, 1 },  uv[2] = { 0, 0};
				cgltf_accessor_read_float(posAcc, v, p, 3);
				if (nrmAcc) cgltf_accessor_read_float(nrmAcc, v, n, 3);
				if (tanAcc) cgltf_accessor_read_float(tanAcc, v, t, 4);
				if (uvAcc) cgltf_accessor_read_float(uvAcc, v, uv, 2);

				Vertex& out = verts[v];
				out.pos		= { p[0], p[1], -p[2] };
				out.normal	= { n[0], n[1], -n[2] };
				out.tangent = { t[0], t[1], -t[2], -t[3]};				// 거울 반전하면 비탄젠트 부호도 뒤집힘
				out.uv		= { uv[0], uv[1] };							// glTF와 D3D 모두 좌상단 원점
				out.color	= { 1, 1, 1 };
			}

			std::vector<uint32_t> idx;
			if (prim.indices)
			{
				idx.resize(prim.indices->count);
				for(cgltf_size i = 0; i < prim.indices->count; ++i)
					idx[i] = (uint32_t)cgltf_accessor_read_index(prim.indices, i);
			}
			// 인덱스가 없는 모델: 정점 순서 그대로
			else
			{
				idx.resize(verts.size());
				for (size_t i = 0; i < idx.size(); ++i) idx[i] = (uint32_t)i;
			}

			// Z를 뒤집으면 삼각형이 도는 방향도 반대가 되므로 다시 뒤집는다.
			for(size_t i = 0; i + 2 < idx.size(); i += 3)
				std::swap(idx[i + 1], idx[i + 2]);

			auto mesh = std::make_unique<Mesh>();
			mesh->Create(device, verts, idx);
			return mesh;
		}
	}

	Model::Model() = default;
	Model::~Model() = default;
	
	void Model::Load(Renderer& renderer, Scene& scene, const std::string& path, const DirectX::XMFLOAT3& offset)
	{
		cgltf_options options{};
		cgltf_data* data = nullptr;

		if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success)
			throw std::runtime_error("glTF parse failed: " + path);
		if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success)
			throw std::runtime_error("glTF buffer load failed: " + path);

		ID3D12Device* device = renderer.GetDevice().Get();

		// 1) 메시: glTF 메시 하나는 프리미티브(머티리얼 단위 조각) 여러 개로 이루어진다
		std::vector<std::vector<Mesh*>> meshTable(data->meshes_count);
		for (cgltf_size m = 0; m < data->meshes_count; ++m)
		{
			const cgltf_mesh& gm = data->meshes[m];
			for (cgltf_size p = 0; p < gm.primitives_count; ++p)
			{
				Mesh* built = nullptr;
				if (gm.primitives[p].type == cgltf_primitive_type_triangles)
				{
					if (auto mesh = BuildMesh(device, gm.primitives[p]))
					{
						built = mesh.get();
						mMeshes.push_back(std::move(mesh));
					}
				}
				meshTable[m].push_back(built);
			}
		}

		// 2) 노드: 메시가 달린 노드마다 엔티티 생성
		const XMMATRIX flipZ = XMMatrixScaling(1.0f, 1.0f, -1.0f);
		for (cgltf_size n = 0; n < data->nodes_count; ++n)
		{
			const cgltf_node& node = data->nodes[n];
			if(!node.mesh) continue;

			// 부모 변환까지 누적된 월드 행렬 (메모리 배치가 XMFLOAT4X4와 같음)
			cgltf_float wm[16];
			cgltf_node_transform_world(&node, wm);
			XMFLOAT4X4 f;
			memcpy(&f, wm, sizeof(f));

			// 정점을 Z 반전했으므로 행렬도 같은 기준으로 변환: M' = S * M * S
			const XMMATRIX world = flipZ * XMLoadFloat4x4(&f) * flipZ;

			const size_t meshIndex = (size_t)(node.mesh - data->meshes);
			const std::string baseName = node.name ? node.name : (node.mesh->name ? node.mesh->name : "Node");

			for (size_t p = 0; p < meshTable[meshIndex].size(); ++p)
			{
				Mesh* mesh = meshTable[meshIndex][p];
				if(!mesh) continue;

				Entity& e = scene.Create(meshTable[meshIndex].size() > 1
											? baseName + "_" + std::to_string(p) : baseName);
				e.mesh = mesh;
				e.transform = Transform::FromMatrix(world);
				e.transform.position.x += offset.x;
				e.transform.position.y += offset.y;
				e.transform.position.z += offset.z;
			}
		}
		cgltf_free(data);
	}
	
	void Model::Shutdown()
	{
		for(auto& m : mMeshes) m->Shutdown();
		mMeshes.clear();
	}
}
