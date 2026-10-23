---
name: sheath-dev
description: How to build, test and write code for Sheath (native Bible reader, C++17, libsword 1.9.0, sqlite3, Be API). Load before any Sheath task.
---

# Sheath development

## Commands (the ONLY way to build and test)
- `timeout 300 sh tools/test-core.sh` — builds libsword if missing, prepares fixtures, compiles and
  runs every host test binary. Prints `PASS <name>` / `FAIL <name>: ...` lines and a last line
  `TOTAL <n> PASS <p> FAIL <f>`; exits 1 if f > 0 or n == 0.
- `sh tools/haiku-typecheck.sh` — type-checks src/ui against real Haiku headers (once it exists).
- Always run test binaries with `timeout` (e.g. `timeout 60 ./.deps/test_core`). Never run a test
  under gdb without `timeout 60 gdb -batch ...`. A hang is a bug to find, not something to wait on.
- To find a hang: `timeout 20 gdb -batch -ex run -ex bt --args <bin>` won't stop by itself, so run
  the binary with `timeout -s INT 10 gdb -batch -ex run -ex 'bt 15' --args <bin>` and read the stack.
- Scratch files: .deps/tmp/. Build outputs: .deps/ (not build/, not /tmp).
- The tests run on THIS Linux host. There is no VM in these tasks; don't try to use one.

## libsword 1.9.0 API: use it, don't re-implement it
Headers: .deps/sword/include/sword/. Never hand-write book tables, book-name lists or verse counts:
SWORD has them for every versification (KJV, KJVA, NRSV, Catholic, Synodal, ...).
```cpp
#include <versificationmgr.h>
#include <versekey.h>
const sword::VersificationMgr::System* sys =
    sword::VersificationMgr::getSystemVersificationMgr()->getVersificationSystem(v11n.c_str());
int n = sys->getBookCount();                       // books, OT then NT
const sword::VersificationMgr::Book* b = sys->getBook(i);   // 0-based
b->getOSISName();  b->getLongName();  b->getChapterMax();  b->getVerseMax(chapter);
int idx = sys->getBookNumberByOSISName("John");    // 0-based, <0 if unknown
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
