# Changelog

Changes in each hebOCR release since development resumed in 2026. The
history of the original project (2005–2013) is in the file `ChangeLog`.

## 0.12.0 (unreleased)

### New

- **hebocr-gui**, a graphical program for recognizing one page at a time:
  - open an image from the menu, by drag and drop, or from the command line
  - every engine option in a settings panel, each with a tooltip; settings
    are remembered between sessions
  - run recognition in the background, with a Stop button
  - view the original image, the processed black-and-white image, and the
    engine's layout analysis, with zoom and pan
  - edit the recognized text right to left, with a text size slider and a
    menu of installed fonts that support Hebrew; save it as UTF-8 text or
    HTML
  - the equivalent `hebocr` command for the current settings, with a Copy
    button
  - select an area of the page with the mouse, and read only that area
    (for example the Hebrew text on a page that also has text in another
    script)
  - opens grayscale and color TIFF files, which the engine itself cannot
- **Options Guide** (`docs/options.md`, and Help ▸ Options Guide in the
  program, with search and adjustable text size): explains every setting
  and option, how the engine reads a page, and its known limitations.
- **Roadmap** of planned work (`docs/ROADMAP.md`).
- **Linux AppImage**: a single file that runs on most Linux distributions
  (as new as Ubuntu 24.04 or newer) without installing anything. It
  replaces the Linux download that needed Qt installed.
- A program icon (a scanned and a digital alef, by Aharon N. Varady), and
  the build number shown next to the version in
  About and in `hebocr --version`.
- Credits for all known contributors, and Help ▸ Third-Party Software and
  Help ▸ About Qt; both downloads include the license notices of every
  bundled library.
- The command-line option `--no-niqud` (`--no-nikud` still works).

### Fixed

- **Multi-column pages are read right to left.** When the number of
  columns was unknown, above 6, or free layout was chosen, the left column
  of a Hebrew page could be read before the right one; a title above the
  columns could reverse them even with the column count set. Titles and
  full-width paragraphs now stay in place between the columns, and no text
  block can be dropped from the reading order.
- Undersized memory allocations in the array statistics and histogram
  code; one was a heap overflow.
- Code that newer compilers treat as errors.
- The command-line help described the thresholding types wrongly
  ("normal, none, fine" are adaptive, fixed and fine adaptive), and
  offered font models that are empty.

### Changed

- "Niqud" and the Open Siddur Project's romanization of vowel names in
  all user-facing text.

## 0.11.1 (2026-11-11)

The first release since 2013, continuing the 0.11 branch of Kobi Zamir's
and Diego Iastrubni's code.

### New

- **Windows builds**, made automatically by GitHub for every change,
  together with Linux builds; publishing a release attaches both.
- PNG, JPEG, BMP, GIF and TGA input.
- A CMake build and automated regression tests.

### Fixed

- Fixes from the libhocr 0.10.18 package (December 2013) and from Debian:
  memory leaks, and format-string safety in the command-line tool.
- Many out-of-bounds memory reads that could crash the engine or make its
  results depend on leftover memory, found with Valgrind and
  AddressSanitizer.
- A crash when column detection failed.
- Holam was output as a sin dot (U+05C2) on letters other than shin.
- Truncated PNM files were read as partly blank images.
