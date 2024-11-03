    # To be inserted at 8006b7f4
    .include "../../Globals.s"
    .include "../../../m-ex/Header.s"

    .set playerdata, 31
    .set player, 30

    backupall

# General AS's

    # Check If Fox or Falco
    lwz r3, 0x4(playerdata)
    cmpwi r3, 0x1
    beq FoxFalco
    cmpwi r3, 0x16
    beq FoxFalco

    # Check If Anyone Else
    b Exit

# /////////////////////////////////////////////////////////////////////////////

FoxFalco:
    # CHECK IF ENABLED
    li r0, OSD.FighterSpecificTech    # OSD ID
    lwz r4, -0x77C0(r13)
    lwz r4, 0x1F24(r4)
    li r3, 1
    slw r0, r3, r0
    and. r0, r0, r4
    beq Exit

    # Branch to AS Functions
    lwz r3, 0x10(playerdata)
    cmpwi r3, 0x15B                 # Ground Side B Start
    beq SideBStart
    cmpwi r3, 0x15E                 # Air Side B Start
    beq SideBStart

    cmpwi r3, 0x15C                 # Ground Side B
    beq SideB
    cmpwi r3, 0x15F                 # Air Side B
    beq SideB

    cmpwi r3, 0x15D                 # Ground Side B End
    beq SideBEnd
    cmpwi r3, 0x160                 # Air Side B End
    beq SideBEnd

    cmpwi r3, 0x169
    beq ShineGroundLoop
    cmpwi r3, 0x16E
    beq ShineAirLoop

    b Exit

# /////////////////////////////////////////////////////////////////////////////

SideBStart:
    # Check If Pressed B
    lwz r3, 0x668(playerdata)
    rlwinm. r3, r3, 0, 22, 22
    beq Exit

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
    bl ShortenEarlyPressText
    mflr r6
    Message_Display

    b Exit

# /////////////////////////////////////////////////////////////////////////////

SideB:
    # Check If Pressed B
    lwz r3, 0x668(playerdata)
    rlwinm. r3, r3, 0, 22, 22
    beq Exit

    # Get Press Frame
    lfs f1, 0x894(playerdata)
    fctiwz f1, f1
    stfd f1, 0xF0(sp)
    lwz r3, 0xF4(sp)
    addi r7, r3, 0x1

    li r3, 8                    # ID
    lbz r4, 0xC(playerdata)     # queue
    load r5, MSGCOLOR_GREEN
    bl ShortenTypeText
    mflr r6
    Message_Display

    b Exit

# /////////////////////////////////////////////////////////////////////////////

SideBEnd:
    # Check If Pressed B
    lwz r3, 0x668(playerdata)
    rlwinm. r3, r3, 0, 22, 22
    beq Exit

    li r3, 8                    # ID
    lbz r4, 0xC(playerdata)     # queue
    load r5, MSGCOLOR_RED
    bl ShortenLatePressText
    mflr r6
    Message_Display

    b Exit

# /////////////////////////////////////////////////////////////////////////////

ShineGroundLoop:
    # Check For JC
    bl CheckForJumpCancel
    cmpwi r3, 0x0
    beq Exit

ShineGroundLoop_Interrupted:

ShineGroundLoop_SetColor:
    load r5, MSGCOLOR_RED
    lhz r3, 0x23F8(playerdata)
    cmpwi r3, 0x1
    bne ShineGroundLoop_EndSetColor
    load r5, MSGCOLOR_GREEN
ShineGroundLoop_EndSetColor:

    li r3, 8                    # ID
    lbz r4, 0xC(playerdata)     # queue
    bl ActOOShineText
    mflr r6
    lhz r7, 0x23F8(playerdata)
    Message_Display

    b Exit

# /////////////////////////////////////////////////////////////////////////////

ShineAirLoop:
    # Check For Remaining Jump
    lbz r3, 0x1968(playerdata)      # Jumps Used
    lwz r0, 0x0168(playerdata)      # Total Jumps
    cmpw r3, r0
    bge Exit

    # Check For JC
    bl CheckForJumpCancel
    cmpwi r3, 0x0
    beq Exit

ShineAirLoop_Interrupted:

ShineAirLoop_SetColor:
    load r5, MSGCOLOR_RED
    lhz r3, 0x23F8(playerdata)
    cmpwi r3, 0x1
    bne ShineAirLoop_EndSetColor
    load r5, MSGCOLOR_GREEN
ShineAirLoop_EndSetColor:

    li r3, 8                    # ID
    lbz r4, 0xC(playerdata)     # queue
    bl ActOOShineText
    mflr r6
    lhz r7, 0x23F8(playerdata)
    Message_Display

    b Exit

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
    b CheckForJumpCancel_Exit

CheckForJumpCancel_CheckButtons:
    lwz r0, 0x0668(playerdata)
    rlwinm. r0, r0, 0, 20, 21
    beq CheckForJumpCancel_NoButtons
    li r3, 0x1
    b CheckForJumpCancel_Exit

CheckForJumpCancel_NoButtons:
    li r3, 0x0

CheckForJumpCancel_Exit:
    lwz r0, 0x003C(sp)
    addi sp, sp, 56
    mtlr r0
    blr

###################
## TEXT CONTENTS ##
###################

ShortenEarlyPressText:
    blrl
    .string "Shorten Press\n%df Early"
    .align 2

ShortenTypeText:
    blrl
    .string "Shorten Press\nFrame %d/4"
    .align 2

ShortenLatePressText:
    blrl
    .string "Shorten Press\nLate"
    .align 2

ActOOShineText:
    blrl
    .string "Act OOShine\nFrame %d"
    .align 2

##############################

Exit:
    restoreall
    lwz r12, 0x219C(r31)
