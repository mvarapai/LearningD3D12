#include "RenderItem.h"
#include <DirectXColors.h>

using namespace DirectX;


RenderItem RenderItem::CreatePaintedCube(MeshGeometry<Vertex>* meshGeometry, UINT objCBIndex)
{
	RenderItem renderItem = RenderItem();
	renderItem.mGeometry = meshGeometry;
	renderItem.mObjCBIndex = objCBIndex;

	std::vector<Vertex> vertices =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
	};

	std::vector<std::uint16_t> indices =
	{
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7
	};

	renderItem.mSubmesh = meshGeometry->AddVertexData(vertices, indices);

	return renderItem;
}

RenderItem RenderItem::CreateGrid(MeshGeometry<Vertex>* meshGeometry, UINT objCBIndex, UINT numRows, float cellLength)
{
	RenderItem renderItem = RenderItem();
	renderItem.mGeometry = meshGeometry;
	renderItem.mObjCBIndex = objCBIndex;
	renderItem.mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_LINELIST;

	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;

	float offset = 0.5f * (numRows - 1) * cellLength;

	// Specify horizontal lines
	for (UINT x = 1; x < numRows - 1; x++)
	{
		XMFLOAT4 color = XMFLOAT4(Colors::Gray);

		Vertex v1 = { }, v2 = { }, v3 = { }, v4 = { };

		v1.Pos = XMFLOAT3(x * cellLength - offset, 0, -offset);
		v1.Color = color;

		v2.Pos = XMFLOAT3(x * cellLength - offset, 0, (numRows - 1) * cellLength - offset);
		v2.Color = color;

		indices.push_back(vertices.size());
		vertices.push_back(v1);

		indices.push_back(vertices.size());
		vertices.push_back(v2);

		// Vertical columns
		v3.Pos = XMFLOAT3(-offset, 0, x * cellLength - offset);
		v3.Color = color;

		v4.Pos = XMFLOAT3((numRows - 1) * cellLength - offset, 0, x * cellLength - offset);
		v4.Color = color;

		indices.push_back(vertices.size());
		vertices.push_back(v3);

		indices.push_back(vertices.size());
		vertices.push_back(v4);
	}

	renderItem.mSubmesh = meshGeometry->AddVertexData(vertices, indices);

	return renderItem;
}

