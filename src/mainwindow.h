#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QTimer>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>

#include "worker.h"
#include "utils/hex_parser.h"

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void onBrowseInput();
  void onBrowseOutput();
  void onStartStop();
  void onPauseResume();
  void onTimerToggled(bool checked);
  void onWorkerProgressChanged(qint64 processed, qint64 total);
  void onWorkerFileProgressChanged(const QString &fileName, qint64 processed, qint64 total);
  void onWorkerStatusMessage(const QString &msg);
  void onWorkerFinished();

private:
  void createUI();
  void startProcessing();
  void stopProcessing();
  QByteArray parseHexKey(const QString &hex);

  // UI элементы
  QLineEdit *m_inputDirEdit;
  QLineEdit *m_outputDirEdit;
  QLineEdit *m_fileMaskEdit;
  QLineEdit *m_xorKeyEdit;
  QCheckBox *m_deleteInputCheck;
  QRadioButton *m_overwriteRadio;
  QRadioButton *m_counterRadio;
  QRadioButton *m_onceRadio;
  QRadioButton *m_timerRadio;
  QSpinBox *m_timerIntervalSpin;
  QPushButton *m_startButton;
  QPushButton *m_pauseButton;
  QProgressBar *m_progressBar;
  QLabel *m_statusLabel;
  QLabel *m_fileStatusLabel;

  // Worker и поток
  QThread *m_workerThread;
  Worker *m_worker;
  QTimer *m_timer;
  bool m_isProcessing;
};

#endif // MAINWINDOW_H