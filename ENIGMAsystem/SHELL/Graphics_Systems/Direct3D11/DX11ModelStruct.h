       /** Copyright (C) 2013 Robert B. Colton, Adriano Tumminelli
***
*** This file is a part of the ENIGMA Development Environment.
***
*** ENIGMA is free software: you can redistribute it and/or modify it under the
*** terms of the GNU General Public License as published by the Free Software
*** Foundation, version 3 of the license or any later version.
***
*** This application and its source code is distributed AS-IS, WITHOUT ANY
*** WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
*** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
*** details.
***
*** You should have received a copy of the GNU General Public License along
*** with this code. If not, see <http://www.gnu.org/licenses/>
**/

#include "Bridges/General/DX11Context.h"
#include "../General/GSd3d.h"
#include "../General/GSprimitives.h"
#include "Universal_System/var4.h"
#include "Universal_System/roomsystem.h"
#include <math.h>
#include <stdlib.h>


using namespace std;

#define __GETR(x) ((x & 0x0000FF))
#define __GETG(x) ((x & 0x00FF00)>>8)
#define __GETB(x) ((x & 0xFF0000)>>16)
#define __GETRf(x) fmod(__GETR(x),256)
#define __GETGf(x) fmod(x/256,256)
#define __GETBf(x) fmod(x/65536,256)*

#include <iostream>
#include <map>
#include <list>
#include "Universal_System/fileio.h"
#include "Universal_System/estring.h"

#include <vector>
using std::vector;

extern int ptypes_by_id[16];
namespace enigma {
  extern unsigned char currentcolor[4];

  //split a string and convert to float
  vector<float> float_split(const string& str, const char& ch) {
    string next;
    vector<float> result;

    for (string::const_iterator it = str.begin(); it != str.end(); it++)
	{
		if (*it == ch)
		{
			if (!next.empty())
			{
				result.push_back(atof(next.c_str()));
				next.clear();
			}
        } else {
            next += *it;
        }
    }
    if (!next.empty())
         result.push_back(atof(next.c_str()));
    return result;
  }

  //obj model parsing functions
  void string_parse( string *s )
  {
	size_t spaces = 0;
	bool trimmed = false;
	bool checknormal = false;
	for (unsigned int i = 0; i < s->size() ; i++)
	{
		//comment
		if ((*s)[i] == '#')
		{
			s->erase(i, s->length() - i);
			break;
		}
		else if((*s)[i] == ' ')
		{
			if (!trimmed)
			{
				s->erase(i,1);
				i--;
			}
			else
			{
				if (spaces >= 1)
				{
					s->erase(i,1);
					i--;
				}
				spaces++;
			}
		}
		else
		{
			if((*s)[i] == '/')
			{
				(*s)[i] = ' ';
				if(checknormal)
				{
					s->erase(i, 1);
					checknormal = false;
				}
				else
					checknormal = true;
			}
			else
				checknormal = false;
			spaces = 0;
			trimmed = true;
		}
	}
	//end trim
	if (s->size() > 0) {
		if ((*s)[s->size()-1] == ' ')
		{
			s->erase(s->size()-1, 1);
		}
	}
  }
}

union VertexElement {
	unsigned long d;
	gs_scalar f;

	VertexElement(gs_scalar v): f(v) {}
	VertexElement(unsigned long v): d(v) {}
};

/* Mesh clearing has a memory leak */
class Mesh
{
  public:
  unsigned currentPrimitive; //The type of the current primitive being added to the model

