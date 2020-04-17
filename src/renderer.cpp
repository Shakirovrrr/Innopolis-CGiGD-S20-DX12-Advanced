#include "renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

void Renderer::OnInit() {
	LoadPipeline();
	LoadAssets();

	baseTime = high_resolution_clock::now();
}

void Renderer::OnUpdate() {
	high_resolution_clock::time_point currentTime = high_resolution_clock::now();
	duration<float> elapsedTime = duration_cast<duration<float>>(currentTime - baseTime);
	baseTime = currentTime;

	angle += velocityRotation * elapsedTime.count();
	eye_position += XMVECTOR({sin(angle), 0.f, cos(angle)}) * velocityForward * elapsedTime.count();

	XMVECTOR focus_position = eye_position + XMVECTOR({sin(angle), 0.f, cos(angle)});

	XMVECTOR up_direction = XMVECTOR({0.0f, 1.f, 0.f});
	view = XMMatrixLookAtLH(eye_position, focus_position, up_direction);
	world_view_projection = XMMatrixTranspose(
		XMMatrixTranspose(projection) *
		XMMatrixTranspose(view) *
		XMMatrixTranspose(world));
	//world_view_projection = XMMatrixTranspose(world * view * projection);
	//world_view_projection = world * view * projection;
	//world_view_projection = view * world;
	//world_view_projection = world;
	memcpy(constant_buffer_data_begin, &world_view_projection, sizeof(world_view_projection));
}

void Renderer::OnRender() {
	PopulateCommandList();
	ID3D12CommandList *command_lists[] = {command_list.Get()};

	command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

	ThrowIfFailed(swap_chain->Present(0, 0));

	WaitForPreviousFrame();
}

void Renderer::OnDestroy() {
	WaitForPreviousFrame();
	CloseHandle(fence_event);
}

void Renderer::OnKeyDown(UINT8 key) {
	switch (key) {
		case 0x41 - 'a' + 'd':
			velocityRotation = 1.0f;
			break;
		case 0x41 - 'a' + 'a':
			velocityRotation = -1.0f;
			break;
		case 0x41 - 'a' + 'w':
			velocityForward = 1.0f;
			break;
		case 0x41 - 'a' + 's':
			velocityForward = -1.0f;
			break;
		case VK_OEM_MINUS:
			if (max_draw_call_num > 0) {
				max_draw_call_num--;
				std::wstring kek = L"Max draw call decreased: " + std::to_wstring(max_draw_call_num) + L"\n";
				OutputDebugString(kek.c_str());
			}
			break;
		case VK_OEM_PLUS:
			if (max_draw_call_num < modelLoader.GetMaterialNumber()) {
				max_draw_call_num++;
				std::wstring kek = L"Max draw call increased: " + std::to_wstring(max_draw_call_num) + L"\n";
				OutputDebugString(kek.c_str());
			}
			break;

		default:
			break;
	}
}

void Renderer::OnKeyUp(UINT8 key) {
	switch (key) {
		case 0x41 - 'a' + 'd':
			velocityRotation = 0.0f;
			break;
		case 0x41 - 'a' + 'a':
			velocityRotation = 0.0f;
			break;
		case 0x41 - 'a' + 'w':
			velocityForward = 0.0f;
			break;
		case 0x41 - 'a' + 's':
			velocityForward = 0.0f;
			break;
		default:
			break;
	}
}

