#include "Model.h"
#include "Renderer\Renderer.h"
#include "Renderer\Mesh.h"
#include "Scene\Scene.h"
#include "Renderer\Texture.h"
#include "Renderer\Material.h"

#include <stdexcept>
#include <utility>
#include <filesystem>
#include <map>
#include <cmath>
#include <cstring>

#pragma warning(push, 0)					// 서드파티헤더 경고 끄기
#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"
#pragma warning(pop)

using namespace DirectX;

namespace baek
{
	namespace
	{
		// glTF의 색상 값은 선형이고, 엔진의 Material::baseColor는 sRGB(색상 피커 기준)라서 변환
		float LinearToSrgbF(float c)
		{
			return powf(c < 0.0f ? 0.0f : c, 1.0f / 2.2f);
		}

		// 파일에 탄젠트가 없을 때: 삼각형의 변과 UV 변화량으로부터 계산
		void ComputeTangents(std::vector<Vertex>& verts, std::vector<uint32_t>& idx)
		{
			std::vector<XMFLOAT3> tanSum(verts.size(), XMFLOAT3( 0, 0, 0 ));
			std::vector<XMFLOAT3> bitSum(verts.size(), XMFLOAT3( 0, 0, 0 ));
			auto add = [](XMFLOAT3& dst, FXMVECTOR v) { XMStoreFloat3(&dst, XMLoadFloat3(&dst) + v); };

			// 1) 삼각형마다 T(U 방향), B(V 방향)를 구해 세 정점에 누적
			for (size_t i = 0; i + 2 < idx.size(); i += 3)
			{
				const uint32_t i0 = idx[i], i1 = idx[i+1], i2 = idx[i+2];
				const XMVECTOR p0 = XMLoadFloat3(&verts[i0].pos);
				const XMVECTOR e1 = XMLoadFloat3(&verts[i1].pos) - p0;
				const XMVECTOR e2 = XMLoadFloat3(&verts[i2].pos) - p0;

				const float du1 = verts[i1].uv.x - verts[i0].uv.x, dv1 = verts[i1].uv.y - verts[i0].uv.y;
				const float du2 = verts[i2].uv.x - verts[i0].uv.x, dv2 = verts[i2].uv.y - verts[i0].uv.y;

				const float det = du1 * dv2 - du2 * dv1;
				if(fabsf(det) < 1e-12f) continue;			// UV가 겹친 삼각형은 건너뜀
				const float r = 1.0f / det;

				const XMVECTOR t = (e1 * dv2 - e2 * dv1) * r;
				const XMVECTOR b = (e2 * du1 - e1 * du2) * r;
				for (uint32_t k : { i0, i1, i2 })
				{
					add(tanSum[k], t);
					add(bitSum[k], b);
				}
			}

			// 2) 정점마다 노멀에 수직이 되게 다듬고, 비탄젠트 부호 결정
			for (size_t v = 0; v < verts.size(); ++v)
			{
				const XMVECTOR n = XMLoadFloat3(&verts[v].normal);
				XMVECTOR t = XMLoadFloat3(&tanSum[v]);
				t = t - n * XMVector3Dot(n, t);

				if (XMVectorGetX(XMVector3LengthSq(t)) < 1e-12f)				// 계산 실패: 아무 수직 방향이나 사용
				{
					const XMVECTOR axis = fabsf(verts[v].normal.y) < 0.99f ? XMVectorSet(0, 1, 0, 0) : XMVectorSet(1, 0, 0, 0);
					t = XMVector3Cross(axis, n);
				}
				t = XMVector3Normalize(t);

				// glTF 노멀 맵은 초록 채널이 "이미지 위쪽" = V가 줄어드는 바향
				const float d = XMVectorGetX(XMVector3Dot(XMVector3Cross(n, t), XMLoadFloat3(&bitSum[v])));

				XMFLOAT3 tf;
				XMStoreFloat3(&tf, t);
				verts[v].tangent = { tf.x, tf.y, tf.z, d < 0.0f ? 1.0f : -1.0f };
			}
		}

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

			if(!tanAcc && uvAcc)
				ComputeTangents(verts, idx);

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
		{
			cgltf_free(data);
			throw std::runtime_error("glTF buffer load failed: " + path);
		}

		ID3D12Device* device = renderer.GetDevice().Get();
		const std::filesystem::path dir = std::filesystem::path(path).parent_path();

