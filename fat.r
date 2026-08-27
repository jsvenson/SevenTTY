#include "Processes.r"
#include "CodeFragments.r"

/* Fat (universal) application cfrg — one fragment per architecture, built as
   raw data because Rez's cfrg template only accepts a single array entry.
   Layout (see CodeFragments.r 'cfrg' type):
     header: 8 longints, [2]=1 (version), [7]=entry count
     each entry: archType, updateLevel, currentVersion, oldDefVersion,
                 appStackSize, appSubFolderID, usage, where, offset,
                 length, reserved, reserved, entryLength, pstring name

   IMPORTANT — the 68K entry MUST come first. The 68K loader (BasiliskII)
   uses the first cfrg entry when a data fork is present; if the PPC entry
   (data-fork locator) is first, the 68K machine tries to run the PEF in the
   data fork and crashes on launch. With the 'm68k' entry (resource-fork
   locator, i.e. the classic CODE resources) first, the 68K loader finds the
   68K code and launches correctly.

   Entry 1 (68K):   arch 'm68k', where = 2 (resource fork) -> CODE resources
   Entry 2 (PPC):   arch 'pwpc', where = 1 (data fork)      -> PEF in data fork */

#ifndef CFRAG_NAME
#define CFRAG_NAME "SevenTTY"
#endif

data 'cfrg' (0) {
	$"00000000 00000000 00000001 00000000"
	$"00000000 00000000 00000000 00000002"

	$"6d36386b 00000000 00000000 00000000"  /* 'm68k' */
	$"00000000 0000 01 02 00000000 00000000" /* usage=1 where=2 (resource fork) */
	$"00000000 00000000 0034"               /* reserved, reserved, entryLength */
	$"08 536576656e545459 00"               /* pstring "SevenTTY" + pad to 52B */

	$"70777063 00000000 00000000 00000000"  /* 'pwpc' */
	$"00000000 0000 01 01 00000000 00000000" /* usage=1 where=1 (data fork) */
	$"00000000 00000000 0034"               /* reserved, reserved, entryLength */
	$"08 536576656e545459 00"               /* pstring "SevenTTY" + pad to 52B */
};