void Renderer::LoadPipeline() {
	// Create debug layer
	UINT dxgi_factory_flag = 0;
#ifdef _DEBUG
	ComPtr<ID3D12Debug> debug_controller;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
		debug_controller->EnableDebugLayer();
		dxgi_factory_flag |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif

	// Create device
	ComPtr<IDXGIFactory4> dxgi_factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgi_factory_flag, IID_PPV_ARGS(&dxgi_factory)));

	ComPtr<IDXGIAdapter1> hardware_adapter;
	ThrowIfFailed(dxgi_factory->EnumAdapters1(0, &hardware_adapter));
	ThrowIfFailed(D3D12CreateDevice(hardware_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

	// Create a direct command queue
	D3D12_COMMAND_QUEUE_DESC queue_descriptor = {};
	queue_descriptor.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queue_descriptor.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(device->CreateCommandQueue(&queue_descriptor, IID_PPV_ARGS(&command_queue)));

	// Create swap chain
	DXGI_SWAP_CHAIN_DESC1 swap_chain_desctiptor = {};
	swap_chain_desctiptor.BufferCount = frame_number;
	swap_chain_desctiptor.Width = GetWidth();
	swap_chain_desctiptor.Height = GetHeight();
	swap_chain_desctiptor.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desctiptor.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desctiptor.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swap_chain_desctiptor.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> temp_swap_chain;
	ThrowIfFailed(dxgi_factory->CreateSwapChainForHwnd(
		command_queue.Get(),
		Win32Window::GetHwnd(),
		&swap_chain_desctiptor,
		nullptr,
		nullptr,
		&temp_swap_chain
	));
	ThrowIfFailed(dxgi_factory->MakeWindowAssociation(Win32Window::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));
	ThrowIfFailed(temp_swap_chain.As(&swap_chain));

	frame_index = swap_chain->GetCurrentBackBufferIndex();

	// Create descriptor heap for render target view

	D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_descriptor = {};
	rtv_heap_descriptor.NumDescriptors = frame_number;
	rtv_heap_descriptor.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtv_heap_descriptor.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(device->CreateDescriptorHeap(&rtv_heap_descriptor, IID_PPV_ARGS(&rtv_heap)));
	rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_descriptor = {};
	dsv_heap_descriptor.NumDescriptors = 1;
	dsv_heap_descriptor.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsv_heap_descriptor.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(device->CreateDescriptorHeap(&dsv_heap_descriptor, IID_PPV_ARGS(&dsv_heap)));

	D3D12_DESCRIPTOR_HEAP_DESC cbv_srv_heap_descriptor = {};
	cbv_srv_heap_descriptor.NumDescriptors = 1 + 1; // 1 CBV + 1 SRV
	cbv_srv_heap_descriptor.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbv_srv_heap_descriptor.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	ThrowIfFailed(device->CreateDescriptorHeap(&cbv_srv_heap_descriptor, IID_PPV_ARGS(&cbv_srv_heap)));

	// Create render target view for each frame
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rtv_heap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < frame_number; i++) {
		ThrowIfFailed(swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i])));
		device->CreateRenderTargetView(render_targets[i].Get(), nullptr, rtv_handle);
		std::wstring render_target_name = L"Render target ";
		render_target_name += std::to_wstring(i);
		render_targets[i]->SetName(render_target_name.c_str());
		rtv_handle.Offset(1, rtv_descriptor_size);
	}

	CD3DX12_RESOURCE_DESC depth_texture_descriptor(
		D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		0, this->width, this->height, 1, 1,
		DXGI_FORMAT_D32_FLOAT, 1, 0,
		D3D12_TEXTURE_LAYOUT_UNKNOWN,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE
	);
	D3D12_CLEAR_VALUE clearVal;
	clearVal.Format = DXGI_FORMAT_D32_FLOAT;
	clearVal.DepthStencil.Depth = 1.0f;
	clearVal.DepthStencil.Stencil = 0;

	ThrowIfFailed(
		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&depth_texture_descriptor,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearVal,
			IID_PPV_ARGS(&depth_stencil)
		)
	);
	depth_stencil->SetName(L"Depth stencil");

	device->CreateDepthStencilView(depth_stencil.Get(), nullptr, dsv_heap->GetCPUDescriptorHandleForHeapStart());

	// Create command allocator
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)));
}

void Renderer::LoadAssets() {
	// Create a root signature

	D3D12_FEATURE_DATA_ROOT_SIGNATURE rs_feature_data = {};
	rs_feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rs_feature_data, sizeof(rs_feature_data)))) {
		rs_feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
	CD3DX12_ROOT_PARAMETER1 root_paramters[2];

	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

	root_paramters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
	root_paramters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);

	D3D12_ROOT_SIGNATURE_FLAGS rs_flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.MipLODBias = 0.0f;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.Filter = D3D12_FILTER_ANISOTROPIC;
	sampler.MaxAnisotropy = 16;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_descriptor;
	root_signature_descriptor.Init_1_1(_countof(root_paramters), root_paramters, 1, &sampler, rs_flags);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&root_signature_descriptor,
		rs_feature_data.HighestVersion, &signature, &error));
	ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(),
		signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));


	// Create full PSO
	ComPtr<ID3DBlob> vertex_shader;
	ComPtr<ID3DBlob> pixel_shader;

	UINT compile_flags = 0;
