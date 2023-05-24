#include "Scene.h"
#include "dxHelper.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <map>

class compTynyIdx
{
public:
	bool operator() (const tinyobj::index_t& a, const tinyobj::index_t& b) const
	{
		return
			(a.vertex_index != b.vertex_index) ? (a.vertex_index < b.vertex_index) :
			(a.normal_index != b.normal_index) ? (a.normal_index < b.normal_index) : (a.texcoord_index < b.texcoord_index);
	}

};

Mesh loadMeshFromOBJFile(const char* filename, bool optimizeVertexxCount)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename);

	if (!err.empty())
	{
		throw Error(err.c_str());
	}

	if (shapes.size() != 1)
	{
		throw Error("The obj file includes several meshes.\n");
	}

	std::vector<tinyobj::index_t>& I = shapes[0].mesh.indices;
	uint numTri = (uint)shapes[0].mesh.num_face_vertices.size();

	if (I.size() != 3 * numTri)
	{
		throw Error("The mesh includes non-triangle faces.\n");
	}

	Mesh mesh;
	mesh.vtxArr.resize(numTri * 3);
	mesh.tdxArr.resize(numTri);

	std::map<tinyobj::index_t, uint, compTynyIdx> tinyIdxToVtxIdx;
	uint numVtx = 0;

	if (optimizeVertexxCount)
	{
		for (uint i = 0; i < 3 * numTri; ++i)
		{
			tinyobj::index_t& tinyIdx = I[i];
			auto iterAndBool = tinyIdxToVtxIdx.insert({ tinyIdx, numVtx });

			if (iterAndBool.second)
			{
				mesh.vtxArr[numVtx].position = *((float3*)&attrib.vertices[tinyIdx.vertex_index * 3]);
				mesh.vtxArr[numVtx].normal = *((float3*)&attrib.normals[tinyIdx.normal_index * 3]);
				mesh.vtxArr[numVtx].texcoord = (tinyIdx.texcoord_index == -1) ? float2(0, 0) :
					*((float2*)&attrib.texcoords[tinyIdx.texcoord_index * 2]);
				numVtx++;
			}

			((uint*)mesh.tdxArr.data())[i] = iterAndBool.first->second;
		}
		mesh.vtxArr.resize(numVtx);
	}

	else
	{
		for (uint i = 0; i < 3 * numTri; ++i)
		{
			tinyobj::index_t& idx = I[i];
			mesh.vtxArr[i].position = *((float3*)&attrib.vertices[idx.vertex_index * 3]);
			mesh.vtxArr[i].normal = *((float3*)&attrib.normals[idx.normal_index * 3]);
			mesh.vtxArr[i].texcoord = (idx.texcoord_index == -1) ?
				float2(0, 0) : *((float2*)&attrib.texcoords[idx.texcoord_index * 2]);

			((uint*)mesh.tdxArr.data())[i] = i;
		}
	}

	return mesh;
}

Mesh generateParallelogramMesh(const float3& corner, const float3& side1, const float3& side2)
{
	Mesh mesh;
	float3 faceNormal = normalize(cross(side1, side2));

	mesh.vtxArr.resize(4);
	mesh.vtxArr[0].position = corner;
	mesh.vtxArr[1].position = corner + side1;
	mesh.vtxArr[2].position = corner + side1 + side2;
	mesh.vtxArr[3].position = corner + side2;
	mesh.vtxArr[0].normal = faceNormal;
	mesh.vtxArr[1].normal = faceNormal;
	mesh.vtxArr[2].normal = faceNormal;
	mesh.vtxArr[3].normal = faceNormal;
	mesh.vtxArr[0].texcoord = float2(0.f, 0.f);
	mesh.vtxArr[1].texcoord = float2(0.f, 0.f);
	mesh.vtxArr[2].texcoord = float2(0.f, 0.f);
	mesh.vtxArr[3].texcoord = float2(0.f, 0.f);

	mesh.tdxArr.resize(2);
	mesh.tdxArr[0] = uint3(0, 1, 2);
	mesh.tdxArr[1] = uint3(2, 3, 0);

	return mesh;
}

