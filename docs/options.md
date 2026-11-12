# hebOCR Options Guide

This guide explains every setting in the hebOCR program (`hebocr-gui`)
and every option of the command-line engine (`hebocr`). Each entry gives
the name used in the program, the matching command-line flag, the
allowed values and the default.

This guide was written in 2026 from the engine's source code and from
the settings menus of Kobi Zamir's original hocr-gtk program; the
`hebocr --help` text has only one line per option. Kobi Zamir's original
user documentation for the 0.10 series, in Hebrew, is on Wikibooks:
[Hocr - הפיכת תמונה עם אותיות עבריות לקובץ טקסט](https://he.wikibooks.org/wiki/Hocr_-_%D7%94%D7%A4%D7%99%D7%9B%D7%AA_%D7%AA%D7%9E%D7%95%D7%A0%D7%94_%D7%A2%D7%9D_%D7%90%D7%95%D7%AA%D7%99%D7%95%D7%AA_%D7%A2%D7%91%D7%A8%D7%99%D7%95%D7%AA_%D7%9C%D7%A7%D7%95%D7%91%D7%A5_%D7%98%D7%A7%D7%A1%D7%98).
Corrections are welcome.

## About hebOCR

hebOCR (originally *hocr*) was written by Kobi (Yaacov) Zamir from 2005
and released under the GNU GPL. It was designed for old, yellowed pages,
complex page layouts, and vocalized text: its 0.6 series was the first
Hebrew OCR program able to read text with niqud and keep the niqud in
its output. Diego Iastrubni later contributed the qhOCR interface and
reorganized the code. The original project offered three ways to use the
engine: the hocr-gtk program, the `hocr` command line, and a Python
interface (hocr-python). Development stopped around 2013; it was revived
in 2026.

## How hebOCR reads a page

hebOCR works in stages. Each group of settings controls one of them.

1. **Image processing.** The image is converted to grayscale and its
   contrast is stretched. It is enlarged if the letters are small, then
   converted to pure black and white ("thresholding"). Optional clean-up
   filters run next, and finally the page is straightened if it is
   slightly rotated. The *Black & White* tab shows the result.
2. **Page layout.** The engine finds columns, then lines within each
   column, then words within each line, then letters within each word.
   Blobs that are too wide to be one letter are cut apart ("slicing").
   The *Layout* tab shows the result: columns in blue, lines in red,
   letters highlighted.
3. **Recognition.** Each letter is measured (height, width, position,
   edges, holes, …) and compared against a set of rules for each Hebrew
   letter. Dots and marks above and below letters are recognized
   separately as niqud.
4. **Output.** The letters are written out as text, line by line.

When recognition is poor, look at the *Black & White* and *Layout* tabs
first. Most problems start there: letters that are broken or blotchy,
or lines and words that were found wrongly. Fix those with the settings
below before anything else.

## Recognition

How letters are identified and how the text is written out.

**Font model** (`-z NUM`). The set of letter-shape rules used for
recognition. The only working model is **David**, the familiar Hebrew
book typeface (`-z 0`, the default). It also reads many similar
typefaces reasonably well. The engine contains five more model slots
(`-z 1` to `-z 5`), but they are empty. Kobi Zamir added them in 2008;
only the first of them ever had rules, and he removed those a month
later. Choosing one makes every letter come out as `*`, so the program
offers only David.

**Recognize niqud** (on by default; `-n` turns it off). Recognizes the
vowel points (qamats, pataḥ, tsere, segol, ḥiriq, ḥolam, qubuts, shəva,
and the ḥataf vowels), dagesh (so shuruq comes out as vav with dagesh),
and the shin and sin dots. Cantillation marks (ṭe'amim) are not
recognized. Turn it off for
unvocalized text: it is faster, and stray specks are not mistaken for
vowels.

**Linguistics check** (`-A`, off by default). Applies a simple Hebrew
spelling rule while choosing between similar-looking letters. At the end
of a word it prefers the final forms (ך ם ן ף ץ) and punctuation. Inside
a word it prefers the regular forms (כ מ נ פ צ). Turn it on when final
letters are being confused with their regular forms.

**Left-to-right text** (`-R`, off by default). Reads columns and
letters from left to right instead of right to left. hebOCR only knows
Hebrew letter shapes, so this is mainly for experiments.

**Output** (`-h` for HTML). *Plain text* writes UTF-8 text, one line per
line of the page. *HTML* writes an hOCR file: HTML in which every column,
paragraph and line is marked with its position on the page
(`title="bbox x1 y1 x2 y2"`). That is useful for aligning the text with
the image later.

## Image processing

How the scanned image is turned into the clean black-and-white page the
rest of the engine reads.

**Thresholding** (`-T NUM`). How each gray pixel is decided to be ink
(black) or paper (white).

- **Adaptive** (`-T 0`, default). Each pixel is compared with the
  average brightness of the area around it (70 × 70 pixels). This copes
  well with uneven lighting, shadows near the spine, and yellowed paper.
  It is controlled by *Adaptive threshold*.
- **Fixed** (`-T 1`). One cutoff for the whole page. It works for clean,
  evenly lit scans and gives the most predictable results. It is
  controlled by *Threshold*.
- **Fine adaptive** (`-T 2`). A smoother version of adaptive
  thresholding that blends the local cutoffs between areas. Try it when
  adaptive thresholding leaves blotchy patches.

(The original `--help` text called these "normal, none, fine". That was
misleading: "normal" is adaptive, and "none" is the fixed cutoff.)

**Threshold** (`-t NUM`, 1–100; default *Auto* = 60). With *Fixed*
thresholding, pixels darker than this percentage of white count as ink.
**Raise it** for faint or light print, and **lower it** when letters run
together or the paper texture shows up as specks. With the adaptive
types it acts as a floor: anything darker than this always counts as
ink. Kobi's hocr-gtk offered presets of 90 (very light print), 70
(light), 40 (dark) and 20 (very dark).

**Adaptive threshold** (`-a NUM`, 1–100; default *Auto* = 75 for
adaptive, 80 for fine adaptive). With the adaptive types, a pixel counts
as ink if it is darker than this percentage of the average around it.
**Raise it** for faint print, since more pixels then count as ink and
thin strokes are kept. **Lower it** when letters look bloated or
speckled. hocr-gtk's presets were 90, 80, 60 and 50, from light to dark
print. It has no effect with *Fixed* thresholding.

**Scale** (`-s NUM`, 1–9; default *Auto*). Enlarges the image before
thresholding. The engine needs letters roughly 30 pixels tall or more.
With *Auto*, letters under 15 pixels tall are enlarged ×3, and letters
under 30 pixels ×2. Set it yourself if the automatic choice is wrong.
Larger scales are slower.

**Auto scale** (on by default; `-S` turns it off). Lets the engine
choose the scale when *Scale* is *Auto*. Turn it off to process the
image at its original size.

**Rotate** (`-q DEG`). Rotates the image clockwise by this many degrees
(negative for counter-clockwise) before recognition. Use it for pages
scanned sideways or noticeably crooked.

**Auto rotate (deskew)** (on by default; `-Q` turns it off). Detects a
slight tilt in the text lines and straightens the page. Turn it off if
it straightens a page that was already straight, for example one with
slanted decorative elements.

The next four are clean-up filters, applied to the black-and-white
image. Each one changes every stroke by about one pixel. Their effect
depends on resolution: they matter more for small type.

**Separate touching letters** (opening, `-1`). Breaks thin connections
between letters, for example where ink has spread, while keeping letter
shapes. It also removes small specks.

**Join broken letter parts** (closing, `-2`). Closes small gaps and
breaks in strokes, while keeping letter shapes. Use it for worn type or
broken letters.

**Remove specks** (erosion, `-3`). Removes dust and specks by making
everything thinner. It also thins heavy, over-inked letters whose
counters (the holes in letters such as ס and ם) have filled in. Avoid it
on thin or faint print.

**Restore faint letters** (dilation, `-4`). Fills in missing parts of
faint or thin letters by making every stroke thicker. Avoid it on heavy
print, where letters may merge.

The descriptions follow the original author's one-line notes for these
options: "try to separate touching fonts", "try to connect disconnected
font parts", "try to delete artefacts", "try to redraw missing font
parts".

## Page layout

How the page is divided into columns, lines, words and letters.

**Columns** (`-c NUM`; default *Auto*). The number of text columns.
Columns are read from right to left (left to right with *Left-to-right
text*), each from top to bottom. A title or other block wider than its
column is treated as a band across the page: the columns above it are
read first, then the band, then the columns below it. *Auto* (`-c 0`)
detects the columns. Choose a number (`-c 1`, `-c 2`, …) when
detection gets it wrong: for example, a single-column page detected as
two because of a wide gap, or a bilingual edition with parallel columns.
*Free layout* (`-c 255`) does not look for columns at all. It finds
blocks of text wherever they are, which suits irregular layouts with
notes, captions or text wrapped around pictures.

**Word spacing** (`-w NUM`, −3 to 3; default *Normal* = 0). How large a
gap between letters must be to count as a space between words.

- **Positive values** (spaced) treat larger gaps as still being inside a
  word. Use them for widely spaced type, and especially for words
  *letter-spaced for emphasis* (ח ב ת  צ י ו ן). Without this they come
  out as separate single letters.
- **Negative values** (tight) start a new word at smaller gaps. Use them
  for tightly set type where words run together.

hocr-gtk's presets were −3 (very tight), −2 (tight), 2 (spaced) and 3
(very spaced).

**Slicing threshold** (`-x NUM`, 1–250; default *Auto* = 85). When a blob
is too wide to be one letter (see *Slicing width*), the engine cuts it
where a column of pixels has little ink. A cut is made where the ink is
at most this percentage of the line's average. **Higher** values cut
more readily, separating touching letters; **lower** values cut more
carefully, avoiding cuts through wide letters such as ש and ם.

**Slicing width** (`-X NUM`, 50–250; default *Auto* = 150). How wide a
blob must be, as a percentage of the average letter width, before the
engine tries to cut it. **Lower** values make the engine consider
narrower blobs, which is more aggressive. **Higher** values only cut
very wide blobs, which is more careful.

hocr-gtk combined these two as *Font slicing*. *Aggressive* used slicing
threshold 90 with slicing width 50, and *careful* used threshold 2 with
width 254.

**Slicing line leeway** (`-E NUM`, percent of line height; default
*Auto* = 0). When measuring ink to decide where to cut, the engine
normally looks only within the text line itself. Leeway extends that
band above and below the line by this percentage of the line height, so
that tall letters (ל), descenders (ק ן ף ץ) and niqud count too. Try
20–40 if letters are being sliced through their vowel points or tails.

## Diagnostics

Settings that help you understand what the engine did. They do not
improve recognition.

**Draw grid on images** (`-g`). Draws a grid on the *Black & White* and
*Layout* images. It helps you judge letter sizes in pixels.

**Number lines by letter index** (`-j`). Starts each output line with
the index of its first letter in parentheses, e.g. `(0030)`. The numbers
match the letter images saved by `-f` (see below). They are useful when
studying how individual letters were recognized.

## Command-line only

These options exist only in the command-line engine.

**Input and output**

- `-i FILE`, `--image-in`: the image to read. Accepts PNG, JPEG, BMP,
  GIF, TGA, PNM/PGM/PPM, and black-and-white TIFF.
- `-o FILE`, `--text-out`: where to write the text. Without it the text
  goes to the console.
- `-u FILE`, `--data-out`: writes per-letter data, the measurements the
  recognizer used for each letter. For studying and improving the
  engine.
- `-v`, `--version`: print the version and exit.
- `-N`, `--no-gtk`: obsolete. It was needed when the engine could use
  GTK for images; it has no effect in current builds.

**Saving intermediate images.** All are written using the path given
with `-O`. For example, `-O out/page -b` writes `out/page-image-bw.pgm`.

- `-O PATH`, `--images-out-path`: path prefix for saved images.
- `-G TYPE`, `--images-out-type`: image format for saved images
  (`tiff`, `jpeg` or `png`). This only worked in the old GTK builds;
  current builds always save PGM/PPM files.
- `-C`, `--save-copy`: save a copy of the input image.
- `-b`, `--save-bw`: save the black-and-white image. `-B` saves it and
  stops without recognizing.
- `-l`, `--save-layout`: save the layout image. `-L` saves it and stops
  after layout analysis.
- `-f`, `--save-fonts`: save an image of every letter found. `-F` saves
  them and stops.

**Debugging**

- `-d`, `--debug`: print progress information.
- `-D`, `--debug-extra`: print detailed debugging information.
- `-y NUM`, `--font-filter`: debug one of the letter-measurement filters.
- `-Y`, `--font-filter-list`: list the filters available to `-y`.

## Known limitations

- **One font model.** Only the David rules exist; see *Font model*.
  Typefaces that differ much from David recognize poorly.
- **Digits** are in the engine's character set but are rarely recognized
  correctly. Because each line is read from right to left, a number that
  is recognized also comes out reversed (1901 as 1091).
- **Ṭe'amim** (cantillation marks) are not recognized.
- **Hebrew only.** Text in other scripts comes out as Hebrew-looking
  gibberish.
- **Letter size.** The rules expect letters roughly 30–60 pixels tall.
  Very high resolution scans are not necessarily better: scale them down
  if letters are much larger than that.
- **Speed.** The engine works on one letter at a time, using one
  processor core. Noisy images with many specks can take minutes.

## Tips

- **Start from the defaults.** Change one setting at a time and compare
  the *Black & White* and *Layout* tabs.
- **Resolution matters most.** Scans at 300 dpi or more work best. Small
  type, especially with niqud, needs enough pixels for the dots.
- **Letters broken or faint?** Raise *Adaptive threshold* (or switch to
  *Fixed* and raise *Threshold*), or try *Join broken letter parts* or
  *Restore faint letters*.
- **Letters blotchy or joined?** Lower the threshold, or try *Separate
  touching letters* or *Remove specks*.
- **Letter-spaced words split into letters?** Raise *Word spacing* to 2.
- **Columns or lines wrong?** Set *Columns* yourself, or try *Free
  layout*.
- **Two-page spreads** should be split into single pages first.
- **Phone photos** of pages are slow and recognize poorly. Use a scanner
  where possible.