  vector<VertexElement> vertices; // Temporary vertices container for the current primitive until they are batched
  vector<unsigned> indices; // Temporary indices that can optionally be supplied, otherwise they will get generated by the batcher.
  vector<VertexElement> triangleVertices; // The vertices added to triangle primitives batched into a single triangle list to be buffered to the GPU
  vector<VertexElement> triangleIndexedVertices; // The vertices added to indexed triangle primitives batched into a single triangle list to be buffered to the GPU
  vector<unsigned> triangleIndices; // The triangle indices either concatenated by batching or supplied in the temporary container.
  vector<VertexElement> lineVertices; // The vertices added to line primitives batched into a single line list to be buffered to the GPU
  vector<VertexElement> lineIndexedVertices; // The vertices added to indexed line primitives batched into a single line list to be buffered to the GPU
  vector<unsigned> lineIndices; // The line indices either concatenated by batching or supplied in the temporary container.
  vector<VertexElement> pointVertices; // The vertices added to point primitives batched into a single point list to be buffered to the GPU
  vector<VertexElement> pointIndexedVertices; // The vertices added to indexed point primitives batched into a single point list to be buffered to the GPU
  vector<unsigned> pointIndices; // The point indices either concatenated by batching or supplied in the temporary container.

  unsigned vertexStride; // Whether the vertices are 2D or 3D
  bool useDepth; // Whether or not the Z-values should be treated as a depth component
  bool useColors; // If colors have been added to the model
  bool useTextures; // If texture coordinates have been added
  bool useNormals; // If normals have been added

  unsigned pointCount; // The number of indices in the point buffer
  unsigned triangleCount; // The number of indices in the triangle buffer
  unsigned triangleVertCount; // The number of vertices in the triangle buffer
  unsigned lineCount; // The number of indices in the line buffer
  unsigned lineVertCount; //The number of vertices in the line buffer

  unsigned indexedoffset; // The number of indexed vertices
  unsigned pointIndexedCount; // The number of point indices
  unsigned triangleIndexedCount; // The number of triangle indices
  unsigned lineIndexedCount; // The number of line indices

  // Indexed primitives are first since the indices must be offset, and keeps them as small as possible.
  // INDEXEDTRIANGLES|INDEXEDLINES|INDEXEDPOINTS|TRIANGLES|LINES|POINTS
  ID3D11Buffer* vertexbuffer;    // Interleaved vertex buffer object TRIANGLES|LINES|POINTS with triangles first since they are most likely to be used
  ID3D11Buffer* indexbuffer;    // Interleaved index buffer object TRIANGLES|LINES|POINTS with triangles first since they are most likely to be used

  bool vbodynamic; // Whether the buffer is dynamically allocated in system memory, should be true for simple primitive calls
  bool vbobuffered; // Whether or not the buffer objects have been generated
  bool vboindexed; // Whether or not the model contains any indexed primitives or just regular lists

  void SetPrimitive(int pr) {
	vbobuffered = false;
	currentPrimitive = pr;
  }

  Mesh (bool dynamic)
  {
	triangleIndexedVertices.reserve(64000);
	pointIndexedVertices.reserve(64000);
	lineIndexedVertices.reserve(64000);
	pointVertices.reserve(64000);
	pointIndices.reserve(64000);
	lineVertices.reserve(64000);
	lineIndices.reserve(64000);
	triangleVertices.reserve(64000);
	triangleIndices.reserve(64000);
	vertices.reserve(64000);
	indices.reserve(64000);

    vertexbuffer = NULL;    // the pointer to the vertex buffer
	indexbuffer = NULL;    // the pointer to the index buffer

    vbobuffered = false;
	vbodynamic = dynamic;

	vertexStride = 0;
	useDepth = false;
	useColors = false;
    useTextures = false;
    useNormals = false;

	pointCount = 0;
	triangleCount = 0;
	triangleVertCount = 0;
	lineCount = 0;
	lineVertCount = 0;

	indexedoffset = 0;
	pointIndexedCount = 0;
	triangleIndexedCount = 0;
	lineIndexedCount = 0;

    currentPrimitive = 0;
  }

  ~Mesh()
  {
	// Release the buffers and make sure we don't leave hanging pointers.
	if (vertexbuffer != NULL) {
		vertexbuffer->Release();
		vertexbuffer = NULL;
	}
	if (indexbuffer != NULL) {
		indexbuffer->Release();
		indexbuffer = NULL;
	}
  }

