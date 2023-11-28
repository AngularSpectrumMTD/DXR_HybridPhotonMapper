#include "utility/OBJ.h"
#include <codecvt>

OBJ_MODEL::OBJ_MODEL() {
}
OBJ_MODEL::OBJ_MODEL(std::unique_ptr<dx12::RenderDeviceDX12>& device, const char* FileName) {
	OBJ_Load(device, FileName);
}

bool OBJ_MODEL::OBJ_Load(std::unique_ptr<dx12::RenderDeviceDX12>& device, const char* FileName) {
	vec4i Face[3];
	vector <DirectX::XMFLOAT3> Vertex;
	vector <DirectX::XMFLOAT3> Normal;
	vector <DirectX::XMFLOAT2> uv;

	s32 matID = 0;
	char key[255] = { 0 };

	FILE* fp = NULL;
	fopen_s(&fp, FileName, "rt");

	fseek(fp, SEEK_SET, 0);

	DirectX::XMFLOAT3 vec3d;
	DirectX::XMFLOAT2 vec2d;

	while (!feof(fp))
	{
		//key mtlib / v / vn etc...
		memset(key, 0, sizeof(char) * 255);
		fscanf_s(fp, "%s ", key, sizeof(key));

		if (strcmp(key, "mtllib") == 0) {
			fscanf_s(fp, "%s ", key, sizeof(key));
			LoadMaterialFromFile(device, key);
		}
		if (strcmp(key, "v") == 0) {
			fscanf_s(fp, "%f %f %f", &vec3d.x, &vec3d.y, &vec3d.z);
			Vertex.push_back(vec3d);
		}
		if (strcmp(key, "vn") == 0) {
			fscanf_s(fp, "%f %f %f", &vec3d.x, &vec3d.y, &vec3d.z);
			Normal.push_back(vec3d);
		}
		if (strcmp(key, "vt") == 0) {
			fscanf_s(fp, "%f %f", &vec2d.x, &vec2d.y);
			uv.push_back(vec2d);
		}
		if (strcmp(key, "usemtl") == 0) {
			fscanf_s(fp, "%s ", key, sizeof(key));
			for (s32 i = 0; i < (signed)Material.size(); i++) {
				if (strcmp(key, Material[i].MaterialName.c_str()) == 0)
				{
					matID = i;
				}
			}
		}
		//face id0=vertex 1=UV 2=normal
		if (strcmp(key, "f") == 0) {
			Face[0].w = -1;
			Face[1].w = -1;
			Face[2].w = -1;
			fscanf_s(fp, "%d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d", 
				&Face[0].x, &Face[1].x, &Face[2].x, 
				&Face[0].y, &Face[1].y, &Face[2].y, 
				&Face[0].z, &Face[1].z, &Face[2].z, 
				&Face[0].w, &Face[1].w, &Face[2].w);
			//Triangle
			if ((Face[0].w == -1) && (Face[1].w == -1) && (Face[2].w == -1)) {
				Material[matID].TriangleVertexIDTbl.push_back(Face[0].x - 1);
				Material[matID].TriangleVertexIDTbl.push_back(Face[0].y - 1);
				Material[matID].TriangleVertexIDTbl.push_back(Face[0].z - 1);
				Material[matID].TriangleUVIDTbl.push_back(Face[1].x - 1);
				Material[matID].TriangleUVIDTbl.push_back(Face[1].y - 1);
				Material[matID].TriangleUVIDTbl.push_back(Face[1].z - 1);
				Material[matID].TriangleNormalIDTbl.push_back(Face[2].x - 1);
				Material[matID].TriangleNormalIDTbl.push_back(Face[2].y - 1);
				Material[matID].TriangleNormalIDTbl.push_back(Face[2].z - 1);
			}
			else {//Rectangle
				Material[matID].QuadrangleVertexIDTbl.push_back(Face[0].x - 1);
				Material[matID].QuadrangleVertexIDTbl.push_back(Face[0].y - 1);
				Material[matID].QuadrangleVertexIDTbl.push_back(Face[0].z - 1);
				Material[matID].QuadrangleVertexIDTbl.push_back(Face[0].w - 1);
				Material[matID].QuadrangleUVIDTbl.push_back(Face[1].x - 1);
				Material[matID].QuadrangleUVIDTbl.push_back(Face[1].y - 1);
				Material[matID].QuadrangleUVIDTbl.push_back(Face[1].z - 1);
				Material[matID].QuadrangleUVIDTbl.push_back(Face[1].w - 1);
				Material[matID].QuadrangleNormalIDTbl.push_back(Face[2].x - 1);
				Material[matID].QuadrangleNormalIDTbl.push_back(Face[2].y - 1);
				Material[matID].QuadrangleNormalIDTbl.push_back(Face[2].z - 1);
				Material[matID].QuadrangleNormalIDTbl.push_back(Face[2].w - 1);
			}
		}
	}
	for (s32 j = 0; j < (signed)Material.size(); j++) {
		for (s32 i = 0; i < (signed)Material[j].TriangleVertexIDTbl.size(); i++) {
			utility::VertexPNT Tri;
			Tri.Position = Vertex[Material[j].TriangleVertexIDTbl[i]];
			Tri.Normal = Normal[Material[j].TriangleNormalIDTbl[i]];
			Tri.UV = uv[Material[j].TriangleUVIDTbl[i]];
			Material[j].TrigonTbl.push_back(Tri);
		}
		for (s32 i = 0; i < (signed)Material[j].QuadrangleVertexIDTbl.size(); i++) {
			utility::VertexPNT Quad;
			Quad.Position = Vertex[Material[j].QuadrangleVertexIDTbl[i]];
			Quad.Normal = Normal[Material[j].QuadrangleNormalIDTbl[i]];
			Quad.UV = uv[Material[j].QuadrangleUVIDTbl[i]];
			Material[j].TetragonTbl.push_back(Quad);
		}
		/*Material[j].TriangleVertexIDTbl.clear();
		Material[j].TriangleNormalIDTbl.clear();
		Material[j].TriangleUVIDTbl.clear();
		Material[j].QuadrangleVertexIDTbl.clear();
		Material[j].QuadrangleNormalIDTbl.clear();
		Material[j].QuadrangleUVIDTbl.clear();*/
	}
	Vertex.clear();
	Normal.clear();
	uv.clear();
	return true;
}

