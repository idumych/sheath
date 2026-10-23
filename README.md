# Sheath

A Bible reader for [Renku OS](https://github.com/RenkuOS/Source) and Haiku, built on the
[SWORD](https://crosswire.org/sword/) library. Loosely inspired by AndBible and
[LinBible](https://github.com/idumych/linbible).

- Any SWORD Bible module, in its own versification
- Words of Christ in red, headings, footnotes
- Strong's numbers that open the Greek or Hebrew dictionary entry
- Go to a reference by typing it ("jn 3:16", "Psalm 23")

## Building

On Haiku or Renku:

    pkgman install sword_devel
    make            # ./Sheath
    make test       # core tests (downloads the KJV and StrongsGreek modules once)
    make hpkg       # sheath-<version>-<arch>.hpkg

CI builds the package on Haiku R1/beta6 (`.github/workflows/haiku.yml`).

## Licence

GPL-3.0. SWORD is GPL-2.0-or-later.
