#include <StdInc.h>
#include <Hooking.h>
#include <type_traits>
#include <Streaming.h>
#include <Error.h>
#include <ICoreGameInit.h>
#include <nutsnbolts.h>
#include <MinHook.h>

static std::vector<std::tuple<std::string, std::string, rage::ResourceFlags>> g_customStreamingFiles;
std::set<std::string> g_customStreamingFileRefs;
static std::map<std::string, std::vector<std::string>, std::less<>> g_customStreamingFilesByTag;
static bool g_reloadStreamingFiles;



static int SehRoutine(const char* whatPtr, PEXCEPTION_POINTERS exception)
{
	if (IsErrorException(exception))
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if (exception->ExceptionRecord->ExceptionCode & 0x80000000)
	{
		if (!whatPtr)
		{
			whatPtr = "a safe-call operation";
		}

		FatalErrorNoExcept("An exception occurred (%08x at %p) during %s. The game will be terminated.",
			exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress,
			whatPtr);
	}

	return EXCEPTION_CONTINUE_SEARCH;
}


void DLL_EXPORT CfxCollection_AddStreamingFileByTag(const std::string& tag, const std::string& fileName, rage::ResourceFlags flags)
{
	auto baseName = std::string(strrchr(fileName.c_str(), '/') + 1);

	g_customStreamingFilesByTag[tag].push_back(fileName);
	g_customStreamingFiles.push_back({ fileName, tag, flags });
	g_customStreamingFileRefs.insert(baseName);

	g_reloadStreamingFiles = true;
}

void DLL_EXPORT CfxCollection_RemoveStreamingTag(const std::string& tag)
{
}

void DLL_EXPORT CfxCollection_BackoutStreamingTag(const std::string& tag)
{
}

void DLL_EXPORT CfxCollection_SetStreamingLoadLocked(bool locked)
{
}

static std::vector<std::pair<std::string, std::string>> g_dataFiles;

static void LoadDataFiles();

namespace streaming
{
	void DLL_EXPORT AddMetaToLoadList(bool before, const std::string& meta)
	{

	}

	void DLL_EXPORT AddDefMetaToLoadList(const std::string& meta)
	{

	}

	void DLL_EXPORT AddDataFileToLoadList(const std::string& type, const std::string& path)
	{
		g_dataFiles.push_back({ type, path });
		if (Instance<ICoreGameInit>::Get()->GetGameLoaded() && !Instance<ICoreGameInit>::Get()->HasVariable("gameKilled"))
		{
			LoadDataFiles();
		}
	}

	void DLL_EXPORT RemoveDataFileFromLoadList(const std::string& type, const std::string& path)
	{
		auto dataFilePair = std::make_pair(type, path);
		g_dataFiles.erase(std::remove(g_dataFiles.begin(), g_dataFiles.end(), dataFilePair), g_dataFiles.end());
	}

	void DLL_EXPORT SetNextLevelPath(const std::string& path)
	{

	}
}

struct FakeRawStreamer
{
	// if this is big, do proper splitting
	std::vector<std::string> fileNames;

	FakeRawStreamer()
	{
		fileNames.reserve(4096);
	}

	size_t RegisterFile(const std::string& fileName)
	{
		// try finding one
		auto it = std::find(fileNames.begin(), fileNames.end(), fileName);

		if (it != fileNames.end())
		{
			return it - fileNames.begin();
		}

		fileNames.push_back(fileName);
		return fileNames.size() - 1;
	}

	const std::string& GetFile(size_t index)
	{
		return fileNames[index];
	}
};

static FakeRawStreamer rawStreamer;

static std::unordered_map<int, std::list<std::tuple<uint32_t, StreamHandle>>> g_handleStack;

static StreamHandle GetRawHandle(size_t index)
{
	StreamHandle h;
	h.imgIndex = 0xFE;
	h.offset = 0xDEADED;
	h.size = index + 1;
	h.unk = 0;
	return h;
}

static std::set<int> myidx;

enum class LoadPhase
{
	Main,
	Early
};

