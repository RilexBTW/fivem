// miscellaneous online support patches

#include "StdInc.h"

static HookFunction hookFunction([] ()
{
	// don't unload streamed fonts - this crashes the pause menu
	hook::return_function(hook::get_pattern("83 3D ? ? ? ? FF 74 ? 6A FF"));

	// function which maintains counters related to MP_WARNING_5/MP_WARNING_6 messages; which are 'too slowly' and 'run out of streaming memory' disconnects
	//hook::return_function(hook::get_pattern("8B EA 56 8A F1 57", -0x08));

	// ignore initial loading screens
	//hook::put<uint8_t>(hook::get_pattern("74 ? 80 3D ? ? ? ? ? 74 ? E8 ? ? ? ? 0F B6 05"), 0XEB);
	//hook::put<uint8_t>(0x59D5A7, 0xEB);
	//hook::put<uint8_t>(0x402B49, 0xEB);

	// emergency streaming safeguard related to process address space or something; this is
	// typically a sign of something else being wrong but I can't be bothered to fix it
	//hook::put<uint8_t>(0xAC1F93, 0xEB);

	// try getting rid of CStreaming::IsEnoughMemoryFor recalibrating VRAM specification in streaming subsystem
	//hook::nop(0xAC1F5C, 5); // call with 'retn 4' in it
	//hook::nop(0xAC1F57, 1); // push
});
