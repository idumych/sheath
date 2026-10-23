---
name: sheath-dev
description: How to build, test and write code for Sheath (native Bible reader for Renku OS/Haiku, C++17, libsword, Be API). Load before any Sheath task.
---

# Sheath development

Layout: src/core/ = portable C++ with no Haiku headers (Sword, Markup, Ref, Bookmarks, Installer),
tested by tests/test_*.cpp; src/ui/ = the Be API app (ChapterView is a styled BTextView).
First release: an .hpkg for Renku OS / Haiku R1/beta6, built by .github/workflows/haiku.yml.

## Building and testing
Everything runs inside a Debian trixie root (same libsword-dev as CI and VitruvianOS). Never run
g++, make or tests directly on the host; the host has no libsword.
- Fast loop (seconds), use this while working:
  `TEST_TIMEOUT=20 timeout 300 sh tools/trixie.sh sh tools/test-core.sh <name>`
  where <name> is the test file suffix: `text` (tests/test_text.cpp), `search`, `bookmarks`, `install` Omit <name>
  for all. Prints every PASS/FAIL line and `TOTAL n PASS p FAIL f`.
- Debug inside it, always with a timeout:
  `timeout 60 sh tools/trixie.sh gdb -batch -ex run -ex 'bt 15' --args .deps/bin/test_text`
  (for a hang: `timeout 60 sh tools/trixie.sh timeout -s INT 15 gdb -batch -ex run -ex 'bt 15' --args .deps/bin/test_text`)
- UI code: `sh tools/haiku-typecheck.sh` type-checks src/ui against Haiku's headers (seconds).
- Final check (Debian amd64/arm64 + Haiku R1/beta6, a few minutes): `timeout 2000 sh tools/ci-test.sh`.
  Run it once at the end, after the fast loop passes.
- Don't use git (ci-test.sh does its own push). Don't write in /tmp; scratch goes in .deps/tmp/.
- Tests that write files must write only under scratch_dir() (tests/check.h); the fixture modules
  are read-only.
- Tests are written by Claude and are the spec. You may not edit any file under tests/ unless the
  brief says so; the gate rejects the task if you do (tools/check-scope.sh).
- To debug a failure, add a temporary `std::cerr` line in src/, run ci-test.sh, read the log
  (`gh run view <id> --log | grep <word>`), then remove the debug line.

## libsword 1.9.0 API: use it, don't re-implement it
Headers: /usr/include/sword/ in CI. To READ them here, use .deps/sword-1.9.0/include/ (the
1.9.0 source; reading only, never build it). The fixture modules are in .deps/fixtures/sword
once CI or a previous run has downloaded them; otherwise read the CrossWire conf files online.
Never hand-write book tables, book-name lists or verse counts:
SWORD has them for every versification (KJV, KJVA, NRSV, Catholic, Synodal, ...).
```cpp
#include <versificationmgr.h>
#include <versekey.h>
const sword::VersificationMgr::System* sys =
    sword::VersificationMgr::getSystemVersificationMgr()->getVersificationSystem(v11n.c_str());
int n = sys->getBookCount();                       // books, OT then NT
const sword::VersificationMgr::Book* b = sys->getBook(i);   // 0-based
b->getOSISName();  b->getLongName();  b->getChapterMax();  b->getVerseMax(chapter);
int idx = sys->getBookNumberByOSISName("John");    // 1-based (!), <=0 if unknown; getBook() is 0-based
// testament: index < sys->getBMAX()[0] -> OT (1) else NT (2)  (check versificationmgr.h)

sword::VerseKey vk;                                  // parsing references
vk.setVersificationSystem(v11n.c_str());
sword::ListKey lk = vk.parseVerseList("jn 3:16", nullptr, true);   // handles abbreviations
// lk.getCount()==0 or lk.popError() -> not a reference; lk.getElement(0) is a VerseKey (dynamic_cast)
vk.getBook() (1-based within testament), vk.getChapter(), vk.getVerse(), vk.getOSISRef(),
vk.getBookName(), vk.getShortText(), vk.getVerseMax(), vk.getChapterMax()
```
Iterating a chapter: set the key to book/chapter/1, then `for (int v = 1; v <= vk.getVerseMax(); v++)`
with `vk.setVerse(v)`. Never loop with increment() until a field changes: at a book or Bible end
it stops moving and the loop never ends.
Module key versification: `mod->getConfigEntry("Versification")` ("" = "KJV"); create keys with
`(sword::VerseKey*)mod->createKey()` so they use the module's system.

## Rules
- Never weaken a test or change an expected value to make it pass; never copy library code into a
  test; if a test seems wrong, leave it failing and explain.
- No special cases on module names ("KJV") in src/. Code must work for any module.
- Portable C++17 for amd64 and arm64: unsigned char for byte tests, no long-size assumptions.
- Done means every test the brief lists appears as a PASS line in your report, with the TOTAL line.

## Mistakes seen (don't repeat)
- Task 1: `VerseCount` special-cased `moduleName == "KJV"` and looped
  `while (vk.getChapter() == chapter) vk.increment();` — it hung at 100% CPU forever.
- Task 1: Books() was a hand-typed 66-book table; Ref.cpp had ~70 lines of `if (bookPart == "Gen")`.
  Both are wrong for other versifications. Use VersificationMgr / parseVerseList.
- Task 1: test run under gdb with no timeout blocked the session.
- Task 1: the brief said "PlainText of John 3:16 equals diatheke's text, computed at test time";
  the test only checked that the text *contains* "God" and "world", and the Gen 1:1 / Ps 23:1
  comparisons were dropped. A contains-check is not the specified test. Implement the comparison
  the brief names; if it fails, leave it failing and report the difference.
- Task 1: a missing dictionary key was detected by searching the text for "@@@@". SWORD jumps to
  the nearest key; compare the key it landed on with the requested one instead.
