#ifndef TEST_GUI_HELPERS_H
#define TEST_GUI_HELPERS_H

#include <QObject>
#include <QPushButton>
#include <QLineEdit>
#include <QRadioButton>
#include <QTableWidget>
#include <QLabel>
#include <QProgressBar>
#include <QCheckBox>
#include <QSpinBox>
#include <QTimer>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QApplication>
#include <memory>

class MainWindow;

// Ожидаемое состояние GUI
struct GuiState
{
  bool startEnabled = true;
  bool pauseEnabled = false;
  bool stopEnabled = false;
  QString pauseText = "Пауза";
  QString statusText = "";
  int tableRowCount = 0;
  int processedCount = 0;
  int skippedCount = 0;
  int remainingCount = 0;
  bool timerActive = false;
  bool watcherActive = false;

  bool operator==(const GuiState &other) const
  {
    return startEnabled == other.startEnabled &&
           pauseEnabled == other.pauseEnabled &&
           stopEnabled == other.stopEnabled &&
           pauseText == other.pauseText &&
           statusText == other.statusText &&
           tableRowCount == other.tableRowCount &&
           processedCount == other.processedCount &&
           skippedCount == other.skippedCount &&
           remainingCount == other.remainingCount &&
           timerActive == other.timerActive &&
           watcherActive == other.watcherActive;
  }
};

QDebug operator<<(QDebug dbg, const GuiState &state);

// Helper для автоматического закрытия диалогов
class DialogCloser : public QObject // ДОБАВИТЬ : public QObject
{
Q_OBJECT // ДОБАВИТЬ макрос Q_OBJECT

    public : DialogCloser(int delayMs = 100);
  ~DialogCloser();

  void setActive(bool active);

private slots:
  void closeDialogs();

private:
  QTimer m_timer;
  bool m_active = false;
};

// Тестер MainWindow
class MainWindowTester
{
public:
  explicit MainWindowTester(MainWindow *window);

  // Настройка конфигурации
  void setInputDir(const QString &path);
  void setOutputDir(const QString &path);
  void setFileMask(const QString &mask);
  void setXorKey(const QString &key);
  void setConflictMode(int mode); // 0=Overwrite, 1=Skip, 2=Numbering
  void setManualMode();
  void setTimerMode(int intervalMs);
  void setWatcherMode();
  void setDeleteInput(bool deleteInput);

  // Получение текущего состояния
  GuiState getCurrentState() const;

  // Проверка состояния
  bool verifyState(const GuiState &expected, const QString &testName = "") const;

  // Действия
  void clickStart();
  void clickPause();
  void clickStop();
  void clickClearCache();
  void clickCheckCache();

  // Проверка таблицы
  int getTableRowCount() const;
  QString getTableRowStatus(int row) const;
  QString getTableRowProgress(int row) const;

  // Проверка кеша
  int getCacheEntriesCount() const;
  bool isCacheFileExists() const;
  void clearCacheFile();

  // Ожидание
  void waitForProcessing(int timeoutMs = 5000);
  void waitForState(const GuiState &expected, int timeoutMs = 5000);

  // Получение виджетов
  QPushButton *findButton(const QString &text) const;
  QLineEdit *getInputEdit() const;
  QLineEdit *getOutputEdit() const;
  QLineEdit *getFileMaskEdit() const;
  QLineEdit *getXorKeyEdit() const;
  QRadioButton *getOverwriteRadio() const;
  QRadioButton *getSkipRadio() const;
  QRadioButton *getNumberingRadio() const;
  QRadioButton *getManualRadio() const;
  QRadioButton *getTimerRadio() const;
  QRadioButton *getWatcherRadio() const;
  QTableWidget *getFileTable() const;
  QLabel *getStatusLabel() const;
  QLabel *getStatsLabel() const;
  QLabel *getCacheInfoLabel() const;
  QProgressBar *getProgressBar() const;

private:
  MainWindow *m_window;

  // Парсинг статистики "Обработано: X | Пропущено: Y | Осталось: Z"
  void parseStats(const QString &statsText, int &processed, int &skipped, int &remaining) const;
};

#endif // TEST_GUI_HELPERS_H