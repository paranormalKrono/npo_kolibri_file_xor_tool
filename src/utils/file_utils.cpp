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

  do
  {
    newPath = info.absolutePath() + "/" +
              info.baseName() + "_" +
              QString::number(counter);
    if (!info.suffix().isEmpty())
    {
      newPath += "." + info.suffix();
    }
    counter++;
  } while (QFile::exists(newPath));

  return newPath;
}