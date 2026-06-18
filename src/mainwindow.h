#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QTimer>
#include <QFileSystemWatcher>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QProgressBar>
#include <QLabel>
#include <QMutex>
#include <QQueue>
#include <QTableWidget>
#include <QMap>
#include <QPointer>
#include "worker.h"

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

signals:
  void workerFinished();

private slots:
  void onBrowseInput();
  void onBrowseOutput();
  void onStart();
  void onPauseResume();
  void onStop();
  void onTimerToggled(bool checked);
  void onWatcherToggled(bool checked);
  void onClearCache();
  void onCheckCache();
  void onXorKeyChanged(const QString &text);

  void onWorkerProgressChanged(qint64 processed, qint64 total);
  void onWorkerFileProgressChanged(const QString &fileName, qint64 processed, qint64 total);
  void onWorkerStatusMessage(const QString &msg);
  void onWorkerFinished();
  void onWorkerFileStarted(const QString &fileName, qint64 fileSize);
  void onWorkerFileFinished(const QString &fileName, bool success);
  void onWorkerFileSkipped(const QString &fileName);

  void onFileChanged(const QString &path);
  void onDirectoryChanged(const QString &path);

private:
  void createUI();
  void setupConnections();
  void updateXorKeyValidation();
  void updateFileTable(const QString &fileName, const QString &status, int progress = -1);

  QPointer<QLineEdit> m_inputDirEdit;
  QPointer<QLineEdit> m_outputDirEdit;
  QPointer<QLineEdit> m_fileMaskEdit;
  QPointer<QLineEdit> m_xorKeyEdit;
  QPointer<QLabel> m_xorKeyStatusLabel;
  QPointer<QCheckBox> m_deleteInputCheck;
  QPointer<QRadioButton> m_overwriteRadio;
  QPointer<QRadioButton> m_skipRadio;
  QPointer<QRadioButton> m_numberingRadio;

  QPointer<QRadioButton> m_manualRadio;
  QPointer<QRadioButton> m_timerRadio;
  QPointer<QRadioButton> m_watcherRadio;
  QPointer<QSpinBox> m_timerIntervalSpin;

  QPointer<QPushButton> m_browseInputBtn;
  QPointer<QPushButton> m_browseOutputBtn;
  QPointer<QPushButton> m_startButton;
  QPointer<QPushButton> m_pauseButton;
  QPointer<QPushButton> m_stopButton;
  QPointer<QPushButton> m_clearCacheBtn;
  QPointer<QPushButton> m_checkCacheBtn;

  QPointer<QProgressBar> m_progressBar;
  QPointer<QLabel> m_fileStatusLabel;
  QPointer<QLabel> m_statusLabel;
  QPointer<QLabel> m_statsLabel;
  QPointer<QLabel> m_cacheInfoLabel;

  QPointer<QTableWidget> m_fileTable;
  QMap<QString, int> m_fileRowMap;

  QThread *m_workerThread = nullptr;
  Worker *m_worker = nullptr;
  QTimer *m_timer = nullptr;
  QFileSystemWatcher *m_watcher = nullptr;

  QQueue<QString> m_fileQueue;
  QMutex m_queueMutex;

  bool m_watcherActive = false;
  bool m_isProcessing = false;
  int m_processedCount = 0;
  int m_skippedCount = 0;
  int m_totalFiles = 0;
};

#endif // MAINWINDOW_H