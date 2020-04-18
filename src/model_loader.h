#pragma once

#include "dx12_labs.h"
#include "tiny_obj_loader.h"

struct DrawCallParams {
	unsigned int index_num;
	unsigned int start_index;
	unsigned int start_vertex;
};

class ModelLoader {
public:
	ModelLoader() = default;
	~ModelLoader() = default;

	HRESULT LoadModel(std::string path);

	const FullVertex *GetVertexBuffer() const;
	const unsigned int GetVertexBufferSize() const;
	const unsigned int GetVertexNumber() const;

	const unsigned int *GetIndexBuffer() const;
	const unsigned int GetIndexBufferSize() const;
	const unsigned int GetIndexNumber() const;

	const unsigned int GetMaterialNumber() const;
	const DrawCallParams GetDrawCallParams(unsigned int material_id) const;
	const std::string GetTexturePath(unsigned int material_id) const;
	const bool HasTexture(unsigned int material_id) const;
	const unsigned int GetTextureNumber() const;

protected:
	std::string obj_path;

	std::vector<FullVertex> vertices;
	std::vector<unsigned int> indices;
	std::vector<tinyobj::material_t> materials;

	std::vector<DrawCallParams> per_material_draw_call_params;

	std::string GetBinPath(std::string shader_file);
};