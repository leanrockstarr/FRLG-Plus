RS_BattleTower_Elevator_MapScripts::
	map_script MAP_SCRIPT_ON_FRAME_TABLE, BattleTower_Elevator_MapScript2_1607DA
	map_script MAP_SCRIPT_ON_WARP_INTO_MAP_TABLE, BattleTower_Elevator_MapScript2_160821
	.byte 0

BattleTower_Elevator_MapScript2_1607DA::
	map_script_2 VAR_TEMP_0, 0, BattleTower_Elevator_EventScript_1607E4
	.2byte 0

BattleTower_Elevator_EventScript_1607E4::
	setvar VAR_TEMP_0, 1
	applymovement 1, BattleTower_Elevator_Movement_160835
	applymovement OBJ_EVENT_ID_PLAYER, BattleTower_Elevator_Movement_160839
	waitmovement 0
	special AnimateElevator
	waitstate
	delay 64
	applymovement 1, BattleTower_Elevator_Movement_16083D
	applymovement OBJ_EVENT_ID_PLAYER, BattleTower_Elevator_Movement_160841
	waitmovement 0
	setvar VAR_TEMP_0, 0
	warp MAP_RS_BATTLE_TOWER_CORRIDOR, 255, 8, 1
	waitstate
	end

BattleTower_Elevator_MapScript2_160821::
	map_script_2 VAR_TEMP_1, 0, BattleTower_Elevator_EventScript_16082B
	.2byte 0

BattleTower_Elevator_EventScript_16082B::
	setvar VAR_TEMP_1, 1
	turnobject OBJ_EVENT_ID_PLAYER, 2
	end

BattleTower_Elevator_Movement_160835::
	walk_up
	walk_right
	face_down
	step_end

BattleTower_Elevator_Movement_160839::
	walk_up
	walk_up
	face_down
	step_end

BattleTower_Elevator_Movement_16083D::
	walk_down
	walk_left
	walk_down
	set_invisible
	step_end

BattleTower_Elevator_Movement_160841::
	delay_16
	delay_16
	walk_down
	walk_down
	step_end
