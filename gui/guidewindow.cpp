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

#include "guidewindow.h"

#include <QAction>
#include <QCloseEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QScrollBar>
#include <QSettings>
#include <QSlider>
#include <QStyle>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

GuideWindow::GuideWindow (QWidget *parent)
  : QDialog (parent)
{
  setWindowTitle (tr ("hebOCR Options Guide"));
  setObjectName ("guideWindow");

  /* search */
  m_search = new QLineEdit ();
  m_search->setObjectName ("guideSearch");
  m_search->setPlaceholderText (tr ("Search the guide"));
  m_search->setClearButtonEnabled (true);
  m_search->setMinimumWidth (220);
  m_matchLabel = new QLabel ();
  m_matchLabel->setMinimumWidth (m_matchLabel->fontMetrics ().horizontalAdvance ("888 of 888") + 6);
  m_prev = new QToolButton ();
  m_prev->setArrowType (Qt::UpArrow);
  m_prev->setToolTip (tr ("Previous match (Shift+Enter)"));
  m_next = new QToolButton ();
  m_next->setArrowType (Qt::DownArrow);
  m_next->setToolTip (tr ("Next match (Enter)"));
  connect (m_search, &QLineEdit::textChanged, this, &GuideWindow::searchChanged);
  connect (m_search, &QLineEdit::returnPressed, this, &GuideWindow::findNext);
  connect (m_prev, &QToolButton::clicked, this, &GuideWindow::findPrevious);
  connect (m_next, &QToolButton::clicked, this, &GuideWindow::findNext);

  auto *findAct = new QAction (this);
  findAct->setShortcut (QKeySequence::Find);
  connect (findAct, &QAction::triggered, this, [this] {
    m_search->setFocus ();
    m_search->selectAll ();
  });
  addAction (findAct);
  auto *prevAct = new QAction (this);
  prevAct->setShortcuts ({ QKeySequence (Qt::SHIFT | Qt::Key_Return),
                           QKeySequence (Qt::SHIFT | Qt::Key_Enter),
                           QKeySequence::FindPrevious });
  connect (prevAct, &QAction::triggered, this, &GuideWindow::findPrevious);
  addAction (prevAct);
  auto *nextAct = new QAction (this);
  nextAct->setShortcut (QKeySequence::FindNext);
  connect (nextAct, &QAction::triggered, this, &GuideWindow::findNext);
  addAction (nextAct);

  /* text size */
  m_size = new QSlider (Qt::Horizontal);
  m_size->setObjectName ("guideTextSize");
  m_size->setRange (8, 32);
  m_size->setFixedWidth (160);
  m_size->setToolTip (tr ("Size of the guide's text"));
  m_sizeLabel = new QLabel ();
  m_sizeLabel->setMinimumWidth (m_sizeLabel->fontMetrics ().horizontalAdvance ("88 pt") + 4);
  connect (m_size, &QSlider::valueChanged, this, &GuideWindow::setTextSize);

  auto *bar = new QHBoxLayout ();
  bar->setContentsMargins (6, 6, 6, 6);
  bar->addWidget (m_search);
  bar->addWidget (m_prev);
  bar->addWidget (m_next);
  bar->addWidget (m_matchLabel);
  bar->addStretch (1);
  bar->addWidget (new QLabel (tr ("Text size:")));
  bar->addWidget (m_size);
  bar->addWidget (m_sizeLabel);

  /* the guide itself: read-only, but selectable and copyable */
  m_browser = new QTextBrowser ();
  m_browser->setObjectName ("guideText");
  m_browser->setOpenExternalLinks (true);
  m_browser->setTextInteractionFlags (Qt::TextBrowserInteraction);
  QFile f (QStringLiteral (":/docs/options.md"));
  if (f.open (QIODevice::ReadOnly))
    m_browser->setMarkdown (QString::fromUtf8 (f.readAll ()));
  else
    m_browser->setPlainText (tr ("The options guide is missing from this build."));

  auto *layout = new QVBoxLayout (this);
  layout->setContentsMargins (0, 0, 0, 0);
  layout->setSpacing (0);
  layout->addLayout (bar);
  layout->addWidget (m_browser, 1);

  QSettings s;
  restoreGeometry (s.value ("guide/geometry").toByteArray ());
  if (!s.contains ("guide/geometry"))
    resize (860, 780);
  const int size = s.value ("guide/textSize", 12).toInt ();
  m_size->setValue (size);
  setTextSize (size);
  searchChanged (QString ());
}

