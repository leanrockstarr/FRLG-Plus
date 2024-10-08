.set LOCALID_CLEFABLE_MASTER, 1
.set TRAINER, VAR_0x8009

MtMoon_B1F_MapScripts::
	map_script MAP_SCRIPT_ON_TRANSITION, MtMoon_B1F_MapScript_SetMasterTrainerMovementTypes
	.byte 0

MtMoon_B1F_MapScript_SetMasterTrainerMovementTypes::
	setvar TRAINER, MASTER_CLEFABLE
	callnative ShouldChangeMasterTrainerMovementType
	call_if_eq VAR_RESULT, FALSE, MtMoon_B1F_MapScript_ClefableMasterMovementType
	end

MtMoon_B1F_MapScript_ClefableMasterMovementType::
	setobjectmovementtype LOCALID_CLEFABLE_MASTER, MOVEMENT_TYPE_RAISE_HAND_AND_STOP
	return
