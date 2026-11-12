hebOCR for macOS
================

hebOCR is a free Hebrew OCR engine that can recognize niqud (vowel
points). It was written by Kobi (Yaacov) Zamir, with contributions by
Diego Iastrubni and others, and is free software under the GNU GPL
version 3 (see COPYING). This build runs on Macs with Apple silicon
(M1 or later) and macOS 14 Sonoma or later.

Installing
----------

Open the disk image and drag hebOCR into the Applications folder.

The first time you open it, macOS will refuse, because hebOCR is not yet
signed by a registered Apple developer:

  1. Open hebOCR once (double-click). macOS says it cannot be opened;
     click Done (or OK).
  2. Open System Settings > Privacy & Security, scroll down, and click
     "Open Anyway" next to the message about hebOCR. Confirm with your
     password.

After that it opens normally. (Alternatively, in Terminal:
xattr -dr com.apple.quarantine /Applications/hebOCR.app)

Using hebOCR
------------

Open an image (or drag one onto the window), adjust settings on the left
if you like, and choose Run OCR (F5). Help > Options Guide explains every
setting.

The command-line engine is inside the app:

  /Applications/hebOCR.app/Contents/MacOS/hebocr -i page.png -o page.txt

Project home: https://github.com/aharonium/hebocr
