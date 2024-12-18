    # To be inserted at 8006b7f4
    # Runs once per frame, use for OSDs that don't require a specific injection point

    .include "../Globals.s"
    .include "../../m-ex/Header.s"

    .set playerdata, 31
    .set player, 30

    backupall

OSD_FighterSpecificTech:
    li r0, OSD.FighterSpecificTech    # OSD ID
    lwz r4, -0x77C0(r13)
    lwz r4, 0x1F24(r4)
    li r3, 1
    slw r0, r3, r0
    and. r0, r0, r4
    beq OSD_Lockout

    # Check Fighter
    lwz r3, 0x4(playerdata)  # load fighter Internal ID
    cmpwi r3, Fox.Int
    beq FoxFalco
    cmpwi r3, Falco.Int
    beq FoxFalco
    cmpwi r3, Yoshi.Int
    beq Yoshi

    # Check If Anyone Else
    b OSD_Lockout

# /////////////////////////////////////////////////////////////////////////////

FoxFalco:
    lwz r3, 0x10(playerdata)  # load action state ID
    cmpwi r3, 0x15B                 # Ground Side B Start
    beq FoxFalco_SideBStart
    cmpwi r3, 0x15E                 # Air Side B Start
    beq FoxFalco_SideBStart

    cmpwi r3, 0x15C                 # Ground Side B
    beq FoxFalco_SideB
    cmpwi r3, 0x15F                 # Air Side B
    beq FoxFalco_SideB

    cmpwi r3, 0x15D                 # Ground Side B End
    beq FoxFalco_SideBEnd
    cmpwi r3, 0x160                 # Air Side B End
    beq FoxFalco_SideBEnd

    cmpwi r3, 0x169
    beq FoxFalco_ShineGroundLoop
    cmpwi r3, 0x16E
    beq FoxFalco_ShineAirLoop

    b OSD_Lockout

# --------

FoxFalco_SideBStart:
    # Check If Pressed B
    lwz r3, 0x668(playerdata)
    rlwinm. r3, r3, 0, 22, 22
    beq OSD_Lockout

    # Get Frames Early
    lwz r7, 0x590(playerdata)       # get anim data
    lfs f1, 0x008(r7)               # get anim length (float)
    fctiwz f1, f1
    stfd f1, 0xF0(sp)
    lwz r7, 0xF4(sp)
    lhz r3, 0x23F8(playerdata)
    sub r7, r7, r3
    subi r7, r7, 0x1

    li r3, OSD.FighterSpecificTech  # ID
    lbz r4, 0xC(playerdata)         # queue
    load r5, MSGCOLOR_RED
    bl FoxFalco_ShortenEarlyPressText
    mflr r6
    Message_Display

    b OSD_Lockout

# --------

FoxFalco_SideB:
    # Check If Pressed B
    lwz r3, 0x668(playerdata)
    rlwinm. r3, r3, 0, 22, 22
    beq OSD_Lockout

    # Get Press Frame
    lfs f1, 0x894(playerdata)
    fctiwz f1, f1
    stfd f1, 0xF0(sp)
    lwz r3, 0xF4(sp)
    addi r7, r3, 0x1

    li r3, OSD.FighterSpecificTech  # ID
    lbz r4, 0xC(playerdata)         # queue
    load r5, MSGCOLOR_GREEN
    bl FoxFalco_ShortenTypeText
    mflr r6
    Message_Display

    b OSD_Lockout

# --------

FoxFalco_SideBEnd:
    # Check If Pressed B
    lwz r3, 0x668(playerdata)
    rlwinm. r3, r3, 0, 22, 22
    beq OSD_Lockout

    li r3, OSD.FighterSpecificTech  # ID
    lbz r4, 0xC(playerdata)         # queue
    load r5, MSGCOLOR_RED
    bl FoxFalco_ShortenLatePressText
    mflr r6
    Message_Display

    b OSD_Lockout

# --------

FoxFalco_ShineGroundLoop:
    # Check For JC
    bl CheckForJumpCancel
    cmpwi r3, 0x0
    beq OSD_Lockout

FoxFalco_ShineGroundLoop_SetColor:
    load r5, MSGCOLOR_RED
    lhz r3, 0x23F8(playerdata)
    cmpwi r3, 0x1
    bne FoxFalco_ShineGroundLoop_EndSetColor
    load r5, MSGCOLOR_GREEN

FoxFalco_ShineGroundLoop_EndSetColor:
    li r3, OSD.FighterSpecificTech  # ID
    lbz r4, 0xC(playerdata)         # queue
    bl FoxFalco_ActOOShineText
    mflr r6
    lhz r7, 0x23F8(playerdata)
    Message_Display

    b OSD_Lockout

# --------

FoxFalco_ShineAirLoop:
    # Check For Remaining Jump
    lbz r3, 0x1968(playerdata)      # Jumps Used
    lwz r0, 0x0168(playerdata)      # Total Jumps
    cmpw r3, r0
    bge OSD_Lockout

    # Check For JC
    bl CheckForJumpCancel
    cmpwi r3, 0x0
    beq OSD_Lockout