std::wstring StringToWString(std::string str)
{
	const int bufSize = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, (wchar_t*)NULL, 0);
	wchar_t* WCSPtr = new wchar_t[bufSize];

	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, WCSPtr, bufSize);

	std::wstring wstr(WCSPtr, WCSPtr + bufSize - 1);
	delete[] WCSPtr;
	return wstr;
}

bool OBJ_MODEL::LoadMaterialFromFile(std::unique_ptr<dx12::RenderDeviceDX12>& device, const char* FileName) {
	FILE* fp = NULL;
	fopen_s(&fp, FileName, "rt");
	char key[255] = { 0 };
	bool flag = false;
	bool flag2 = false;
	DirectX::XMFLOAT4 vec4d;
	vec4d.x = 0.0f;
	vec4d.y = 0.0f;
	vec4d.z = 0.0f;
	vec4d.w = 0.0f;
	MATERIAL mtl;

	mtl.Reflection4Color.emission = vec4d;
	mtl.Reflection4Color.ambient = vec4d;
	mtl.Reflection4Color.diffuse = vec4d;
	mtl.Reflection4Color.specular = vec4d;

	mtl.Shininess = 0.0f;
	mtl.TexID = 0;

	fseek(fp, SEEK_SET, 0);

	while (!feof(fp))
	{
		//key newmtl / Ka / Kd etc...
		fscanf_s(fp, "%s ", key, sizeof(key));
		if (strcmp(key, "newmtl") == 0)
		{
			if (flag) { Material.push_back(mtl); mtl.TexID = 0; }
			flag = true;
			fscanf_s(fp, "%s ", key, sizeof(key));
			mtl.MaterialName = key;
			flag2 = false;
		}
		if (strcmp(key, "Ka") == 0)
		{
			fscanf_s(fp, "%f %f %f", &vec4d.x, &vec4d.y, &vec4d.z);
			mtl.Reflection4Color.ambient = vec4d;
		}
		if (strcmp(key, "Kd") == 0)
		{
			fscanf_s(fp, "%f %f %f", &vec4d.x, &vec4d.y, &vec4d.z);
			mtl.Reflection4Color.diffuse = vec4d;
		}
		if (strcmp(key, "Ks") == 0)
		{
			fscanf_s(fp, "%f %f %f", &vec4d.x, &vec4d.y, &vec4d.z);
			mtl.Reflection4Color.specular = vec4d;
		}
		if (strcmp(key, "Ns") == 0)
		{
			fscanf_s(fp, "%f", &vec4d.x);
			mtl.Shininess = vec4d.x;
		}
		if (strcmp(key, "map_Kd") == 0)//only use map_Kd
		{
			fscanf_s(fp, "%s ", key, sizeof(key));
			for (s32 i = 0; i < (signed)Material.size(); i++) {
				if (strcmp(key, Material[i].TextureName.c_str()) == 0) {
					flag2 = true;
					mtl.TexID = Material[i].TexID;
					break;
				}
			}
			if (flag2) {
			}
			else {
				mtl.TextureName = key;

				if (
					mtl.Reflection4Color.diffuse.x == 0 &&
					mtl.Reflection4Color.diffuse.y == 0 &&
					mtl.Reflection4Color.diffuse.z == 0 &&
					mtl.Reflection4Color.diffuse.w == 0 
					)
				{
					mtl.Reflection4Color.diffuse = DirectX::XMFLOAT4(1.0, 1.0, 1.0, 1.0);
				}

				mtl.DiffuseTexture = utility::LoadTextureFromFile(device, StringToWString(mtl.TextureName));
				//generate tex by this name
			}
		}
	}
	fclose(fp);

	if (flag)
	{
		Material.push_back(mtl);
	}

	return true;
}

