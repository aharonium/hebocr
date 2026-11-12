/*
 * hebocr-gui: a graphical front end for the hebOCR Hebrew OCR engine
 *
 * Copyright (C) 2026 the hebOCR contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QEventLoop>
#include <QIcon>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTimer>

int
main (int argc, char *argv[])
{
  QApplication app (argc, argv);
  QApplication::setApplicationName ("hebOCR");
  QApplication::setOrganizationName ("hebOCR");
  QApplication::setApplicationVersion (QStringLiteral (HEBOCR_VERSION " (" HEBOCR_BUILD_LABEL ")"));
  QIcon icon;
  for (int size : { 16, 24, 32, 48, 64, 128, 256 })
    icon.addFile (QStringLiteral (":/icons/hebocr-%1.png").arg (size), QSize (size, size));
  QApplication::setWindowIcon (icon);
  QApplication::setDesktopFileName ("hebocr");

  QCommandLineParser parser;
  parser.setApplicationDescription ("Graphical front end for the hebOCR Hebrew OCR engine");
  parser.addHelpOption ();
  parser.addVersionOption ();
  parser.addPositionalArgument ("image", "Image file to open");
  QCommandLineOption screenshot ("screenshot",
    "Open the image, run OCR, save a screenshot of the window to FILE and exit "
    "(used for documentation and automated testing).", "FILE");
  parser.addOption (screenshot);
  QCommandLineOption screenshotGuide ("screenshot-guide",
    "Also save a screenshot of the Options Guide window to FILE.", "FILE");
  parser.addOption (screenshotGuide);
  QCommandLineOption screenshotTab ("screenshot-tab",
    "Image tab to show in the screenshot: 0 original, 1 black and white, 2 layout.", "N", "0");
  parser.addOption (screenshotTab);
  parser.process (app);

  MainWindow window;
  window.resize (1400, 900);
  window.show ();

  const QStringList args = parser.positionalArguments ();
  if (!args.isEmpty ())
    window.openImage (args.first ());

  if (parser.isSet (screenshot))
    {
      if (!args.isEmpty ())
        {
          window.runOcr ();
          QEventLoop loop;
          QTimer poll;
          QObject::connect (&poll, &QTimer::timeout, [&] {
            if (!window.isRunning ())
              loop.quit ();
          });
          poll.start (100);
          QTimer::singleShot (600000, &loop, &QEventLoop::quit);
          loop.exec ();
        }
      window.showImageTab (parser.value (screenshotTab).toInt ());
      QApplication::processEvents ();
      const bool ok = window.grab ().save (parser.value (screenshot));
      if (parser.isSet (screenshotGuide))
        {
          window.showGuide ();
          QApplication::processEvents ();
          /* show the search in action */
          for (QWidget *top : QApplication::topLevelWidgets ())
            if (auto *search = top->findChild<QLineEdit *> ("guideSearch"))
              search->setText ("word spacing");
          QApplication::processEvents ();
          for (QWidget *top : QApplication::topLevelWidgets ())
            if (top->isVisible () && top != &window)
              top->grab ().save (parser.value (screenshotGuide));
        }
      return ok && (args.isEmpty () || !window.textEdit ()->toPlainText ().isEmpty ()) ? 0 : 1;
    }

  return app.exec ();
}
