/*
 * Automated tests for hebocr-gui (run headless with QT_QPA_PLATFORM=offscreen)
 *
 * Copyright (C) 2026 the hebOCR contributors
 * Licensed under the GNU General Public License, version 3 or later.
 */

#include "mainwindow.h"
#include "guidewindow.h"

#include <QComboBox>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QDockWidget>
#include <QTextBlock>
#include <QDropEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QtTest>

class TestGui : public QObject
{
  Q_OBJECT

private:
  QTemporaryDir m_dir;

  /* deliver a complete drag-and-drop of FILE to TARGET */
  static bool
  dropFile (QWidget *target, const QString &file)
  {
    QMimeData mime;
    mime.setUrls ({ QUrl::fromLocalFile (file) });
    const QPoint pos = target->rect ().center ();
    QDragEnterEvent enter (pos, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent (target, &enter);
    if (!enter.isAccepted ())
      return false;
    /* real drags send move events as the pointer moves; a widget that
     * accepts the enter but rejects the moves never receives the drop */
    QDragMoveEvent move (pos, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent (target, &move);
    if (!move.isAccepted ())
      return false;
    QDropEvent drop (QPointF (pos), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent (target, &drop);
    return drop.isAccepted ();
  }

  QString
  copyTestImage (const QString &name)
  {
    const QString path = m_dir.filePath (name);
    QFile::remove (path);
    QFile::copy (QStringLiteral (TEST_IMAGE), path);
    return path;
  }

private slots:
  void
  dropOnEachImageTab ()
  {
    MainWindow w;
    w.show ();
    const auto views = w.findChildren<ImageView *> ();
    QCOMPARE (views.size (), 3);
    int n = 0;
    for (ImageView *view : views)
      {
        const QString name = QStringLiteral ("dropped_%1.png").arg (++n);
        QVERIFY2 (dropFile (view->viewport (), copyTestImage (name)),
                  "image view rejected a dropped file");
        QVERIFY (w.windowTitle ().contains (name));
      }
  }

  void
  dropOnText ()
  {
    MainWindow w;
    w.show ();
    auto *text = w.findChild<QPlainTextEdit *> ("ocrText");
    QVERIFY (text);
    QVERIFY (dropFile (text->viewport (), copyTestImage ("dropped_text.png")));
    QVERIFY (w.windowTitle ().contains ("dropped_text.png"));
  }

  void
  textSizeSlider ()
  {
    MainWindow w;
    auto *slider = w.findChild<QSlider *> ("textSize");
    auto *text = w.findChild<QPlainTextEdit *> ("ocrText");
    QVERIFY (slider && text);
    slider->setValue (30);
    QCOMPARE (text->font ().pointSize (), 30);
    slider->setValue (12);
    QCOMPARE (text->font ().pointSize (), 12);
  }

  void
  longHebrewLinesShowTheirStart ()
  {
    MainWindow w;
    w.resize (900, 600);
    w.show ();
    auto *text = w.findChild<QPlainTextEdit *> ("ocrText");
    QVERIFY (text);
    QString line;
    for (int i = 0; i < 40; ++i)
      line += QStringLiteral ("הקדמה למהדורה שניה ");
    text->setPlainText (QStringLiteral ("שורה קצרה\n") + line);
    QCoreApplication::processEvents ();
    QScrollBar *bar = text->horizontalScrollBar ();
    QVERIFY (bar->maximum () > 0);
    /* right-to-left lines begin at the right edge, i.e. scrolled to the end */
    QCOMPARE (bar->value (), bar->maximum ());
  }

  void
  commandLineOptions ()
  {
    MainWindow w;
    auto *cmd = w.findChild<QLineEdit *> ("commandLine");
    auto *fontModel = w.findChild<QComboBox *> ("fontModel");
    auto *spacing = w.findChild<QComboBox *> ("wordSpacing");
    auto *columns = w.findChild<QComboBox *> ("columns");
    auto *thresholdType = w.findChild<QComboBox *> ("thresholdType");
    auto *adaptive = w.findChild<QSpinBox *> ("adaptiveThreshold");
    QVERIFY (cmd && fontModel && spacing && columns && thresholdType && adaptive);

    /* only the working David model is offered, and it needs no -z */
    QCOMPARE (fontModel->count (), 1);
    QVERIFY (!cmd->text ().contains ("-z"));

    spacing->setCurrentIndex (spacing->findData (2));
    QVERIFY (cmd->text ().contains ("-w 2"));
    spacing->setCurrentIndex (spacing->findData (-3));
    QVERIFY (cmd->text ().contains ("-w -3"));

    columns->setCurrentIndex (columns->findData (255));
    QVERIFY (cmd->text ().contains ("-c 255"));

    QVERIFY (adaptive->isEnabled ());
    thresholdType->setCurrentIndex (thresholdType->findData (1));
    QVERIFY (cmd->text ().contains ("-T 1"));
    QVERIFY (!adaptive->isEnabled ());
  }

  void
  hebrewFontMenu ()
  {
    MainWindow w;
    auto *fonts = w.findChild<QFontComboBox *> ("textFont");
    auto *text = w.findChild<QPlainTextEdit *> ("ocrText");
    QVERIFY (fonts && text);
    QCOMPARE (fonts->writingSystem (), QFontDatabase::Hebrew);
    QVERIFY (fonts->count () > 0);
    /* every listed font supports Hebrew; choosing one changes the text font
     * but keeps its size */
    const int size = text->font ().pointSize ();
    for (int i = 0; i < fonts->count (); ++i)
      QVERIFY (QFontDatabase::writingSystems (fonts->itemText (i))
                 .contains (QFontDatabase::Hebrew));
    fonts->setCurrentIndex (fonts->count () - 1);
    QCOMPARE (text->font ().family (), fonts->currentFont ().family ());
    QCOMPARE (text->font ().pointSize (), size);
  }

  void
  commandBarIsOneRow ()
  {
    MainWindow w;
    auto *dock = w.findChild<QDockWidget *> ("commandDock");
    QVERIFY (dock);
    QCOMPARE (dock->windowTitle (), QStringLiteral ("Equivalent Command"));
    /* the duplicate log under the command line is gone */
    QCOMPARE (dock->widget ()->findChildren<QPlainTextEdit *> ().size (), 0);
    QVERIFY (dock->widget ()->findChild<QLineEdit *> ("commandLine"));
  }

  void
  guideSearchAndSize ()
  {
    GuideWindow g;
    auto *search = g.findChild<QLineEdit *> ("guideSearch");
    QVERIFY (search);
    search->setText ("slicing");
    QVERIFY (g.matchCount () > 3);
    /* the current match is selected, i.e. text selection works */
    QVERIFY (g.browser ()->textCursor ().selectedText ().compare ("slicing", Qt::CaseInsensitive) == 0);
    search->setText ("zzzz-not-in-guide");
    QCOMPARE (g.matchCount (), 0);

    g.setTextSize (20);
    QCOMPARE (g.browser ()->font ().pointSize (), 20);

    /* section headings get extra space above them */
    bool sawHeading = false;
    for (QTextBlock b = g.browser ()->document ()->begin (); b.isValid (); b = b.next ())
      if (b.blockFormat ().headingLevel () == 2)
        {
          sawHeading = true;
          QVERIFY (b.blockFormat ().topMargin () > 20);
        }
    QVERIFY (sawHeading);
  }

  void
  optionsGuideIsBundled ()
  {
    QFile guide (QStringLiteral (":/docs/options.md"));
    QVERIFY (guide.open (QIODevice::ReadOnly));
    const QString text = QString::fromUtf8 (guide.readAll ());
    QVERIFY (text.contains ("Slicing width"));
    QVERIFY (text.contains ("Word spacing"));
  }
};

QTEST_MAIN (TestGui)
#include "test_gui.moc"
