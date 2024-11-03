    # To be inserted at 8006b7f8
    .include "../Globals.s"
    .include "../../m-ex/Header.s"

    .set playerdata, 31
    .set player, 30

    backupall

    # Check if Yoshi
    lwz r3, 0x4(playerdata)
    cmpwi r3, Yoshi.Int
    bne Exit

    # Check if enabled
    li r0, OSD.SpacieTech    # OSD ID
    lwz r4, -0x77C0(r13)
    lwz r4, 0x1F24(r4)
    li r3, 1
    slw r0, r3, r0
    and. r0, r0, r4
    beq Exit

    # Branch to AS Functions
    lwz r3, 0x10(playerdata)
    cmpwi r3, 0x159    # Parry Start
    beq ParryStart

    b Exit

# /////////////////////////////////////////////////////////////////////////////

ParryStart:
    bl CheckForJumpCancel
    cmpwi r3, 0x0
    beq Exit

ParryStart_SetColor:
    load r5, MSGCOLOR_RED
    lhz r3, 0x23F8(playerdata)
    cmpwi r3, 0x1
    bne ParryStart_EndSetColor
    load r5, MSGCOLOR_GREEN

ParryStart_EndSetColor:
    li r3, OSD.SpacieTech       # ID
    lbz r4, 0xC(playerdata)     # queue
    bl JumpOoParryText
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

JumpOoParryText:
    blrl
    .string "Jump OOParry\nFrame %d"
    .align 2

##############################

Exit:
    restoreall
    cmplwi r12, 0
