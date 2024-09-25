# ASM

Aitch: I don't write much melee asm, but here's some things I've picked up by hacking on unclepunch's asm.
Here are some things you should know before diving into here, and some questions I have.

## Notes:
- If you see a load with offset around 0x23ec-0x2424 in OSD related code,
it's probably reading or writing to `struct MEX` or `struct TM` in `struct FighterData` in `MexTK/include/fighter.h`.
- `Moonwalk_Exit` is not called that for any reason, it's just a generic exit.

## Questions:
- What is R13??
- There seems to be two distict styles of asm: one that import mex.h and one that does not.
I'm pretty sure the mex import asm files are newer. 
Should I convert the old style asm files to new ones? 
If so, how?
