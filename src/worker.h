#ifndef WORKER_H
#define WORKER_H

#include <QObject>
#include <QThread>
#include <atomic>
#include <QString>
#include <QByteArray>

#include "utils/file_utils.h"

class Worker : public QObject
{
  Q_OBJECT

public:
  explicit Worker(QObject *parent = nullptr);

  void setConfig(const QString &inputDir,
                 const QString &outputDir,
                 const QString &fileMask,
                 const QByteArray &xorKey,
                 bool deleteInput,
                 bool overwriteMode);

public slots:
  void processFiles();
  void pause();
  void resume();
  void stop();

signals:
  void progressChanged(qint64 processed, qint64 total);
  void fileProgressChanged(const QString &fileName, qint64 processed, qint64 total);
  void statusMessage(const QString &msg);
  void finished();

private:
  QString getUniquePath(const QString &basePath);

  std::atomic<bool> m_isPaused{false};
  std::atomic<bool> m_isStopped{false};

  QString m_inputDir;
  QString m_outputDir;
  QString m_fileMask;
  QByteArray m_xorKey;
  bool m_deleteInput = false;
  bool m_overwriteMode = true;
};

#endif // WORKER_H