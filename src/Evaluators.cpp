// https://github.com/CedricGuillemet/Imogen
//
// The MIT License(MIT)
// 
// Copyright(c) 2018 Cedric Guillemet
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include <GL/gl3w.h>    // Initialize with gl3wInit()
#include "Evaluators.h"
#include "Evaluation.h"

Evaluators gEvaluators;

struct EValuationFunction
{
	const char *szFunctionName;
	void *function;
};

static const EValuationFunction evaluationFunctions[] = {
	{ "Log", (void*)Log },
	{ "ReadImage", (void*)Evaluation::ReadImage },
	{ "WriteImage", (void*)Evaluation::WriteImage },
	{ "GetEvaluationImage", (void*)Evaluation::GetEvaluationImage },
	{ "SetEvaluationImage", (void*)Evaluation::SetEvaluationImage },
	{ "SetEvaluationImageCube", (void*)Evaluation::SetEvaluationImageCube },
	{ "AllocateImage", (void*)Evaluation::AllocateImage },
	{ "FreeImage", (void*)Evaluation::FreeImage },
	{ "SetThumbnailImage", (void*)Evaluation::SetThumbnailImage },
	{ "Evaluate", (void*)Evaluation::Evaluate},
	{ "SetBlendingMode", (void*)Evaluation::SetBlendingMode},
	{ "GetEvaluationSize", (void*)Evaluation::GetEvaluationSize},
	{ "SetEvaluationSize", (void*)Evaluation::SetEvaluationSize },
	{ "SetEvaluationCubeSize", (void*)Evaluation::SetEvaluationCubeSize },
	{ "CubemapFilter", (void*)Evaluation::CubemapFilter},
	{ "SetProcessing", (void*)Evaluation::SetProcessing},
	{ "Job", (void*)Evaluation::Job },
	{ "JobMain", (void*)Evaluation::JobMain },
	{ "memmove", memmove },
	{ "strcpy", strcpy },
	{ "strlen", strlen },
	{ "fabsf", fabsf },
};

static void libtccErrorFunc(void *opaque, const char *msg)
{
	Log(msg);
	Log("\n");
}

void LogPython(const std::string &str)
{
	Log(str.c_str());
}

PYBIND11_MAKE_OPAQUE(Image);
PYBIND11_EMBEDDED_MODULE(Imogen, m) 
{
	pybind11::class_<Image>(m, "Image");
	m.def("Log", LogPython );
	m.def("ReadImage", Evaluation::ReadImage );
	m.def("WriteImage", Evaluation::WriteImage );
	m.def("GetEvaluationImage", Evaluation::GetEvaluationImage );
	m.def("SetEvaluationImage", Evaluation::SetEvaluationImage );
	m.def("SetEvaluationImageCube", Evaluation::SetEvaluationImageCube );
	m.def("AllocateImage", Evaluation::AllocateImage );
	m.def("FreeImage", Evaluation::FreeImage );
	m.def("SetThumbnailImage", Evaluation::SetThumbnailImage );
	m.def("Evaluate", Evaluation::Evaluate );
	m.def("SetBlendingMode", Evaluation::SetBlendingMode );
	m.def("GetEvaluationSize", Evaluation::GetEvaluationSize );
	m.def("SetEvaluationSize", Evaluation::SetEvaluationSize );
	m.def("SetEvaluationCubeSize", Evaluation::SetEvaluationCubeSize );
	m.def("CubemapFilter", Evaluation::CubemapFilter );
	m.def("SetProcessing", Evaluation::SetProcessing );
	/*
	m.def("Job", Evaluation::Job );
	m.def("JobMain", Evaluation::JobMain );
	*/
	m.def("accessor_api", []() {
		auto d = pybind11::dict();

		d["target"] = 10;

		auto l = pybind11::list();
		l.append(5);
		l.append(-1);
		l.append(-1);
		d["inputs"] = l;

		return d;
	});

	
	/*
	m.def("GetImage", []() {
		auto i = new Image;
		//pImage i;
		//i.a = 14;
		//printf("new img %p \n", &i);
		return i;
	});

	m.def("SaveImage", [](Image image) {
		//printf("Saving image %d\n", image.a);
		//printf("save img %p \n", image);
	});
	*/
}
std::string Evaluators::GetEvaluator(const std::string& filename)
{
	return mEvaluatorScripts[filename].mText;
}

