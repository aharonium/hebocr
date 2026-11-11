# hebOCR Roadmap

A record of planned work and wishes for future releases. Items are
grouped by theme, not ordered by priority. Proposals and corrections are
welcome; individual items can be turned into GitHub issues as work on
them begins.

## Recognition: more font models

The engine recognizes letters with **hand-written rules**: for each
Hebrew letter, a function scores measurements of a letter image (height,
width, position, edges, holes, …). Only one rule set works: **David**.
Five more slots exist (`-z 1` to `-z 5`), but they are empty. Adding a
model today means writing a new rule set by hand, which is slow and
needs expert tuning.

- **Models for more typefaces.** Start with the digital Hebrew fonts
  listed by the Open Siddur Project
  (<https://opensiddur.org/wp-content/plugins/custom-fonts-display/data/fonts.json>),
  then historical book faces.
- **A trainable recognizer.** Instead of hand-writing rules, learn them
  from examples. The engine already computes, and can export (`-u`), the
  measurements its rules use for every letter. A model trained on those
  measurements, or on the letter images themselves, would make new
  typefaces a matter of data rather than code.
- **Synthetic training data from digital fonts.** Render known text,
  with and without niqud, in each font. This gives unlimited letter
  images whose correct reading is known exactly, at many sizes and with
  simulated scan noise.
- **Real training data.** Build a public collection of page images in
  different Hebrew types, each paired with a proofread transcription.
  Pages already transcribed by hand (such as Open Siddur transcriptions
  of scanned works) are ideal. The collection would also serve as a
  benchmark for measuring accuracy.
- **Typeface identification.** Recognize which typeface (or nearest
  model) a page is set in, report how confident that match is, and apply
  the matching model automatically. Mixed pages (e.g. square script with
  Rashi script commentary) would use different models per region.

## Recognition: accuracy

- **Niqud.** Fix systematic confusions seen in testing: tsere read as
  shəva, pataḥ as ḥiriq, missing ḥolam and qamats, dropped dagesh.
- **Ṭe'amim (cantillation marks)**, not recognized at all today.
- **Accuracy measurement.** Compare OCR output with a proofread
  transcription and report the character error rate, overall and for
  letters and niqud separately. This makes every change measurable.
- **Faster processing of noisy images** such as phone photos, which can
  take minutes per page.

## Other engines and services

- **Tesseract OCR.** Run Tesseract alongside or instead of hebOCR: for
  other scripts (for example Ethiopic/Ge'ez in bilingual works), and to
  compare or combine Hebrew results where the two engines disagree.
- **Dicta Nakdan** (<https://nakdan.dicta.org.il>), Avi Shmidman's
  automatic vocalizer. Add niqud to unvocalized OCR output, or check
  hebOCR's niqud against Nakdan's suggestions. Its terms of use and
  available programming interface need checking first.
- **Improve existing machine transcriptions.** The Internet Archive
  provides its own OCR for each scanned page, linked from its IIIF
  manifests. When processing an Archive item, show that text beside
  hebOCR's, highlight where they differ, and let the user take the
  better reading, or combine them automatically.

## Input and batch processing

- **Batch processing** of a folder of images or a multi-page PDF.
- **IIIF manifests.** Accept a manifest URL, or a web page that links
  to one (such as an Internet Archive item page). Support IIIF
  Presentation 2 and 3, show thumbnails of every page with its label,
  and select single pages, ranges (`1-29`) or all pages. Cache
  downloaded images, and request the best resolution the image server
  offers.
- **Batch output** as one combined file, with each page headed by its
  number, label and source, or as one file per page named by number and
  label.
- **Settings per page range**, e.g. hebOCR for pages 1–29 and another
  engine for the rest of a bilingual book.
- **Regions.** Draw areas on a page and mark each as Hebrew, another
  script, or ignore. Region templates can be reused across pages with a
  repeating layout.
- **Page preparation.** Split two-page spreads, crop margins, and read
  grayscale and color TIFF in the command-line engine (the GUI already
  converts them).

## Program

- **Hebrew user interface** (translation of menus and settings).
- **Saved setting presets**, e.g. "vocalized 19th-century type" or
  "modern print".
- **Side-by-side proofreading**: click a line of text to highlight it
  on the page image, and the reverse.
- **Windows installer** with Start menu entry and file associations, and
  a signed executable.

## Done

- **0.11.1**: first Windows build; memory-safety fixes; PNG/JPEG input;
  CMake build; automated builds and releases.
- **0.12.0**: hebocr-gui for one page at a time; options guide; corrected
  command-line help.
