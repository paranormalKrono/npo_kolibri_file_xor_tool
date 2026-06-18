#include "file_utils.h"
#include <QFile>
#include <QFileInfo>

QString FileUtils::getUniquePath(const QString &basePath)
{
  if (!QFile::exists(basePath))
  {
    return basePath;
  }

  QFileInfo info(basePath);
  int counter = 1;
  QString newPath;

  QString fileName = info.fileName(); // "test.bin.xor"
  QString dir = info.absolutePath();

  // Находим первое расширение
  int dotIndex = fileName.indexOf('.');

  QString baseName;
  QString extensions;

  if (dotIndex == -1)
  {
    // Нет расширений
    baseName = fileName;
    extensions = "";
  }
  else
  {
    baseName = fileName.left(dotIndex);  // "test"
    extensions = fileName.mid(dotIndex); // ".bin.xor"
  }

  do
  {
    newPath = dir + "/" + baseName + "_" + QString::number(counter) + extensions;
    counter++;
  } while (QFile::exists(newPath));

  return newPath;
}

QString FileUtils::getProcessedPath(const QString &outputDir, const QString &fileName)
{
  return outputDir + "/" + fileName + ".xor";
}