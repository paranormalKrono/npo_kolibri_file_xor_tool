#ifndef WORKER_H
#define WORKER_H

#include <QObject>
#include <QThreadPool>
#include <QString>
#include <QByteArray>
#include <QStringList>
#include <memory>
#include <atomic>
#include <QMutex>
#include <QMap>
#include "file_result.h"
#include "file_processor.h"

class IBinaryOperation;
class FileCache;

enum class WorkerState
{
  Idle,
  Running,
  Paused,
  Stopping
};

class Worker : public QObject
{
  Q_OBJECT

public:
  explicit Worker(QObject *parent = nullptr);
  ~Worker();

  void setConfig(const QString &inputDir,
                 const QString &outputDir,
                 const QString &fileMask,
                 const QString &operationName,
                 const QByteArray &operationParams,
                 bool deleteInput,
                 ConflictMode conflictMode,
                 int threadCount = 8);

  void setFilesToProcess(const QStringList &files);

public slots:
  void processFiles();
  void pause();
  void resume();
  void stop();

  void onFileResult(const FileResult &result);
  void onFileProgress(const QString &fileName, qint64 processed, qint64 total);

signals:
  void progressChanged(qint64 processed, qint64 total);
  void fileProgressChanged(const QString &fileName, qint64 processed, qint64 total);
  void statusMessage(const QString &msg);
  void finished();
  void fileStarted(const QString &fileName, qint64 fileSize);
  void fileFinished(const QString &fileName, bool success);
  void fileSkipped(const QString &fileName);
  void totalFilesCount(int count);

private:
  std::atomic<WorkerState> m_state{WorkerState::Idle};
  std::atomic<bool> m_isPaused{false};
  std::atomic<bool> m_isStopped{false};

  QMutex m_counterMutex;
  std::atomic<int> m_filesRemaining{0};
  bool m_finishedEmitted = false;

  QThreadPool *m_threadPool;
  std::unique_ptr<FileCache> m_cache;
  std::unique_ptr<IBinaryOperation> m_operation;

  QString m_inputDir;
  QString m_outputDir;
  QString m_fileMask;
  bool m_deleteInput = false;
  ConflictMode m_conflictMode = ConflictMode::Skip;
  int m_threadCount = 8;

  QStringList m_filesToProcess;

  // Прогресс
  std::atomic<qint64> m_totalProcessed{0};
  qint64 m_totalSize = 0;
  QMap<QString, qint64> m_fileProgress;
  QMutex m_progressMutex;
};

#endif // WORKER_H