#include "test_gui_helpers.h"
#include "mainwindow.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTest>
#include <QElapsedTimer>
#include <QObject>
#include <QtTest>

QDebug operator<<(QDebug dbg, const GuiState &state)
{
  dbg.nospace() << "GuiState("
                << "start=" << state.startEnabled
                << ", pause=" << state.pauseEnabled << "(" << state.pauseText << ")"
                << ", stop=" << state.stopEnabled
                << ", table=" << state.tableRowCount
                << ", processed=" << state.processedCount
                << ", skipped=" << state.skippedCount
                << ", remaining=" << state.remainingCount
                << ", timer=" << state.timerActive
                << ", watcher=" << state.watcherActive
                << ")";
  return dbg;
}

// ==========================================
// DialogCloser
// ==========================================

DialogCloser::DialogCloser(int delayMs)
{
  m_timer.setInterval(delayMs);
  connect(&m_timer, &QTimer::timeout, this, &DialogCloser::closeDialogs);
}

DialogCloser::~DialogCloser()
{
  m_timer.stop();
}

void DialogCloser::setActive(bool active)
{
  m_active = active;
  if (active)
  {
    m_timer.start();
  }
  else
  {
    m_timer.stop();
  }
}

void DialogCloser::closeDialogs()
{
  if (!m_active)
    return;

  // Закрываем все видимые QDialog (включая QMessageBox)
  for (QWidget *widget : QApplication::topLevelWidgets())
  {
    if (QDialog *dialog = qobject_cast<QDialog *>(widget))
    {
      if (dialog->isVisible())
      {
        // Находим кнопку OK и нажимаем её
        QDialogButtonBox *buttonBox = dialog->findChild<QDialogButtonBox *>();
        if (buttonBox)
        {
          if (QPushButton *okBtn = buttonBox->button(QDialogButtonBox::Ok))
          {
            okBtn->click();
            continue;
          }
        }
        // Если кнопки нет — закрываем диалог
        dialog->reject();
      }
    }
  }
}

// ==========================================
// MainWindowTester
// ==========================================

MainWindowTester::MainWindowTester(MainWindow *window)
    : m_window(window)
{
}

void MainWindowTester::setInputDir(const QString &path)
{
  getInputEdit()->setText(path);
}

void MainWindowTester::setOutputDir(const QString &path)
{
  getOutputEdit()->setText(path);
}

void MainWindowTester::setFileMask(const QString &mask)
{
  getFileMaskEdit()->setText(mask);
}

void MainWindowTester::setXorKey(const QString &key)
{
  getXorKeyEdit()->setText(key);
}

void MainWindowTester::setConflictMode(int mode)
{
  switch (mode)
  {
  case 0:
    getOverwriteRadio()->setChecked(true);
    break;
  case 1:
    getSkipRadio()->setChecked(true);
    break;
  case 2:
    getNumberingRadio()->setChecked(true);
    break;
  }
}

void MainWindowTester::setManualMode()
{
  getManualRadio()->setChecked(true);
}

void MainWindowTester::setTimerMode(int intervalMs)
{
  getTimerRadio()->setChecked(true);
  QSpinBox *spin = m_window->findChild<QSpinBox *>();
  if (spin)
    spin->setValue(intervalMs);
}

void MainWindowTester::setWatcherMode()
{
  getWatcherRadio()->setChecked(true);
}

void MainWindowTester::setDeleteInput(bool deleteInput)
{
  QCheckBox *check = m_window->findChild<QCheckBox *>();
  if (check)
    check->setChecked(deleteInput);
}

GuiState MainWindowTester::getCurrentState() const
{
  GuiState state;

  QPushButton *startBtn = findButton("Старт");
  QPushButton *pauseBtn = findButton("Пауза");
  QPushButton *continueBtn = findButton("Продолжить");
  QPushButton *stopBtn = findButton("Стоп");

  state.startEnabled = startBtn ? startBtn->isEnabled() : false;

  if (pauseBtn)
  {
    state.pauseEnabled = pauseBtn->isEnabled();
    state.pauseText = pauseBtn->text();
  }
  else if (continueBtn)
  {
    state.pauseEnabled = continueBtn->isEnabled();
    state.pauseText = continueBtn->text();
  }

  state.stopEnabled = stopBtn ? stopBtn->isEnabled() : false;

  QLabel *statusLabel = getStatusLabel();
  if (statusLabel)
    state.statusText = statusLabel->text();

  QTableWidget *table = getFileTable();
  if (table)
    state.tableRowCount = table->rowCount();

  QLabel *statsLabel = getStatsLabel();
  if (statsLabel)
  {
    parseStats(statsLabel->text(),
               state.processedCount,
               state.skippedCount,
               state.remainingCount);
  }

  return state;
}

