#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "mainwindow.h"
#include "test_gui_helpers.h"
#include "test_helpers.h"

class TestMainWindow : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void init();
  void cleanup();

  void test_initialState();
  void test_validation_data();
  void test_validation();
  void test_fullProcessingCycle();
  void test_conflictModes_data();
  void test_conflictModes();
  void test_pauseResume();
  void test_stopDuringProcessing();
  void test_cacheBehavior();
  void test_clearCache();
  void test_doubleStart();
  void test_stopWithoutStart();
  void test_restartAfterFinish();

private:
  QTemporaryDir *m_inputDir = nullptr;
  QTemporaryDir *m_outputDir = nullptr;
  std::unique_ptr<DialogCloser> m_dialogCloser;

  void setupValidConfig(MainWindowTester &tester);
  void cleanDirectories();
};

void TestMainWindow::initTestCase()
{
  m_inputDir = new QTemporaryDir();
  m_outputDir = new QTemporaryDir();
  QVERIFY(m_inputDir->isValid() && m_outputDir->isValid());
}

void TestMainWindow::cleanupTestCase()
{
  delete m_inputDir;
  delete m_outputDir;
}

void TestMainWindow::init()
{
  m_dialogCloser = std::make_unique<DialogCloser>(50);
  m_dialogCloser->setActive(true);
  cleanDirectories();
}

void TestMainWindow::cleanup()
{
  m_dialogCloser->setActive(false);
  cleanDirectories();
}

void TestMainWindow::cleanDirectories()
{
  // Очищаем входную директорию
  QDir inDir(m_inputDir->path());
  for (const QString &file : inDir.entryList(QDir::Files))
  {
    QFile::remove(inDir.absoluteFilePath(file));
  }

  // Очищаем выходную директорию
  QDir outDir(m_outputDir->path());
  for (const QString &file : outDir.entryList(QDir::Files))
  {
    QFile::remove(outDir.absoluteFilePath(file));
  }
}

void TestMainWindow::setupValidConfig(MainWindowTester &tester)
{
  tester.setInputDir(m_inputDir->path());
  tester.setOutputDir(m_outputDir->path());
  tester.setFileMask("*.bin");
  tester.setXorKey("1234567890ABCDEF");
  tester.setManualMode();
  tester.setConflictMode(0);
}

void TestMainWindow::test_initialState()
{
  MainWindow window;
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));

  MainWindowTester tester(&window);

  GuiState expected;
  expected.startEnabled = true;
  expected.pauseEnabled = false;
  expected.stopEnabled = false;
  expected.pauseText = "Пауза";
  expected.tableRowCount = 0;

  QVERIFY2(tester.verifyState(expected, "initial"),
           "Initial GUI state is incorrect");
}

void TestMainWindow::test_validation_data()
{
  QTest::addColumn<QString>("inputDir");
  QTest::addColumn<QString>("outputDir");
  QTest::addColumn<QString>("xorKey");
  QTest::addColumn<QString>("description");

  QTest::newRow("empty_input")
      << "" << m_outputDir->path() << "1234567890ABCDEF"
      << "empty input dir";
  QTest::newRow("empty_output")
      << m_inputDir->path() << "" << "1234567890ABCDEF"
      << "empty output dir";
  QTest::newRow("invalid_key")
      << m_inputDir->path() << m_outputDir->path() << "INVALID"
      << "invalid XOR key";
  QTest::newRow("short_key")
      << m_inputDir->path() << m_outputDir->path() << "1234"
      << "short XOR key";
}

void TestMainWindow::test_validation()
{
  QFETCH(QString, inputDir);
  QFETCH(QString, outputDir);
  QFETCH(QString, xorKey);
  QFETCH(QString, description);

  MainWindow window;
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));

  MainWindowTester tester(&window);
  tester.setInputDir(inputDir);
  tester.setOutputDir(outputDir);
  tester.setXorKey(xorKey);

  GuiState initialState = tester.getCurrentState();

  tester.clickStart();
  QTest::qWait(200);

  QVERIFY2(tester.verifyState(initialState, description),
           qPrintable("State changed after invalid config: " + description));
}

