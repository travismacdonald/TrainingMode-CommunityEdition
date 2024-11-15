    # To be inserted at 800cc570
    .include "../../Globals.s"

    .set playerdata, 30
    .set player, 29

    backup

    # fulfill requirements of the static function before branching
    mr r4, r29             # store player to r4
    bl Text                # store the address of OSD text to r6
    mflr r6
    li r10, OSD.ActOoJump  # store the OSD kind ID to r10

    branchl r12, 0x8000550c
    b Exit

Text:
    blrl
    .string "Aerial OoDblJump\nFrame %d"
    .align 2

Exit:
    restore
    cmpwi r3, 0
