hebOCR for Windows
==================

hebOCR is a free Hebrew OCR engine that can recognize niqud (vowel
points). It was written by Kobi (Yaacov) Zamir, with contributions by
Diego Iastrubni and others, and is free software under the GNU GPL
version 3 (see COPYING).

Keep hebocr.exe and hebocr-gui.exe together with the .dll files and
subfolders in this folder.

Using the graphical program
---------------------------

Double-click hebocr-gui.exe. Open an image (or drag one onto the
window), adjust settings on the left if you like, and press Run OCR
(F5). The recognized text appears on the right, where you can edit it
and save it as UTF-8 text.

The "Black & White" and "Layout" tabs show how the engine saw the
page: the processed image, and the columns, lines and letters it found.
If recognition is poor, these help you choose better settings. Hover
over any setting for a short explanation, or press F1 for the full
Options Guide. The Engine panel at the bottom shows the equivalent
hebocr command line.

Using the command-line program
------------------------------

Basic use (in PowerShell or Command Prompt, from this folder):

  .\hebocr.exe -i page.png -o page.txt

The output is UTF-8 text. Open it in a UTF-8 aware editor (Notepad,
VS Code, LibreOffice). Hebrew printed to the console window may look
garbled; write to a file with -o instead.

Supported input: PNG, JPEG, BMP, GIF, TGA, PNM/PGM/PPM, and
black-and-white TIFF. Color images are converted to grayscale.

Useful options
--------------

  -n          do not recognize niqud (faster, for unvocalized text)
  -h          write HTML (hOCR, with positions) instead of plain text
  -w NUM      word spacing, -3 (tight) to 3 (spaced); try 2 for
              letter-spaced words
  -c NUM      columns: 0 auto, 1.. number of columns, 255 free layout
  -q DEG      rotate the image clockwise by DEG degrees
  -A          linguistics check (final letter forms at word ends)

  .\hebocr.exe --help-all     lists every option

options-guide.md (also Help > Options Guide, F1, in hebocr-gui.exe)
explains every option and when to change it.

Tips
----

- Clean, straight, well-lit scans at 300 dpi work best. Phone photos
  of pages work poorly and can be very slow.
- Scan one page per image; split two-page spreads first.

Project home: https://github.com/aharonium/hebocr
