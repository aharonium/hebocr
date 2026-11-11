hebOCR for Windows
==================

hebOCR is a free Hebrew OCR engine that can recognize nikud (vowel
points). It was written by Kobi (Yaacov) Zamir, with contributions by
Diego Iastrubni and others, and is free software under the GNU GPL
version 3 (see COPYING).

Keep hebocr.exe together with the .dll files in this folder.

Basic use (in PowerShell or Command Prompt, from this folder)
--------------------------------------------------------------

  .\hebocr.exe -i page.png -o page.txt

The output is UTF-8 text. Open it in a UTF-8 aware editor (Notepad,
VS Code, LibreOffice). Hebrew printed to the console window may look
garbled; write to a file with -o instead.

Supported input: PNG, JPEG, BMP, GIF, TGA, PNM/PGM/PPM, and
black-and-white TIFF. Color images are converted to grayscale.

Useful options
--------------

  -n          do not recognize nikud (faster, for unvocalized text)
  -h          write HTML instead of plain text
  -z NUM      use recognition font NUM (1-6)
  -c NUM      columns: 1.. number of columns, 0 auto
  -q DEG      rotate the image clockwise by DEG degrees
  -A          run a linguistics check

  .\hebocr.exe --help-all     lists every option

Tips
----

- Clean, straight, well-lit scans at 300 dpi work best. Phone photos
  of pages work poorly and can be very slow.
- Scan one page per image; split two-page spreads first.

Project home: https://github.com/aharonium/hebocr
