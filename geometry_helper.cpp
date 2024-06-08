
#include <DirectXMath.h>

#include "geometry.h"

using namespace DirectX;

void CreateGrid(StaticGeometryUploader<Vertex>* meshGeometry, UINT numRows, float cellLength)
{
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

	meshGeometry->AddVertexData(vertices, indices);
}

void CreateTerrain(StaticGeometryUploader<Vertex>* meshGeometry, UINT n, UINT m, float width, float depth)
{
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

			XMFLOAT2 uv(0.05 * x, 0.05 * z);

			vertices.push_back(Vertex{
				{ x, height, z }, n , uv });
		}
	}

	// Generate indices
	for (UINT i = 0; i < m - 1; i++)
	{
		for (UINT j = 0; j < n - 1; j++)
		{
			// Generate indices for quad down and to the right
			indices.push_back(j + i * n);
			indices.push_back((j + 1) + i * n);
			indices.push_back(j + (i + 1) * n);

			indices.push_back((j + 1) + i * n);
			indices.push_back((j + 1) + (i + 1) * n);
			indices.push_back(j + (i + 1) * n);
		}
	}

	meshGeometry->AddVertexData(vertices, indices);
}

void CreatePlane(StaticGeometryUploader<Vertex>* meshGeometry, UINT n, UINT m, float width, float depth)
{
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
			float height = -5.0f;

			XMFLOAT3 n(0.0f, 1.0f, 0.0f);
			XMFLOAT2 uv(0.01 * x, 0.01 * z);
			vertices.push_back(Vertex{
				{ x, height, z }, n , uv });
		}
	}

	// Generate indices
	for (UINT i = 0; i < m - 1; i++)
	{
		for (UINT j = 0; j < n - 1; j++)
		{
			// Generate indices for quad down and to the right
			indices.push_back(j + i * n);
			indices.push_back((j + 1) + i * n);
			indices.push_back(j + (i + 1) * n);

			indices.push_back((j + 1) + i * n);
			indices.push_back((j + 1) + (i + 1) * n);
			indices.push_back(j + (i + 1) * n);
		}
	}

	meshGeometry->AddVertexData(vertices, indices);
}

