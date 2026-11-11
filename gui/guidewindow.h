/*
 * hebocr-gui: the Options Guide window
 *
 * Copyright (C) 2026 the hebOCR contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <QDialog>
#include <QList>
#include <QTextCursor>

class QLabel;
class QLineEdit;
class QSlider;
class QTextBrowser;
class QToolButton;

/* Shows docs/options.md with search and an adjustable text size.
 * The text can be selected and copied. */
class GuideWindow : public QDialog
{
  Q_OBJECT
public:
  explicit GuideWindow (QWidget *parent = nullptr);

  void setTextSize (int points);
  int matchCount () const { return m_matches.size (); }
  QTextBrowser *browser () const { return m_browser; }

public slots:
  void findNext ();
  void findPrevious ();

protected:
  void closeEvent (QCloseEvent *event) override;

private slots:
  void searchChanged (const QString &text);

private:
  void applyFormatting ();
  void showMatch (int index);

  QTextBrowser *m_browser;
  QLineEdit *m_search;
  QLabel *m_matchLabel;
  QToolButton *m_prev;
  QToolButton *m_next;
  QSlider *m_size;
  QLabel *m_sizeLabel;
  QList<QTextCursor> m_matches;
  int m_current = -1;
};