  void ClearData()
  {
    triangleVertices.clear();
	pointVertices.clear();
	lineVertices.clear();
	triangleIndexedVertices.clear();
	pointIndexedVertices.clear();
	lineIndexedVertices.clear();
	triangleIndices.clear();
	pointIndices.clear();
	lineIndices.clear();
  }

  void Clear()
  {
    ClearData();

	triangleIndexedVertices.reserve(64000);
	pointIndexedVertices.reserve(64000);
	lineIndexedVertices.reserve(64000);
	pointVertices.reserve(64000);
	pointIndices.reserve(64000);
	lineVertices.reserve(64000);
	lineIndices.reserve(64000);
	triangleVertices.reserve(64000);
	triangleIndices.reserve(64000);
	vertices.reserve(64000);
	indices.reserve(64000);

	vbobuffered = false;

	vertexStride = 0;
	useColors = false;
    useTextures = false;
    useNormals = false;

	pointCount = 0;
	triangleCount = 0;
	triangleVertCount = 0;
	lineCount = 0;
	lineVertCount = 0;

	indexedoffset = 0;
	pointIndexedCount = 0;
	triangleIndexedCount = 0;
	lineIndexedCount = 0;
  }

  unsigned GetStride() {
	unsigned stride = vertexStride;
    if (useNormals) stride += 3;
	if (useTextures) stride += 2;
    if (useColors) stride += 1;
	return stride;
  }

  void Begin(int pt)
  {
    vbobuffered = false;
    currentPrimitive = pt;
  }

  void AddVertex(gs_scalar x, gs_scalar y)
  {
    vertices.push_back(x); vertices.push_back(y); vertices.push_back(0.0f);
	vertexStride = 3;
  }

  void AddVertex(gs_scalar x, gs_scalar y, gs_scalar z)
  {
    vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
	vertexStride = 3;
  }

  void AddIndex(unsigned ind)
  {
    indices.push_back(ind);
  }

  void AddNormal(gs_scalar nx, gs_scalar ny, gs_scalar nz)
  {
    vertices.push_back(nx); vertices.push_back(ny); vertices.push_back(nz);
	useNormals = true;
  }

  void AddTexture(gs_scalar tx, gs_scalar ty)
  {
    vertices.push_back(tx); vertices.push_back(ty);
	useTextures = true;
  }

  void AddColor(int col, double alpha)
  {
	//DWORD final = D3DCOLOR_ARGB( (unsigned char)(alpha*255), __GETR(col), __GETG(col), __GETB(col) );
	//vertices.push_back(final);
	useColors = true;
  }