void
GuideWindow::setTextSize (int points)
{
  points = qBound (m_size->minimum (), points, m_size->maximum ());
  QFont font = m_browser->font ();
  font.setPointSize (points);
  m_browser->setFont (font);
  m_browser->document ()->setDefaultFont (font);
  if (m_size->value () != points)
    {
      const QSignalBlocker block (m_size);
      m_size->setValue (points);
    }
  m_sizeLabel->setText (tr ("%1 pt").arg (points));
  applyFormatting ();
}

/* more space above headings, scaled to the text size, so each section
 * stands apart */
void
GuideWindow::applyFormatting ()
{
  const qreal em = QFontMetricsF (m_browser->font ()).height ();
  QTextCursor cursor (m_browser->document ());
  cursor.beginEditBlock ();
  for (QTextBlock b = m_browser->document ()->begin (); b.isValid (); b = b.next ())
    {
      const int level = b.blockFormat ().headingLevel ();
      if (level <= 0)
        continue;
      QTextBlockFormat fmt = b.blockFormat ();
      fmt.setTopMargin (level <= 2 ? 1.8 * em : 1.0 * em);
      fmt.setBottomMargin (0.4 * em);
      cursor.setPosition (b.position ());
      cursor.setBlockFormat (fmt);
    }
  cursor.endEditBlock ();
  /* formatting changes don't count as edits */
  m_browser->document ()->setModified (false);
}

void
GuideWindow::searchChanged (const QString &text)
{
  m_matches.clear ();
  m_current = -1;
  QList<QTextEdit::ExtraSelection> highlights;
  if (!text.isEmpty ())
    {
      QTextCharFormat mark;
      mark.setBackground (QColor (255, 230, 120));
      mark.setForeground (Qt::black);
      QTextCursor c (m_browser->document ());
      while (true)
        {
          c = m_browser->document ()->find (text, c);
          if (c.isNull ())
            break;
          m_matches << c;
          QTextEdit::ExtraSelection sel;
          sel.cursor = c;
          sel.format = mark;
          highlights << sel;
        }
    }
  m_browser->setExtraSelections (highlights);

  const bool any = !m_matches.isEmpty ();
  m_prev->setEnabled (any);
  m_next->setEnabled (any);
  if (text.isEmpty ())
    m_matchLabel->clear ();
  else if (!any)
    m_matchLabel->setText (tr ("No matches"));
  if (any)
    showMatch (0);
}

void
GuideWindow::showMatch (int index)
{
  if (m_matches.isEmpty ())
    return;
  m_current = (index + m_matches.size ()) % m_matches.size ();
  m_browser->setTextCursor (m_matches.at (m_current));
  m_browser->ensureCursorVisible ();
  /* place the match about a third of the way down, with context above */
  QScrollBar *bar = m_browser->verticalScrollBar ();
  const int y = m_browser->cursorRect ().top ();
  bar->setValue (bar->value () + y - m_browser->viewport ()->height () / 3);
  m_matchLabel->setText (tr ("%1 of %2").arg (m_current + 1).arg (m_matches.size ()));
}

void
GuideWindow::findNext ()
{
  showMatch (m_current + 1);
}

void
GuideWindow::findPrevious ()
{
  showMatch (m_current - 1);
}

void
GuideWindow::closeEvent (QCloseEvent *event)
{
  QSettings s;
  s.setValue ("guide/geometry", saveGeometry ());
  s.setValue ("guide/textSize", m_size->value ());
  QDialog::closeEvent (event);
}
