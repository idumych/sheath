# Test plan: Sheath

Sheath reads SWORD modules, which it downloads from CrossWire, so the feature checks need the
VM to have internet access (VirtualBox NAT is enough). No host fixture.

`tools/renku-test.sh` installs a CI build on the Renku VM, launches it and takes screenshots;
`tools/renku-test.sh do STEP...` drives it. Read each screenshot (OCR is only a rough check).

## Fresh state

Haiku / Renku: delete `~/config/settings/Sheath_settings`, `~/config/settings/Sheath_bookmarks`,
`~/config/settings/Sheath_network_ok` and `~/.sword` (`tools/renku-test.sh fresh` does this).

## Feature checks

### install-module
Steps: launch with fresh state. The window says "No Bible is installed yet". Open the "Library"
menu, choose "Download modules…" (or press "Download" on the toolbar). Press "Refresh list", confirm "Download". In the language
menu choose "en", type "KJV" in the filter field, select the line starting "KJV —", press
"Install". Close the Modules window.
PASS when: the Modules window status reads "KJV installed." and the reader window shows
Genesis or John text with "KJV" in the window title.

### read-chapter
Steps: type "jn 3:16" in the "Go to…" field on the toolbar and press Enter.
PASS when: OCR of the window shows "For God so loved the world" and the title contains "John 3".

### strongs-lookup
Steps: View menu, turn on "Strong's numbers". Also install "StrongsGreek" as in
install-module. Click the small blue number after "loved" in John 3:16 (it reads 25).
PASS when: a window titled "Strong's G25" opens and shows "agapao".

### search
Steps: Search menu, "Search…" (or the "Search" toolbar button), type "only begotten", choose "Exact phrase", press "Search".
PASS when: the status line reads "6 verses." and the list contains "John 3:16".

### bookmark
Steps: right-click verse 16 of John 3, choose "Bookmark John 3:16". Quit with Alt+Q, launch again.
PASS when: the Bookmarks menu lists "John 3:16" and Sheath reopens at John 3.