bool MainWindowTester::verifyState(const GuiState &expected, const QString &testName) const
{
  GuiState actual = getCurrentState();

  bool ok = true;
  QString errors;

  if (actual.startEnabled != expected.startEnabled)
  {
    errors += QString("Start: expected %1, got %2\n")
                  .arg(expected.startEnabled)
                  .arg(actual.startEnabled);
    ok = false;
  }
  if (actual.pauseEnabled != expected.pauseEnabled)
  {
    errors += QString("Pause enabled: expected %1, got %2\n")
                  .arg(expected.pauseEnabled)
                  .arg(actual.pauseEnabled);
    ok = false;
  }
  if (actual.pauseText != expected.pauseText)
  {
    errors += QString("Pause text: expected '%1', got '%2'\n")
                  .arg(expected.pauseText, actual.pauseText);
    ok = false;
  }
  if (actual.stopEnabled != expected.stopEnabled)
  {
    errors += QString("Stop: expected %1, got %2\n")
                  .arg(expected.stopEnabled)
                  .arg(actual.stopEnabled);
    ok = false;
  }
  if (actual.tableRowCount != expected.tableRowCount)
  {
    errors += QString("Table rows: expected %1, got %2\n")
                  .arg(expected.tableRowCount)
                  .arg(actual.tableRowCount);
    ok = false;
  }

  if (!ok)
  {
    qWarning() << "State mismatch" << testName << ":\n"
               << errors.trimmed();
    qWarning() << "Actual state:" << actual;
    qWarning() << "Expected state: start=" << expected.startEnabled
               << "pause=" << expected.pauseEnabled << "stop=" << expected.stopEnabled
               << "table=" << expected.tableRowCount;
  }

  return ok;
}

void MainWindowTester::clickStart()
{
  QPushButton *btn = findButton("Старт");
  if (btn && btn->isEnabled())
    btn->click();
}

void MainWindowTester::clickPause()
{
  QPushButton *btn = findButton("Пауза");
  if (!btn)
    btn = findButton("Продолжить");
  if (btn && btn->isEnabled())
    btn->click();
}

void MainWindowTester::clickStop()
{
  QPushButton *btn = findButton("Стоп");
  if (btn && btn->isEnabled())
    btn->click();
}

void MainWindowTester::clickClearCache()
{
  QPushButton *btn = findButton("Очистить кеш");
  if (btn && btn->isEnabled())
    btn->click();
}

void MainWindowTester::clickCheckCache()
{
  QPushButton *btn = findButton("Проверить кеш");
  if (btn && btn->isEnabled())
    btn->click();
}

int MainWindowTester::getTableRowCount() const
{
  QTableWidget *table = getFileTable();
  return table ? table->rowCount() : 0;
}

QString MainWindowTester::getTableRowStatus(int row) const
{
  QTableWidget *table = getFileTable();
  if (!table || row >= table->rowCount())
    return "";
  QTableWidgetItem *item = table->item(row, 1);
  return item ? item->text() : "";
}

QString MainWindowTester::getTableRowProgress(int row) const
{
  QTableWidget *table = getFileTable();
  if (!table || row >= table->rowCount())
    return "";
  QTableWidgetItem *item = table->item(row, 2);
  return item ? item->text() : "";
}

int MainWindowTester::getCacheEntriesCount() const
{
  QString outputDir = getOutputEdit()->text();
  if (outputDir.isEmpty())
    return 0;

  QString cacheFile = outputDir + "/.xor_tool_cache.json";
  QFile file(cacheFile);
  if (!file.open(QIODevice::ReadOnly))
    return 0;

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();

  if (!doc.isObject())
    return 0;
  return doc.object().size();
}

bool MainWindowTester::isCacheFileExists() const
{
  QString outputDir = getOutputEdit()->text();
  if (outputDir.isEmpty())
    return false;
  return QFile::exists(outputDir + "/.xor_tool_cache.json");
}

void MainWindowTester::clearCacheFile()
{
  QString outputDir = getOutputEdit()->text();
  if (outputDir.isEmpty())
    return;
  QFile::remove(outputDir + "/.xor_tool_cache.json");
}

void MainWindowTester::waitForProcessing(int timeoutMs)
{
  QElapsedTimer timer;
  timer.start();

  while (timer.elapsed() < timeoutMs)
  {
    QCoreApplication::processEvents();
    GuiState state = getCurrentState();
    if (state.startEnabled && !state.stopEnabled)
    {
      // Обработка завершена
      break;
    }
    QTest::qWait(100);
  }
}

