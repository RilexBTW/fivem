/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "Pool.h"
#include <Hooking.h>

static const char* poolEntriesTable[] = {
	"CAtdNodeFrameAddress",
	"PtrNode Double",
	"PtrNode Single",
	"Building",
	"Vehicles",
	"VehicleStruct",
	"Cam",
	"CQueriableTaskInfo",
	"InteriorInst",
	"QuadTreeNodes",
	"Object",
	"Event",
	"CAnimBlender",
	"CAtdNodeAnimChangePooledObject",
	"CAtdNodeAnimPlayer",
	"crExpressionProcessor",
	"crFrameFilterBoneAnalogue",
	"crFrameFilterBoneMask",
	"crmtObserver",
	"Dummy Object",
	"Task",
	"PortalInst",
	"Anim Manager",
	"BoundsStore",
	"StaticBounds",
	"PhysicsStore",
	"AnimatedBuilding",
	"Blendshape Store",
	"fragInstGta",
	"fragInstNMGta",
	"phInstGta",
	"CStuntJump",
	"Targetting",
	"CDummyTask",
	"ExplosionType",
	"Particle System",
	"EntryInfoNodes",
	"NavMeshRoute",
	"PatrolRoute",
	"PointRoute",
	"PedAttractor",
	"RenderIds"
};

CPool** CPools::ms_pBuildingPool;
CPool** CPools::ms_pQuadTreePool;
CPool** CPools::ms_pObjectsPool;
CPool** CPools::ms_pPedPool;
CPool** CPools::ms_pVehiclePool;

static hook::thiscall_stub<void*(CPool*)> _allocate([]()
{
	return hook::get_call(hook::get_pattern("E8 ? ? ? ? 85 C0 74 ? FF B4 24 ? ? ? ? 8B C8"));
});

void* CPool::Allocate() 
{ 
	return _allocate(this);
}

static HookFunction hookFunc([]()
{
	CPools::ms_pBuildingPool = *hook::get_pattern<CPool**>("33 FF C6 44 24 17 01", -24);
	CPools::ms_pQuadTreePool = *hook::get_pattern<CPool**>("68 20 03 00 00 8B C8", 13);
	CPools::ms_pVehiclePool = *hook::get_pattern<CPool**>("A3 ? ? ? ? C3 C7 05 ? ? ? ? ? ? ? ? C3 CC CC CC CC CC CC CC CC CC CC CC E8", 1);
	CPools::ms_pObjectsPool = *hook::get_pattern<CPool**>("A3 ? ? ? ? C3 C7 05 ? ? ? ? ? ? ? ? C3 CC CC CC CC CC CC CC CC CC CC CC 56 6A", 1);
	CPools::ms_pPedPool = *hook::get_pattern<CPool**>("A1 ? ? ? ? F3 0F 11 7C 24 ? F3 0F 11 74 24", 1);
});
