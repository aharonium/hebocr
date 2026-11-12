/*
 * hebocr-gui: a graphical front end for the hebOCR Hebrew OCR engine
 *
 * Copyright (C) 2026 the hebOCR contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * The GUI runs the hebocr command-line engine as a separate process, so
 * it exposes exactly the engine's options, a slow page can be stopped,
 * and an engine crash cannot take the GUI down with it.
 */

#include "mainwindow.h"
#include "guidewindow.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontComboBox>
#include <QFontDialog>
#include <QFormLayout>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGroupBox>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QGraphicsRectItem>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSlider>
#include <QTextBrowser>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringConverter>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWheelEvent>

#ifndef HEBOCR_VERSION
#define HEBOCR_VERSION "dev"
#endif
#ifndef HEBOCR_BUILD_LABEL
#define HEBOCR_BUILD_LABEL "local build"
#endif

/* first local file in a drag, or an empty string */
static QString
droppedFilePath (const QMimeData *mime)
{
  if (!mime || !mime->hasUrls ())
    return QString ();
  for (const QUrl &url : mime->urls ())
    if (url.isLocalFile ())
      return url.toLocalFile ();
  return QString ();
}

/* ------------------------------------------------------------------ */
/* ImageView                                                           */
/* ------------------------------------------------------------------ */

ImageView::ImageView (QWidget *parent)
  : QGraphicsView (parent),
    m_scene (new QGraphicsScene (this)),
    m_pixmap (new QGraphicsPixmapItem ())
{
  m_scene->addItem (m_pixmap);
  setScene (m_scene);
  /* panning and selecting are handled in the mouse events below */
  setDragMode (QGraphicsView::NoDrag);
  setTransformationAnchor (QGraphicsView::AnchorUnderMouse);
  setRenderHint (QPainter::SmoothPixmapTransform);
  setBackgroundBrush (palette ().window ());
  setAlignment (Qt::AlignCenter);
  setAcceptDrops (true);
  clearImage ();
}

/* QGraphicsView normally hands drags to its scene, which rejects files,
 * so a file dropped on the image never reached the main window. */
void
ImageView::dragEnterEvent (QDragEnterEvent *event)
{
  if (!droppedFilePath (event->mimeData ()).isEmpty ())
    event->acceptProposedAction ();
  else
    event->ignore ();
}

void
ImageView::dragMoveEvent (QDragMoveEvent *event)
{
  if (!droppedFilePath (event->mimeData ()).isEmpty ())
    event->acceptProposedAction ();
  else
    event->ignore ();
}

void
ImageView::dropEvent (QDropEvent *event)
{
  const QString path = droppedFilePath (event->mimeData ());
  if (path.isEmpty ())
    {
      event->ignore ();
      return;
    }
  event->acceptProposedAction ();
  emit fileDropped (path);
}

void
ImageView::setImage (const QImage &image)
{
  removeSelectionItem ();
  for (QGraphicsItem *item : m_scene->items ())
    if (item != m_pixmap)
      {
        m_scene->removeItem (item);
        delete item;
      }
  m_pixmap->setPixmap (QPixmap::fromImage (image));
  m_pixmap->setVisible (true);
  m_scene->setSceneRect (m_pixmap->boundingRect ());
  m_hasImage = !image.isNull ();
  updateCursor ();
  zoomToFit ();
}

void
ImageView::clearImage (const QString &message)
{
  removeSelectionItem ();
  m_pixmap->setPixmap (QPixmap ());
  m_pixmap->setVisible (false);
  for (QGraphicsItem *item : m_scene->items ())
    if (item != m_pixmap)
      {
        m_scene->removeItem (item);
        delete item;
      }
  m_hasImage = false;
  updateCursor ();
  resetTransform ();
  if (!message.isEmpty ())
    {
      QGraphicsTextItem *text = m_scene->addText (message);
      text->setDefaultTextColor (palette ().color (QPalette::PlaceholderText));
      m_scene->setSceneRect (text->boundingRect ());
    }
}

void
ImageView::zoomBy (double factor)
{
  if (!m_hasImage)
    return;
  m_fit = false;
  scale (factor, factor);
}

void
ImageView::zoomToFit ()
{
  if (!m_hasImage)
    return;
  m_fit = true;
  fitInView (m_pixmap, Qt::KeepAspectRatio);
}

void
ImageView::zoomActualSize ()
{
  if (!m_hasImage)
    return;
  m_fit = false;
  resetTransform ();
}

void
ImageView::setSelectable (bool on)
{
  m_selectable = on;
  if (!on)
    clearSelection ();
  updateCursor ();
}

void
ImageView::updateCursor ()
{
  if (m_panning)
    viewport ()->setCursor (Qt::ClosedHandCursor);
  else if (m_spaceDown || (m_hasImage && !m_selectable))
    viewport ()->setCursor (Qt::OpenHandCursor);
  else if (m_hasImage && m_selectable)
    viewport ()->setCursor (Qt::CrossCursor);
  else
    viewport ()->unsetCursor ();
}

QPointF
ImageView::clampToImage (const QPointF &p) const
{
  const QRectF r = m_pixmap->boundingRect ();
  return QPointF (qBound (r.left (), p.x (), r.right ()),
                  qBound (r.top (), p.y (), r.bottom ()));
}

void
ImageView::showSelectionRect (const QRectF &rect)
{
  if (!m_selectionItem)
    {
      QColor c = palette ().color (QPalette::Highlight);
      QPen pen (c, 2, Qt::DashLine);
      pen.setCosmetic (true); /* 2 screen pixels at any zoom */
      c.setAlpha (40);
      m_selectionItem = m_scene->addRect (rect, pen, QBrush (c));
      m_selectionItem->setZValue (1);
    }
  else
    m_selectionItem->setRect (rect);
}

void
ImageView::removeSelectionItem ()
{
  /* the scene deletes it with the other items; just forget it, and
   * report the selection as gone */
  if (m_selectionItem)
    {
      m_scene->removeItem (m_selectionItem);
      delete m_selectionItem;
      m_selectionItem = nullptr;
    }
  m_selecting = false;
  if (!m_selection.isNull ())
    {
      m_selection = QRect ();
      emit selectionChanged (m_selection);
    }
}

void
ImageView::setSelection (const QRect &rect)
{
  const QRect bounded = rect & m_pixmap->boundingRect ().toRect ();
  if (!m_hasImage || bounded.width () < 4 || bounded.height () < 4)
    {
      clearSelection ();
      return;
    }
  showSelectionRect (bounded);
  m_selection = bounded;
  emit selectionChanged (m_selection);
}

void
ImageView::clearSelection ()
{
  removeSelectionItem ();
}

void
ImageView::mousePressEvent (QMouseEvent *event)
{
  const bool pan = event->button () == Qt::MiddleButton
    || (event->button () == Qt::LeftButton && (m_spaceDown || !m_selectable));
  if (pan && m_hasImage)
    {
      m_panning = true;
      m_panStart = event->pos ();
      updateCursor ();
      event->accept ();
      return;
    }
  if (event->button () == Qt::LeftButton && m_selectable && m_hasImage)
    {
      m_selecting = true;
      m_anchor = clampToImage (mapToScene (event->pos ()));
      showSelectionRect (QRectF (m_anchor, m_anchor));
      event->accept ();
      return;
    }
  QGraphicsView::mousePressEvent (event);
}

void
ImageView::mouseMoveEvent (QMouseEvent *event)
{
  if (m_panning)
    {
      const QPoint delta = event->pos () - m_panStart;
      m_panStart = event->pos ();
      horizontalScrollBar ()->setValue (horizontalScrollBar ()->value () - delta.x ());
      verticalScrollBar ()->setValue (verticalScrollBar ()->value () - delta.y ());
      event->accept ();
      return;
    }
  if (m_selecting)
    {
      showSelectionRect (QRectF (m_anchor, clampToImage (mapToScene (event->pos ())))
                           .normalized ());
      event->accept ();
      return;
    }
  QGraphicsView::mouseMoveEvent (event);
}

