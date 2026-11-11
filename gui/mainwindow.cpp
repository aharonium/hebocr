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
#include <QPushButton>
#include <QScrollArea>
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
  setDragMode (QGraphicsView::ScrollHandDrag);
  setTransformationAnchor (QGraphicsView::AnchorUnderMouse);
  setRenderHint (QPainter::SmoothPixmapTransform);
  setBackgroundBrush (palette ().window ());
  setAlignment (Qt::AlignCenter);
  clearImage ();
}

void
ImageView::setImage (const QImage &image)
{
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
  zoomToFit ();
}

void
ImageView::clearImage (const QString &message)
{
  m_pixmap->setPixmap (QPixmap ());
  m_pixmap->setVisible (false);
  for (QGraphicsItem *item : m_scene->items ())
    if (item != m_pixmap)
      {
        m_scene->removeItem (item);
        delete item;
      }
  m_hasImage = false;
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

  m_text = new QPlainTextEdit ();
  m_text->setPlaceholderText (tr ("Recognized text will appear here. You can edit it before saving."));
  m_text->setTabChangesFocus (false);
  /* one OCR line per text line, so the text lines up with the page */
  m_text->setLineWrapMode (QPlainTextEdit::NoWrap);

  auto *split = new QSplitter (Qt::Horizontal);
  split->addWidget (m_imageTabs);
  split->addWidget (m_text);
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

  /* recognition */
  auto *recBox = new QGroupBox (tr ("Recognition"));
  auto *rec = new QFormLayout (recBox);
  m_font = new QComboBox ();
  m_font->addItem (tr ("Default"), 0);
  for (int i = 1; i <= 6; ++i)
    m_font->addItem (tr ("Font %1").arg (i), i);
  m_font->setToolTip (tr ("Which built-in font model to recognize with (-z)."));
  m_nikud = new QCheckBox (tr ("Recognize nikud"));
  m_nikud->setToolTip (tr ("Recognize vowel points. Turn off for unvocalized text (-n)."));
  m_linguistics = new QCheckBox (tr ("Linguistics check"));
  m_linguistics->setToolTip (tr ("Use word-shape rules to correct some letters (-A)."));
  m_ltr = new QCheckBox (tr ("Left-to-right text"));
  m_ltr->setToolTip (tr ("Treat the text as left-to-right (-R)."));
  m_format = new QComboBox ();
  m_format->addItem (tr ("Plain text"), "text");
  m_format->addItem (tr ("HTML"), "html");
  m_format->setToolTip (tr ("Output format (-h for HTML)."));
  rec->addRow (tr ("Font model:"), m_font);
  rec->addRow (m_nikud);
  rec->addRow (m_linguistics);
  rec->addRow (m_ltr);
  rec->addRow (tr ("Output:"), m_format);

  /* image processing */
  auto *imgBox = new QGroupBox (tr ("Image processing"));
  auto *img = new QFormLayout (imgBox);
  m_thresholdType = new QComboBox ();
  m_thresholdType->addItem (tr ("Normal"), 0);
  m_thresholdType->addItem (tr ("None"), 1);
  m_thresholdType->addItem (tr ("Fine"), 2);
  m_thresholdType->setToolTip (tr ("How the image is converted to black and white (-T)."));
  m_threshold = autoSpin (0, 100);
  m_threshold->setToolTip (tr ("Black/white threshold, 1–100 (-t). Auto lets the engine decide."));
  m_adaptiveThreshold = autoSpin (0, 100);
  m_adaptiveThreshold->setToolTip (tr ("Adaptive threshold, 1–100 (-a)."));
  m_scale = autoSpin (0, 9);
  m_scale->setToolTip (tr ("Scale the image up by 1–9 before recognition (-s)."));
  m_autoScale = new QCheckBox (tr ("Auto scale"));
  m_autoScale->setToolTip (tr ("Let the engine choose a scale when none is set (off: -S)."));
  m_rotate = new QDoubleSpinBox ();
  m_rotate->setRange (-180.0, 180.0);
  m_rotate->setDecimals (1);
  m_rotate->setSingleStep (0.5);
  m_rotate->setSuffix (tr ("°"));
  m_rotate->setSpecialValueText (tr ("None"));
  m_rotate->setMinimum (-180.0);
  m_rotate->setToolTip (tr ("Rotate the image clockwise by this many degrees (-q)."));
  m_autoRotate = new QCheckBox (tr ("Auto rotate (deskew)"));
  m_autoRotate->setToolTip (tr ("Straighten slightly rotated pages (off: -Q)."));
  m_opening = new QCheckBox (tr ("Separate touching letters"));
  m_opening->setToolTip (tr ("Morphological opening (-1)."));
  m_closing = new QCheckBox (tr ("Join broken letter parts"));
  m_closing->setToolTip (tr ("Morphological closing (-2)."));
  m_erosion = new QCheckBox (tr ("Remove specks"));
  m_erosion->setToolTip (tr ("Erosion (-3)."));
  m_dilation = new QCheckBox (tr ("Thicken faint letters"));
  m_dilation->setToolTip (tr ("Dilation (-4)."));
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

  /* segmentation */
  auto *segBox = new QGroupBox (tr ("Page layout"));
  auto *seg = new QFormLayout (segBox);
  m_columns = autoSpin (0, 255);
  m_columns->setToolTip (tr ("Number of text columns; Auto detects them, 255 = free layout (-c)."));
  m_slicing = autoSpin (0, 250);
  m_slicing->setToolTip (tr ("Threshold for cutting touching letters apart, 1–250 (-x)."));
  m_slicingWidth = autoSpin (49, 250);
  m_slicingWidth->setToolTip (tr ("Letter-slicing width, 50–250 (-X)."));
  m_slicingLine = autoSpin (0, 90);
  m_slicingLine->setToolTip (tr ("Line leeway for letter slicing, 1–90 (-E)."));
  m_fontSpacing = new QSpinBox ();
  m_fontSpacing->setRange (-10, 10);
  m_fontSpacing->setValue (0);
  m_fontSpacing->setSpecialValueText (QString ());
  m_fontSpacing->setToolTip (tr ("Letter spacing: negative = tight, positive = spaced, 0 = normal (-w)."));
  seg->addRow (tr ("Columns:"), m_columns);
  seg->addRow (tr ("Slicing threshold:"), m_slicing);
  seg->addRow (tr ("Slicing width:"), m_slicingWidth);
  seg->addRow (tr ("Slicing line leeway:"), m_slicingLine);
  seg->addRow (tr ("Letter spacing:"), m_fontSpacing);

  /* debug */
  auto *dbgBox = new QGroupBox (tr ("Diagnostics"));
  auto *dbg = new QFormLayout (dbgBox);
  m_grid = new QCheckBox (tr ("Draw grid on images"));
  m_grid->setToolTip (tr ("Draw a grid on the black-and-white and layout images (-g)."));
  m_fontNumbers = new QCheckBox (tr ("Show font numbers in text"));
  m_fontNumbers->setToolTip (tr ("Print the engine's internal letter numbers (-j)."));
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
  for (QComboBox *c : { m_font, m_format, m_thresholdType })
    connect (c, &QComboBox::currentIndexChanged, this, update);
  for (QCheckBox *c : { m_nikud, m_linguistics, m_ltr, m_autoScale, m_autoRotate,
                        m_opening, m_closing, m_erosion, m_dilation, m_grid, m_fontNumbers })
    connect (c, &QCheckBox::toggled, this, update);
  for (QSpinBox *s : { m_threshold, m_adaptiveThreshold, m_scale, m_columns,
                       m_slicing, m_slicingWidth, m_slicingLine, m_fontSpacing })
    connect (s, &QSpinBox::valueChanged, this, update);
  connect (m_rotate, &QDoubleSpinBox::valueChanged, this, update);
  connect (m_ltr, &QCheckBox::toggled, this, &MainWindow::applyTextDirection);
}

