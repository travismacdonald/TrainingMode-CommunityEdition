# To be inserted at 80080614
# just before the branch to color hitboxes, we reverse the ID draw order

# regular:
# r3 = r31 + 2324 + r25 * 312 (FighterData.hitbox[i])

# re-ordered:
# r3 = r31 + 2324 + (3-r25) * 312 (FighterData.hitbox[3-i])

li r3, 3
sub r3, r3, r25
mulli r3, r3, 312
addi r3, r3, 2324
add r3, r3, r31
