    # To be inserted at 8000550c
    .include "../../Globals.s"
    .include "../../../m-ex/Header.s"

    .set playerdata, 31
    .set player, 30

    backup

    # This static function needs to store Interrupt Bool to r3 and store player to r4 before being called
    mr player, r4
    lwz playerdata, 0x2c(player)

    # Check For Interrupt
    cmpwi r3, 0x0
    beq Exit

    # CHECK IF ENABLED
    li r0, OSD.ActOoJump
    lwz r4, -0x77C0(r13)
    lwz r4, 0x1F24(r4)
    li r3, 1
    slw r0, r3, r0
    and. r0, r0, r4
    beq Exit

CheckForFollower:
    mr r3, playerdata
    branchl r12, 0x80005510
    cmpwi r3, 0x1
    beq Exit


CheckForPreviousActionState:
    lhz r3, TM_OneASAgo(playerdata)
    cmpwi r3, ASID_JumpF
    beq PreviousStateIsJump
    cmpwi r3, ASID_JumpB
    beq PreviousStateIsJump
    cmpwi r3, ASID_JumpAerialF
    beq PreviousStateIsDoubleJump
    cmpwi r3, ASID_JumpAerialB
    beq PreviousStateIsDoubleJump
    # Aerial jump state IDs are 0x155, 0x156, 0x157, 0x158, 0x159 for fighters which have multiple jumps (Jigglypuff, Kirby)
    lwz r4, 0x168(playerdata) # load max_jumps
    cmpwi r4, 0x2
    ble Exit
    cmpwi r3, 0x155 # first aerial jump
    blt Exit
    cmpwi r3, 0x159 # last(5th) aerial jump
    bgt Exit
    b PreviousStateIsDoubleJump

PreviousStateIsJump:
    lfs f1, 0x150(playerdata)  # get jump_v_initial_velocity
    b CheckForCutoffFrames

PreviousStateIsDoubleJump:
    lfs f1, 0x150(playerdata)  # get jump_v_initial_velocity
    lfs f2, 0x160(playerdata)  # get air_jump_v_multiplier
    fmuls f1, f1, f2           # calculate double jump vertical initial velocity

CheckForCutoffFrames:
    lhz r7, TM_FramesinOneASAgo(playerdata)
    # Disable OSD if the act is after frames to reach the peak of fighter's jump (+ some additional frames)
    lfs f2, 0x16C(playerdata)  # get gravity
    fdivs f1, f1, f2           # calculate frames to reach the peak of fighter's jump
    fctiwz f1, f1              # round down
    stfd f1, 0x80(sp)
    lwz r4, 0x84(sp)           # load the integer part
    addi r4, r4, 0x5           # add some frames
    cmpw r7, r4
    bgt Exit


CheckForPreviousFrame:
    cmpwi r7, 0x1
    bne RedText

GreenText:
    load r5, MSGCOLOR_GREEN
    b CheckForDoubleJump

RedText:
    load r5, MSGCOLOR_RED


CheckForDoubleJump:
    lwz r3, 0x10(playerdata) # load current action state ID
    cmpwi r3, ASID_JumpAerialF
    beq DoubleJump
    cmpwi r3, ASID_JumpAerialB
    beq DoubleJump
    # First aerial jump state ID is 0x155 for fighters which have multiple jumps (Jigglypuff, Kirby)
    lwz r4, 0x168(playerdata) # load max_jumps
    cmpwi r4, 0x2
    ble CheckForAerial
    cmpwi r3, 0x155 # first aerial jump
    beq DoubleJump

CheckForAerial:
    cmpwi r3, ASID_AttackAirN
    blt CheckForSpecialMove
    cmpwi r3, ASID_AttackAirLw
    bgt CheckForSpecialMove
    b Aerial

CheckForSpecialMove:
    cmpwi r3, ASID_BarrelCannonWait
    ble Exit
    b SpecialMove

DoubleJump:
    li r3, OSD.Miscellaneous    # message kind / Originally OSD.ActOoJump is natural, but use other ID to show at the same time as Aerial OoJump for fighters with DoubleJump cancel techniques
    bl DoubleJumpText
    mflr r6
    b DisplayText

Aerial:
    li r3, OSD.ActOoJump        # message kind
    bl AerialText
    mflr r6
    b DisplayText

SpecialMove:
    li r3, OSD.ActOoJump        # message kind
    bl SpecialMoveText
    mflr r6

DisplayText:
    lbz r4, 0xC(playerdata)     # message queue
    Message_Display
    b Exit

###################
## TEXT CONTENTS ##
###################

DoubleJumpText:
    blrl
    .string "DoubleJump OoJump\nFrame %d"
    .align 2

AerialText:
    blrl
    .string "Aerial OoJump\nFrame %d"
    .align 2

SpecialMoveText:
    blrl
    .string "Special OoJump\nFrame %d"
    .align 2

##############################

Exit:
    restore
    blr