void
ImageView::mouseReleaseEvent (QMouseEvent *event)
{
  if (m_panning && (event->button () == Qt::MiddleButton || event->button () == Qt::LeftButton))
    {
      m_panning = false;
      updateCursor ();
      event->accept ();
      return;
    }
  if (m_selecting && event->button () == Qt::LeftButton)
    {
      m_selecting = false;
      const QRectF r = m_selectionItem ? m_selectionItem->rect () : QRectF ();
      /* a click without a real drag clears the selection */
      if (r.width () < 4 || r.height () < 4)
        clearSelection ();
      else
        setSelection (r.toAlignedRect ());
      event->accept ();
      return;
    }
  QGraphicsView::mouseReleaseEvent (event);
}

void
ImageView::keyPressEvent (QKeyEvent *event)
{
  if (event->key () == Qt::Key_Space && !event->isAutoRepeat ())
    {
      m_spaceDown = true;
      updateCursor ();
      event->accept ();
      return;
    }
  QGraphicsView::keyPressEvent (event);
}

void
ImageView::keyReleaseEvent (QKeyEvent *event)
{
  if (event->key () == Qt::Key_Space && !event->isAutoRepeat ())
    {
      m_spaceDown = false;
      updateCursor ();
      event->accept ();
      return;
    }
  QGraphicsView::keyReleaseEvent (event);
}

void
ImageView::wheelEvent (QWheelEvent *event)
{
  if (event->modifiers () & Qt::ControlModifier)
    {
      zoomBy (event->angleDelta ().y () > 0 ? 1.25 : 0.8);
      event->accept ();
      return;
    }
  QGraphicsView::wheelEvent (event);
}

void
ImageView::resizeEvent (QResizeEvent *event)
{
  QGraphicsView::resizeEvent (event);
  if (m_fit)
    zoomToFit ();
}

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

namespace
{
  /* a spin box whose minimum means "automatic" (option left out) */
  QSpinBox *
  autoSpin (int min, int max, const QString &autoText = QObject::tr ("Auto"))
  {
    auto *spin = new QSpinBox ();
    spin->setRange (min, max);
    spin->setSpecialValueText (autoText);
    spin->setValue (min);
    return spin;
  }

  QString
  quoteArg (const QString &arg)
  {
    if (arg.isEmpty () || arg.contains (QLatin1Char (' ')))
      return QLatin1Char ('"') + arg + QLatin1Char ('"');
    return arg;
  }

  /* load an engine output image (PGM/PPM, whatever its extension) */
  QImage
  loadEngineImage (const QString &path)
  {
    QImageReader reader (path);
    reader.setDecideFormatFromContent (true);
    return reader.read ();
  }

  void
  setComboData (QComboBox *combo, const QVariant &value)
  {
    const int i = combo->findData (value);
    combo->setCurrentIndex (i >= 0 ? i : 0);
  }

  const char *kDefaultFonts[] = {
    "SBL Hebrew", "Ezra SIL", "Taamey Frank CLM", "Frank Ruehl CLM",
    "David CLM", "Times New Roman", "FreeSerif", "DejaVu Serif"
  };
}

/* ------------------------------------------------------------------ */
/* MainWindow                                                          */
/* ------------------------------------------------------------------ */

MainWindow::MainWindow (QWidget *parent)
  : QMainWindow (parent), m_ticker (new QTimer (this))
{
  setWindowTitle (tr ("hebOCR"));
  setAcceptDrops (true);

  buildActions ();
  buildCentral ();
  buildSettingsDock ();
  buildLogDock ();

  m_status = new QLabel (tr ("Open an image to begin (File ▸ Open, or drag it here)."));
  statusBar ()->addWidget (m_status, 1);

  m_ticker->setInterval (200);
  connect (m_ticker, &QTimer::timeout, this, &MainWindow::tick);

  loadSettings ();
  setRunning (false);
  updateCommandLine ();
}

MainWindow::~MainWindow ()
{
  if (m_process)
    {
      m_process->disconnect (this);
      m_process->kill ();
      m_process->waitForFinished (2000);
    }
  delete m_workDir;
}

