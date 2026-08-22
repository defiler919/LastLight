// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gameplay/DarkwellGameplayTags.h"

namespace DarkwellGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Alive, "State.Player.Alive");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Dead, "State.Player.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Escaped, "State.Player.Escaped");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Torch_On, "State.Player.Torch.On");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Torch_Lowered, "State.Player.Torch.Lowered");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Torch_Swinging, "State.Player.Torch.Swinging");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Torch_Deterrent, "State.Player.Torch.Deterrent");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Torch_Overheated, "State.Player.Torch.Overheated");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Lantern_On, "State.Player.Lantern.On");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Lantern_Focused, "State.Player.Lantern.Focused");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Lantern_Flashing, "State.Player.Lantern.Flashing");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Weapon_Ready, "State.Player.Weapon.Ready");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Weapon_Empty, "State.Player.Weapon.Empty");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Weapon_Reloading, "State.Player.Weapon.Reloading");
	UE_DEFINE_GAMEPLAY_TAG(State_World_Door_Closed, "State.World.Door.Closed");
	UE_DEFINE_GAMEPLAY_TAG(State_World_Door_Open, "State.World.Door.Open");
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_Idle, "State.Enemy.Idle");
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_Investigating, "State.Enemy.Investigating");
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_Hunting, "State.Enemy.Hunting");
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_Repelled, "State.Enemy.Repelled");
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_LightStunned, "State.Enemy.LightStunned");
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_Dead, "State.Enemy.Dead");
	UE_DEFINE_GAMEPLAY_TAG(Enemy_Archetype_Stalker, "Enemy.Archetype.Stalker");
	UE_DEFINE_GAMEPLAY_TAG(Enemy_Archetype_Warden, "Enemy.Archetype.Warden");
	UE_DEFINE_GAMEPLAY_TAG(State_Mission_FindFuse, "State.Mission.FindFuse");
	UE_DEFINE_GAMEPLAY_TAG(State_Mission_ReachExit, "State.Mission.ReachExit");
	UE_DEFINE_GAMEPLAY_TAG(State_Mission_Escaped, "State.Mission.Escaped");
	UE_DEFINE_GAMEPLAY_TAG(Equipment_Left_Shotgun, "Equipment.Left.Shotgun");
	UE_DEFINE_GAMEPLAY_TAG(Equipment_Right_Torch, "Equipment.Right.Torch");
	UE_DEFINE_GAMEPLAY_TAG(Equipment_Right_Lantern, "Equipment.Right.Lantern");
	UE_DEFINE_GAMEPLAY_TAG(Item_Ammo_ShotgunShell, "Item.Ammo.ShotgunShell");
	UE_DEFINE_GAMEPLAY_TAG(Item_Material_Scrap, "Item.Material.Scrap");
}
