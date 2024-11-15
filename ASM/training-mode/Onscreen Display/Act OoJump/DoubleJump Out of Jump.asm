    # To be inserted at 800cb400
    .include "../../Globals.s"

    .set playerdata, 31
    .set player, 30

    backup

    # fulfill requirements of the static function before branching
    mr r4, r30                 # store player to r4
    bl Text                    # store the address of OSD text to r6
    mflr r6
    li r10, OSD.Miscellaneous  # store the OSD kind ID to r10 / Originally OSD.ActOoJump is natural, but use other ID to show at the same time as Aerial OoJump for fighters with DoubleJump cancel techniques

    branchl r12, 0x8000550c
    b Exit

Text:
    blrl
    .string "DblJump OoJump\nFrame %d"
    .align 2

Exit:
    restore
    cmpwi r3, 0