void MainWindowTester::waitForState(const GuiState &expected, int timeoutMs)
{
  QElapsedTimer timer;
  timer.start();

  while (timer.elapsed() < timeoutMs)
  {
    QCoreApplication::processEvents();
    if (getCurrentState() == expected)
    {
      return;
    }
    QTest::qWait(50);
  }
}

QPushButton *MainWindowTester::findButton(const QString &text) const
{
  for (auto *btn : m_window->findChildren<QPushButton *>())
  {
    if (btn->text() == text)
      return btn;
  }
  return nullptr;
}

QLineEdit *MainWindowTester::getInputEdit() const
{
  auto edits = m_window->findChildren<QLineEdit *>();
  return edits.size() > 0 ? edits[0] : nullptr;
}

QLineEdit *MainWindowTester::getOutputEdit() const
{
  auto edits = m_window->findChildren<QLineEdit *>();
  return edits.size() > 1 ? edits[1] : nullptr;
}

QLineEdit *MainWindowTester::getFileMaskEdit() const
{
  auto edits = m_window->findChildren<QLineEdit *>();
  return edits.size() > 2 ? edits[2] : nullptr;
}

QLineEdit *MainWindowTester::getXorKeyEdit() const
{
  auto edits = m_window->findChildren<QLineEdit *>();
  return edits.size() > 3 ? edits[3] : nullptr;
}

QRadioButton *MainWindowTester::getOverwriteRadio() const
{
  for (auto *r : m_window->findChildren<QRadioButton *>())
  {
    if (r->text() == "Перезаписать")
      return r;
  }
  return nullptr;
}

QRadioButton *MainWindowTester::getSkipRadio() const
{
  for (auto *r : m_window->findChildren<QRadioButton *>())
  {
    if (r->text() == "Пропустить")
      return r;
  }
  return nullptr;
}

QRadioButton *MainWindowTester::getNumberingRadio() const
{
  for (auto *r : m_window->findChildren<QRadioButton *>())
  {
    if (r->text() == "Добавить счётчик")
      return r;
  }
  return nullptr;
}

QRadioButton *MainWindowTester::getManualRadio() const
{
  for (auto *r : m_window->findChildren<QRadioButton *>())
  {
    if (r->text() == "Ручной запуск")
      return r;
  }
  return nullptr;
}

QRadioButton *MainWindowTester::getTimerRadio() const
{
  for (auto *r : m_window->findChildren<QRadioButton *>())
  {
    if (r->text().startsWith("По таймеру"))
      return r;
  }
  return nullptr;
}

QRadioButton *MainWindowTester::getWatcherRadio() const
{
  for (auto *r : m_window->findChildren<QRadioButton *>())
  {
    if (r->text().contains("Отслеживание"))
      return r;
  }
  return nullptr;
}

QTableWidget *MainWindowTester::getFileTable() const
{
  return m_window->findChild<QTableWidget *>();
}

QLabel *MainWindowTester::getStatusLabel() const
{
  auto labels = m_window->findChildren<QLabel *>();
  for (auto *l : labels)
  {
    if (l->text() == "Готов к работе" ||
        l->text().contains("Обработка") ||
        l->text().contains("завершена") ||
        l->text().contains("Ожидание") ||
        l->text().contains("активно"))
    {
      return l;
    }
  }
  return labels.isEmpty() ? nullptr : labels.last();
}

QLabel *MainWindowTester::getStatsLabel() const
{
  for (auto *l : m_window->findChildren<QLabel *>())
  {
    if (l->text().contains("Обработано:"))
      return l;
  }
  return nullptr;
}

QLabel *MainWindowTester::getCacheInfoLabel() const
{
  for (auto *l : m_window->findChildren<QLabel *>())
  {
    if (l->text().contains("Кеш") || l->text().contains("кеш"))
      return l;
  }
  return nullptr;
}

QProgressBar *MainWindowTester::getProgressBar() const
{
  return m_window->findChild<QProgressBar *>();
}

void MainWindowTester::parseStats(const QString &statsText, int &processed, int &skipped, int &remaining) const
{
  processed = skipped = remaining = 0;

  QRegularExpression re("Обработано:\\s*(\\d+)\\s*\\|\\s*Пропущено:\\s*(\\d+)\\s*\\|\\s*Осталось:\\s*(\\d+)");
  QRegularExpressionMatch match = re.match(statsText);

  if (match.hasMatch())
  {
    processed = match.captured(1).toInt();
    skipped = match.captured(2).toInt();
    remaining = match.captured(3).toInt();
  }
}