void TestMainWindow::test_fullProcessingCycle()
{
  TestHelpers::createTestFile(m_inputDir->path() + "/test.bin", 1);

  MainWindow window;
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));

  MainWindowTester tester(&window);
  setupValidConfig(tester);

  // Шаг 1: Начальное состояние
  GuiState state1;
  state1.startEnabled = true;
  state1.pauseEnabled = false;
  state1.stopEnabled = false;
  QVERIFY(tester.verifyState(state1, "before start"));

  // Шаг 2: Запускаем обработку и ждём завершения
  tester.clickStart();
  tester.waitForProcessing(5000);

  // Шаг 3: Проверяем конечное состояние
  GuiState state3;
  state3.startEnabled = true;
  state3.pauseEnabled = false;
  state3.stopEnabled = false;
  state3.pauseText = "Пауза";
  state3.tableRowCount = 1;
  state3.processedCount = 1;
  QVERIFY(tester.verifyState(state3, "after finish"));

  QVERIFY(QFile::exists(m_outputDir->path() + "/test.bin.xor"));
  QVERIFY(tester.isCacheFileExists());
  QCOMPARE(tester.getCacheEntriesCount(), 1);
}

void TestMainWindow::test_conflictModes_data()
{
  QTest::addColumn<int>("mode");
  QTest::addColumn<QString>("expectedFile");
  QTest::addColumn<bool>("shouldOverwrite");

  QTest::newRow("Overwrite") << 0 << "test.bin.xor" << true;
  QTest::newRow("Skip") << 1 << "test.bin.xor" << false;
  QTest::newRow("Numbering") << 2 << "test_1.bin.xor" << false;
}

void TestMainWindow::test_conflictModes()
{
  QFETCH(int, mode);
  QFETCH(QString, expectedFile);
  QFETCH(bool, shouldOverwrite);

  TestHelpers::createTestFile(m_inputDir->path() + "/test.bin", 1);

  QString existingPath = m_outputDir->path() + "/test.bin.xor";
  QVERIFY(TestHelpers::createExistingFile(existingPath, "EXISTING"));

  MainWindow window;
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));

  MainWindowTester tester(&window);
  setupValidConfig(tester);
  tester.setConflictMode(mode);

  tester.clickStart();
  tester.waitForProcessing(5000);

  QString expectedPath = m_outputDir->path() + "/" + expectedFile;
  QVERIFY2(QFile::exists(expectedPath),
           qPrintable("Expected file not found: " + expectedPath));

  if (shouldOverwrite)
  {
    QFile file(existingPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(file.readAll() != "EXISTING");
    file.close();
  }
  else
  {
    QVERIFY(TestHelpers::verifyFileContent(existingPath, "EXISTING"));
  }
}

void TestMainWindow::test_pauseResume()
{
  // Создаём большой файл (200 МБ) — обработка займёт несколько секунд
  TestHelpers::createTestFile(m_inputDir->path() + "/large.bin", 50);

  MainWindow window;
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));

  MainWindowTester tester(&window);
  setupValidConfig(tester);
  tester.setFileMask("large.bin");

  tester.clickStart();

  // Ждём, пока начнётся обработка (таблица заполнится)
  QElapsedTimer timer;
  timer.start();
  while (tester.getTableRowCount() == 0 && timer.elapsed() < 2000)
  {
    QTest::qWait(50);
  }

  // Пауза
  tester.clickPause();
  QTest::qWait(200);

  GuiState pausedState;
  pausedState.startEnabled = false;
  pausedState.pauseEnabled = true;
  pausedState.stopEnabled = true;
  pausedState.pauseText = "Продолжить";
  pausedState.tableRowCount = 1;
  QVERIFY(tester.verifyState(pausedState, "paused"));

  // Возобновление
  tester.clickPause();
  QTest::qWait(100);

  GuiState resumedState;
  resumedState.startEnabled = false;
  resumedState.pauseEnabled = true;
  resumedState.stopEnabled = true;
  resumedState.pauseText = "Пауза";
  resumedState.tableRowCount = 1;
  QVERIFY(tester.verifyState(resumedState, "resumed"));

  tester.waitForProcessing(30000);
}