#ifdef _DEBUG
	compile_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif // _DEBUG


	std::wstring shader_path = GetBinPath(std::wstring(L"shaders.hlsl"));
	ThrowIfFailed(D3DCompileFromFile(shader_path.c_str(), nullptr, nullptr,
		"VSMain", "vs_5_0", compile_flags, 0, &vertex_shader, &error));
	ThrowIfFailed(D3DCompileFromFile(shader_path.c_str(), nullptr, nullptr,
		"PSMain", "ps_5_0", compile_flags, 0, &pixel_shader, &error));

	D3D12_INPUT_ELEMENT_DESC input_element_descriptors[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"DIFFUSE", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_descriptor = {};
	pso_descriptor.InputLayout = {input_element_descriptors, _countof(input_element_descriptors)};
	pso_descriptor.pRootSignature = root_signature.Get();
	pso_descriptor.VS = CD3DX12_SHADER_BYTECODE(vertex_shader.Get());
	pso_descriptor.PS = CD3DX12_SHADER_BYTECODE(pixel_shader.Get());
	pso_descriptor.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	pso_descriptor.RasterizerState.FrontCounterClockwise = TRUE;
	//pso_descriptor.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	//pso_descriptor.RasterizerState.DepthClipEnable = FALSE;
	//pso_descriptor.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pso_descriptor.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	pso_descriptor.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	pso_descriptor.DepthStencilState.DepthEnable = TRUE;
	pso_descriptor.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	pso_descriptor.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	pso_descriptor.DepthStencilState.StencilEnable = FALSE;
	pso_descriptor.SampleMask = UINT_MAX;
	pso_descriptor.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso_descriptor.NumRenderTargets = 1;
	pso_descriptor.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pso_descriptor.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pso_descriptor.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&pso_descriptor, IID_PPV_ARGS(&pipeline_state)));

	// Create command list
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator.Get(),
		pipeline_state.Get(), IID_PPV_ARGS(&command_list)));
	//ThrowIfFailed(command_list->Close());

	ThrowIfFailed(modelLoader.LoadModel(obj_file));
	max_draw_call_num = modelLoader.GetMaterialNumber();

	const UINT vertex_buffer_size = modelLoader.GetVertexBufferSize();
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(modelLoader.GetVertexBufferSize()),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&vertex_buffer))
	);
	vertex_buffer->SetName(L"Vertex buffer");

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(modelLoader.GetVertexBufferSize()),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&upload_vertex_buffer))
	);

	D3D12_SUBRESOURCE_DATA vertexData = {};
	vertexData.pData = modelLoader.GetVertexBuffer();
	vertexData.RowPitch = modelLoader.GetVertexBufferSize();
	vertexData.SlicePitch = modelLoader.GetVertexBufferSize();

	UpdateSubresources(command_list.Get(), vertex_buffer.Get(), upload_vertex_buffer.Get(), 0, 0, 1, &vertexData);
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		vertex_buffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	));

	/*UINT8 *vertex_data_begin;
	ThrowIfFailed(vertex_buffer->Map(0, &read_range, reinterpret_cast<void **>(&vertex_data_begin)));
	memcpy(vertex_data_begin, modelLoader.GetVertexBuffer(), modelLoader.GetVertexBufferSize());
	vertex_buffer->Unmap(0, nullptr);*/

	vertex_buffer_view.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
	vertex_buffer_view.StrideInBytes = sizeof(FullVertex);
	vertex_buffer_view.SizeInBytes = modelLoader.GetVertexBufferSize();

	// Create index buffer
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(modelLoader.GetIndexBufferSize()),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&index_buffer))
	);
	index_buffer->SetName(L"Index buffer");

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(modelLoader.GetIndexBufferSize()),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&upload_index_buffer))
	);

	D3D12_SUBRESOURCE_DATA indexData = {};
	indexData.pData = modelLoader.GetIndexBuffer();
	indexData.RowPitch = modelLoader.GetIndexBufferSize();
	indexData.SlicePitch = modelLoader.GetIndexBufferSize();

	UpdateSubresources(command_list.Get(), index_buffer.Get(), upload_index_buffer.Get(), 0, 0, 1, &indexData);
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		index_buffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_INDEX_BUFFER
	));

	index_buffer_view.BufferLocation = index_buffer->GetGPUVirtualAddress();
	index_buffer_view.SizeInBytes = modelLoader.GetIndexBufferSize();
	index_buffer_view.Format = DXGI_FORMAT_R32_UINT;

	// Constant buffer init
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constant_buffer))
	);

	CD3DX12_CPU_DESCRIPTOR_HANDLE cbv_srv_heap_handle(cbv_srv_heap->GetCPUDescriptorHandleForHeapStart());
	const unsigned int cbv_srv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_descriptor = {};
	cbv_descriptor.BufferLocation = constant_buffer->GetGPUVirtualAddress();
	cbv_descriptor.SizeInBytes = (sizeof(world_view_projection) + 255) & ~255;
	device->CreateConstantBufferView(&cbv_descriptor, cbv_srv_heap_handle);
	cbv_srv_heap_handle.Offset(1, cbv_srv_descriptor_size);

	CD3DX12_RANGE read_range(0, 0);
	ThrowIfFailed(constant_buffer->Map(0, &read_range, reinterpret_cast<void **>(&constant_buffer_data_begin)));
	memcpy(constant_buffer_data_begin, &world_view_projection, sizeof(world_view_projection));

	// Create texture
	std::string texFile = "models/default.png";
	int texWidth, texHeight, texChannels;
	unsigned char *image = stbi_load(texFile.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	D3D12_RESOURCE_DESC textureDescriptor = {};
	textureDescriptor.Width = texWidth;
	textureDescriptor.Height = texHeight;
	textureDescriptor.DepthOrArraySize = 1;
	textureDescriptor.MipLevels = 1;
	textureDescriptor.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDescriptor.SampleDesc.Count = 1;
	textureDescriptor.SampleDesc.Quality = 0;
	textureDescriptor.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDescriptor.Flags = D3D12_RESOURCE_FLAG_NONE;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDescriptor,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&texture))
	);
	texture->SetName(L"Texture data");

	const UINT64 upload_buffer_size = GetRequiredIntermediateSize(texture.Get(), 0, 1);

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&upload_texture))
	);

	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = image;
	textureData.RowPitch = texWidth * STBI_rgb_alpha;
	textureData.SlicePitch = textureData.RowPitch * texHeight;

	UpdateSubresources(command_list.Get(), texture.Get(), upload_texture.Get(), 0, 0, 1, &textureData);
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		texture.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDescriptor = {};
	srvDescriptor.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescriptor.Format = textureDescriptor.Format;
	srvDescriptor.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDescriptor.Texture2D.MipLevels = 1;

	device->CreateShaderResourceView(texture.Get(), &srvDescriptor, cbv_srv_heap_handle);

	ThrowIfFailed(command_list->Close());
	ID3D12CommandList *command_lists[] = {command_list.Get()};
	command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

	// Create synchronization objects
	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	fence_value = 1;
	fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence_event == nullptr) {
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void Renderer::PopulateCommandList() {
	// Reset allocators and lists
	ThrowIfFailed(command_allocator->Reset());
	ThrowIfFailed(command_list->Reset(command_allocator.Get(), pipeline_state.Get()));


	// Set initial state
	command_list->SetGraphicsRootSignature(root_signature.Get());
	ID3D12DescriptorHeap *heaps[] = {cbv_srv_heap.Get()};
	command_list->SetDescriptorHeaps(_countof(heaps), heaps);

	const unsigned int cbv_srv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbv_srv_handle(cbv_srv_heap->GetGPUDescriptorHandleForHeapStart());
	command_list->SetGraphicsRootDescriptorTable(0, cbv_srv_handle);
	cbv_srv_handle.Offset(1, cbv_srv_descriptor_size);
	command_list->SetGraphicsRootDescriptorTable(1, cbv_srv_handle);
	command_list->RSSetViewports(1, &view_port);
	command_list->RSSetScissorRects(1, &scissor_rect);


	// Resource barrier from present to RT
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		render_targets[frame_index].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	));


	// Record commands
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rtv_heap->GetCPUDescriptorHandleForHeapStart(),
		frame_index, rtv_descriptor_size);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_handle(dsv_heap->GetCPUDescriptorHandleForHeapStart());
	command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, &dsv_handle);
	const float clear_color[] = {0.f, 0.f, 0.f, 1.f};
	command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);
	command_list->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
	command_list->IASetIndexBuffer(&index_buffer_view);

	for (unsigned int material_id = 0; material_id < modelLoader.GetMaterialNumber() && material_id < max_draw_call_num; material_id++) {
		DrawCallParams params = modelLoader.GetDrawCallParams(material_id);
		command_list->DrawIndexedInstanced(params.index_num, 1, params.start_index, params.start_vertex, 0);
	}


	// Resource barrier from RT to present
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		render_targets[frame_index].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT
	));

	// Close command list
	ThrowIfFailed(command_list->Close());
}

void Renderer::WaitForPreviousFrame() {
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// Signal and increment the fence value.
	const UINT64 prev_fence_value = fence_value;
	ThrowIfFailed(command_queue->Signal(fence.Get(), prev_fence_value));
	fence_value++;

	if (fence->GetCompletedValue() < prev_fence_value) {
		ThrowIfFailed(fence->SetEventOnCompletion(prev_fence_value, fence_event));
		WaitForSingleObject(fence_event, INFINITE);
	}

	frame_index = swap_chain->GetCurrentBackBufferIndex();
}

std::wstring Renderer::GetBinPath(std::wstring shader_file) {
	WCHAR buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	std::wstring module_path = buffer;
	std::wstring::size_type pos = module_path.find_last_of(L"\\/");
	return module_path.substr(0, pos + 1) + shader_file;
}
