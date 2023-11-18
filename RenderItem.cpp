#include "RenderItem.h"
#include "MathHelper.h"
#include <DirectXColors.h>

using namespace DirectX;


RenderItem RenderItem::CreatePaintedCube(StaticGeometry<Vertex>* meshGeometry, UINT objCBIndex, UINT matIndex)
{
	RenderItem renderItem = RenderItem();
	renderItem.mGeometry = meshGeometry;
	renderItem.mObjCBIndex = objCBIndex;
	renderItem.mMaterialIndex = matIndex;

	std::vector<Vertex> vertices =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, -1.0f, -1.0f) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT3(-1.0f, +1.0f, -1.0f) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT3(+1.0f, +1.0f, -1.0f) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT3(+1.0f, -1.0f, -1.0f) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT3(-1.0f, -1.0f, +1.0f) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT3(-1.0f, +1.0f, +1.0f) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT3(+1.0f, +1.0f, +1.0f) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT3(+1.0f, -1.0f, +1.0f) })
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

RenderItem RenderItem::CreateGrid(StaticGeometry<Vertex>* meshGeometry, UINT objCBIndex, UINT matIndex, UINT numRows, float cellLength)
{
	RenderItem renderItem = RenderItem();
	renderItem.mGeometry = meshGeometry;
	renderItem.mObjCBIndex = objCBIndex;
	renderItem.mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	renderItem.mMaterialIndex = matIndex;

	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;

	float offset = 0.5f * (numRows - 1) * cellLength;

	// Specify horizontal lines
	for (UINT x = 1; x < numRows - 1; x++)
	{
		//XMFLOAT4 color = XMFLOAT4(Colors::Gray);

		Vertex v1 = { }, v2 = { }, v3 = { }, v4 = { };

		v1.Pos = XMFLOAT3(x * cellLength - offset, 0, -offset);
		//v1.Color = color;

		v2.Pos = XMFLOAT3(x * cellLength - offset, 0, (numRows - 1) * cellLength - offset);
		//v2.Color = color;

		indices.push_back(static_cast<uint16_t>(vertices.size()));
		vertices.push_back(v1);

		indices.push_back(static_cast<uint16_t>(vertices.size()));
		vertices.push_back(v2);

		// Vertical columns
		v3.Pos = XMFLOAT3(-offset, 0, x * cellLength - offset);
		//v3.Color = color;

		v4.Pos = XMFLOAT3((numRows - 1) * cellLength - offset, 0, x * cellLength - offset);
		//v4.Color = color;

		indices.push_back(static_cast<uint16_t>(vertices.size()));
		vertices.push_back(v3);

		indices.push_back(static_cast<uint16_t>(vertices.size()));
		vertices.push_back(v4);
	}

	renderItem.mSubmesh = meshGeometry->AddVertexData(vertices, indices);

	return renderItem;
}

RenderItem RenderItem::CreateTerrain(StaticGeometry<Vertex>* meshGeometry, UINT objCBIndex, UINT matIndex, UINT n, UINT m, float width, float depth)
{
	RenderItem renderItem = RenderItem();
	renderItem.mGeometry = meshGeometry;
	renderItem.mObjCBIndex = objCBIndex;
	renderItem.mMaterialIndex = matIndex;
	
	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;

	float dx = width / static_cast<float>(n - 1);
	float dz = depth / static_cast<float>(m - 1);

	float zeroX = -width / 2;
	float zeroZ = depth / 2;

	// Generate vertices
	for (UINT i = 0; i < m; i++)
	{
		for (UINT j = 0; j < n; j++)
		{
			float x = zeroX + j * dx;
			float z = zeroZ - i * dz;
			float height = MathHelper::TerrainNoise(x, z);

			XMFLOAT3 n(
				-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
				1.0f,
				-0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

			vertices.push_back(Vertex{
				{ x, height, z }, n });
		}
	}

	// Generate indices
	for (UINT i = 0; i < m - 1; i++)
	{
		for (UINT j = 0; j < n - 1; j++)
		{
			// Generate indices for quad down and to the right
			indices.push_back(	j			+	i		* n);
			indices.push_back(	(j + 1)		+	i		* n);
			indices.push_back(	j			+	(i + 1) * n);

			indices.push_back(	(j + 1)		+	i		* n);
			indices.push_back(	(j + 1)		+ (i + 1)	* n);
			indices.push_back(	j			+ (i + 1)	* n);
		}
	}

	renderItem.mSubmesh = meshGeometry->AddVertexData(vertices, indices);

	return renderItem;
}