void
MainWindow::buildLogDock ()
{
  auto *w = new QWidget ();
  auto *v = new QVBoxLayout (w);
  v->setContentsMargins (4, 4, 4, 4);
  auto *cmdRow = new QHBoxLayout ();
  cmdRow->addWidget (new QLabel (tr ("Command line:")));
  m_commandLine = new QLineEdit ();
  m_commandLine->setReadOnly (true);
  m_commandLine->setToolTip (tr ("The equivalent hebocr command for the current settings."));
  cmdRow->addWidget (m_commandLine, 1);
  auto *copyCmd = new QPushButton (tr ("Copy"));
  connect (copyCmd, &QPushButton::clicked, this, [this] {
    QApplication::clipboard ()->setText (m_commandLine->text ());
  });
  cmdRow->addWidget (copyCmd);
  v->addLayout (cmdRow);

  m_log = new QPlainTextEdit ();
  m_log->setReadOnly (true);
  m_log->setMaximumBlockCount (2000);
  m_log->setFont (QFontDatabase::systemFont (QFontDatabase::FixedFont));
  v->addWidget (m_log);

  auto *dock = new QDockWidget (tr ("Engine"), this);
  dock->setObjectName ("engineDock");
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
  resizeDocks ({ dock }, { 140 }, Qt::Vertical);
}

/* ------------------------------------------------------------------ */
/* settings                                                            */
/* ------------------------------------------------------------------ */

