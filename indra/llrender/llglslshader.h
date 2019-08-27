/** 
 * @file llglslshader.h
 * @brief GLSL shader wrappers
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLGLSLSHADER_H
#define LL_LLGLSLSHADER_H

#ifdef LL_RELEASE_FOR_DOWNLOAD
#define UNIFORM_ERRS LL_WARNS_ONCE("Shader")
#else
#define UNIFORM_ERRS LL_ERRS("Shader")
#endif

#include "llgl.h"
#include "llrender.h"
#include "llstaticstringtable.h"

class LLShaderFeatures
{
public:
	bool atmosphericHelpers;
	bool calculatesLighting;
	bool calculatesAtmospherics;
	bool hasLighting; // implies no transport (it's possible to have neither though)
	bool isAlphaLighting; // indicates lighting shaders need not be linked in (lighting performed directly in alpha shader to match deferred lighting functions)
	bool isShiny;
	bool isFullbright; // implies no lighting
	bool isSpecular;
	bool hasWaterFog; // implies no gamma
	bool hasTransport; // implies no lighting (it's possible to have neither though)
	bool hasSkinning;	
	bool hasObjectSkinning;
	bool hasAtmospherics;
	bool hasGamma;
	S32 mIndexedTextureChannels;
	bool disableTextureIndex;
	bool hasAlphaMask;
	bool attachNothing;

	// char numLights;
	
	LLShaderFeatures();
};

class LLGLSLShader
{
public:

	enum 
	{
		SG_DEFAULT = 0,
		SG_SKY,
		SG_WATER
	};

	struct gl_uniform_data_t {
		std::string name;
		GLenum type = -1;
		GLint size = -1;
		U32 texunit_priority = UINT_MAX; // Lower gets earlier texunits indices.
	};
	
	static std::set<LLGLSLShader*> sInstances;
	static bool sProfileEnabled;

	LLGLSLShader();
	~LLGLSLShader();

	static GLuint sCurBoundShader;
	static LLGLSLShader* sCurBoundShaderPtr;
	static S32 sIndexedTextureChannels;
	static bool sNoFixedFunction;

	static void initProfile();
	static void finishProfile(bool emit_report = true);

	static void startProfile();
	static void stopProfile(U32 count, U32 mode);

	void unload();
	void clearStats();
	void dumpStats();
	void placeProfileQuery();
	void readProfileQuery(U32 count, U32 mode);

	BOOL createShader(std::vector<LLStaticHashedString> * attributes,
						std::vector<LLStaticHashedString> * uniforms,
						U32 varying_count = 0,
						const char** varyings = nullptr);
	BOOL attachShader(const std::string& shader);
	void attachShader(GLuint shader);
	void attachShaders(GLuint* shader = nullptr, S32 count = 0);
	BOOL mapAttributes(const std::vector<LLStaticHashedString> * attributes);
	BOOL mapUniforms(const std::vector<LLStaticHashedString> *);
	void mapUniform(const gl_uniform_data_t& gl_uniform, const std::vector<LLStaticHashedString> *);

	GLint getUniformLocation(const LLStaticHashedString& uniform)
	{
		GLint ret = -1;
		if (mProgramObject > 0)
		{
			auto iter = mUniformMap.find(uniform);
			if (iter != mUniformMap.cend())
			{
				if (gDebugGL)
				{
					stop_glerror();
					if (iter->second != glGetUniformLocation(mProgramObject, uniform.String().c_str()))
					{
						LL_ERRS() << "Uniform does not match." << LL_ENDL;
					}
					stop_glerror();
				}
				ret = iter->second;
			}
		}

		return ret;
	}

	GLint getUniformLocation(U32 index)
	{
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return -1;
		}
		return mUniform[index];
	}

	template <typename T, int N>
	bool updateUniform(std::vector<std::pair<GLint, T> >& cache, S32 uniform, const F32* val)
	{
		if (mProgramObject > 0)
		{
			if (uniform >= 0)
			{
				typename std::vector<std::pair<GLint, T> >::iterator iter = std::find_if(cache.begin(), cache.end(), [&](const auto& pair) { return pair.first == uniform; });
				if (iter == cache.cend())
				{
					T tmp;
					memcpy(&tmp, val, sizeof(F32)*N);
					cache.emplace_back(std::make_pair(uniform, tmp));
					return true;
				}
				else if (memcmp(&iter->second, val, sizeof(F32)*N))
				{
					memcpy(&iter->second, val, sizeof(F32)*N);
					return true;
				}
			}
		}
		return false;
	}

	void uniform1i(U32 index, GLint i)
	{
		F32 val = i;
		if (updateUniform<LLVector4, 1>(mValueVec4, getUniformLocation(index), &val))
		{
			glUniform1i(mUniform[index], i);
		}
	}

	void uniform1f(U32 index, GLfloat v)
	{

		if (updateUniform<LLVector4, 1>(mValueVec4, getUniformLocation(index), &v))
		{
			glUniform1f(mUniform[index], v);
		}
	}

	void uniform2f(U32 index, GLfloat x, GLfloat y)
	{
		F32 val [] = { x, y };
		if (updateUniform<LLVector4, 2>(mValueVec4, getUniformLocation(index), val))
		{
			glUniform2f(mUniform[index], x, y);
		}
	}

	void uniform3f(U32 index, GLfloat x, GLfloat y, GLfloat z)
	{
		F32 val [] = { x, y, z };
		if (updateUniform<LLVector4, 3>(mValueVec4, getUniformLocation(index), val))
		{
			glUniform3f(mUniform[index], x, y, z);
		}
	}

	void uniform4f(U32 index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
	{
		F32 val [] = { x, y, z, w };
		if (updateUniform<LLVector4, 4>(mValueVec4, getUniformLocation(index), val))
		{
			glUniform4f(mUniform[index], x, y, z, w);
		}
	}

	void uniform1iv(U32 index, U32 count, const GLint* i)
	{
		F32 val [] = { static_cast<const F32>(i[0]) };
		if (updateUniform<LLVector4, 1>(mValueVec4, getUniformLocation(index), val) || count > 1)
		{
			glUniform1iv(mUniform[index], count, i);
		}
	}

	void uniform1fv(U32 index, U32 count, const GLfloat* v)
	{

		if (updateUniform<LLVector4, 1>(mValueVec4, getUniformLocation(index), v) || count > 1)
		{
			glUniform1fv(mUniform[index], count, v);
		}
	}

	void uniform2fv(U32 index, U32 count, const GLfloat* v)
	{

		if (updateUniform<LLVector4, 2>(mValueVec4, getUniformLocation(index), v) || count > 1)
		{
			glUniform2fv(mUniform[index], count, v);
		}
	}

	void uniform3fv(U32 index, U32 count, const GLfloat* v)
	{

		if (updateUniform<LLVector4, 3>(mValueVec4, getUniformLocation(index), v) || count > 1)
		{
			glUniform3fv(mUniform[index], count, v);
		}
	}

	void uniform4fv(U32 index, U32 count, const GLfloat* v)
	{

		if (updateUniform<LLVector4, 4>(mValueVec4, getUniformLocation(index), v) || count > 1)
		{
			glUniform4fv(mUniform[index], count, v);
		}
	}

	void uniformMatrix3fv(U32 index, U32 count, GLboolean transpose, const GLfloat *v)
	{
		if (updateUniform<LLMatrix3, 9>(mValueMat3, getUniformLocation(index), v) || count > 1)
		{
			glUniformMatrix3fv(mUniform[index], count, transpose, v);
		}
	}

	void uniformMatrix3x4fv(U32 index, U32 count, GLboolean transpose, const GLfloat *v)
	{
		if (updateUniform<LLMatrix4, 12>(mValueMat4, getUniformLocation(index), v) || count > 1)
		{
			glUniformMatrix3x4fv(mUniform[index], count, transpose, v);
		}
	}

	void uniformMatrix4fv(U32 index, U32 count, GLboolean transpose, const GLfloat *v)
	{
		if (updateUniform<LLMatrix4, 16>(mValueMat4, getUniformLocation(index), v) || count > 1)
		{
			glUniformMatrix4fv(mUniform[index], count, transpose, v);
		}
	}

	void uniform1i(const LLStaticHashedString& uniform, GLint i)
	{
		GLint location = getUniformLocation(uniform);

		if (location < 0)
			return;
		F32 val = i;
		if (updateUniform<LLVector4, 1>(mValueVec4, getUniformLocation(uniform), &val))
		{
			glUniform1i(location, i);
		}
	}

	void uniform1f(const LLStaticHashedString& uniform, GLfloat v)
	{
		GLint location = getUniformLocation(uniform);

		if (location < 0)
			return;
		if (updateUniform<LLVector4, 1>(mValueVec4, location, &v))
		{
			glUniform1f(location, v);
		}
	}

	void uniform2f(const LLStaticHashedString& uniform, GLfloat x, GLfloat y)
	{
		GLint location = getUniformLocation(uniform);

		if (location < 0)
			return;
		F32 val [] = { x, y };
		if (updateUniform<LLVector4, 2>(mValueVec4, location, val))
		{
			glUniform2f(location, x, y);
		}
	}

	void uniform3f(const LLStaticHashedString& uniform, GLfloat x, GLfloat y, GLfloat z)
	{
		GLint location = getUniformLocation(uniform);

		if (location < 0)
			return;
		F32 val [] = { x, y, z };
		if (updateUniform<LLVector4, 3>(mValueVec4, location, val))
		{
			glUniform3f(location, x, y, z);
		}
	}

	void uniform1fv(const LLStaticHashedString& uniform, U32 count, const GLfloat* v)
	{
		GLint location = getUniformLocation(uniform);

		if (location < 0)
			return;
		if (updateUniform<LLVector4, 1>(mValueVec4, location, v))
		{
			glUniform1fv(location, count, v);
		}
	}

	void uniform2fv(const LLStaticHashedString& uniform, U32 count, const GLfloat* v)
	{
		GLint location = getUniformLocation(uniform);

		if (location < 0)
			return;
		if (updateUniform<LLVector4, 2>(mValueVec4, location, v))
		{
			glUniform2fv(location, count, v);
		}
	}

	void uniform3fv(const LLStaticHashedString& uniform, U32 count, const GLfloat* v)
	{
		GLint location = getUniformLocation(uniform);

		if (location < 0)
			return;
		if (updateUniform<LLVector4, 3>(mValueVec4, location, v))
		{
			glUniform3fv(location, count, v);
		}
	}

	void uniform4fv(const LLStaticHashedString& uniform, U32 count, const GLfloat* v)
	{
		GLint location = getUniformLocation(uniform);

		if (location < 0)
			return;
		if (updateUniform<LLVector4, 4>(mValueVec4, location, v))
		{
			glUniform4fv(location, count, v);
		}
	}

	void uniformMatrix4fv(const LLStaticHashedString& uniform, U32 count, GLboolean transpose, const GLfloat *v)
	{
		GLint location = getUniformLocation(uniform);

		if (location < 0)
			return;
		if (updateUniform<LLMatrix4, 16>(mValueMat4, location, v))
		{
			glUniformMatrix4fv(location, count, transpose, v);
		}
	}

	void setMinimumAlpha(F32 minimum);

	void vertexAttrib4f(U32 index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
	
	GLint getAttribLocation(U32 attrib);
	GLint mapUniformTextureChannel(GLint location, GLenum type);
	
	void addPremutations(std::map<std::string, std::string>& map)
	{
		mDefines.insert(map.begin(), map.end());
	}
	void addPermutation(const std::string& name, const std::string& value);
	void removePermutations(std::map<std::string, std::string>& map)
	{
		for (auto entry : map)
		{
			mDefines.erase(entry.first);
		}
	}
	void removePermutation(const std::string& name);
	
	//enable/disable texture channel for specified uniform
	//if given texture uniform is active in the shader, 
	//the corresponding channel will be active upon return
	//returns channel texture is enabled in from [0-MAX)
	S32 enableTexture(S32 uniform, LLTexUnit::eTextureType mode = LLTexUnit::TT_TEXTURE);
	S32 disableTexture(S32 uniform, LLTexUnit::eTextureType mode = LLTexUnit::TT_TEXTURE);
	
	// bindTexture returns the texture unit we've bound the texture to.
	// You can reuse the return value to unbind a texture when required.
	S32 bindTexture(const std::string& uniform, LLTexture *texture, LLTexUnit::eTextureType mode = LLTexUnit::TT_TEXTURE);
	S32 bindTexture(S32 uniform, LLTexture *texture, LLTexUnit::eTextureType mode = LLTexUnit::TT_TEXTURE);
	S32 unbindTexture(const std::string& uniform, LLTexUnit::eTextureType mode = LLTexUnit::TT_TEXTURE);
	S32 unbindTexture(S32 uniform, LLTexUnit::eTextureType mode = LLTexUnit::TT_TEXTURE);
	
    BOOL link(BOOL suppress_errors = FALSE);
	void bind();
	void unbind();

	// Unbinds any previously bound shader by explicitly binding no shader.
	static void bindNoShader(void);

	U32 mMatHash[LLRender::NUM_MATRIX_MODES];
	U32 mLightHash;

	GLuint mProgramObject;
#if LL_RELEASE_WITH_DEBUG_INFO
	struct attr_name
	{
		GLint loc;
		const char *name;
		void operator = (GLint _loc) { loc = _loc; }
		operator GLint () { return loc; }
	};
	std::vector<attr_name> mAttribute; //lookup table of attribute enum to attribute channel
#else
	std::vector<GLint> mAttribute; //lookup table of attribute enum to attribute channel
#endif
	U32 mAttributeMask;  //mask of which reserved attributes are set (lines up with LLVertexBuffer::getTypeMask())
	std::vector<GLint> mUniform;   //lookup table of uniform enum to uniform location
	LLStaticStringTable<GLint> mUniformMap; //lookup map of uniform name to uniform location
	std::map<GLint, std::string> mUniformNameMap; //lookup map of uniform location to uniform name

	//There are less naive ways to do this than just having several vectors for the differing types, but this method is of least complexity and has some inherent type-safety.
	std::vector<std::pair<GLint, LLVector4> > mValueVec4; //lookup map of uniform location to last known value
	std::vector<std::pair<GLint, LLMatrix3> > mValueMat3; //lookup map of uniform location to last known value
	std::vector<std::pair<GLint, LLMatrix4> > mValueMat4; //lookup map of uniform location to last known value
	
	std::vector<GLint> mTexture;   //lookup table of texture uniform enum to texture channel
	S32 mTotalUniformSize;
	S32 mActiveTextureChannels;
	S32 mShaderLevel;
	S32 mShaderGroup;
	BOOL mUniformsDirty;
	LLShaderFeatures mFeatures;
	std::vector< std::pair< std::string, GLenum > > mShaderFiles;
	std::string mName;
	boost::unordered_map<std::string, std::string> mDefines;

	//statistcis for profiling shader performance
	U32 mTimerQuery;
	U32 mSamplesQuery;
	U64 mTimeElapsed;
	static U64 sTotalTimeElapsed;
	U32 mTrianglesDrawn;
	static U32 sTotalTrianglesDrawn;
	U64 mSamplesDrawn;
	static U64 sTotalSamplesDrawn;
	U32 mDrawCalls;
	static U32 sTotalDrawCalls;

	bool mTextureStateFetched;
	std::vector<U32> mTextureMagFilter;
	std::vector<U32> mTextureMinFilter;

private:
	void unloadInternal();
};

//UI shader (declared here so llui_libtest will link properly)
extern LLGLSLShader			gUIProgram;
//output vec4(color.rgb,color.a*tex0[tc0].a)
extern LLGLSLShader			gSolidColorProgram;
//Alpha mask shader (declared here so llappearance can access properly)
extern LLGLSLShader			gAlphaMaskProgram;


#endif