void
MainWindow::buildActions ()
{
  QMenu *fileMenu = menuBar ()->addMenu (tr ("&File"));
  QMenu *ocrMenu = menuBar ()->addMenu (tr ("&OCR"));
  QMenu *viewMenu = menuBar ()->addMenu (tr ("&View"));
  QMenu *helpMenu = menuBar ()->addMenu (tr ("&Help"));

  QToolBar *tb = addToolBar (tr ("Main"));
  tb->setObjectName ("mainToolBar");
  tb->setToolButtonStyle (Qt::ToolButtonTextBesideIcon);

  m_openAct = new QAction (style ()->standardIcon (QStyle::SP_DialogOpenButton),
    tr ("&Open Image…"), this);
  m_openAct->setShortcut (QKeySequence::Open);
  connect (m_openAct, &QAction::triggered, this, &MainWindow::openImageDialog);

  m_runAct = new QAction (style ()->standardIcon (QStyle::SP_MediaPlay),
    tr ("&Run OCR"), this);
  m_runAct->setShortcuts ({ QKeySequence (Qt::Key_F5),
                            QKeySequence (Qt::CTRL | Qt::Key_R) });
  connect (m_runAct, &QAction::triggered, this, &MainWindow::runOcr);

  m_stopAct = new QAction (style ()->standardIcon (QStyle::SP_MediaStop),
    tr ("&Stop"), this);
  m_stopAct->setShortcut (Qt::Key_Escape);
  connect (m_stopAct, &QAction::triggered, this, &MainWindow::stopOcr);

  m_saveAct = new QAction (style ()->standardIcon (QStyle::SP_DialogSaveButton),
    tr ("&Save Text…"), this);
  m_saveAct->setShortcut (QKeySequence::Save);
  connect (m_saveAct, &QAction::triggered, this, &MainWindow::saveText);

  m_copyAct = new QAction (tr ("&Copy All Text"), this);
  m_copyAct->setShortcut (QKeySequence (Qt::CTRL | Qt::SHIFT | Qt::Key_C));
  connect (m_copyAct, &QAction::triggered, this, &MainWindow::copyText);

  QAction *quitAct = new QAction (tr ("&Quit"), this);
  quitAct->setShortcut (QKeySequence::Quit);
  connect (quitAct, &QAction::triggered, this, &QWidget::close);

  fileMenu->addAction (m_openAct);
  fileMenu->addAction (m_saveAct);
  fileMenu->addAction (m_copyAct);
  fileMenu->addSeparator ();
  fileMenu->addAction (quitAct);

  ocrMenu->addAction (m_runAct);
  ocrMenu->addAction (m_stopAct);
  ocrMenu->addSeparator ();
  m_clearSelectionAct = ocrMenu->addAction (tr ("&Clear Selection (Read Whole Page)"));
  m_clearSelectionAct->setShortcut (QKeySequence (Qt::CTRL | Qt::Key_D));
  m_clearSelectionAct->setEnabled (false);
  connect (m_clearSelectionAct, &QAction::triggered, this, [this] {
    m_originalView->clearSelection ();
  });
  ocrMenu->addSeparator ();
  QAction *resetAct = ocrMenu->addAction (tr ("Reset Settings to Defaults"));
  connect (resetAct, &QAction::triggered, this, &MainWindow::resetSettings);

  QAction *zoomIn = viewMenu->addAction (tr ("Zoom &In"));
  zoomIn->setShortcut (QKeySequence::ZoomIn);
  QAction *zoomOut = viewMenu->addAction (tr ("Zoom &Out"));
  zoomOut->setShortcut (QKeySequence::ZoomOut);
  QAction *zoomFit = viewMenu->addAction (tr ("&Fit to Window"));
  zoomFit->setShortcut (QKeySequence (Qt::CTRL | Qt::Key_0));
  QAction *zoom100 = viewMenu->addAction (tr ("&Actual Size"));
  zoom100->setShortcut (QKeySequence (Qt::CTRL | Qt::Key_1));
  viewMenu->addSeparator ();
  QAction *wrapAct = viewMenu->addAction (tr ("&Wrap Long Lines"));
  wrapAct->setCheckable (true);
  connect (wrapAct, &QAction::toggled, this, [this] (bool on) {
    m_text->setLineWrapMode (on ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
  });
  QAction *fontAct = viewMenu->addAction (tr ("Text &Font…"));
  connect (fontAct, &QAction::triggered, this, &MainWindow::chooseTextFont);

  auto currentView = [this] () {
    return qobject_cast<ImageView *> (m_imageTabs->currentWidget ());
  };
  connect (zoomIn, &QAction::triggered, this, [=] { if (auto *v = currentView ()) v->zoomBy (1.25); });
  connect (zoomOut, &QAction::triggered, this, [=] { if (auto *v = currentView ()) v->zoomBy (0.8); });
  connect (zoomFit, &QAction::triggered, this, [=] { if (auto *v = currentView ()) v->zoomToFit (); });
  connect (zoom100, &QAction::triggered, this, [=] { if (auto *v = currentView ()) v->zoomActualSize (); });

  QAction *guideAct = helpMenu->addAction (tr ("&Options Guide"));
  guideAct->setShortcut (QKeySequence::HelpContents);
  connect (guideAct, &QAction::triggered, this, &MainWindow::showGuide);
  helpMenu->addSeparator ();
  QAction *thirdPartyAct = helpMenu->addAction (tr ("&Third-Party Software"));
  connect (thirdPartyAct, &QAction::triggered, this, &MainWindow::showThirdParty);
  QAction *aboutQtAct = helpMenu->addAction (tr ("About &Qt"));
  connect (aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt);
  helpMenu->addSeparator ();
  QAction *aboutAct = helpMenu->addAction (tr ("&About hebOCR"));
  connect (aboutAct, &QAction::triggered, this, &MainWindow::about);

  tb->addAction (m_openAct);
  tb->addSeparator ();
  tb->addAction (m_runAct);
  tb->addAction (m_stopAct);
  tb->addSeparator ();
  tb->addAction (m_saveAct);
  tb->addSeparator ();
  tb->addAction (zoomFit);
  tb->addAction (zoom100);
}

void
MainWindow::buildCentral ()
{
  m_imageTabs = new QTabWidget ();
  m_originalView = new ImageView ();
  m_bwView = new ImageView ();
  m_layoutView = new ImageView ();
  m_originalView->clearImage (tr ("No image open"));
  m_bwView->clearImage (tr ("Run OCR to see the processed black-and-white image"));
  m_layoutView->clearImage (tr ("Run OCR to see the detected columns, lines and letters"));
  m_imageTabs->addTab (m_originalView, tr ("Original"));
  m_imageTabs->addTab (m_bwView, tr ("Black && White"));
  m_imageTabs->addTab (m_layoutView, tr ("Layout"));

  m_originalView->setSelectable (true);
  m_originalView->setObjectName ("originalView");
  m_originalView->setToolTip (tr ("Drag to select the area to read; click to clear the selection.\n"
                                  "Middle-drag or Space+drag to pan; Ctrl+wheel to zoom."));
  connect (m_originalView, &ImageView::selectionChanged, this, &MainWindow::selectionChanged);

  for (ImageView *view : { m_originalView, m_bwView, m_layoutView })
    connect (view, &ImageView::fileDropped, this, [this] (const QString &path) {
      if (!isRunning ())
        openImage (path);
    });

  m_text = new QPlainTextEdit ();
  m_text->setObjectName ("ocrText");
  m_text->setPlaceholderText (tr ("Recognized text will appear here. You can edit it before saving."));
  /* keep Hebrew line starts in view when the text reflows (new text,
   * a different size, a resized window) */
  connect (m_text->horizontalScrollBar (), &QScrollBar::rangeChanged,
           this, &MainWindow::scrollTextToLineStarts);
  /* image files dropped on the text open as images; Ctrl+wheel resizes */
  m_text->viewport ()->installEventFilter (this);
  m_text->setTabChangesFocus (false);
  /* one OCR line per text line, so the text lines up with the page */
  m_text->setLineWrapMode (QPlainTextEdit::NoWrap);

  /* text size control, right-aligned above the text */
  m_textSize = new QSlider (Qt::Horizontal);
  m_textSize->setObjectName ("textSize");
  m_textSize->setRange (8, 72);
  m_textSize->setFixedWidth (180);
  m_textSize->setToolTip (tr ("Size of the recognized text (also Ctrl+mouse wheel over the text)."));
  m_textSizeLabel = new QLabel ();
  m_textSizeLabel->setMinimumWidth (m_textSizeLabel->fontMetrics ().horizontalAdvance ("88 pt") + 4);
  connect (m_textSize, &QSlider::valueChanged, this, &MainWindow::setTextSize);

  /* only fonts that cover the Hebrew block are offered */
  m_textFont = new QFontComboBox ();
  m_textFont->setObjectName ("textFont");
  m_textFont->setWritingSystem (QFontDatabase::Hebrew);
  m_textFont->setToolTip (tr ("Font for the recognized text. Only installed fonts that "
                              "support Hebrew are listed."));
  m_textFont->setMaximumWidth (240);
  connect (m_textFont, &QFontComboBox::currentFontChanged, this, &MainWindow::setTextFamily);

  auto *textHeader = new QHBoxLayout ();
  textHeader->setContentsMargins (4, 2, 4, 2);
  textHeader->addStretch (1);
  textHeader->addWidget (new QLabel (tr ("Text size:")));
  textHeader->addWidget (m_textSize);
  textHeader->addWidget (m_textSizeLabel);
  textHeader->addSpacing (8);
  textHeader->addWidget (m_textFont);

  auto *textPane = new QWidget ();
  auto *textLayout = new QVBoxLayout (textPane);
  textLayout->setContentsMargins (0, 0, 0, 0);
  textLayout->setSpacing (0);
  textLayout->addLayout (textHeader);
  textLayout->addWidget (m_text, 1);

  auto *split = new QSplitter (Qt::Horizontal);
  split->addWidget (m_imageTabs);
  split->addWidget (textPane);
  split->setStretchFactor (0, 1);
  split->setStretchFactor (1, 1);
  split->setObjectName ("mainSplitter");
  setCentralWidget (split);
}

void
MainWindow::buildSettingsDock ()
{
  auto *panel = new QWidget ();
  auto *outer = new QVBoxLayout (panel);
  const QString more = tr ("\n\nSee Help ▸ Options Guide (F1) for details.");

  /* recognition */
  auto *recBox = new QGroupBox (tr ("Recognition"));
  recBox->setToolTip (tr ("How letters are identified and how the text is written out."));
  auto *rec = new QFormLayout (recBox);
  m_fontModel = new QComboBox ();
  m_fontModel->addItem (tr ("David"), 0);
  m_fontModel->setToolTip (tr ("The letter-shape rules used for recognition. David, the common "
                               "Hebrew book typeface, is the only model the engine has; it also "
                               "reads many similar typefaces (-z).") + more);
  m_niqud = new QCheckBox (tr ("Recognize niqud"));
  m_niqud->setToolTip (tr ("Recognize niqud (vowel points), dagesh and the shin/sin dots. "
                           "Turn off for unvocalized text (-n).") + more);
  m_linguistics = new QCheckBox (tr ("Linguistics check"));
  m_linguistics->setToolTip (tr ("Prefer final letter forms (ך ם ן ף ץ) at the end of words and "
                                 "regular forms inside words (-A).") + more);
  m_ltr = new QCheckBox (tr ("Left-to-right text"));
  m_ltr->setToolTip (tr ("Read columns and letters from left to right (-R).") + more);
  m_format = new QComboBox ();
  m_format->addItem (tr ("Plain text"), "text");
  m_format->addItem (tr ("HTML (hOCR, with positions)"), "html");
  m_format->setToolTip (tr ("Plain UTF-8 text, or HTML marking the position of every column, "
                            "paragraph and line on the page (-h).") + more);
  rec->addRow (tr ("Font model:"), m_fontModel);
  rec->addRow (m_niqud);
  rec->addRow (m_linguistics);
  rec->addRow (m_ltr);
  rec->addRow (tr ("Output:"), m_format);

  /* image processing */
  auto *imgBox = new QGroupBox (tr ("Image processing"));
  imgBox->setToolTip (tr ("How the scan is turned into the clean black-and-white page the "
                          "engine reads. Check the Black && White tab."));
  auto *img = new QFormLayout (imgBox);
  m_thresholdType = new QComboBox ();
  m_thresholdType->addItem (tr ("Adaptive"), 0);
  m_thresholdType->addItem (tr ("Fixed"), 1);
  m_thresholdType->addItem (tr ("Fine adaptive"), 2);
  m_thresholdType->setToolTip (tr ("How each pixel is judged ink or paper. Adaptive compares it with "
                                   "its surroundings (good for uneven lighting); Fixed uses one cutoff "
                                   "for the whole page; Fine adaptive is a smoother adaptive (-T).") + more);
  m_threshold = autoSpin (0, 100);
  m_threshold->setSuffix (tr ("%"));
  m_threshold->setToolTip (tr ("Fixed thresholding: pixels darker than this count as ink (default 60). "
                               "Raise for faint print, lower if letters run together. With adaptive "
                               "types, anything darker than this is always ink (-t).") + more);
  m_adaptiveThreshold = autoSpin (0, 100);
  m_adaptiveThreshold->setSuffix (tr ("%"));
  m_adaptiveThreshold->setToolTip (tr ("Adaptive thresholding: a pixel is ink if darker than this "
                                       "percentage of its surroundings (default 75, fine 80). Raise for "
                                       "faint print, lower if letters look bloated (-a).") + more);
  m_scale = autoSpin (0, 9);
  m_scale->setPrefix (tr ("×"));
  m_scale->setToolTip (tr ("Enlarge the image before recognition. Auto enlarges ×3 if letters are "
                           "under 15 pixels tall, ×2 if under 30 (-s).") + more);
  m_autoScale = new QCheckBox (tr ("Auto scale"));
  m_autoScale->setToolTip (tr ("Let the engine choose the scale when Scale is Auto (off: -S).") + more);
  m_rotate = new QDoubleSpinBox ();
  m_rotate->setRange (-180.0, 180.0);
  m_rotate->setDecimals (1);
  m_rotate->setSingleStep (0.5);
  m_rotate->setSuffix (tr ("°"));
  m_rotate->setToolTip (tr ("Rotate the image clockwise by this many degrees; negative for "
                            "counter-clockwise (-q).") + more);
  m_autoRotate = new QCheckBox (tr ("Auto rotate (deskew)"));
  m_autoRotate->setToolTip (tr ("Straighten slightly tilted pages (off: -Q).") + more);
  m_opening = new QCheckBox (tr ("Separate touching letters"));
  m_opening->setToolTip (tr ("Break thin connections between letters; also removes small specks "
                             "(morphological opening, -1).") + more);
  m_closing = new QCheckBox (tr ("Join broken letter parts"));
  m_closing->setToolTip (tr ("Close small breaks in worn or broken letters "
                             "(morphological closing, -2).") + more);
  m_erosion = new QCheckBox (tr ("Remove specks"));
  m_erosion->setToolTip (tr ("Remove dust and specks by thinning everything; also thins heavy, "
                             "over-inked letters (erosion, -3).") + more);
  m_dilation = new QCheckBox (tr ("Restore faint letters"));
  m_dilation->setToolTip (tr ("Fill in missing parts of faint letters by thickening every stroke "
                              "(dilation, -4).") + more);
  img->addRow (tr ("Thresholding:"), m_thresholdType);
  img->addRow (tr ("Threshold:"), m_threshold);
  img->addRow (tr ("Adaptive threshold:"), m_adaptiveThreshold);
  img->addRow (tr ("Scale:"), m_scale);
  img->addRow (m_autoScale);
  img->addRow (tr ("Rotate:"), m_rotate);
  img->addRow (m_autoRotate);
  img->addRow (m_opening);
  img->addRow (m_closing);
  img->addRow (m_erosion);
  img->addRow (m_dilation);

  /* page layout */
  auto *segBox = new QGroupBox (tr ("Page layout"));
  segBox->setToolTip (tr ("How the page is divided into columns, lines, words and letters. "
                          "Check the Layout tab."));
  auto *seg = new QFormLayout (segBox);
  m_columns = new QComboBox ();
  m_columns->addItem (tr ("Auto-detect"), 0);
  m_columns->addItem (tr ("1 column"), 1);
  for (int i = 2; i <= 6; ++i)
    m_columns->addItem (tr ("%1 columns").arg (i), i);
  m_columns->addItem (tr ("Free layout"), 255);
  m_columns->setToolTip (tr ("Number of text columns. Free layout finds blocks of text anywhere, "
                             "for irregular pages (-c).") + more);
  m_wordSpacing = new QComboBox ();
  m_wordSpacing->addItem (tr ("Very tight"), -3);
  m_wordSpacing->addItem (tr ("Tight"), -2);
  m_wordSpacing->addItem (tr ("Slightly tight"), -1);
  m_wordSpacing->addItem (tr ("Normal"), 0);
  m_wordSpacing->addItem (tr ("Slightly spaced"), 1);
  m_wordSpacing->addItem (tr ("Spaced"), 2);
  m_wordSpacing->addItem (tr ("Very spaced"), 3);
  m_wordSpacing->setToolTip (tr ("How wide a gap must be to separate words. Choose Spaced for widely "
                                 "spaced type or words letter-spaced for emphasis (ח ב ת); Tight "
                                 "when words run together (-w).") + more);
  m_slicing = autoSpin (0, 250);
  m_slicing->setSuffix (tr ("%"));
  m_slicing->setToolTip (tr ("Where to cut blobs that are too wide to be one letter: higher cuts "
                             "more readily (default 85) (-x).") + more);
  m_slicingWidth = autoSpin (49, 250);
  m_slicingWidth->setSuffix (tr ("%"));
  m_slicingWidth->setToolTip (tr ("How wide a blob must be, compared with an average letter, before "
                                  "it is cut: lower is more aggressive (default 150) (-X).") + more);
  m_slicingLine = autoSpin (0, 90);
  m_slicingLine->setSuffix (tr ("%"));
  m_slicingLine->setToolTip (tr ("Also count ink this far above and below the line when deciding "
                                 "where to cut, so niqud and tall letters are respected (-E).") + more);
  seg->addRow (tr ("Columns:"), m_columns);
  seg->addRow (tr ("Word spacing:"), m_wordSpacing);
  seg->addRow (tr ("Slicing threshold:"), m_slicing);
  seg->addRow (tr ("Slicing width:"), m_slicingWidth);
  seg->addRow (tr ("Slicing line leeway:"), m_slicingLine);

  /* diagnostics */
  auto *dbgBox = new QGroupBox (tr ("Diagnostics"));
  dbgBox->setToolTip (tr ("Help you see what the engine did; they don't improve recognition."));
  auto *dbg = new QFormLayout (dbgBox);
  m_grid = new QCheckBox (tr ("Draw grid on images"));
  m_grid->setToolTip (tr ("Draw a grid on the Black && White and Layout images (-g).") + more);
  m_fontNumbers = new QCheckBox (tr ("Number lines by letter index"));
  m_fontNumbers->setToolTip (tr ("Start each line with the index of its first letter, "
                                 "e.g. (0030) (-j).") + more);
  dbg->addRow (m_grid);
  dbg->addRow (m_fontNumbers);

  auto *reset = new QPushButton (tr ("Reset to defaults"));
  connect (reset, &QPushButton::clicked, this, &MainWindow::resetSettings);

  outer->addWidget (recBox);
  outer->addWidget (imgBox);
  outer->addWidget (segBox);
  outer->addWidget (dbgBox);
  outer->addWidget (reset);
  outer->addStretch (1);

  auto *scroll = new QScrollArea ();
  scroll->setWidget (panel);
  scroll->setWidgetResizable (true);
  scroll->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);

  auto *dock = new QDockWidget (tr ("Settings"), this);
  dock->setObjectName ("settingsDock");
  dock->setWidget (scroll);
  dock->setFeatures (QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
  addDockWidget (Qt::LeftDockWidgetArea, dock);
  scroll->setMinimumWidth (panel->sizeHint ().width () + 20);

  /* keep the command line preview current */
  auto update = [this] { updateCommandLine (); };
  for (QComboBox *c : { m_fontModel, m_format, m_thresholdType, m_columns, m_wordSpacing })
    connect (c, &QComboBox::currentIndexChanged, this, update);
  for (QCheckBox *c : { m_niqud, m_linguistics, m_ltr, m_autoScale, m_autoRotate,
                        m_opening, m_closing, m_erosion, m_dilation, m_grid, m_fontNumbers })
    connect (c, &QCheckBox::toggled, this, update);
  for (QSpinBox *sp : { m_threshold, m_adaptiveThreshold, m_scale,
                        m_slicing, m_slicingWidth, m_slicingLine })
    connect (sp, &QSpinBox::valueChanged, this, update);
  connect (m_rotate, &QDoubleSpinBox::valueChanged, this, update);
  connect (m_ltr, &QCheckBox::toggled, this, &MainWindow::applyTextDirection);
  connect (m_thresholdType, &QComboBox::currentIndexChanged,
           this, &MainWindow::updateThresholdControls);

  /* names for automated tests */
  m_fontModel->setObjectName ("fontModel");
  m_thresholdType->setObjectName ("thresholdType");
  m_adaptiveThreshold->setObjectName ("adaptiveThreshold");
  m_columns->setObjectName ("columns");
  m_wordSpacing->setObjectName ("wordSpacing");
}

void
MainWindow::updateThresholdControls ()
{
  /* the adaptive percentage means nothing with a fixed threshold */
  m_adaptiveThreshold->setEnabled (m_thresholdType->currentData ().toInt () != 1);
}

void
MainWindow::buildLogDock ()
{
  /* one row: the hebocr command for the current settings, and Copy */
  auto *w = new QWidget ();
  auto *row = new QHBoxLayout (w);
  row->setContentsMargins (6, 4, 6, 4);
  m_commandLine = new QLineEdit ();
  m_commandLine->setObjectName ("commandLine");
  m_commandLine->setReadOnly (true);
  m_commandLine->setFont (QFontDatabase::systemFont (QFontDatabase::FixedFont));
  m_commandLine->setToolTip (tr ("The hebocr command that does the same as the current settings. "
                                 "Use it in scripts or a terminal."));
  row->addWidget (m_commandLine, 1);
  auto *copyCmd = new QPushButton (tr ("Copy"));
  copyCmd->setToolTip (tr ("Copy the command to the clipboard"));
  connect (copyCmd, &QPushButton::clicked, this, [this] {
    QApplication::clipboard ()->setText (m_commandLine->text ());
    m_status->setText (tr ("Command copied to the clipboard."));
  });
  row->addWidget (copyCmd);
  w->setSizePolicy (QSizePolicy::Preferred, QSizePolicy::Fixed);

  auto *dock = new QDockWidget (tr ("Equivalent Command"), this);
  dock->setObjectName ("commandDock");
  dock->setWidget (w);
  dock->setFeatures (QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                     | QDockWidget::DockWidgetClosable);
  addDockWidget (Qt::BottomDockWidgetArea, dock);
  QMenu *viewMenu = nullptr;
  for (QAction *a : menuBar ()->actions ())
    if (a->menu () && a->text ().contains (tr ("View")))
      viewMenu = a->menu ();
  if (viewMenu)
    {
      viewMenu->addSeparator ();
      viewMenu->addAction (dock->toggleViewAction ());
    }
}

/* ------------------------------------------------------------------ */
/* settings                                                            */
/* ------------------------------------------------------------------ */

void
MainWindow::resetSettings ()
{
  m_loadingSettings = true;
  m_fontModel->setCurrentIndex (0);
  m_niqud->setChecked (true);
  m_linguistics->setChecked (false);
  m_ltr->setChecked (false);
  m_format->setCurrentIndex (0);
  m_thresholdType->setCurrentIndex (0);
  m_threshold->setValue (m_threshold->minimum ());
  m_adaptiveThreshold->setValue (m_adaptiveThreshold->minimum ());
  m_scale->setValue (m_scale->minimum ());
  m_autoScale->setChecked (true);
  m_rotate->setValue (0.0);
  m_autoRotate->setChecked (true);
  m_opening->setChecked (false);
  m_closing->setChecked (false);
  m_erosion->setChecked (false);
  m_dilation->setChecked (false);
  setComboData (m_columns, 0);
  setComboData (m_wordSpacing, 0);
  m_slicing->setValue (m_slicing->minimum ());
  m_slicingWidth->setValue (m_slicingWidth->minimum ());
  m_slicingLine->setValue (m_slicingLine->minimum ());
  m_grid->setChecked (false);
  m_fontNumbers->setChecked (false);
  m_loadingSettings = false;
  applyTextDirection ();
  updateThresholdControls ();
  updateCommandLine ();
}

void
MainWindow::loadSettings ()
{
  resetSettings ();
  QSettings s;
  m_loadingSettings = true;
  restoreGeometry (s.value ("window/geometry").toByteArray ());
  restoreState (s.value ("window/state").toByteArray ());
  if (auto *split = findChild<QSplitter *> ("mainSplitter"))
    split->restoreState (s.value ("window/splitter").toByteArray ());

  s.beginGroup ("ocr2");
  m_niqud->setChecked (s.value ("nikud", true).toBool ());
  m_linguistics->setChecked (s.value ("linguistics", false).toBool ());
  m_ltr->setChecked (s.value ("ltr", false).toBool ());
  m_format->setCurrentIndex (s.value ("format", 0).toInt ());
  m_thresholdType->setCurrentIndex (s.value ("thresholdType", 0).toInt ());
  m_threshold->setValue (s.value ("threshold", m_threshold->minimum ()).toInt ());
  m_adaptiveThreshold->setValue (s.value ("adaptiveThreshold", m_adaptiveThreshold->minimum ()).toInt ());
  m_scale->setValue (s.value ("scale", m_scale->minimum ()).toInt ());
  m_autoScale->setChecked (s.value ("autoScale", true).toBool ());
  m_rotate->setValue (s.value ("rotate", 0.0).toDouble ());
  m_autoRotate->setChecked (s.value ("autoRotate", true).toBool ());
  m_opening->setChecked (s.value ("opening", false).toBool ());
  m_closing->setChecked (s.value ("closing", false).toBool ());
  m_erosion->setChecked (s.value ("erosion", false).toBool ());
  m_dilation->setChecked (s.value ("dilation", false).toBool ());
  setComboData (m_columns, s.value ("columns", 0).toInt ());
  setComboData (m_wordSpacing, s.value ("wordSpacing", 0).toInt ());
  m_slicing->setValue (s.value ("slicing", m_slicing->minimum ()).toInt ());
  m_slicingWidth->setValue (s.value ("slicingWidth", m_slicingWidth->minimum ()).toInt ());
  m_slicingLine->setValue (s.value ("slicingLine", m_slicingLine->minimum ()).toInt ());
  m_grid->setChecked (s.value ("grid", false).toBool ());
  m_fontNumbers->setChecked (s.value ("fontNumbers", false).toBool ());
  s.endGroup ();

  QFont font;
  if (s.contains ("text/font"))
    font.fromString (s.value ("text/font").toString ());
  else
    {
      const QStringList families = QFontDatabase::families ();
      for (const char *name : kDefaultFonts)
        if (families.contains (QString::fromLatin1 (name)))
          {
            font = QFont (QString::fromLatin1 (name));
            break;
          }
      font.setPointSize (18);
    }
  m_text->setFont (font);
  m_loadingSettings = false;
  setTextFamily (font);
  setTextSize (font.pointSize () > 0 ? font.pointSize () : 18);
  applyTextDirection ();
  updateThresholdControls ();
}

void
MainWindow::saveSettings ()
{
  QSettings s;
  s.setValue ("window/geometry", saveGeometry ());
  s.setValue ("window/state", saveState ());
  if (auto *split = findChild<QSplitter *> ("mainSplitter"))
    s.setValue ("window/splitter", split->saveState ());
  s.setValue ("text/font", m_text->font ().toString ());

  s.beginGroup ("ocr2");
  s.setValue ("nikud", m_niqud->isChecked ());
  s.setValue ("linguistics", m_linguistics->isChecked ());
  s.setValue ("ltr", m_ltr->isChecked ());
  s.setValue ("format", m_format->currentIndex ());
  s.setValue ("thresholdType", m_thresholdType->currentIndex ());
  s.setValue ("threshold", m_threshold->value ());
  s.setValue ("adaptiveThreshold", m_adaptiveThreshold->value ());
  s.setValue ("scale", m_scale->value ());
  s.setValue ("autoScale", m_autoScale->isChecked ());
  s.setValue ("rotate", m_rotate->value ());
  s.setValue ("autoRotate", m_autoRotate->isChecked ());
  s.setValue ("opening", m_opening->isChecked ());
  s.setValue ("closing", m_closing->isChecked ());
  s.setValue ("erosion", m_erosion->isChecked ());
  s.setValue ("dilation", m_dilation->isChecked ());
  s.setValue ("columns", m_columns->currentData ().toInt ());
  s.setValue ("wordSpacing", m_wordSpacing->currentData ().toInt ());
  s.setValue ("slicing", m_slicing->value ());
  s.setValue ("slicingWidth", m_slicingWidth->value ());
  s.setValue ("slicingLine", m_slicingLine->value ());
  s.setValue ("grid", m_grid->isChecked ());
  s.setValue ("fontNumbers", m_fontNumbers->isChecked ());
  s.endGroup ();
}

void
MainWindow::applyTextDirection ()
{
  const bool ltr = m_ltr->isChecked ();
  m_text->setLayoutDirection (ltr ? Qt::LeftToRight : Qt::RightToLeft);
  QTextOption opt = m_text->document ()->defaultTextOption ();
  opt.setTextDirection (ltr ? Qt::LeftToRight : Qt::RightToLeft);
  /* Qt mirrors AlignLeft/AlignRight in right-to-left mode unless they
   * are marked absolute */
  opt.setAlignment (Qt::AlignAbsolute | (ltr ? Qt::AlignLeft : Qt::AlignRight));
  m_text->document ()->setDefaultTextOption (opt);
  scrollTextToLineStarts ();
}

void
MainWindow::scrollTextToLineStarts ()
{
  /* without wrapping, long lines scroll sideways; show the edge where
   * lines begin (the right edge for Hebrew) */
  if (!m_ltr)
    return; /* still building the window */
  QScrollBar *bar = m_text->horizontalScrollBar ();
  bar->setValue (m_ltr->isChecked () ? bar->minimum () : bar->maximum ());
}

/* ------------------------------------------------------------------ */
/* engine invocation                                                   */
/* ------------------------------------------------------------------ */

QString
MainWindow::enginePath () const
{
#ifdef Q_OS_WIN
  const QString exe = QStringLiteral ("hebocr.exe");
#else
  const QString exe = QStringLiteral ("hebocr");
#endif
  const QString local = QDir (QCoreApplication::applicationDirPath ()).filePath (exe);
  if (QFileInfo::exists (local))
    return local;
  return QStandardPaths::findExecutable (QStringLiteral ("hebocr"));
}

QStringList
MainWindow::engineArguments (bool forDisplay) const
{
  QStringList a;
  a << "-i" << (forDisplay ? (m_imagePath.isEmpty () ? tr ("image.png")
                                                     : QFileInfo (m_imagePath).fileName ())
                           : QDir::toNativeSeparators (m_engineInput));
  a << "-o" << (forDisplay ? QStringLiteral ("output.txt")
                           : QDir::toNativeSeparators (m_workDir->filePath ("output.txt")));

  if (m_format->currentData ().toString () == "html")
    a << "-h";
  if (m_fontModel->currentData ().toInt () > 0)
    a << "-z" << QString::number (m_fontModel->currentData ().toInt ());
  if (!m_niqud->isChecked ())
    a << "-n";
  if (m_linguistics->isChecked ())
    a << "-A";
  if (m_ltr->isChecked ())
    a << "-R";

  if (m_thresholdType->currentData ().toInt () != 0)
    a << "-T" << QString::number (m_thresholdType->currentData ().toInt ());
  if (m_threshold->value () > m_threshold->minimum ())
    a << "-t" << QString::number (m_threshold->value ());
  if (m_adaptiveThreshold->value () > m_adaptiveThreshold->minimum ())
    a << "-a" << QString::number (m_adaptiveThreshold->value ());
  if (m_scale->value () > m_scale->minimum ())
    a << "-s" << QString::number (m_scale->value ());
  if (!m_autoScale->isChecked ())
    a << "-S";
  if (m_rotate->value () != 0.0)
    a << "-q" << QString::number (m_rotate->value ());
  if (!m_autoRotate->isChecked ())
    a << "-Q";
  if (m_opening->isChecked ())
    a << "-1";
  if (m_closing->isChecked ())
    a << "-2";
  if (m_erosion->isChecked ())
    a << "-3";
  if (m_dilation->isChecked ())
    a << "-4";

  if (m_columns->currentData ().toInt () > 0)
    a << "-c" << QString::number (m_columns->currentData ().toInt ());
  if (m_slicing->value () > m_slicing->minimum ())
    a << "-x" << QString::number (m_slicing->value ());
  if (m_slicingWidth->value () > m_slicingWidth->minimum ())
    a << "-X" << QString::number (m_slicingWidth->value ());
  if (m_slicingLine->value () > m_slicingLine->minimum ())
    a << "-E" << QString::number (m_slicingLine->value ());
  if (m_wordSpacing->currentData ().toInt () != 0)
    a << "-w" << QString::number (m_wordSpacing->currentData ().toInt ());

  if (m_grid->isChecked ())
    a << "-g";
  if (m_fontNumbers->isChecked ())
    a << "-j";

  /* the GUI always asks for the diagnostic images, but they are not
   * part of the command a user would normally type */
  if (!forDisplay)
    a << "-O" << QDir::toNativeSeparators (m_workDir->filePath ("page"))
      << "-b" << "-l";
  return a;
}

void
MainWindow::selectionChanged (const QRect &rect)
{
  m_selection = rect;
  m_clearSelectionAct->setEnabled (!rect.isNull ());
  if (!rect.isNull ())
    m_status->setText (tr ("Selected area: %1 × %2 pixels at (%3, %4). Run OCR reads only this "
                           "area; Ctrl+D or a click clears it.")
                         .arg (rect.width ()).arg (rect.height ()).arg (rect.x ()).arg (rect.y ()));
  else if (!m_imagePath.isEmpty () && !isRunning ())
    m_status->setText (tr ("No area selected: Run OCR reads the whole page."));
  updateCommandLine ();
}

void
MainWindow::updateCommandLine ()
{
  if (m_loadingSettings || !m_commandLine)
    return;
  QStringList parts { QStringLiteral ("hebocr") };
  for (const QString &arg : engineArguments (true))
    parts << quoteArg (arg);
  QString command = parts.join (QLatin1Char (' '));
  /* the engine has no cropping option: the program crops the image first */
  if (!m_selection.isNull ())
    command += tr ("    # selected area only (%1x%2 at %3,%4): the program crops the image first")
                 .arg (m_selection.width ()).arg (m_selection.height ())
                 .arg (m_selection.x ()).arg (m_selection.y ());
  m_commandLine->setText (command);
}

void
MainWindow::runOcr ()
{
  if (isRunning ())
    return;
  if (m_imagePath.isEmpty ())
    {
      openImageDialog ();
      if (m_imagePath.isEmpty ())
        return;
    }
  if (!m_text->toPlainText ().isEmpty () && m_text->document ()->isModified ()
      && !maybeDiscardEdits ())
    return;

  const QString engine = enginePath ();
  if (engine.isEmpty ())
    {
      QMessageBox::critical (this, tr ("hebOCR"),
        tr ("The hebocr engine was not found. It should be in the same folder as this program."));
      return;
    }

  delete m_workDir;
  m_workDir = new QTemporaryDir ();
  if (!m_workDir->isValid ())
    {
      QMessageBox::critical (this, tr ("hebOCR"), tr ("Could not create a temporary folder."));
      return;
    }

  /* The engine reads PNG, JPEG, BMP, GIF, TGA and PNM itself (and only
   * black-and-white TIFF). Anything else, including grayscale or color
   * TIFF from scanners, is handed over as a temporary grayscale PNG. */
  static const QStringList engineFormats = {
    "png", "jpg", "jpeg", "bmp", "gif", "tga", "pgm", "ppm", "pnm", "pbm"
  };
  m_engineInput = m_imagePath;
  if (!m_selection.isNull ())
    {
      /* read only the selected area */
      m_engineInput = m_workDir->filePath ("input-area.png");
      if (!m_image.copy (m_selection).convertToFormat (QImage::Format_Grayscale8)
             .save (m_engineInput, "PNG"))
        {
          QMessageBox::critical (this, tr ("hebOCR"),
            tr ("Could not prepare the selected area for the engine."));
          return;
        }
    }
  else if (!engineFormats.contains (QFileInfo (m_imagePath).suffix ().toLower ()))
    {
      m_engineInput = m_workDir->filePath ("input.png");
      if (!m_image.convertToFormat (QImage::Format_Grayscale8).save (m_engineInput, "PNG"))
        {
          QMessageBox::critical (this, tr ("hebOCR"),
            tr ("Could not prepare the image for the engine."));
          return;
        }
    }

  m_process = new QProcess (this);
  m_process->setProcessChannelMode (QProcess::MergedChannels);
  m_process->setWorkingDirectory (m_workDir->path ());
  connect (m_process, &QProcess::finished, this, &MainWindow::processFinished);
  connect (m_process, &QProcess::errorOccurred, this, &MainWindow::processError);
  connect (m_process, &QProcess::readyRead, this, [this] {
    m_engineOutput += QString::fromLocal8Bit (m_process->readAll ());
  });

  m_engineOutput.clear ();
  m_bwView->clearImage (tr ("Working…"));
  m_layoutView->clearImage (tr ("Working…"));

  m_elapsed.start ();
  setRunning (true);
  m_process->start (engine, engineArguments (false));
}

void
MainWindow::stopOcr ()
{
  if (!isRunning ())
    return;
  m_process->kill ();
  m_status->setText (tr ("Stopped."));
}

void
MainWindow::showImageTab (int index)
{
  m_imageTabs->setCurrentIndex (index);
}

bool
MainWindow::isRunning () const
{
  return m_process && m_process->state () != QProcess::NotRunning;
}

void
MainWindow::processFinished (int exitCode, QProcess::ExitStatus status)
{
  const double secs = m_elapsed.elapsed () / 1000.0;
  m_engineOutput += QString::fromLocal8Bit (m_process->readAll ());
  m_process->deleteLater ();
  m_process = nullptr;
  setRunning (false);

  if (status != QProcess::NormalExit)
    {
      m_status->setText (tr ("OCR stopped after %1 s.").arg (secs, 0, 'f', 1));
      m_bwView->clearImage (tr ("No result"));
      m_layoutView->clearImage (tr ("No result"));
      return;
    }

  const QString bw = m_workDir->filePath ("page-image-bw.pgm");
  const QString layout = m_workDir->filePath ("page-image-layout.pgm");
  QImage bwImage = loadEngineImage (bw);
  QImage layoutImage = loadEngineImage (layout);
  if (!bwImage.isNull ())
    m_bwView->setImage (bwImage);
  else
    m_bwView->clearImage (tr ("No image produced"));
  if (!layoutImage.isNull ())
    m_layoutView->setImage (layoutImage);
  else
    m_layoutView->clearImage (tr ("No image produced"));

  QFile out (m_workDir->filePath ("output.txt"));
  if (exitCode != 0 || !out.open (QIODevice::ReadOnly))
    {
      reportEngineError (exitCode != 0
        ? tr ("The engine could not recognize this image (exit code %1).").arg (exitCode)
        : tr ("The engine finished but produced no text file."));
      return;
    }
  const QString text = QString::fromUtf8 (out.readAll ());
  m_text->setPlainText (text);
  m_text->document ()->setModified (false);
  QTimer::singleShot (0, this, &MainWindow::scrollTextToLineStarts);
  const int lines = text.count (QLatin1Char ('\n'));
  m_status->setText ((m_selection.isNull ()
                        ? tr ("Done in %1 s — %2 lines recognized from %3.")
                        : tr ("Done in %1 s — %2 lines recognized from the selected area of %3."))
                       .arg (secs, 0, 'f', 1)
                       .arg (lines)
                       .arg (QFileInfo (m_imagePath).fileName ()));
}

void
MainWindow::processError (QProcess::ProcessError error)
{
  if (error == QProcess::FailedToStart)
    {
      setRunning (false);
      m_status->setText (tr ("Could not start the hebocr engine."));
      QMessageBox::critical (this, tr ("hebOCR"),
        tr ("Could not start the hebocr engine:\n%1")
          .arg (QDir::toNativeSeparators (enginePath ())));
      m_process->deleteLater ();
      m_process = nullptr;
    }
}

void
MainWindow::tick ()
{
  if (isRunning ())
    m_status->setText (tr ("Recognizing… %1 s (press Esc to stop)")
                         .arg (m_elapsed.elapsed () / 1000.0, 0, 'f', 1));
}

void
MainWindow::setRunning (bool running)
{
  m_runAct->setEnabled (!running);
  m_stopAct->setEnabled (running);
  m_openAct->setEnabled (!running);
  if (running)
    {
      m_ticker->start ();
      QApplication::setOverrideCursor (Qt::BusyCursor);
    }
  else
    {
      m_ticker->stop ();
      if (QApplication::overrideCursor ())
        QApplication::restoreOverrideCursor ();
    }
}

void
MainWindow::reportEngineError (const QString &summary)
{
  m_status->setText (summary);
  QMessageBox box (QMessageBox::Warning, tr ("hebOCR"), summary, QMessageBox::Ok, this);
  QString details = tr ("Command:\n%1\n\nEngine messages:\n%2")
                      .arg (m_commandLine->text (),
                            m_engineOutput.trimmed ().isEmpty () ? tr ("(none)")
                                                                 : m_engineOutput.trimmed ());
  box.setDetailedText (details);
  box.exec ();
}

/* ------------------------------------------------------------------ */
/* files                                                               */
/* ------------------------------------------------------------------ */

bool
MainWindow::openImage (const QString &path)
{
  QImageReader reader (path);
  reader.setAutoTransform (true);
  QImage image = reader.read ();
  if (image.isNull ())
    {
      QMessageBox::warning (this, tr ("hebOCR"),
        tr ("Could not open %1:\n%2").arg (QDir::toNativeSeparators (path), reader.errorString ()));
      return false;
    }
  /* the engine runs in a temporary folder, so always pass an absolute path */
  m_imagePath = QFileInfo (path).absoluteFilePath ();
  m_image = image;
  m_originalView->setImage (image);
  m_bwView->clearImage (tr ("Run OCR to see the processed black-and-white image"));
  m_layoutView->clearImage (tr ("Run OCR to see the detected columns, lines and letters"));
  m_imageTabs->setCurrentWidget (m_originalView);
  setWindowTitle (tr ("%1 — hebOCR").arg (QFileInfo (path).fileName ()));
  m_status->setText (tr ("%1 × %2 pixels. Press F5 to run OCR.")
                       .arg (image.width ()).arg (image.height ()));
  QSettings ().setValue ("files/lastDir", QFileInfo (path).absolutePath ());
  updateCommandLine ();
  return true;
}

void
MainWindow::openImageDialog ()
{
  const QString dir = QSettings ().value ("files/lastDir",
    QStandardPaths::writableLocation (QStandardPaths::PicturesLocation)).toString ();
  const QString path = QFileDialog::getOpenFileName (this, tr ("Open Image"), dir,
    tr ("Images (*.png *.jpg *.jpeg *.tif *.tiff *.bmp *.gif *.pgm *.ppm *.pnm *.pbm);;All files (*)"));
  if (!path.isEmpty ())
    openImage (path);
}

void
MainWindow::saveText ()
{
  if (m_text->toPlainText ().isEmpty ())
    return;
  const bool html = m_format->currentData ().toString () == "html";
  QString suggested = m_imagePath.isEmpty ()
    ? QStringLiteral ("output")
    : QFileInfo (m_imagePath).completeBaseName ();
  suggested += html ? QStringLiteral (".html") : QStringLiteral (".txt");
  const QString dir = QSettings ().value ("files/lastSaveDir",
    m_imagePath.isEmpty () ? QDir::homePath () : QFileInfo (m_imagePath).absolutePath ()).toString ();
  const QString path = QFileDialog::getSaveFileName (this, tr ("Save Text"),
    QDir (dir).filePath (suggested),
    html ? tr ("HTML (*.html *.htm);;All files (*)")
         : tr ("Text (*.txt);;All files (*)"));
  if (path.isEmpty ())
    return;
  QFile f (path);
  if (!f.open (QIODevice::WriteOnly | QIODevice::Truncate))
    {
      QMessageBox::warning (this, tr ("hebOCR"), tr ("Could not save %1.").arg (path));
      return;
    }
  QTextStream ts (&f);
  ts.setEncoding (QStringConverter::Utf8);
  ts << m_text->toPlainText ();
  m_text->document ()->setModified (false);
  QSettings ().setValue ("files/lastSaveDir", QFileInfo (path).absolutePath ());
  m_status->setText (tr ("Saved %1").arg (QDir::toNativeSeparators (path)));
}

void
MainWindow::copyText ()
{
  QApplication::clipboard ()->setText (m_text->toPlainText ());
  m_status->setText (tr ("Text copied to the clipboard."));
}

void
MainWindow::chooseTextFont ()
{
  bool ok = false;
  QFont font = QFontDialog::getFont (&ok, m_text->font (), this, tr ("Text Font"));
  if (ok)
    {
      m_text->setFont (font);
      setTextFamily (font);
      setTextSize (font.pointSize ());
    }
}

void
MainWindow::setTextSize (int points)
{
  points = qBound (m_textSize->minimum (), points, m_textSize->maximum ());
  QFont font = m_text->font ();
  if (font.pointSize () != points)
    {
      font.setPointSize (points);
      m_text->setFont (font);
    }
  if (m_textSize->value () != points)
    {
      const QSignalBlocker block (m_textSize);
      m_textSize->setValue (points);
    }
  m_textSizeLabel->setText (tr ("%1 pt").arg (points));
}

void
MainWindow::setTextFamily (const QFont &chosen)
{
  QFont font = m_text->font ();
  if (font.family () != chosen.family ())
    {
      font.setFamily (chosen.family ());
      m_text->setFont (font);
    }
  if (m_textFont->currentFont ().family () != chosen.family ())
    {
      const QSignalBlocker block (m_textFont);
      m_textFont->setCurrentFont (chosen);
    }
}

void
MainWindow::showGuide ()
{
  static QPointer<GuideWindow> guide;
  if (!guide)
    {
      guide = new GuideWindow (this);
      guide->setAttribute (Qt::WA_DeleteOnClose);
    }
  guide->show ();
  guide->raise ();
  guide->activateWindow ();
}

bool
MainWindow::maybeDiscardEdits ()
{
  return QMessageBox::question (this, tr ("hebOCR"),
    tr ("You have edited the recognized text. Replace it with a new OCR result?"),
    QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Yes;
}

void
MainWindow::about ()
{
  QMessageBox box (this);
  box.setWindowTitle (tr ("About hebOCR"));
  box.setIconPixmap (windowIcon ().pixmap (64, 64));
  box.setTextFormat (Qt::RichText);
  box.setText (tr ("<h3>hebOCR %1</h3><p><small>%2</small></p>"
    "<p>A free Hebrew OCR engine with niqud recognition.</p>")
      .arg (QStringLiteral (HEBOCR_VERSION), QStringLiteral (HEBOCR_BUILD_LABEL)));
  box.setInformativeText (tr (
    "<p>Engine by Kobi (Yaacov) Zamir, with contributions by Diego Iastrubni, "
    "Oron Peled, Yuval Tanny, Mikael Ylikoski, and Lior Kaplan and the Debian "
    "Hebrew Packaging Team.</p>"
    "<p>Revived in 2026 by Aharon N. Varady for "
    "<a href=\"https://opensiddur.org\">the Open Siddur Project</a>.</p>"
    "<p>Free software under the GNU General Public License, version 3 or later. "
    "Built with Qt, GLib, LibTIFF and stb_image: see Help ▸ Third-Party Software.</p>"
    "<p><a href=\"https://github.com/aharonium/hebocr\">github.com/aharonium/hebocr</a></p>"));
  box.setStandardButtons (QMessageBox::Ok);
  box.exec ();
}

void
MainWindow::showThirdParty ()
{
  QDialog dialog (this);
  dialog.setWindowTitle (tr ("Third-Party Software"));
  auto *browser = new QTextBrowser ();
  browser->setOpenExternalLinks (true);
  QFile f (QStringLiteral (":/docs/third-party.md"));
  QString text = f.open (QIODevice::ReadOnly) ? QString::fromUtf8 (f.readAll ())
                                              : tr ("The notice is missing from this build.");
  /* in the Windows package, link the generated full list */
  /* Windows: next to the program; Linux AppImage: usr/share/doc/hebocr */
  const QDir appDir (QCoreApplication::applicationDirPath ());
  QString notices = appDir.filePath ("THIRD-PARTY-NOTICES.txt");
  if (!QFileInfo::exists (notices))
    notices = appDir.filePath ("../share/doc/hebocr/THIRD-PARTY-NOTICES.txt");
  if (!QFileInfo::exists (notices)) /* macOS: hebOCR.app/Contents/Resources */
    notices = appDir.filePath ("../Resources/THIRD-PARTY-NOTICES.txt");
  if (QFileInfo::exists (notices))
    text += tr ("\n\n[Open the full list of bundled libraries (THIRD-PARTY-NOTICES.txt)](%1)")
              .arg (QUrl::fromLocalFile (notices).toString ());
  browser->setMarkdown (text);
  auto *buttons = new QDialogButtonBox (QDialogButtonBox::Close);
  connect (buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  auto *layout = new QVBoxLayout (&dialog);
  layout->addWidget (browser);
  layout->addWidget (buttons);
  dialog.resize (720, 640);
  dialog.exec ();
}

/* ------------------------------------------------------------------ */
/* window events                                                       */
/* ------------------------------------------------------------------ */

void
MainWindow::closeEvent (QCloseEvent *event)
{
  if (m_text->document ()->isModified () && !m_text->toPlainText ().isEmpty ())
    {
      const auto answer = QMessageBox::question (this, tr ("hebOCR"),
        tr ("The recognized text has unsaved edits. Quit anyway?"),
        QMessageBox::Yes | QMessageBox::Cancel);
      if (answer != QMessageBox::Yes)
        {
          event->ignore ();
          return;
        }
    }
  if (isRunning ())
    m_process->kill ();
  saveSettings ();
  event->accept ();
}

bool
MainWindow::eventFilter (QObject *watched, QEvent *event)
{
  if (watched == m_text->viewport ())
    {
      switch (event->type ())
        {
        case QEvent::DragEnter:
        case QEvent::DragMove:
          {
            auto *e = static_cast<QDragMoveEvent *> (event);
            if (!droppedFilePath (e->mimeData ()).isEmpty ())
              {
                e->acceptProposedAction ();
                return true;
              }
            break;
          }
        case QEvent::Drop:
          {
            auto *e = static_cast<QDropEvent *> (event);
            const QString path = droppedFilePath (e->mimeData ());
            if (!path.isEmpty ())
              {
                e->acceptProposedAction ();
                if (!isRunning ())
                  openImage (path);
                return true;
              }
            break;
          }
        case QEvent::Wheel:
          {
            auto *e = static_cast<QWheelEvent *> (event);
            if (e->modifiers () & Qt::ControlModifier)
              {
                setTextSize (m_textSize->value () + (e->angleDelta ().y () > 0 ? 1 : -1));
                return true;
              }
            break;
          }
        default:
          break;
        }
    }
  return QMainWindow::eventFilter (watched, event);
}

void
MainWindow::dragEnterEvent (QDragEnterEvent *event)
{
  if (!droppedFilePath (event->mimeData ()).isEmpty () && !isRunning ())
    event->acceptProposedAction ();
}

void
MainWindow::dropEvent (QDropEvent *event)
{
  const QString path = droppedFilePath (event->mimeData ());
  if (!path.isEmpty ())
    openImage (path);
}
