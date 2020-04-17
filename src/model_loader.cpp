#include "model_loader.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

HRESULT ModelLoader::LoadModel(std::string path) {
	// Create and upload vertex buffer
	std::string obj_path = GetBinPath(std::string());
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

	std::map<std::tuple<int, int, int>, unsigned int> indices_map;

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++) {
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			int fv = shapes[s].mesh.num_face_vertices[f];

			// Loop over vertices in the face.
			// per-face material
			int material_ids = shapes[s].mesh.material_ids[f];
			for (size_t v = 0; v < fv; v++) {
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				std::tuple<int, int, int> idx_tuple = std::make_tuple(idx.vertex_index, idx.normal_index, idx.texcoord_index);

				if (indices_map.count(idx_tuple) > 0) {
					indices.push_back(indices_map[idx_tuple]);
				} else {

					tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
					tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
					tinyobj::real_t vz = -1.0f - attrib.vertices[3 * idx.vertex_index + 2];
					tinyobj::real_t nx = (idx.normal_index > -1) ? attrib.normals[3 * idx.normal_index + 0] : 0.0f;
					tinyobj::real_t ny = (idx.normal_index > -1) ? attrib.normals[3 * idx.normal_index + 1] : 0.0f;
					tinyobj::real_t nz = (idx.normal_index > -1) ? -1.0f - attrib.normals[3 * idx.normal_index + 2] : 0.0f;
					tinyobj::real_t tu = (idx.texcoord_index > -1) ? attrib.texcoords[2 * idx.texcoord_index + 0] : 0.0f;
					tinyobj::real_t tv = (idx.texcoord_index > -1) ? 1.0f - attrib.texcoords[2 * idx.texcoord_index + 1] : 0.0f;

					materials[material_ids].diffuse;

					FullVertex vertex = {};
					vertex.position = {vx, vy, vz};
					vertex.normal = {nx, ny, nz};
					vertex.texcoord = {tu, tv};
					vertex.diffuseColor = {
						materials[material_ids].diffuse[0],
						materials[material_ids].diffuse[1],
						materials[material_ids].diffuse[2]
					};

					indices.push_back(vertices.size());
					indices_map[idx_tuple] = vertices.size();
					vertices.push_back(vertex);
				}
			}
			index_offset += fv;
		}
	};

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
	DrawCallParams param = {};
	param.index_num = static_cast<unsigned int>(indices.size());
	param.start_index = 0;
	param.start_vertex = 0;
	return param;
}

std::string ModelLoader::GetBinPath(std::string shader_file) {
	CHAR buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string module_path = buffer;
	std::string::size_type pos = module_path.find_last_of("\\/");
	return module_path.substr(0, pos + 1) + shader_file;
}