void TestMainWindow::test_stopDuringProcessing()
{
  // Создаём большой файл (200 МБ)
  TestHelpers::createTestFile(m_inputDir->path() + "/large_stop.bin", 50);

  MainWindow window;
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));

  MainWindowTester tester(&window);
  setupValidConfig(tester);
  tester.setFileMask("large_stop.bin");

  tester.clickStart();

  // Ждём, пока начнётся обработка
  QElapsedTimer timer;
  timer.start();
  while (tester.getTableRowCount() == 0 && timer.elapsed() < 2000)
  {
    QTest::qWait(50);
  }

  // Стоп
  tester.clickStop();
  tester.waitForProcessing(5000);

  GuiState stoppedState;
  stoppedState.startEnabled = true;
  stoppedState.pauseEnabled = false;
  stoppedState.stopEnabled = false;
  stoppedState.tableRowCount = 1;
  QVERIFY(tester.verifyState(stoppedState, "stopped"));
}

void TestMainWindow::test_cacheBehavior()
{
  TestHelpers::createTestFile(m_inputDir->path() + "/test.bin", 1);

  MainWindow window;
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));

  MainWindowTester tester(&window);
  setupValidConfig(tester);

  // Первый запуск
  tester.clickStart();
  tester.waitForProcessing(5000);

  QVERIFY(tester.isCacheFileExists());
  QCOMPARE(tester.getCacheEntriesCount(), 1);

  // Второй запуск — файл должен быть пропущен
  tester.clickStart();
  tester.waitForProcessing(5000);

  GuiState state = tester.getCurrentState();
  QCOMPARE(state.processedCount, 0);
  QCOMPARE(state.skippedCount, 1);

  QCOMPARE(tester.getCacheEntriesCount(), 1);
}

void TestMainWindow::test_clearCache()
{
  TestHelpers::createTestFile(m_inputDir->path() + "/test.bin", 1);

  MainWindow window;
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));

  MainWindowTester tester(&window);
  setupValidConfig(tester);

  // Создаём кеш
  tester.clickStart();
  tester.waitForProcessing(5000);
  QVERIFY(tester.isCacheFileExists());

  // Очищаем кеш
  tester.clickClearCache();
  QTest::qWait(200);

  QVERIFY(!tester.isCacheFileExists());
}

void TestMainWindow::test_doubleStart()
{
  // Создаём большой файл, чтобы обработка заняла время
  TestHelpers::createTestFile(m_inputDir->path() + "/test.bin", 50);

  MainWindow window;
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));

  MainWindowTester tester(&window);
  setupValidConfig(tester);

  tester.clickStart();

  // Ждём, пока начнётся обработка
  QElapsedTimer timer;
  timer.start();
  while (tester.getTableRowCount() == 0 && timer.elapsed() < 2000)
  {
    QTest::qWait(50);
  }

  // Второй клик должен игнорироваться
  tester.clickStart();
  QTest::qWait(100);

  GuiState state;
  state.startEnabled = false;
  state.pauseEnabled = true;
  state.stopEnabled = true;
  state.tableRowCount = 1;
  QVERIFY(tester.verifyState(state, "after double start"));

  tester.waitForProcessing(30000);
}

void TestMainWindow::test_stopWithoutStart()
{
  MainWindow window;
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));

  MainWindowTester tester(&window);

  GuiState initialState = tester.getCurrentState();

  tester.clickStop();
  QTest::qWait(100);

  QVERIFY(tester.verifyState(initialState, "after stop without start"));
}
void TestMainWindow::test_restartAfterFinish()
{
  TestHelpers::createTestFile(m_inputDir->path() + "/test.bin", 1);

  MainWindow window;
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));

  MainWindowTester tester(&window);
  setupValidConfig(tester);

  for (int i = 0; i < 3; ++i)
  {
    tester.clickStart();

    QSignalSpy finishedSpy(&window, &MainWindow::workerFinished);
    QVERIFY(finishedSpy.wait(5000));

    QCoreApplication::processEvents();
    QTest::qWait(100);

    GuiState state;
    state.startEnabled = true;
    state.pauseEnabled = false;
    state.stopEnabled = false;
    state.tableRowCount = 1;

    // На первом запуске — обработан, на последующих — пропущен
    if (i == 0)
    {
      state.processedCount = 1;
      state.skippedCount = 0;
    }
    else
    {
      state.processedCount = 0;
      state.skippedCount = 1;
    }

    QVERIFY2(tester.verifyState(state, qPrintable(QString("after restart %1").arg(i + 1))),
             "State incorrect after restart");
  }
}

QTEST_MAIN(TestMainWindow)
#include "test_mainwindow.moc"