FoxFalco_ShineAirLoop_SetColor:
    load r5, MSGCOLOR_RED
    lhz r3, 0x23F8(playerdata)
    cmpwi r3, 0x1
    bne FoxFalco_ShineAirLoop_EndSetColor
    load r5, MSGCOLOR_GREEN

FoxFalco_ShineAirLoop_EndSetColor:

    li r3, OSD.FighterSpecificTech  # ID
    lbz r4, 0xC(playerdata)         # queue
    bl FoxFalco_ActOOShineText
    mflr r6
    lhz r7, 0x23F8(playerdata)
    Message_Display

    b OSD_Lockout

# /////////////////////////////////////////////////////////////////////////////

Yoshi:
    lwz r3, 0x10(playerdata)  # load action state ID
    cmpwi r3, 0x159    # Parry Start
    beq Yoshi_Parry

    b OSD_Lockout

# --------

Yoshi_Parry:
    bl CheckForJumpCancel
    cmpwi r3, 0x0
    beq OSD_Lockout

Yoshi_PrintJumpOoParryText:
    load r5, MSGCOLOR_WHITE
    li r3, OSD.FighterSpecificTech  # ID
    lbz r4, 0xC(playerdata)         # queue
    bl Yoshi_JumpOoParryText
    mflr r6
    lhz r7, 0x23F8(playerdata)
    Message_Display
    b OSD_Lockout

# /////////////////////////////////////////////////////////////////////////////

CheckForJumpCancel:
    mflr r0
    stw r0, 0x0004(sp)
    stwu sp, -0x0038(sp)
    # Check For JC
    lwz r5, -0x514C(r13)
    lfs f0, 0x0070(r5)
    lfs f1, 0x0624(playerdata)
    fcmpo cr0, f1, f0
    blt CheckForJumpCancel_CheckButtons
    lbz r3, 0x0671(playerdata)
    lwz r0, 0x0074(r5)
    cmpw r3, r0
    bge CheckForJumpCancel_CheckButtons
    li r3, 0x1
    b CheckForJumpCancel_OSD_Lockout

CheckForJumpCancel_CheckButtons:
    lwz r0, 0x0668(playerdata)
    rlwinm. r0, r0, 0, 20, 21
    beq CheckForJumpCancel_NoButtons
    li r3, 0x1
    b CheckForJumpCancel_OSD_Lockout

CheckForJumpCancel_NoButtons:
    li r3, 0x0

CheckForJumpCancel_OSD_Lockout:
    lwz r0, 0x003C(sp)
    addi sp, sp, 56
    mtlr r0
    blr

###################
## TEXT CONTENTS ##
###################

FoxFalco_ShortenEarlyPressText:
    blrl
    .string "Shorten Press\n%df Early"
    .align 2

FoxFalco_ShortenTypeText:
    blrl
    .string "Shorten Press\nFrame %d/4"
    .align 2

FoxFalco_ShortenLatePressText:
    blrl
    .string "Shorten Press\nLate"
    .align 2

FoxFalco_ActOOShineText:
    blrl
    .string "Act OoShine\nFrame %d"
    .align 2

# --------

Yoshi_JumpOoParryText:
    blrl
    .string "Jump OoParry\nFrame %d"
    .align 2

##############################

OSD_Lockout:
    # Check enabled
    li r0, OSD.LockoutTimers
    lwz r4, -0x77C0(r13)
    lwz r4, 0x1F24(r4)
    li r3, 1
    slw r0, r3, r0
    and. r0, r0, r4
    beq Exit

#GetPrevLockout:
#    bl GetPrevLockoutEnd
#    .long 0
#GetPrevLockoutEnd:
#    mflr r3
#    lwz r8, 0x0(r3)
#    lbz r7, 0x674(playerdata) # timer_lstick_smash_y
#    stw r7, 0x0(r3)

    # dont show if in the air
    lwz r3, 0xE0(playerdata) # air_state
    cmpwi r3, 1
    beq Exit

    # dont show if in dtilt / dsmash
    lwz r3, 0x10(playerdata) # state_id
    cmpwi r3, ASID_AttackLw3
    beq Exit
    cmpwi r3, ASID_AttackLw4
    beq Exit

    # dont show if stick is vertical
    lfs f0, 0x624(playerdata) # input.stick.y
    fsubs f1, f1, f1 # zero f1
    fcmpo cr0, f0, f1
    bge Exit

    lbz r7, 0x674(playerdata) # input.timer_lstick_smash_y
    li r5, MSGCOLOR_RED
    cmpwi r7, 4
    blt Lockout_InLockout
    li r5, MSGCOLOR_GREEN
    cmpwi r7, 25
    bge Exit

Lockout_InLockout:
    addi r7, r7, 1 # 1-index timer
    bl LockoutText
    mflr r6
    b Lockout_Message_Display

Lockout_LockoutEnded:
    li r6, 0 # setting format string to null deletes message

Lockout_Message_Display:
    li r3, OSD.LockoutTimers        # ID
    lbz r4, 0xC(playerdata)         # queue
    Message_Display
    b Exit

LockoutText:
    blrl
    .string "DTilt Lockout\nFrame %d"
    .align 2

Exit:
    restoreall
    lwz r12, 0x219C(r31)
