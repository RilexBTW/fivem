// patches for launcher compatibility

#include "StdInc.h"

static bool Return1()
{
	return 1;
}

static HookFunction hookFunction([] ()
{
	// /GS check
	hook::return_function(hook::get_pattern("55 8B EC 81 EC ? ? ? ? 6A ? E8 ? ? ? ? 85 C0 74 ? 6A"));
	// is non writable ret
	hook::jump(hook::get_pattern("55 8B EC 6A ? 68 ? ? ? ? 68 ? ? ? ? 64 A1 ? ? ? ? 50 83 EC ? 53 56 57 A1 ? ? ? ? 31 45 ? 33 C5 50 8D 45 ? 64 A3 ? ? ? ? 89 65 ? C7 45"), Return1);
	//hook::jump(hook::get_pattern("C7 45 FC 00 00 00 00 68 00 00 40 00", -0x33), ret1);
});