Mesh generateRectangleMesh(const float3& center, const float3& size, FaceDir dir)
{
	float3 corner, side1, side2;

	if (dir == FaceDir::up || dir == FaceDir::down)
	{
		if (size.x == 0.f || size.y != 0.f || size.z == 0.f)
		{
			throw Error("You must give width and depthValues for up or down faced rectangle.");
		}
		side1 = float3(0.f, 0.f, size.z);
		side2 = float3(size.x, 0.f, 0.f);

		if (dir == FaceDir::down)
			side2 = -side2;
	}
	else if (dir == FaceDir::front || dir == FaceDir::back)
	{
		if (size.x == 0.f || size.y == 0.f || size.z != 0.f)
		{
			throw Error("You must give width and height values for front or back faced rectangle.");
		}
		side1 = float3(size.x, 0.f, 0.f);
		side2 = float3(0.f, size.y, 0.f);

		if (dir == FaceDir::back)
			side2 = -side2;
	}
	else if (dir == FaceDir::left || dir == FaceDir::right)
	{
		if (size.x != 0 || size.y == 0.f || size.z == 0.f)
		{
			throw Error("You must give height and depth values for left or right faced rectangle.");
		}
		side1 = float3(0.f, size.y, 0.f);
		side2 = float3(0.f, 0.f, size.z);

		if (dir == FaceDir::left)
			side2 = -side2;
	}

	corner = center - 0.5 * (side1 + side2);

	return generateParallelogramMesh(corner, side1, side2);
}

Mesh generateBoxMesh(const float3& lowerCorner, const float3& upperCorner)
{
	float3 diff = upperCorner - lowerCorner;
	Mesh top = generateParallelogramMesh(upperCorner, float3(0.f, 0.f, -diff.z), float3(-diff.x, 0.f, 0.f));
	Mesh right = generateParallelogramMesh(upperCorner, float3(0.f, -diff.y, 0.f), float3(0.f, 0.f, -diff.z));
	Mesh front = generateParallelogramMesh(upperCorner, float3(-diff.x, 0.f, 0.f), float3(0.f, -diff.y, 0.f));
	Mesh bottom = generateParallelogramMesh(lowerCorner, float3(diff.x, 0.f, 0.f), float3(0.f, 0.f, diff.z));
	Mesh left = generateParallelogramMesh(lowerCorner, float3(0.f, 0.f, diff.z), float3(0.f, diff.y, 0.f));
	Mesh back = generateParallelogramMesh(lowerCorner, float3(0.f, diff.y, 0.f), float3(diff.x, 0.f, 0.f));

	Mesh box;
	box.vtxArr.resize(24);
	memcpy(box.vtxArr.data() + 0, top.vtxArr.data(), 4 * sizeof(Vertex));
	memcpy(box.vtxArr.data() + 4, bottom.vtxArr.data(), 4 * sizeof(Vertex));
	memcpy(box.vtxArr.data() + 8, left.vtxArr.data(), 4 * sizeof(Vertex));
	memcpy(box.vtxArr.data() + 12, right.vtxArr.data(), 4 * sizeof(Vertex));
	memcpy(box.vtxArr.data() + 16, front.vtxArr.data(), 4 * sizeof(Vertex));
	memcpy(box.vtxArr.data() + 20, back.vtxArr.data(), 4 * sizeof(Vertex));

	box.tdxArr.resize(12);
	memcpy(box.tdxArr.data() + 0, top.tdxArr.data(), 2 * sizeof(Tridex));
	memcpy(box.tdxArr.data() + 2, bottom.tdxArr.data(), 2 * sizeof(Tridex));
	memcpy(box.tdxArr.data() + 4, left.tdxArr.data(), 2 * sizeof(Tridex));
	memcpy(box.tdxArr.data() + 6, right.tdxArr.data(), 2 * sizeof(Tridex));
	memcpy(box.tdxArr.data() + 8, front.tdxArr.data(), 2 * sizeof(Tridex));
	memcpy(box.tdxArr.data() + 10, back.tdxArr.data(), 2 * sizeof(Tridex));

	for (uint i = 1; i < 6; ++i)
	{
		box.tdxArr[i * 2 + 0].x += i * 4;
		box.tdxArr[i * 2 + 0].y += i * 4;
		box.tdxArr[i * 2 + 0].z += i * 4;
		box.tdxArr[i * 2 + 1].x += i * 4;
		box.tdxArr[i * 2 + 1].y += i * 4;
		box.tdxArr[i * 2 + 1].z += i * 4;
	}

	return box;
}