void LoadStreamingFiles(LoadPhase phase = LoadPhase::Main)
{
	auto infoMgr = CStreamingInfoManager::GetInstance();

	for (auto it = g_customStreamingFiles.begin(); it != g_customStreamingFiles.end();)
	{
		auto [file, tag, flags] = *it;

		// get basename ('thing.#td') and asset name ('thing')
		const char* slashPos = strrchr(file.c_str(), '/');

		if (slashPos == nullptr)
		{
			it = g_customStreamingFiles.erase(it);
			continue;
		}

		auto baseName = std::string{ slashPos + 1 };
		auto nameWithoutExt = baseName.substr(0, baseName.find_last_of('.'));

		const char* extPos = strrchr(baseName.c_str(), '.');

		if (extPos == nullptr)
		{
			it = g_customStreamingFiles.erase(it);
			trace("can't register %s: it doesn't have an extension, why is this in stream/?\n", file);
			continue;
		}

		std::string ext = extPos + 1;

		if (phase == LoadPhase::Early)
		{
			if (ext != "wbn")
			{
				++it;
				continue;
			}
		}

		it = g_customStreamingFiles.erase(it);

		auto moduleIdx = streamingModuleMgr->GetModuleFromExt(ext.c_str());
		if (moduleIdx == 0xFF)
		{
			trace("can't register %s: no streaming module (does this file even belong in stream?)\n", file);
			continue;
		}

		auto module = &streamingModuleMgr->modules[moduleIdx];
		auto entryIdx = module->getIndexByName(nameWithoutExt.c_str());
		
		// if the asset exists
		auto globalIdx = entryIdx + module->startIndex;
		auto& entry = infoMgr->Entries[globalIdx];
		auto fileIndex = rawStreamer.RegisterFile(file);

		if (entry.handle.offset != 0)
		{
			auto& hs = g_handleStack[globalIdx];

			if (hs.empty())
			{
				hs.push_front({ entry.realSize, entry.handle });
			}

			entry.handle = GetRawHandle(fileIndex);
			entry.realSize = flags;
			hs.push_front({ entry.realSize, entry.handle });
		}
		else
		{
			auto& hs = g_handleStack[globalIdx];
			entry.nextOnCd = -1;
			entry.module = moduleIdx;
			entry.flags = 0;

			entry.moduleByIdx = moduleIdx;
			entry.handle = GetRawHandle(fileIndex);
			entry.realSize = flags;
			hs.push_front({ entry.realSize, entry.handle });
		}

		myidx.insert(globalIdx);

		if (flags & 0x40000000)
		{
			entry.flags |= 0x2000;
		}
		else
		{
			entry.flags &= ~0x2000;
		}
	}
}

struct Req
{
	uint32_t a;
	uint32_t b;
	uint16_t idx;
	uint8_t pad[6];
	uint32_t streamHandle;
};

static hook::cdecl_stub<uint32_t(const char* fn, int flag, bool flag2)> rage__pgStreamer__Open([]()
{
	return hook::get_call(hook::get_pattern("89 47 10 83 F8 FF", -10));
});

static bool(__stdcall* g_origOpenStreamHandle)(Req* req);

static bool __stdcall _openStreamHandleHook(Req* req)
{
	if (myidx.find(req->idx) != myidx.end())
	{
		trace("");
	}

	const auto& entry = CStreamingInfoManager::GetInstance()->Entries[req->idx];
	std::string fn;
	if (entry.handle.offset == 0xDEADED)
	{
		fn = rawStreamer.GetFile(entry.handle.size - 1);
	}
	else
	{
		fn = fmt::sprintf("stream:/%u", req->idx);
	}

	auto handle = rage__pgStreamer__Open(fn.c_str(), 0, (entry.flags & 0x2000) != 0);
	req->streamHandle = handle;

	return (handle != -1);
}

static hook::cdecl_stub<void* (const char*, bool)> _loadAudioMetadata([]()
{
	return hook::get_pattern("51 A1 ? ? ? ? 85 C0 74");
});

static hook::cdecl_stub<void* (const char*, char, int)> _addImgFile([]()
{
	return hook::get_pattern("83 EC ? A1 ? ? ? ? 53 56 C6 44 24");
});

static void(__thiscall* dataFileMgr_addDlcLine)(void*, const char* line);
static void** g_dataFileMgr;

template<typename T>
static auto SafeCall(const T& fn, const char* whatPtr = nullptr)
{
#ifndef _DEBUG
	__try
	{
#endif
		return fn();
#ifndef _DEBUG
	}
	__except (SehRoutine(whatPtr, GetExceptionInformation()))
	{
		return std::result_of_t<T()>();
	}
#endif
}

