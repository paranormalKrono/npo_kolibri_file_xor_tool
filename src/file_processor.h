#ifndef FILE_PROCESSOR_H
#define FILE_PROCESSOR_H

#include <QRunnable>
#include <QString>
#include <QByteArray>
#include <atomic>
#include "file_result.h"
#include "operations/ibinary_operation.h"

class Worker;
class FileCache;

class FileProcessor : public QRunnable
{
public:
  FileProcessor(const QString &inputPath,
                const QString &outputDir,
                IBinaryOperation *operation,
                ConflictMode conflictMode,
                const std::atomic<bool> &isPaused,
                const std::atomic<bool> &isStopped,
                FileCache *cache,
                Worker *worker);

  void run() override;

private:
  QString m_inputPath;
  QString m_outputDir;
  IBinaryOperation *m_operation;
  ConflictMode m_conflictMode;
  const std::atomic<bool> &m_isPaused;
  const std::atomic<bool> &m_isStopped;
  FileCache *m_cache;
  Worker *m_worker;
};

#endif // FILE_PROCESSOR_H