Mesh generateCubeMesh(const float3& center, const float3& size, bool bottomCenter)
{
	float3 add = bottomCenter ? float3(0.f, size.y / 2.f, 0.f) : float3(0.f);

	Mesh top = generateRectangleMesh(center + float3(0.f, size.y / 2.f, 0.f) + add, float3(size.x, 0.f, size.z), FaceDir::up);
	Mesh bottom = generateRectangleMesh(center + float3(0.f, -size.y / 2.f, 0.f) + add, float3(size.x, 0.f, size.z), FaceDir::down);
	Mesh left = generateRectangleMesh(center + float3(-size.x / 2.f, 0.f, 0.f) + add, float3(0.f, size.y, size.z), FaceDir::left);
	Mesh right = generateRectangleMesh(center + float3(size.x / 2.f, 0.f, 0.f) + add, float3(0.f, size.y, size.z), FaceDir::right);
	Mesh front = generateRectangleMesh(center + float3(0.f, 0.f, size.z / 2.f) + add, float3(size.x, size.y, 0.f), FaceDir::front);
	Mesh back = generateRectangleMesh(center + float3(0.f, 0.f, -size.z / 2.f) + add, float3(size.x, size.y, 0.f), FaceDir::back);

	Mesh box;
	box.vtxArr.resize(24);
	memcpy(box.vtxArr.data() + 0, top.vtxArr.data(), 4 * sizeof(Vertex));
	memcpy(box.vtxArr.data() + 4, bottom.vtxArr.data(), 4 * sizeof(Vertex));
	memcpy(box.vtxArr.data() + 8, left.vtxArr.data(), 4 * sizeof(Vertex));
	memcpy(box.vtxArr.data() + 12, right.vtxArr.data(), 4 * sizeof(Vertex));
	memcpy(box.vtxArr.data() + 16, front.vtxArr.data(), 4 * sizeof(Vertex));
	memcpy(box.vtxArr.data() + 20, back.vtxArr.data(), 4 * sizeof(Vertex));

	box.tdxArr.resize(12);
	memcpy(box.tdxArr.data() + 0, top.tdxArr.data(), 2 * sizeof(Tridex));
	memcpy(box.tdxArr.data() + 2, bottom.tdxArr.data(), 2 * sizeof(Tridex));
	memcpy(box.tdxArr.data() + 4, left.tdxArr.data(), 2 * sizeof(Tridex));
	memcpy(box.tdxArr.data() + 6, right.tdxArr.data(), 2 * sizeof(Tridex));
	memcpy(box.tdxArr.data() + 8, front.tdxArr.data(), 2 * sizeof(Tridex));
	memcpy(box.tdxArr.data() + 10, back.tdxArr.data(), 2 * sizeof(Tridex));

	for (uint i = 1; i < 6; ++i)
	{
		box.tdxArr[i * 2 + 0].x += i * 4;
		box.tdxArr[i * 2 + 0].y += i * 4;
		box.tdxArr[i * 2 + 0].z += i * 4;
		box.tdxArr[i * 2 + 1].x += i * 4;
		box.tdxArr[i * 2 + 1].y += i * 4;
		box.tdxArr[i * 2 + 1].z += i * 4;
	}

	return box;
}

