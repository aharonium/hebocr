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

#pragma once

#include <QElapsedTimer>
#include <QGraphicsView>
#include <QMainWindow>
#include <QProcess>
#include <QTemporaryDir>

class QAction;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGraphicsPixmapItem;
class QGraphicsScene;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSlider;
class QSpinBox;
class QTabWidget;
class QTimer;

/* Zoomable, pannable image view (Ctrl+wheel zooms). */
class ImageView : public QGraphicsView
{
  Q_OBJECT
public:
  explicit ImageView (QWidget *parent = nullptr);
  void setImage (const QImage &image);
  void clearImage (const QString &message = QString ());
  bool hasImage () const { return m_hasImage; }
  void zoomBy (double factor);
  void zoomToFit ();
  void zoomActualSize ();

signals:
  /* an image file was dropped onto the view */
  void fileDropped (const QString &path);

protected:
  void wheelEvent (QWheelEvent *event) override;
  void resizeEvent (QResizeEvent *event) override;
  void dragEnterEvent (QDragEnterEvent *event) override;
  void dragMoveEvent (QDragMoveEvent *event) override;
  void dropEvent (QDropEvent *event) override;

private:
  QGraphicsScene *m_scene;
  QGraphicsPixmapItem *m_pixmap;
  bool m_hasImage = false;
  bool m_fit = true;
};

class MainWindow : public QMainWindow
{
  Q_OBJECT
public:
  explicit MainWindow (QWidget *parent = nullptr);
  ~MainWindow () override;

  bool openImage (const QString &path);
  void runOcr ();
  bool isRunning () const;
  /* used by --screenshot for documentation */
  QPlainTextEdit *textEdit () const { return m_text; }
  void showImageTab (int index);
  void showGuide ();

protected:
  void closeEvent (QCloseEvent *event) override;
  bool eventFilter (QObject *watched, QEvent *event) override;
  void dragEnterEvent (QDragEnterEvent *event) override;
  void dropEvent (QDropEvent *event) override;

private slots:
  void openImageDialog ();
  void stopOcr ();
  void saveText ();
  void copyText ();
  void chooseTextFont ();
  void resetSettings ();
  void updateCommandLine ();
  void processFinished (int exitCode, QProcess::ExitStatus status);
  void processError (QProcess::ProcessError error);
  void tick ();
  void about ();
  void setTextSize (int points);
  void updateThresholdControls ();
  void scrollTextToLineStarts ();

private:
  void buildActions ();
  void buildCentral ();
  void buildSettingsDock ();
  void buildLogDock ();
  void loadSettings ();
  void saveSettings ();
  void setRunning (bool running);
  void applyTextDirection ();
  bool maybeDiscardEdits ();
  QStringList engineArguments (bool forDisplay) const;
  QString enginePath () const;
  void appendLog (const QString &text);

  /* views */
  QSlider *m_textSize;
  QLabel *m_textSizeLabel;
  QTabWidget *m_imageTabs;
  ImageView *m_originalView;
  ImageView *m_bwView;
  ImageView *m_layoutView;
  QPlainTextEdit *m_text;
  QPlainTextEdit *m_log;
  QLineEdit *m_commandLine;
  QLabel *m_status;

  /* actions */
  QAction *m_openAct;
  QAction *m_runAct;
  QAction *m_stopAct;
  QAction *m_saveAct;
  QAction *m_copyAct;

  /* recognition */
  QComboBox *m_fontModel;
  QCheckBox *m_nikud;
  QCheckBox *m_linguistics;
  QCheckBox *m_ltr = nullptr;
  QComboBox *m_format;

  /* image processing */
  QComboBox *m_thresholdType;
  QSpinBox *m_threshold;
  QSpinBox *m_adaptiveThreshold;
  QSpinBox *m_scale;
  QCheckBox *m_autoScale;
  QDoubleSpinBox *m_rotate;
  QCheckBox *m_autoRotate;
  QCheckBox *m_opening;
  QCheckBox *m_closing;
  QCheckBox *m_erosion;
  QCheckBox *m_dilation;

  /* segmentation */
  QComboBox *m_columns;
  QSpinBox *m_slicing;
  QSpinBox *m_slicingWidth;
  QSpinBox *m_slicingLine;
  QComboBox *m_wordSpacing;

  /* debug */
  QCheckBox *m_grid;
  QCheckBox *m_fontNumbers;

  /* state */
  QString m_imagePath;
  QImage m_image;
  QString m_engineInput;
  QProcess *m_process = nullptr;
  QTemporaryDir *m_workDir = nullptr;
  QElapsedTimer m_elapsed;
  QTimer *m_ticker;
  bool m_loadingSettings = false;
};