  void End()
  {
	//NOTE: This batching only checks for degenerate primitives on triangle strips and fans since the GPU does not render triangles where the two
	//vertices are exactly the same, triangle lists could also check for degenerates, it is unknown whether the GPU will render a degenerative
	//in a line strip primitive.

	unsigned stride = GetStride();
	if (vertices.size() == 0) return;

	// Primitive has ended so now we need to batch the vertices that were given into single lists, eg. line list, triangle list, point list
	// Indices are optionally supplied, model functions can also be added for the end user to supply the indexing themselves for each primitive
	// but the batching system does not care either way if they are not supplied it will automatically generate them.
	switch (currentPrimitive) {
		case enigma_user::pr_pointlist:
			if (indices.size() > 0) {
				pointIndexedVertices.insert(pointIndexedVertices.end(), vertices.begin(), vertices.end());
				for (std::vector<unsigned>::iterator it = indices.begin(); it != indices.end(); ++it) { *it += pointIndexedCount; }
				pointIndices.insert(pointIndices.end(), indices.begin(), indices.end());
			} else {
				pointVertices.insert(pointVertices.end(), vertices.begin(), vertices.end());
				pointCount += vertices.size() / stride;
			}

			break;
		case enigma_user::pr_linelist:
			if (indices.size() > 0) {
				lineIndexedVertices.insert(lineIndexedVertices.end(), vertices.begin(), vertices.end());
				for (std::vector<unsigned>::iterator it = indices.begin(); it != indices.end(); ++it) { *it += lineIndexedCount; }
				lineIndices.insert(lineIndices.end(), indices.begin(), indices.end());
			} else {
				lineVertices.insert(lineVertices.end(), vertices.begin(), vertices.end());
				lineCount += vertices.size() / stride;
			}
			break;
		case enigma_user::pr_linestrip:
			lineIndexedVertices.insert(lineIndexedVertices.end(), vertices.begin(), vertices.end());
			if (indices.size() > 0) {
				for (std::vector<unsigned>::iterator it = indices.begin(); it != indices.end(); ++it) { *it += lineIndexedCount; }
				for (unsigned i = 0; i < indices.size() - 2; i++) {
					lineIndices.push_back(indices[i]);
					lineIndices.push_back(indices[i + 1]);
				}
			} else {
				unsigned offset = (lineIndexedVertices.size() - vertices.size()) / stride;
				for (unsigned i = 0; i < vertices.size() / stride - 1; i++) {
					lineIndices.push_back(offset + i);
					lineIndices.push_back(offset + i + 1);
				}
			}
			break;
		case enigma_user::pr_trianglelist:
			if (indices.size() > 0) {
				triangleIndexedVertices.insert(triangleIndexedVertices.end(), vertices.begin(), vertices.end());
				for (std::vector<unsigned>::iterator it = indices.begin(); it != indices.end(); ++it) { *it += triangleIndexedCount; }
				triangleIndices.insert(triangleIndices.end(), indices.begin(), indices.end());
			} else {
				triangleVertices.insert(triangleVertices.end(), vertices.begin(), vertices.end());
				triangleCount += vertices.size() / stride;
			}
			break;
		case enigma_user::pr_trianglestrip:
			triangleIndexedVertices.insert(triangleIndexedVertices.end(), vertices.begin(), vertices.end());
			if (indices.size() > 0) {
				for (std::vector<unsigned>::iterator it = indices.begin(); it != indices.end(); ++it) { *it += triangleIndexedCount; }
				for (unsigned i = 0; i < indices.size() - 2; i++) {
					// check for and continue if indexed triangle is degenerate, because the GPU won't render it anyway
					if (indices[i] == indices[i + 1] || indices[i] == indices[i + 2]  || indices[i + 1] == indices[i + 2] ) { continue; }
					triangleIndices.push_back(indices[i]);
					triangleIndices.push_back(indices[i+1]);
					triangleIndices.push_back(indices[i+2]);
				}
			} else {
				unsigned offset = (triangleIndexedVertices.size() - vertices.size()) / stride;
				unsigned elements = (vertices.size() / stride - 2);

				for (unsigned i = 0; i < elements; i++) {
					if (i % 2) {
						triangleIndices.push_back(offset + i + 2);
						triangleIndices.push_back(offset + i + 1);
						triangleIndices.push_back(offset + i);
					} else {
						triangleIndices.push_back(offset + i);
						triangleIndices.push_back(offset + i + 1);
						triangleIndices.push_back(offset + i + 2);
					}
				}
			}
			break;
		case enigma_user::pr_trianglefan:
			triangleIndexedVertices.insert(triangleIndexedVertices.end(), vertices.begin(), vertices.end());
			if (indices.size() > 0) {
				for (std::vector<unsigned>::iterator it = indices.begin(); it != indices.end(); ++it) { *it += triangleIndexedCount; }
				for (unsigned i = 1; i < indices.size() - 1; i++) {
					// check for and continue if indexed triangle is degenerate, because the GPU won't render it anyway
					if (indices[0] == indices[i] || indices[0] == indices[i + 1]  || indices[i] == indices[i + 1] ) { continue; }
					triangleIndices.push_back(indices[0]);
					triangleIndices.push_back(indices[i]);
					triangleIndices.push_back(indices[i + 1]);
				}
			} else {
				unsigned offset = (triangleIndexedVertices.size() - vertices.size()) / stride;
				for (unsigned i = 1; i < vertices.size() / stride - 1; i++) {
					triangleIndices.push_back(offset);
					triangleIndices.push_back(offset + i);
					triangleIndices.push_back(offset + i + 1);
				}
			}
			break;
	}

	// Clean up the temporary vertex and index containers now that they have been batched efficiently
	vertices.clear();
	indices.clear();
  }

