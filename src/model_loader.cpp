#include "model_loader.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

typedef std::map<std::tuple<int, int, int>, unsigned int> index_map_type;

HRESULT ModelLoader::LoadModel(std::string path) {
	// Create and upload vertex buffer
	obj_path = GetBinPath(std::string());
	std::string obj_file = obj_path + path;

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	//std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, obj_file.c_str(), obj_path.c_str());

	if (!warn.empty()) {
		std::wstring wwarn(warn.begin(), warn.end());
		wwarn = L"Tiny OBJ reader warning: " + wwarn + L"\n";
		OutputDebugString(wwarn.c_str());
	}

	if (!err.empty()) {
		std::wstring werr(err.begin(), err.end());
		werr = L"Tiny OBJ reader error: " + werr + L"\n";
		OutputDebugString(werr.c_str());
	}

	if (!ret) {
		return E_ABORT;
	}

	//index_map_type indices_map;
	std::vector<std::vector<FullVertex>> per_material_vertices(materials.size());
	std::vector<std::vector<unsigned int>> per_material_indices(materials.size());
	std::vector<index_map_type> per_material_indices_map(materials.size());

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++) {
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			int fv = shapes[s].mesh.num_face_vertices[f];

			// Loop over vertices in the face.
			// per-face material
			int material_id = shapes[s].mesh.material_ids[f];
			for (size_t v = 0; v < fv; v++) {
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				std::tuple<int, int, int> idx_tuple = std::make_tuple(idx.vertex_index, idx.normal_index, idx.texcoord_index);

				if (per_material_indices_map[material_id].count(idx_tuple) > 0) {
					per_material_indices[material_id].push_back(per_material_indices_map[material_id][idx_tuple]);
				} else {

					tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
					tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
					tinyobj::real_t vz = -1.0f - attrib.vertices[3 * idx.vertex_index + 2];
					tinyobj::real_t nx = (idx.normal_index > -1) ? attrib.normals[3 * idx.normal_index + 0] : 0.0f;
					tinyobj::real_t ny = (idx.normal_index > -1) ? attrib.normals[3 * idx.normal_index + 1] : 0.0f;
					tinyobj::real_t nz = (idx.normal_index > -1) ? -1.0f - attrib.normals[3 * idx.normal_index + 2] : 0.0f;
					tinyobj::real_t tu = (idx.texcoord_index > -1) ? attrib.texcoords[2 * idx.texcoord_index + 0] : 0.0f;
					tinyobj::real_t tv = (idx.texcoord_index > -1) ? 1.0f - attrib.texcoords[2 * idx.texcoord_index + 1] : 0.0f;

					materials[material_id].diffuse;

					FullVertex vertex = {};
					vertex.position = {vx, vy, vz};
					vertex.normal = {nx, ny, nz};
					vertex.texcoord = {tu, tv};
					vertex.diffuseColor = {
						materials[material_id].diffuse[0],
						materials[material_id].diffuse[1],
						materials[material_id].diffuse[2]
					};

					per_material_indices[material_id].push_back(per_material_vertices[material_id].size());
					per_material_indices_map[material_id][idx_tuple] = per_material_vertices[material_id].size();
					per_material_vertices[material_id].push_back(vertex);
				}
			}
			index_offset += fv;
		}
	};

	for (size_t material_id = 0; material_id < GetMaterialNumber(); material_id++) {
		DrawCallParams param = {};
		param.index_num = static_cast<unsigned int>(indices.size());
		param.start_index = indices.size();
		param.start_vertex = vertices.size();

		vertices.insert(end(vertices), begin(per_material_vertices[material_id]), end(per_material_vertices[material_id]));
		indices.insert(end(indices), begin(per_material_indices[material_id]), end(per_material_indices[material_id]));

		param.index_num = static_cast<unsigned int>(per_material_indices[material_id].size());
		per_material_draw_call_params.push_back(param);
	}

	return S_OK;
}

const FullVertex *ModelLoader::GetVertexBuffer() const {
	return vertices.data();
}

const unsigned int ModelLoader::GetVertexBufferSize() const {
	return static_cast<unsigned int>(vertices.size() * sizeof(FullVertex));
}

const unsigned int ModelLoader::GetVertexNumber() const {
	return vertices.size();
}

const unsigned int *ModelLoader::GetIndexBuffer() const {
	return indices.data();
}

const unsigned int ModelLoader::GetIndexBufferSize() const {
	return static_cast<unsigned int>(indices.size() * sizeof(unsigned int));
}

const unsigned int ModelLoader::GetIndexNumber() const {
	return indices.size();
}

const unsigned int ModelLoader::GetMaterialNumber() const {
	return materials.size();
}

const DrawCallParams ModelLoader::GetDrawCallParams(unsigned int material_id) const {
	return per_material_draw_call_params[material_id];
}

const std::string ModelLoader::GetTexturePath(unsigned int material_id) const {
	return obj_path + "\\" + materials[material_id].diffuse_texname;
}

const bool ModelLoader::HasTexture(unsigned int material_id) const {
	return !materials[material_id].diffuse_texname.empty();
}

const unsigned int ModelLoader::GetTextureNumber() const {
	unsigned int texture_num = 0;
	for (unsigned int  material_id = 0; material_id < materials.size(); material_id++) {
		if (HasTexture(material_id)) {
			texture_num++;
		}
	}

	return texture_num;
}

std::string ModelLoader::GetBinPath(std::string shader_file) {
	CHAR buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string module_path = buffer;
	std::string::size_type pos = module_path.find_last_of("\\/");
	return module_path.substr(0, pos + 1) + shader_file;
}