static void HandleDataFile(const std::pair<std::string, std::string>& dataFile, const char* op)
{
	std::string typeName;
	std::string fileName;

	std::tie(typeName, fileName) = dataFile;

	trace("%s %s %s.\n", op, typeName, fileName);

	//TODO: Check against data_type's at 33 C0 C7 01 ? ? ? ? 89 41 ? 56, to make sure type is valid.
	bool result = SafeCall([&]()
	{
		// Used for adding custom .IMG files.
		if (typeName == "IMG")
		{
			_addImgFile(fileName.c_str(), 1, -1);
			return true;
		}
		// audio metadata is usually handled in the episodes setup2.xml file.
		else if (typeName == "AUDIO_METADATA")
		{
			_loadAudioMetadata(fileName.c_str(), false);
		}

		dataFileMgr_addDlcLine(*g_dataFileMgr, va("%s %s", typeName, fileName));
		return true;
	}, va("%s of %s in data file %s", op, fileName, typeName));
	trace("done %s %s in data file %s\n", op, fileName, typeName);
}

static void LoadDataFiles()
{
	trace("Loading mounted data files (total: %d)\n", g_dataFiles.size());

	for (auto it = g_dataFiles.begin(); it != g_dataFiles.end();)
	{
		auto [fileType, fileName] = *it;

		HandleDataFile(*it, "loading");
		it++;
	}

	g_dataFiles.clear();
}

static void(__thiscall* g_origLoadDlcDataFiles)(void*);
void __fastcall LoadDataFilesOnDlc(void* self)
{
	g_origLoadDlcDataFiles(self);
	LoadDataFiles();
}

static void(*g_origStreamingInfoInit)();

static void StreamingInfoInit()
{
	g_origStreamingInfoInit();

	auto infoMgr = CStreamingInfoManager::GetInstance();

	if (infoMgr->Entries)
	{
		LoadStreamingFiles(LoadPhase::Early);
	}
}

// 33 C0 C7 01 ? ? ? ? 89 41 ? 56 - function contains all file entries (e.g. IPL, POPCYCLE, etc)
//Attach content.dat file
//81 EC ? ? ? ? A1 ? ? ? ? 33 C4 89 84 24 ? ? ? ? 8B 84 24 ? ? ? ? 53 56 68
//Add IMG File
// 83 EC ? A1 ? ? ? ? 53 56 C6 44 24
//Attach audio file 
// 51 A1 ? ? ? ? 85 C0 74
// Get Free spot in txdStore Pool? used with _loadWTDFile
// 8B 15 ? ? ? ? 53 56 8B 72 ? 33 C0 57 85 F6 7E ? 8B 7A ? 8B 5C 24 ? 8D A4 24 ? ? ? ? F6 04 07 ? 75 ? 8B 4A ? 0F AF C8 03 0A 74 ? 39 59 ? 74 ? 40 3B C6 7C ? 83 C8 ? 5F 5E 5B C3 CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 8B 44 24
//Load #TD file
// 8B 0D ? ? ? ? 56 8B 41 ? 57 8B 7C 24 ? F6 04 07 ? 74 ? 33 F6 EB ? 8B 71 ? 0F AF F7 03 31 E8 
static HookFunction hookFunction([]()
{
	MH_Initialize();
	MH_CreateHook(hook::get_pattern("89 84 24 00 01 00 00 A1 ? ? ? ? 56 57", -13), _openStreamHandleHook, (void**)&g_origOpenStreamHandle);
	MH_EnableHook(MH_ALL_HOOKS);

	// don't do img bd/hdd (or layer) check
	{
		auto location = hook::get_pattern<char>("75 0C 66 8B 43 0E");
		hook::nop(location, 2);
		hook::put<uint8_t>(location + 12, 0xEB);
	}

	{
		auto location = hook::get_pattern("83 C4 08 8B 0D ? ? ? ? E8 ? ? ? ? 6A 00 68", 9);
		hook::set_call(&g_origLoadDlcDataFiles, location);
		hook::call(location, LoadDataFilesOnDlc);
	}

	{
		auto location = hook::get_pattern<char>("83 FD 02 0F 84 ? ? ? ? 8B 0D ? ? ? ? 56");
		g_dataFileMgr = (void**)(*(intptr_t*)(location + 11));
		hook::set_call(&dataFileMgr_addDlcLine, location + 16);
	}

	{
		auto location = hook::get_pattern("75 AC E8 ? ? ? ? E8", 7);
		hook::set_call(&g_origStreamingInfoInit, location);
		hook::call(location, StreamingInfoInit);
	}
});

static InitFunction initFunction([]()
{
	OnMainGameFrame.Connect([]()
	{
		if (g_reloadStreamingFiles)
		{
			auto infoMgr = CStreamingInfoManager::GetInstance();

			if (!infoMgr->Entries)
			{
				return;
			}

			LoadStreamingFiles();

			g_reloadStreamingFiles = false;
		}
	});
});