  void BufferGenerate()
  {
	vector<VertexElement> vdata;
	vector<unsigned> idata;

	vdata.reserve(triangleIndexedVertices.size() + lineIndexedVertices.size() + pointIndexedVertices.size() + triangleVertices.size() + lineVertices.size() + pointVertices.size());
	idata.reserve(triangleIndices.size() + lineIndices.size() + pointIndices.size());

	unsigned interleave = 0;

	if (triangleIndices.size() > 0) {
		vdata.insert(vdata.end(), triangleIndexedVertices.begin(), triangleIndexedVertices.end());
		idata.insert(idata.end(), triangleIndices.begin(), triangleIndices.end());
		interleave += triangleIndexedCount;
		triangleVertCount = triangleIndexedVertices.size();
		triangleIndexedCount = triangleIndices.size();
	}

	if (lineIndices.size() > 0) {
		vdata.insert(vdata.end(), lineIndexedVertices.begin(), lineIndexedVertices.end());
		for (unsigned i = 0; i < lineIndices.size(); i++) { lineIndices[i] += interleave; }
		idata.insert(idata.end(), lineIndices.begin(), lineIndices.end());
		interleave += lineIndexedCount;
		lineVertCount = lineIndexedVertices.size();
		lineIndexedCount = lineIndices.size();
	}

	if (pointIndices.size() > 0) {
		vdata.insert(vdata.end(), pointIndexedVertices.begin(), pointIndexedVertices.end());
		for (unsigned i = 0; i < pointIndices.size(); i++) { pointIndices[i] += interleave; }
		idata.insert(idata.end(), pointIndices.begin(), pointIndices.end());
		//pointVertCount = pointIndexedVertices.size();
		pointIndexedCount = pointIndices.size();
	}

	/*
	if (indexbuffer != NULL) {
		D3DINDEXBUFFER_DESC pDesc;
		indexbuffer->GetDesc(&pDesc);
		if (pDesc.Size < idata.size() * sizeof( unsigned ) || idata.size() == 0) {
			indexbuffer->Release();
			indexbuffer = NULL;
		}
	}

	if (idata.size() > 0) {
		vboindexed = true;
		indexedoffset += vdata.size();
		// create a index buffer interface
		if (indexbuffer == NULL) {
			if (vbodynamic) {
				d3dmgr->CreateIndexBuffer(idata.size() * sizeof(unsigned), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_SYSTEMMEM, &indexbuffer, NULL);
			} else {
				d3dmgr->CreateIndexBuffer(idata.size() * sizeof(unsigned), D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_MANAGED, &indexbuffer, NULL);
			}
		}
		// lock index buffer and load the indices into it
		VOID* iVoid;    // a void pointer
		indexbuffer->Lock(0, 0, (void**)&iVoid, D3DLOCK_DISCARD);
		memcpy(iVoid, &idata[0], idata.size() * sizeof(unsigned));
		indexbuffer->Unlock();

		// Clean up temporary interleaved data
		idata.clear();
	} else {
		vboindexed = false;
	}
	*/
	HRESULT result;
	D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc;
	D3D11_SUBRESOURCE_DATA vertexData, indexData;

	// Set up the description of the static index buffer.
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = idata.size() * sizeof(unsigned);
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the index data.
	indexData.pSysMem = &idata[0];
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;

	// Create the index buffer.
	result = m_device->CreateBuffer(&indexBufferDesc, &indexData, &indexbuffer);
	if(FAILED(result))
	{
		return;
	}

	if (triangleCount > 0) {
		vdata.insert(vdata.end(), triangleVertices.begin(), triangleVertices.end());
	}

	if (lineCount > 0) {
		vdata.insert(vdata.end(), lineVertices.begin(), lineVertices.end());
	}

	if (pointCount > 0) {
		vdata.insert(vdata.end(), pointVertices.begin(), pointVertices.end());
	}


	unsigned stride = vertexStride;
	/*
	if (vertexbuffer != NULL) {
		D3DVERTEXBUFFER_DESC pDesc;
		vertexbuffer->GetDesc(&pDesc);
		if (pDesc.Size < vdata.size() * sizeof( gs_scalar )) {
			vertexbuffer->Release();
			vertexbuffer = NULL;
		}
	}
	// create a vertex buffer interface
	if (vertexbuffer == NULL) {
		if (vbodynamic) {
			d3dmgr->CreateVertexBuffer(vdata.size() * sizeof( gs_scalar ), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, GetFVF(), D3DPOOL_SYSTEMMEM, &vertexbuffer, NULL);
		} else {
			d3dmgr->CreateVertexBuffer(vdata.size() * sizeof( gs_scalar ), D3DUSAGE_WRITEONLY, GetFVF(), D3DPOOL_MANAGED, &vertexbuffer, NULL);
		}
	}

	// Send the data to the GPU
	// lock vertex buffer and load the vertices into it
	VOID* pVoid;    // a void pointer
	vertexbuffer->Lock(0, 0, (VOID**)&pVoid, D3DLOCK_DISCARD);
	memcpy(pVoid, &vdata[0], vdata.size() * sizeof(gs_scalar));

	vertexbuffer->Unlock();
*/

	// Set up the description of the static vertex buffer.
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = vdata.size() * sizeof(gs_scalar);
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the vertex data.
	vertexData.pSysMem = &vdata[0];
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	// Now create the vertex buffer.
	result = m_device->CreateBuffer(&vertexBufferDesc, &vertexData, &vertexbuffer);
	if (FAILED(result))
	{
		return;
	}

	// Clean up temporary interleaved data
	vdata.clear();
    // Clean up the data from RAM it is now safe on VRAM
    ClearData();
  }