void Evaluators::SetEvaluators(const std::vector<EvaluatorFile>& evaluatorfilenames)
{
	ClearEvaluators();

	mEvaluatorPerNodeType.clear();
	mEvaluatorPerNodeType.resize(evaluatorfilenames.size(), Evaluator());

	// GLSL
	for (auto& file : evaluatorfilenames)
	{
		if (file.mEvaluatorType != EVALUATOR_GLSL)
			continue;
		const std::string filename = file.mFilename;

		std::ifstream t(file.mDirectory + filename);
		if (t.good())
		{
			std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
			if (mEvaluatorScripts.find(filename) == mEvaluatorScripts.end())
				mEvaluatorScripts[filename] = EvaluatorScript(str);
			else
				mEvaluatorScripts[filename].mText = str;
		}
	}

	std::string baseShader = mEvaluatorScripts["Shader.glsl"].mText;
	for (auto& file : evaluatorfilenames)
	{
		if (file.mEvaluatorType != EVALUATOR_GLSL)
			continue;
		const std::string filename = file.mFilename;

		if (filename == "Shader.glsl")
			continue;

		EvaluatorScript& shader = mEvaluatorScripts[filename];
		std::string shaderText = ReplaceAll(baseShader, "__NODE__", shader.mText);
		std::string nodeName = ReplaceAll(filename, ".glsl", "");
		shaderText = ReplaceAll(shaderText, "__FUNCTION__", nodeName + "()");

		unsigned int program = LoadShader(shaderText, filename.c_str());

		int parameterBlockIndex = glGetUniformBlockIndex(program, (nodeName + "Block").c_str());
		if (parameterBlockIndex != -1)
			glUniformBlockBinding(program, parameterBlockIndex, 1);

		parameterBlockIndex = glGetUniformBlockIndex(program, "EvaluationBlock");
		if (parameterBlockIndex != -1)
			glUniformBlockBinding(program, parameterBlockIndex, 2);
		shader.mProgram = program;
		if (shader.mNodeType != -1)
			mEvaluatorPerNodeType[shader.mNodeType].mGLSLProgram = program;
	}

	if (!gEvaluationStateGLSLBuffer)
	{
		glGenBuffers(1, &gEvaluationStateGLSLBuffer);
		glBindBuffer(GL_UNIFORM_BUFFER, gEvaluationStateGLSLBuffer);

		glBufferData(GL_UNIFORM_BUFFER, sizeof(EvaluationInfo), NULL, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 2, gEvaluationStateGLSLBuffer);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	// C
	for (auto& file : evaluatorfilenames)
	{
		if (file.mEvaluatorType != EVALUATOR_C)
			continue;
		const std::string filename = file.mFilename;
		try
		{
			std::ifstream t(file.mDirectory + filename);
			if (!t.good())
			{
				Log("%s - Unable to load file.\n", filename.c_str());
				continue;
			}
			std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
			if (mEvaluatorScripts.find(filename) == mEvaluatorScripts.end())
				mEvaluatorScripts[filename] = EvaluatorScript(str);
			else
				mEvaluatorScripts[filename].mText = str;

			EvaluatorScript& program = mEvaluatorScripts[filename];
			TCCState *s = tcc_new();

			int *noLib = (int*)s;
			noLib[2] = 1; // no stdlib

			tcc_set_error_func(s, 0, libtccErrorFunc);
			tcc_add_include_path(s, "Nodes/C/");
			tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

			if (tcc_compile_string(s, program.mText.c_str()) != 0)
			{
				Log("%s - Compilation error!\n", filename.c_str());
				continue;
			}

			for (auto& evaluationFunction : evaluationFunctions)
				tcc_add_symbol(s, evaluationFunction.szFunctionName, evaluationFunction.function);

			int size = tcc_relocate(s, NULL);
			if (size == -1)
			{
				Log("%s - Libtcc unable to relocate program!\n", filename.c_str());
				continue;
			}
			program.mMem = malloc(size);
			tcc_relocate(s, program.mMem);

			*(void**)(&program.mCFunction) = tcc_get_symbol(s, "main");
			if (!program.mCFunction)
			{
				Log("%s - No main function!\n", filename.c_str());
			}
			tcc_delete(s);

			if (program.mNodeType != -1)
			{
				mEvaluatorPerNodeType[program.mNodeType].mCFunction = program.mCFunction;
				mEvaluatorPerNodeType[program.mNodeType].mMem = program.mMem;
			}
		}
		catch (...)
		{
			Log("Error at compiling %s", filename.c_str());
		}
	}
	for (auto& file : evaluatorfilenames)
	{
		if (file.mEvaluatorType != EVALUATOR_PYTHON)
			continue;
		const std::string filename = file.mFilename;
		std::string nodeName = ReplaceAll(filename, ".py", "");
		EvaluatorScript& shader = mEvaluatorScripts[filename];
		try
		{
			shader.mPyModule = pybind11::module::import("Nodes.Python.testnode");;
			if (shader.mNodeType != -1)
				mEvaluatorPerNodeType[shader.mNodeType].mPyModule = shader.mPyModule;
		}
		catch (...)
		{
			
		}
	}
	mImogenModule = pybind11::module::import("Imogen");
}

void Evaluators::ClearEvaluators()
{
	// clear
	for (auto& program : mEvaluatorPerNodeType)
	{
		if (program.mGLSLProgram)
			glDeleteProgram(program.mGLSLProgram);
		if (program.mMem)
			free(program.mMem);
	}
}

int Evaluators::GetMask(size_t nodeType)
{
	const std::string& nodeName = gMetaNodes[nodeType].mName;
	int mask = 0;
	auto iter = mEvaluatorScripts.find(nodeName + ".glsl");
	if (iter != mEvaluatorScripts.end())
	{
		mask |= EvaluationGLSL;
		iter->second.mNodeType = int(nodeType);
		//evaluation.mTarget = new RenderTarget;
		//mAllocatedRenderTargets.push_back(evaluation.mTarget);
		mEvaluatorPerNodeType[nodeType].mGLSLProgram = iter->second.mProgram;
	}
	iter = mEvaluatorScripts.find(nodeName + ".c");
	if (iter != mEvaluatorScripts.end())
	{
		mask |= EvaluationC;
		iter->second.mNodeType = int(nodeType);
		mEvaluatorPerNodeType[nodeType].mCFunction = iter->second.mCFunction;
		mEvaluatorPerNodeType[nodeType].mMem = iter->second.mMem;
	}
	iter = mEvaluatorScripts.find(nodeName + ".py");
	if (iter != mEvaluatorScripts.end())
	{
		mask |= EvaluationPython;
		iter->second.mNodeType = int(nodeType);
		mEvaluatorPerNodeType[nodeType].mPyModule = iter->second.mPyModule;
	}
	return mask;
}

void Evaluator::RunPython() const
{
	mPyModule.attr("main")(gEvaluators.mImogenModule.attr("accessor_api")());
}