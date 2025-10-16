#pragma once

#define NOMINMAX

// Window Library
#include <windows.h>
#include <wrl.h>
#include <wrl/client.h>

// D3D Library
#include <d3d11.h>
#include <d3dcompiler.h>

// D2D Library
#include <d2d1.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <dxgi1_3.h>

// Standard Library
#include <cmath>
#include <cassert>
#include <string>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <functional>
#include <filesystem>
#include <iterator>
#include <sstream>
#include <xmmintrin.h>

// Global Included
#include "Core/Public/AppWindow.h"
#include "Core/Public/Types.h"
#include "Core/Public/Memory.h"
#include "Core/Public/Misc/Constant.h"
#include "Core/Public/Misc/Enum.h"
#include "Core/Public/Math/Matrix.h"
#include "Core/Public/Math/Vector.h"
#include "Core/Public/Math/Quaternion.h"
#include "Core/Public/CoreTypes.h"
#include "Core/Public/Misc/Macro.h"
#include "Core/Public/Misc/Function.h"
#include "Core/Public/Misc/ScopeCycleCounter.h"
#include "Editor/Public/EditorEngine.h"

using std::clamp;
using std::unordered_map;
using std::to_string;
using std::function;
using std::wstring;
using std::cout;
using std::cerr;
using std::min;
using std::max;
using std::exception;
using std::stoul;
using std::ofstream;
using std::ifstream;
using std::setw;
using std::sort;
using std::shared_ptr;
using std::unique_ptr;
using std::streamsize;
using Microsoft::WRL::ComPtr;

// File System
namespace filesystem = std::filesystem;
using filesystem::path;
using filesystem::exists;
using filesystem::create_directories;

#define IMGUI_DEFINE_MATH_OPERATORS
#include "Editor/Public/UI/Window/ConsoleWindow.h"

// DT Include
#include "Manager/Public/TimeManager.h"

// 빌드 조건에 따른 Library 분류
#ifdef _DEBUG
#define DIRECTX_TOOL_KIT R"(DirectXTK\DirectXTK_debug)"
#else
#define DIRECTX_TOOL_KIT R"(DirectXTK\DirectXTK)"
#endif

// Library Linking
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dwrite")
#pragma comment(lib, DIRECTX_TOOL_KIT)