u32 OBJ_MODEL::getTrigonTblCount()
{
	u32 count = 0;
	for (auto& m : Material)
	{
		count += m.TrigonTbl.size();
	}
	return count;
}

u32 OBJ_MODEL::getTetragonTblCount()
{
	u32 count = 0;
	for (auto& m : Material)
	{
		count += m.TetragonTbl.size();
	}
	return count;
}

u32 OBJ_MODEL::getTriangleVertexIDTblCount()
{
	u32 count = 0;
	for (auto& m : Material)
	{
		count += m.TriangleVertexIDTbl.size();
	}
	return count;
}

u32 OBJ_MODEL::getTriangleNormalIDTblCount()
{
	u32 count = 0;
	for (auto& m : Material)
	{
		count += m.TriangleNormalIDTbl.size();
	}
	return count;
}

u32 OBJ_MODEL::getTriangleUVIDTblCount()
{
	u32 count = 0;
	for (auto& m : Material)
	{
		count += m.TriangleUVIDTbl.size();
	}
	return count;
}

u32 OBJ_MODEL::getQuadrangleVertexIDTblCount()
{
	u32 count = 0;
	for (auto& m : Material)
	{
		count += m.QuadrangleVertexIDTbl.size();
	}
	return count;
}

u32 OBJ_MODEL::getQuadrangleNormalIDTblCount()
{
	u32 count = 0;
	for (auto& m : Material)
	{
		count += m.QuadrangleNormalIDTbl.size();
	}
	return count;
}

u32 OBJ_MODEL::getQuadrangleUVIDTblCount()
{
	u32 count = 0;
	for (auto& m : Material)
	{
		count += m.QuadrangleUVIDTbl.size();
	}
	return count;
}