Mesh generateSphereMesh(const float3& center, float radius, uint numSegmentsInMeridian, uint numSegmentsInEquator)
{
	uint M = numSegmentsInMeridian;
	uint E = numSegmentsInEquator;

	if (M < 2)
		throw Error("Give number of sides of meridian more than two.");

	if (E < 3)
		throw Error("Give number of sides of equator more than three.");

	Mesh mesh;

	mesh.vtxArr.resize((M - 1) * E + 2);
	mesh.tdxArr.resize(((M - 2) * 2 + 2) * E);

	const float meridianStep = PI / M;
	const float equatorStep = 2 * PI / E;
	float phi;		// latitude which ranges from 0�� at the Equator to 90�� (North or South) at the poles.
	float lambda;	// longitude 

	uint vertexCount = 0;
	uint triangleCount = 0;

	mesh.vtxArr[vertexCount].position = center + radius * float3(0.f, 1.f, 0.f);
	mesh.vtxArr[vertexCount].normal = float3(0.f, 1.f, 0.f);
	mesh.vtxArr[vertexCount].texcoord = float2(0.f, 0.f);
	++vertexCount;

	for (uint e = 1; e <= E; ++e)
	{
		mesh.tdxArr[triangleCount++] = { 0, e, e % E + 1 };
	}

	for (uint m = 1; m < M; ++m)
	{
		phi = 0.5f * PI - m * meridianStep;

		for (uint e = 1; e <= E; ++e)
		{
			lambda = e * equatorStep;
			float cosPhi = cosf(phi);
			float x = cosPhi * sinf(lambda);
			float y = sinf(phi);
			float z = cosPhi * cosf(lambda);

			if (m < M - 1)
			{
				uint i = vertexCount;
				uint j = (e < E) ? (i + 1) : (i + 1 - E);
				uint k = i + E;
				uint l = j + E;

				mesh.tdxArr[triangleCount++] = { j, i ,k };
				mesh.tdxArr[triangleCount++] = { j, k, l };
			}

			mesh.vtxArr[vertexCount].position = center + radius * float3(x, y, z);
			mesh.vtxArr[vertexCount].normal = float3(x, y, z);
			mesh.vtxArr[vertexCount].texcoord = float2(0.f, 0.f);
			++vertexCount;
		}
	}

	uint baseIdx = vertexCount - E - 1;
	for (uint e = 1; e <= E; ++e)
	{
		mesh.tdxArr[triangleCount++] = { vertexCount, baseIdx + e % E + 1, baseIdx + e };
	}

	mesh.vtxArr[vertexCount].position = center + radius * float3(0.f, -1.f, 0.f);
	mesh.vtxArr[vertexCount].normal = float3(0.f, -1.f, 0.f);
	mesh.vtxArr[vertexCount].texcoord = float2(0.f, 0.f);
	++vertexCount;

	return mesh;
}

void SceneLoader::initializeGeometryFromMeshes(Scene* scene, const vector<Mesh*>& meshes)
{
	scene->clear();

	uint numObjs = (uint)meshes.size();
	vector<Vertex>& vtxArr = scene->vtxArr;
	vector<Tridex>& tdxArr = scene->tdxArr;
	vector<SceneObject>& objArr = scene->objArr;

	objArr.resize(numObjs);

	uint totVertices = 0;
	uint totTridices = 0;

	for (uint i = 0; i < numObjs; ++i)
	{
		uint nowVertices = uint(meshes[i]->vtxArr.size());
		uint nowTridices = uint(meshes[i]->tdxArr.size());

		objArr[i].vertexOffset = totVertices;
		objArr[i].tridexOffset = totTridices;
		objArr[i].numVertices = nowVertices;
		objArr[i].numTridices = nowTridices;

		totVertices += nowVertices;
		totTridices += nowTridices;
	}

	vtxArr.resize(totVertices);
	tdxArr.resize(totTridices);

	for (uint i = 0; i < numObjs; ++i)
	{
		memcpy(&vtxArr[objArr[i].vertexOffset], &meshes[i]->vtxArr[0], sizeof(Vertex) * objArr[i].numVertices);
		memcpy(&tdxArr[objArr[i].tridexOffset], &meshes[i]->tdxArr[0], sizeof(Tridex) * objArr[i].numTridices);
	}
}

void SceneLoader::computeModelMatrices(Scene* scene)
{
	for (auto& obj : scene->objArr)
	{
		obj.modelMatrix = composeMatrix(obj.translation, obj.rotation, obj.scale);
	}
}

Scene* SceneLoader::push_simpleSphere()
{
	Scene* scene = new Scene;
	sceneArr.push_back(scene);

	Mesh ground = generateSphereMesh(float3(0, -100.5, -1), 100);
	Mesh sphere = generateSphereMesh(float3(0, 0, -1), 0.5f);

	initializeGeometryFromMeshes(scene, { &ground, &sphere });

	for (uint i = 0; i < scene->objArr.size(); i++)
	{
		scene->objArr[i].translation = float3(0);
	}

	computeModelMatrices(scene);

	return scene;
}