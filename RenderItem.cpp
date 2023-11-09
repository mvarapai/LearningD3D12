#include "RenderItem.h"
#include "MathHelper.h"
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

RenderItem RenderItem::CreateTerrain(MeshGeometry<Vertex>* meshGeometry, UINT objCBIndex, UINT n, UINT m, float width, float depth)
{
	RenderItem renderItem = RenderItem();
	renderItem.mGeometry = meshGeometry;
	renderItem.mObjCBIndex = objCBIndex;
	
	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;

	float dx = width / static_cast<float>(n - 1);
	float dz = depth / static_cast<float>(m - 1);

	float zeroX = -width / 2;
	float zeroZ = depth / 2;

	// Generate vertices
	for (int i = 0; i < m; i++)
	{
		for (int j = 0; j < n; j++)
		{
			float x = zeroX + j * dx;
			float z = zeroZ - i * dz;
			float height = MathHelper::TerrainNoise(x, z);

			XMFLOAT4 Color = { };

			if (height < -10.0f)
			{
				// Sandy beach color.
				Color = XMFLOAT4(1.0f, 0.96f, 0.62f, 1.0f);
			}
			else if (height < 5.0f)
			{
				// Light yellow-green.
				Color = XMFLOAT4(0.48f, 0.77f, 0.46f, 1.0f);
			}
			else if (height < 12.0f)
			{
				// Dark yellow-green.
				Color = XMFLOAT4(0.1f, 0.48f, 0.19f, 1.0f);
			}
			else if (height < 20.0f)
			{
				// Dark brown.
				Color = XMFLOAT4(0.45f, 0.39f, 0.34f, 1.0f);
			}
			else
			{
				// White snow.
				Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			}

			vertices.push_back(Vertex{
				{ x, height, z }, Color });
		}
	}

	// Generate indices
	for (int i = 0; i < m - 1; i++)
	{
		for (int j = 0; j < n - 1; j++)
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