		// 1) 텍스처: 실제로 쓰이는 것만, 용도(sRGB 여부)별로 한 번씩 만든다
		std::map<std::pair<const cgltf_image*, bool>, Texture*> texCache;
		auto getTexture = [&](const cgltf_texture_view& view, bool srgb) -> const Texture*
		{
			if(!view.texture || !view.texture->image) return nullptr;
			const cgltf_image* img = view.texture->image;

			const auto key = std::make_pair(img, srgb);
			if(auto it = texCache.find(key); it != texCache.end()) return it->second;

			auto tex = std::make_unique<Texture>();
			if (img->buffer_view)														// .glb: 파일 안에 들어 있음
			{
				const cgltf_buffer_view* bv = img->buffer_view;
				const uint8_t* bytes = (const uint8_t*)bv->buffer->data + bv->offset;
				tex->LoadFromMemory(renderer, bytes, bv->size, srgb);
			}
			else if (img->uri && strncmp(img->uri, "data:", 5) != 0)					// .gltf: 옆에 있는 이미지 파일
			{
				std::string uri = img->uri;
				cgltf_decode_uri(uri.data());											// %20 같은 인코딩 해제
				uri.resize(strlen(uri.c_str()));
				tex->LoadFromFile(renderer, (dir / uri).string(), srgb);
			}
			else
			{
				return nullptr;
			}

			Texture* raw = tex.get();
			mTextures.push_back(std::move(tex));
			texCache[key] = raw;
			return raw;
		};

		// 2) 머티리얼
		std::vector<Material*> matTable(data->materials_count, nullptr);
		for (cgltf_size i = 0; i < data->materials_count; ++i)
		{
			const cgltf_material& gm = data->materials[i];
			Material& m = scene.CreateMaterial(gm.name ? gm.name : "glTF Material");

			if (gm.has_pbr_metallic_roughness)
			{
				const auto& pbr = gm.pbr_metallic_roughness;
				m.baseColor = { LinearToSrgbF(pbr.base_color_factor[0]),
								LinearToSrgbF(pbr.base_color_factor[1]), 
								LinearToSrgbF(pbr.base_color_factor[2]), };
				m.metallic = pbr.metallic_factor;
				m.roughness = pbr.roughness_factor;
				m.albedoMap = getTexture(pbr.base_color_texture, true);							// 색: sRGB
				m.metallicRoughnessMap = getTexture(pbr.metallic_roughness_texture, false);		// 데이터: 선형
			}
			m.normalMap = getTexture(gm.normal_texture, false);
			m.emissive = { gm.emissive_factor[0], gm.emissive_factor[1], gm.emissive_factor[2] };
			m.emissiveMap = getTexture(gm.emissive_texture, true);

			matTable[i] = &m;
		}

		// 3) 메시: glTF 메시 하나는 프리미티브(머티리얼 단위 조각)마다 메시와 머티리얼을 짝지어 둔다
		struct Part { Mesh* mesh = nullptr; Material* material = nullptr; };
		std::vector<std::vector<Part>> meshTable(data->meshes_count);
		for (cgltf_size m = 0; m < data->meshes_count; ++m)
		{
			const cgltf_mesh& gm = data->meshes[m];
			for (cgltf_size p = 0; p < gm.primitives_count; ++p)
			{
				const cgltf_primitive& prim = gm.primitives[p];
				Part part;

				if (gm.primitives[p].type == cgltf_primitive_type_triangles)
				{
					if (auto mesh = BuildMesh(device, prim))
					{
						part.mesh = mesh.get();
						mMeshes.push_back(std::move(mesh));
					}
					if(prim.material)	part.material = matTable[(size_t)(prim.material - data->materials)];
				}
				meshTable[m].push_back(part);
			}
		}

		// 4) 노드 -> 엔티티
		const XMMATRIX flipZ = XMMatrixScaling(1.0f, 1.0f, -1.0f);
		for (cgltf_size n = 0; n < data->nodes_count; ++n)
		{
			const cgltf_node& node = data->nodes[n];
			if(!node.mesh) continue;

			cgltf_float wm[16];
			cgltf_node_transform_world(&node, wm);
			XMFLOAT4X4 f;
			memcpy(&f, wm, sizeof(f));
			const XMMATRIX world = flipZ * XMLoadFloat4x4(&f) * flipZ;

			const size_t meshIndex = (size_t)(node.mesh - data->meshes);
			const std::vector<Part>& parts = meshTable[meshIndex];
			const std::string baseName = node.name ? node.name : (node.mesh->name ? node.mesh->name : "Node");

			for (size_t p = 0; p < parts.size(); ++p)
			{
				if(!parts[p].mesh) continue;

				Entity& e = scene.Create(parts.size() > 1 ? baseName + "_" + std::to_string(p) : baseName);
				e.mesh = parts[p].mesh;
				e.material = parts[p].material;
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
		for(auto& t : mTextures) t->Shutdown();
		mTextures.clear();
		mMeshes.clear();
	}
}