  void Draw()
  {
	if (!GetStride()) { return; }
    if (vertexbuffer == NULL || !vbobuffered) {
	  vbobuffered = true;
      BufferGenerate();
    }

	unsigned stride = GetStride();
	unsigned offset = 0, base = 0;

	// Set the vertex buffer to active in the input assembler so it can be rendered.
	m_deviceContext->IASetVertexBuffers(0, 1, &vertexbuffer, &stride, &offset);
	if (vboindexed) {
		// Set the index buffer to active in the input assembler so it can be rendered.
		m_deviceContext->IASetIndexBuffer(indexbuffer, DXGI_FORMAT_R32_UINT, 0);
	}

	// Draw the indexed primitives
	if (triangleIndexedCount > 0) {
		m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_deviceContext->DrawIndexed(triangleIndexedCount / 3, offset, base);
		offset += triangleIndexedCount;
		base += triangleVertCount/stride;
	}
	if (lineVertCount > 0) {
		m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		m_deviceContext->DrawIndexed(lineIndexedCount/2, offset, base);
		offset += lineIndexedCount;
		base += lineVertCount/stride;
	}
	if (pointIndexedCount > 0) {
		m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		m_deviceContext->DrawIndexed(pointIndexedCount, offset, base);
	}

	offset = indexedoffset/stride;

	// Draw the unindexed primitives
	if (triangleCount > 0) {
		m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_deviceContext->Draw(triangleCount / 3, offset);
		offset += triangleCount / 3;
	}
	if (lineCount > 0) {
		m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		m_deviceContext->Draw(lineCount / 2, offset);
		offset += lineCount / 2;
	}
	if (pointCount > 0) {
		m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		m_deviceContext->Draw(pointCount, offset);
	}
  }
};

extern vector<Mesh*> meshes;