void
MainWindow::resetSettings ()
{
  m_loadingSettings = true;
  m_font->setCurrentIndex (0);
  m_nikud->setChecked (true);
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
  m_columns->setValue (m_columns->minimum ());
  m_slicing->setValue (m_slicing->minimum ());
  m_slicingWidth->setValue (m_slicingWidth->minimum ());
  m_slicingLine->setValue (m_slicingLine->minimum ());
  m_fontSpacing->setValue (0);
  m_grid->setChecked (false);
  m_fontNumbers->setChecked (false);
  m_loadingSettings = false;
  applyTextDirection ();
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

  s.beginGroup ("ocr");
  m_font->setCurrentIndex (s.value ("font", 0).toInt ());
  m_nikud->setChecked (s.value ("nikud", true).toBool ());
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
  m_columns->setValue (s.value ("columns", m_columns->minimum ()).toInt ());
  m_slicing->setValue (s.value ("slicing", m_slicing->minimum ()).toInt ());
  m_slicingWidth->setValue (s.value ("slicingWidth", m_slicingWidth->minimum ()).toInt ());
  m_slicingLine->setValue (s.value ("slicingLine", m_slicingLine->minimum ()).toInt ());
  m_fontSpacing->setValue (s.value ("fontSpacing", 0).toInt ());
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
      font.setPointSize (16);
    }
  m_text->setFont (font);
  m_loadingSettings = false;
  applyTextDirection ();
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

  s.beginGroup ("ocr");
  s.setValue ("font", m_font->currentIndex ());
  s.setValue ("nikud", m_nikud->isChecked ());
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
  s.setValue ("columns", m_columns->value ());
  s.setValue ("slicing", m_slicing->value ());
  s.setValue ("slicingWidth", m_slicingWidth->value ());
  s.setValue ("slicingLine", m_slicingLine->value ());
  s.setValue ("fontSpacing", m_fontSpacing->value ());
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
  opt.setAlignment (ltr ? Qt::AlignLeft : Qt::AlignRight);
  m_text->document ()->setDefaultTextOption (opt);
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
  if (m_font->currentData ().toInt () > 0)
    a << "-z" << QString::number (m_font->currentData ().toInt ());
  if (!m_nikud->isChecked ())
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

  if (m_columns->value () > m_columns->minimum ())
    a << "-c" << QString::number (m_columns->value ());
  if (m_slicing->value () > m_slicing->minimum ())
    a << "-x" << QString::number (m_slicing->value ());
  if (m_slicingWidth->value () > m_slicingWidth->minimum ())
    a << "-X" << QString::number (m_slicingWidth->value ());
  if (m_slicingLine->value () > m_slicingLine->minimum ())
    a << "-E" << QString::number (m_slicingLine->value ());
  if (m_fontSpacing->value () != 0)
    a << "-w" << QString::number (m_fontSpacing->value ());

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
MainWindow::updateCommandLine ()
{
  if (m_loadingSettings || !m_commandLine)
    return;
  QStringList parts { QStringLiteral ("hebocr") };
  for (const QString &arg : engineArguments (true))
    parts << quoteArg (arg);
  m_commandLine->setText (parts.join (QLatin1Char (' ')));
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
  if (!engineFormats.contains (QFileInfo (m_imagePath).suffix ().toLower ()))
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
    appendLog (QString::fromLocal8Bit (m_process->readAll ()));
  });

  m_log->clear ();
  appendLog (m_commandLine->text () + QLatin1Char ('\n'));
  if (m_engineInput != m_imagePath)
    appendLog (tr ("(converted %1 to grayscale PNG for the engine)\n")
                 .arg (QFileInfo (m_imagePath).fileName ()));
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
  appendLog (QString::fromLocal8Bit (m_process->readAll ()));
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
      m_status->setText (tr ("The engine reported an error (exit code %1). See the Engine panel.")
                           .arg (exitCode));
      return;
    }
  const QString text = QString::fromUtf8 (out.readAll ());
  m_text->setPlainText (text);
  m_text->document ()->setModified (false);
  const int lines = text.count (QLatin1Char ('\n'));
  m_status->setText (tr ("Done in %1 s — %2 lines recognized from %3.")
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
      appendLog (tr ("Could not start %1\n").arg (enginePath ()));
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
MainWindow::appendLog (const QString &text)
{
  if (text.isEmpty ())
    return;
  m_log->moveCursor (QTextCursor::End);
  m_log->insertPlainText (text);
  m_log->moveCursor (QTextCursor::End);
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
    m_text->setFont (font);
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
  QMessageBox::about (this, tr ("About hebOCR"),
    tr ("<h3>hebOCR %1</h3>"
        "<p>A free Hebrew OCR engine with nikud recognition.</p>"
        "<p>Engine by Kobi (Yaacov) Zamir, with contributions by Diego Iastrubni "
        "and others. Revived in 2026.</p>"
        "<p>Free software under the GNU General Public License, version 3.</p>"
        "<p><a href=\"https://github.com/aharonium/hebocr\">github.com/aharonium/hebocr</a></p>")
      .arg (QStringLiteral (HEBOCR_VERSION)));
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

void
MainWindow::dragEnterEvent (QDragEnterEvent *event)
{
  if (event->mimeData ()->hasUrls () && !isRunning ())
    event->acceptProposedAction ();
}

void
MainWindow::dropEvent (QDropEvent *event)
{
  const QList<QUrl> urls = event->mimeData ()->urls ();
  if (!urls.isEmpty () && urls.first ().isLocalFile ())
    openImage (urls.first ().toLocalFile ());
}
