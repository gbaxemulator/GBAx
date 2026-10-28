/* Copyright (c) 2026 GBAx contributors
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. See http://mozilla.org/MPL/2.0/. */
#include "platform/qt/GBAApp.h"
#include "platform/qt/ConfigController.h"
#include "platform/qt/CoreController.h"
#include "platform/qt/Display.h"
#include "platform/qt/LauncherWindow.h"
#include "platform/qt/Window.h"
#include "platform/qt/FirstRunDialog.h"
#include "platform/qt/SettingsView.h"
#include "platform/qt/LauncherIpc.h"
#include "platform/qt/LauncherStyle.h"
#include "platform/qt/input/GamepadButtonEvent.h"
#include "platform/qt/input/GamepadHatEvent.h"
#include <gbax/internal/gba/input.h>

#include <QDialog>
#include <QCheckBox>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QMenuBar>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>
#include <QSignalSpy>
#include <QThreadPool>
#include <QListWidget>
#include <QElapsedTimer>
#include <QTimer>
#include <QScrollBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QLabel>
#include <QLineEdit>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileOpenEvent>

// Deterministic HTTP fixtures: no external services or local sockets needed.
class ArtworkReply : public QNetworkReply {
public:
	ArtworkReply(const QNetworkRequest& request, QByteArray payload, int delay, bool offline, int httpStatus, QObject* parent)
		: QNetworkReply(parent), m_payload(std::move(payload)), m_offline(offline), m_status(httpStatus) {
		setRequest(request);
		setUrl(request.url());
		open(QIODevice::ReadOnly);
		QTimer::singleShot(delay, this, [this]() {
			if (isFinished()) return;
			if (m_offline) setError(QNetworkReply::HostNotFoundError, "Offline test fixture");
			else setAttribute(QNetworkRequest::HttpStatusCodeAttribute, m_status);
			setFinished(true);
			emit readyRead();
			emit finished();
		});
	}
	void abort() override {
		if (isFinished()) return;
		setError(QNetworkReply::OperationCanceledError, "Cancelled test fixture");
		setFinished(true);
		emit finished();
	}
	qint64 bytesAvailable() const override { return m_payload.size() - m_offset + QNetworkReply::bytesAvailable(); }
protected:
	qint64 readData(char* output, qint64 maxSize) override {
		const qint64 count = qMin<qint64>(maxSize, m_payload.size() - m_offset);
		if (count <= 0) return -1;
		memcpy(output, m_payload.constData() + m_offset, count);
		m_offset += count;
		return count;
	}
private:
	QByteArray m_payload;
	qint64 m_offset = 0;
	bool m_offline;
	int m_status;
};

class ArtworkNetwork : public QNetworkAccessManager {
public:
	int delay = 40;
	bool offline = false;
	int imageStatus = 200;
	int active = 0, peak = 0;
	QStringList requested;
protected:
	QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*) override {
		requested.append(request.url().toString());
		QByteArray payload;
		if (request.url().path().endsWith('/')) {
			payload = "<a href=\"007%20-%20Everything%20or%20Nothing%20%28USA%29.png\">cover</a>";
		} else {
			QImage image(120, 160, QImage::Format_RGB32);
			image.fill(QColor(20, 210, 190));
			QBuffer buffer(&payload);
			buffer.open(QIODevice::WriteOnly);
			image.save(&buffer, "PNG");
		}
		auto* reply = new ArtworkReply(request, payload, delay, offline, request.url().path().endsWith('/') ? 200 : imageStatus, this);
		++active;
		peak = qMax(peak, active);
		connect(reply, &QNetworkReply::finished, this, [this]() { --active; });
		return reply;
	}
};

using namespace QGBA;

class PlayerTest : public QObject {
Q_OBJECT
public:
	explicit PlayerTest(ConfigController* config) : m_config(config) {}

private slots:
	void initTestCase() {
		QVERIFY(m_romDir.isValid());
		// Original, minimal ARM program: branch over the header, then loop.
		// No game assets, firmware or commercial ROM data are used.
		QByteArray rom(1024, '\0');
		rom.replace(0, 4, QByteArray::fromHex("2e0000ea"));
		rom.replace(0xa0, 12, QByteArray("GBAX QA ROM "));
		rom[0xb2] = char(0x96);
		rom.replace(0xc0, 4, QByteArray::fromHex("feffffea"));
		m_rom = m_romDir.filePath("test.gba");
		QFile file(m_rom);
		QVERIFY(file.open(QIODevice::WriteOnly));
		QCOMPARE(file.write(rom), qint64(rom.size()));
		m_config->setOption("romDirectory", m_romDir.path());
		m_config->setOption("skipBios", true);
		m_config->setOption("useBios", false);
		m_config->setOption("pauseOnFocusLost", false);
		m_config->setOption("pauseOnMinimize", false);
		m_config->setOption("audioSync", false);
		m_config->setOption("videoSync", false);
		m_config->setQtOption("autoDownloadCovers", false, "gbaxLauncher");
	}

	void artworkUsesWritableUserDirectory() {
		QCOMPARE(GBAApp::artworkDir(), QDir(ConfigController::configDir()).filePath("artwork/cache"));
		QVERIFY(QDir().mkpath(GBAApp::artworkDir()));
		QTemporaryFile probe(QDir(GBAApp::artworkDir()).filePath("write-check-XXXXXX"));
		QVERIFY(probe.open());
		QVERIFY(!QDir::cleanPath(GBAApp::artworkDir()).startsWith(QDir::cleanPath(GBAApp::dataDir()) + "/"));
	}

	void fileOpenCreatesPlayerWithoutExistingWindow() {
		QVERIFY(GBAApp::app()->windows().isEmpty());
		QFileOpenEvent event(m_rom);
		QApplication::sendEvent(GBAApp::app(), &event);
		QCOMPARE(GBAApp::app()->windows().size(), 1);
		QPointer<Window> window = GBAApp::app()->windows().first();
		QVERIFY(window->controller());
		QSignalSpy frames(window->controller().get(), &CoreController::frameAvailable);
		QTRY_VERIFY(frames.count() >= 3);
		window->close();
		QTRY_VERIFY(window.isNull());
	}

	void fullscreenRequestedBeforeShow() {
		QPointer<Window> window = GBAApp::app()->newWindow(false);
		QVERIFY(window);
		window->enterFullScreen();
		window->show();
		QTRY_VERIFY(window->isFullScreen());
		window->exitFullScreen();
		QVERIFY(!window->isFullScreen());
		window->close();
		QTRY_VERIFY(window.isNull());
	}

	void playerLifecycleAndChrome() {
		for (int run = 0; run < 3; ++run) {
			m_config->setQtOption("maximized", false);
			QPointer<Window> window = GBAApp::app()->newWindow();
			QVERIFY(window);
			MultiplayerController multiplayer;
			CoreManager manager;
			manager.setConfig(m_config->config());
			manager.setMultiplayerController(&multiplayer);
			CoreController* core = manager.loadGame(m_rom);
			QVERIFY(core);
			window->setController(core, m_rom);
			QTRY_VERIFY(window->findChild<QWidget*>("gbaxPlayerHeaderBar"));
			auto* header = window->findChild<QWidget*>("gbaxPlayerHeaderBar");
			auto* grip = window->findChild<QSizeGrip*>("gbaxPlayerResizeGrip");
			QVERIFY(grip);
			QVERIFY(window->windowFlags().testFlag(Qt::FramelessWindowHint));
			QVERIFY(header->isVisible());
			QVERIFY(grip->isVisible());
			QVERIFY(!window->menuBar()->isVisible());
			QSignalSpy frames(core, &CoreController::frameAvailable);
			QTRY_VERIFY(frames.count() >= 3);
			if (run == 0) {
				const QString statePath = m_romDir.filePath("test.ss1");
				core->saveState(statePath);
				QTRY_VERIFY(QFileInfo(statePath).size() > 0);
				QSignalSpy loaded(core, &CoreController::stateLoaded);
				core->loadState(statePath);
				QTRY_COMPARE(loaded.count(), 1);
			}
			window->showMaximized();
			QTRY_VERIFY(window->isMaximized());
			window->enterFullScreen();
			QTRY_VERIFY(window->isFullScreen());
			QVERIFY(!header->isVisible());
			QVERIFY(!grip->isVisible());
			window->exitFullScreen();
			QTRY_VERIFY(window->isMaximized());
			QVERIFY(header->isVisible());
			window->showNormal();
			QTRY_VERIFY(grip->isVisible());

			for (bool initiallyPaused : {false, true}) {
				core->setPaused(initiallyPaused);
				QTRY_COMPARE(core->isPaused(), initiallyPaused);
				GamepadButtonEvent show(GamepadButtonEvent::Down(), 7, 0);
				QApplication::sendEvent(window, &show);
				auto* overlay = window->findChild<QDialog*>("gbaxQuickMenuOverlayBackdrop");
				QVERIFY(overlay && overlay->isVisible());
				QTRY_VERIFY(core->isPaused());
				const mInputHatBindings hat = {GBA_KEY_UP, GBA_KEY_RIGHT, GBA_KEY_DOWN, GBA_KEY_LEFT};
				mInputBindHat(window->inputController()->map(), 1234, 0, &hat);
				const auto buttons = overlay->findChildren<QPushButton*>();
				int selected = -1;
				for (int i = 0; i < buttons.size(); ++i) if (buttons[i]->property("selected").toBool()) selected = i;
				QVERIFY(selected >= 0);
				GamepadHatEvent down(GamepadHatEvent::Down(), 0, GamepadHatEvent::DOWN, 1234, window->inputController());
				QApplication::sendEvent(window, &down);
				QVERIFY(buttons[(selected + 1) % buttons.size()]->property("selected").toBool());
				GamepadButtonEvent hide(GamepadButtonEvent::Down(), 7, 0);
				QApplication::sendEvent(window, &hide);
				QVERIFY(!overlay->isVisible());
				QTRY_COMPARE(core->isPaused(), initiallyPaused);
			}
			window->close();
			QTRY_VERIFY_WITH_TIMEOUT(window.isNull(), 5000);
		}
	}

	void closeLauncherDuringScan() {
		for (int i = 0; i < 400; ++i) QVERIFY(QFile::copy(m_rom, m_romDir.filePath(QString("scan-%1.gba").arg(i))));
		for (int run = 0; run < 5; ++run) {
			auto* launcher = new LauncherWindow(m_config);
		std::unique_ptr<LauncherWindow> launcherOwner(launcher);
			QVERIFY(QMetaObject::invokeMethod(launcher, "refreshLibrary", Qt::DirectConnection, Q_ARG(bool, true)));
			launcherOwner.reset();
			QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		}
		QThreadPool::globalInstance()->waitForDone();
	}

	void launcherWorkflowBenchmark() {
		QTemporaryDir library;
		QVERIFY(library.isValid());
		for (int i = 0; i < 110; ++i) {
			const QString stem = QString("QA Unlisted Game %1").arg(i, 3, 10, QLatin1Char('0'));
			QVERIFY(QFile::copy(m_rom, library.filePath(stem + ".gba")));
			if (i % 2 == 0) {
				QImage cover(600, 800, QImage::Format_RGB32);
				cover.fill(QColor::fromHsv((i * 37) % 360, 150, 170));
				QVERIFY(cover.save(library.filePath(stem + ".png")));
			}
		}
		m_config->setOption("romDirectory", library.path());
		QElapsedTimer elapsed;
		elapsed.start();
		auto* launcher = new LauncherWindow(m_config);
		std::unique_ptr<LauncherWindow> launcherOwner(launcher);
		const qint64 constructed = elapsed.elapsed();
		QSignalSpy ready(launcher, &LauncherWindow::startupVisualReady);
		qint64 previousBeat = elapsed.elapsed(), longestBeat = 0;
		QTimer heartbeat;
		heartbeat.setInterval(10);
		connect(&heartbeat, &QTimer::timeout, launcher, [&]() {
			const qint64 now = elapsed.elapsed();
			longestBeat = qMax(longestBeat, now - previousBeat);
			previousBeat = now;
		});
		heartbeat.start();
		launcher->resize(1370, 900);
		launcher->show();
		QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(), 60000);
		const qint64 shown = elapsed.elapsed();
		auto* grid = launcher->findChild<QListWidget*>("grid");
		QVERIFY(grid);
		QTRY_COMPARE_WITH_TIMEOUT(grid->count(), 110, 60000);
		QTest::qWait(300);
		const qint64 populated = elapsed.elapsed();
		qInfo() << "GRID:" << grid->viewport()->size() << grid->gridSize() << grid->iconSize() << grid->visualItemRect(grid->item(0)) << grid->visualItemRect(grid->item(3));
		const QString images = qEnvironmentVariable("GBAX_QA_OUTPUT_DIR");
		if (!images.isEmpty()) { QDir().mkpath(images); launcher->grab().save(images + "/installed.png"); }
		QElapsedTimer browse;
		browse.start();
		QVERIFY(QMetaObject::invokeMethod(launcher, "showBrowseGames"));
		QTest::qWait(300);
		const qint64 browsed = browse.elapsed();
		if (!images.isEmpty()) launcher->grab().save(images + "/browse.png");
		QVERIFY(QMetaObject::invokeMethod(launcher, "showSettings"));
		QTest::qWait(100);
		if (!images.isEmpty()) launcher->grab().save(images + "/settings.png");
		qInfo("WORKFLOW: constructor=%lld ms ready=%lld ms populated=%lld ms browse=%lld ms maxHeartbeatGap=%lld ms", constructed, shown, populated, browsed, longestBeat);
		QVERIFY2(longestBeat < 1000, "The 110-game workflow blocked the GUI for more than one second");
		heartbeat.stop();
		launcherOwner.reset();
		QThreadPool::globalInstance()->waitForDone();
		m_config->setOption("romDirectory", m_romDir.path());
	}

	void artworkDoesNotBlockStartupAndCanCancel() {
		QTemporaryDir library;
		QVERIFY(QFile::copy(m_rom, library.filePath("007 - Everything or Nothing (USA).gba")));
		m_config->setOption("romDirectory", library.path());
		m_config->setQtOption("autoDownloadCovers", true, "gbaxLauncher");
		ArtworkNetwork network;
		network.delay = 5000;
		auto* launcher = new LauncherWindow(m_config, nullptr, &network);
		std::unique_ptr<LauncherWindow> launcherOwner(launcher);
		QSignalSpy ready(launcher, &LauncherWindow::startupVisualReady);
		launcher->show();
		QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(), 2000);
		auto* grid = launcher->findChild<QListWidget*>("grid");
		QTRY_COMPARE_WITH_TIMEOUT(grid->count(), 1, 2000);
		QTRY_VERIFY(network.active > 0);
		QVERIFY(grid->isEnabled());
		QVERIFY(!launcher->findChild<QWidget*>("startupOverlay"));
		QVERIFY(!launcher->findChild<QWidget*>("scanOverlayBackdrop"));
		QVERIFY(QMetaObject::invokeMethod(launcher, "showSettings"));
		QVERIFY(QMetaObject::invokeMethod(launcher, "startBrowseCoverDownloads"));
		QTRY_COMPARE(network.active, 0);
		const int cancelledRequests = network.requested.size();
		QVERIFY(QMetaObject::invokeMethod(launcher, "showAllGames"));
		QTest::qWait(300);
		QCOMPARE(network.requested.size(), cancelledRequests);
		launcherOwner.reset();
		m_config->setQtOption("autoDownloadCovers", false, "gbaxLauncher");
	}

	void artworkIsSharedAndAppearsWithoutRestart() {
		QTemporaryDir library;
		QVERIFY(QFile::copy(m_rom, library.filePath("007 - Everything or Nothing (USA).gba")));
		m_config->setOption("romDirectory", library.path());
		m_config->setQtOption("autoDownloadCovers", true, "gbaxLauncher");
		ArtworkNetwork network;
		auto* launcher = new LauncherWindow(m_config, nullptr, &network);
		std::unique_ptr<LauncherWindow> launcherOwner(launcher);
		launcher->show();
		auto* grid = launcher->findChild<QListWidget*>("grid");
		QTRY_COMPARE(grid->count(), 1);
		auto hasCover = [grid]() {
			const QImage icon = grid->item(0)->icon().pixmap(grid->iconSize()).toImage();
			if (icon.isNull()) return false;
			const QColor color = icon.pixelColor(icon.width() / 2, icon.height() / 2);
			return color.green() > color.red() + 40;
		};
		QTRY_VERIFY_WITH_TIMEOUT(hasCover(), 5000);
		QVERIFY(std::any_of(network.requested.cbegin(), network.requested.cend(), [](const QString& url) { return QUrl(url).path().endsWith(".png"); }));
		const QStringList sharedCoverRequests = network.requested;
		auto* search = launcher->findChild<QLineEdit*>("searchBox");
		search->setText("007: Everything or Nothing");
		QTest::qWait(200); // Let the search debounce apply before switching pages.
		QVERIFY(QMetaObject::invokeMethod(launcher, "showBrowseGames"));
		QTRY_COMPARE(grid->count(), 1);
		QTRY_VERIFY(hasCover());
		for (const QString& url : sharedCoverRequests) QCOMPARE(network.requested.count(url), 1); // Shared covers are not downloaded again when catalogue preloading starts.
		launcherOwner.reset();
		m_config->setQtOption("autoDownloadCovers", false, "gbaxLauncher");
	}

	void artworkConcurrencyAndOfflineCooldown() {
		QTemporaryDir library;
		for (int i = 0; i < 12; ++i) QVERIFY(QFile::copy(m_rom, library.filePath(QString("Offline Fixture %1.gba").arg(i))));
		m_config->setOption("romDirectory", library.path());
		m_config->setQtOption("autoDownloadCovers", true, "gbaxLauncher");
		ArtworkNetwork network;
		network.offline = true;
		network.delay = 60;
		for (int run = 0; run < 2; ++run) {
			const int previous = network.requested.size();
			auto* launcher = new LauncherWindow(m_config, nullptr, &network);
		std::unique_ptr<LauncherWindow> launcherOwner(launcher);
			launcher->resize(1370, 900);
			launcher->show();
			auto* grid = launcher->findChild<QListWidget*>("grid");
			QTRY_COMPARE(grid->count(), 12);
			QTest::qWait(1000);
			QTRY_COMPARE(network.active, 0);
			QVERIFY(network.peak <= 4);
			if (run == 0) QVERIFY(network.peak > 1);
			else { for (int i = previous; i < network.requested.size(); ++i) QVERIFY(!network.requested.mid(0, previous).contains(network.requested.at(i))); } // Missing/offline covers are not retried at every launch.
			launcherOwner.reset();
		}
		m_config->setQtOption("autoDownloadCovers", false, "gbaxLauncher");
	}

	void automaticArtworkCoversWholeFolderWithoutScrolling() {
		QTemporaryDir library;
		for (int i = 0; i < 72; ++i) {
			const QString stem = QString("Background Fixture %1").arg(i, 3, 10, QLatin1Char('0'));
			QVERIFY(QFile::copy(m_rom, library.filePath(stem + ".gba")));
			if (i < 2) {
				QImage cover(80, 90, QImage::Format_RGB32);
				cover.fill(Qt::red);
				QVERIFY(cover.save(library.filePath(stem + ".png")));
			}
		}
		m_config->setOption("romDirectory", library.path());
		m_config->setQtOption("autoDownloadCovers", true, "gbaxLauncher");
		ArtworkNetwork network;
		auto launcher = std::make_unique<LauncherWindow>(m_config, nullptr, &network);
		launcher->show();
		auto* grid = launcher->findChild<QListWidget*>("grid");
		auto* search = launcher->findChild<QLineEdit*>("searchBox");
		search->setText("No results for this query");
		QVERIFY(QMetaObject::invokeMethod(launcher.get(), "showSettings"));
		auto pngRequests = [&]() { return std::count_if(network.requested.cbegin(), network.requested.cend(), [](const QString& url) { return QUrl(url).path().endsWith(".png"); }); };
		// The complete folder must download while every grid row is filtered out.
		QTRY_COMPARE_WITH_TIMEOUT(pngRequests(), 70, 10000);
		QTRY_COMPARE(network.active, 0);
		QVERIFY(network.peak <= 4);
		QVERIFY(network.peak > 1);
		QCOMPARE(grid->verticalScrollBar()->value(), 0);
		auto* status = launcher->findChild<QLabel*>("artworkDownloadStatus");
		QTRY_VERIFY(status->text().startsWith("Artwork finished"));
		const int completedRequests = network.requested.size();
		QVERIFY(QMetaObject::invokeMethod(launcher.get(), "refreshLibrary", Q_ARG(bool, true)));
		QTest::qWait(500);
		QCOMPARE(network.requested.size(), completedRequests); // Cached covers are not fetched again.

		QCheckBox* automatic = nullptr;
		for (auto* check : launcher->findChildren<QCheckBox*>()) if (check->text().contains("Automatically download")) automatic = check;
		QVERIFY(automatic);
		automatic->setChecked(false);
		QVERIFY(QFile::copy(m_rom, library.filePath("Background Fixture Newly Added.gba")));
		QVERIFY(QMetaObject::invokeMethod(launcher.get(), "refreshLibrary", Q_ARG(bool, true)));
		QTest::qWait(500);
		QCOMPARE(network.requested.size(), completedRequests); // Opt-out is respected after a scan.
		automatic->setChecked(true);
		QTRY_COMPARE(pngRequests(), 71);
		QTRY_VERIFY(status->text().startsWith("Artwork finished"));
		launcher.reset();
		m_config->setQtOption("autoDownloadCovers", false, "gbaxLauncher");
	}

	void continueCardFitsWrappedTitle() {
		QTemporaryDir library;
		const QString rom = library.filePath("Avatar - The Last Airbender Extended Adventure With A Much Longer Game Title.gba");
		QVERIFY(QFile::copy(m_rom, rom));
		const QStringList oldMru = m_config->getMRU(ConfigController::MRU::ROM);
		m_config->setMRU({rom});
		const QString statsKey = QString::fromLatin1(QCryptographicHash::hash(QDir::toNativeSeparators(QFileInfo(rom).absoluteFilePath()).toUtf8(), QCryptographicHash::Sha1).toHex());
		m_config->setQtOption(statsKey + "/seconds", 180, "launcherPlayStats");
		m_config->setQtOption(statsKey + "/lastPlayed", QDateTime(QDate(2026, 10, 28), QTime(9, 15)), "launcherPlayStats");
		m_config->setOption("romDirectory", library.path());
		auto launcher = std::make_unique<LauncherWindow>(m_config);
		launcher->resize(1246, 880);
		launcher->show();
		QTest::qWait(300);
		auto* title = launcher->findChild<QLabel*>("continueTitle");
		QVERIFY(title && title->isVisible());
		QVERIFY(title->fontMetrics().horizontalAdvance(title->text()) > title->width());
		QVERIFY(title->height() >= title->heightForWidth(title->width()));
		QVERIFY(title->geometry().bottom() < title->parentWidget()->height());
		auto* stats = launcher->findChild<QLabel*>("continueStats");
		QVERIFY(stats && stats->isVisible());
		QVERIFY(stats->text().contains("2026"));
		QVERIFY(stats->height() >= stats->heightForWidth(stats->width()));
		QVERIFY(stats->geometry().bottom() < stats->parentWidget()->height() - 8);
		const QString images = qEnvironmentVariable("GBAX_QA_OUTPUT_DIR");
		if (!images.isEmpty()) { QDir().mkpath(images); launcher->grab().save(images + "/continue-card.png"); }
		launcher->resize(1000, 704);
		// Resizing is deliberately debounced. Wait for the resulting layout,
		// including when a busy CI runner delays delivery of the resize timer.
		QTRY_VERIFY(title->height() >= title->heightForWidth(title->width()));
		QVERIFY(stats->height() >= stats->heightForWidth(stats->width()));
		QVERIFY(stats->geometry().bottom() < stats->parentWidget()->height() - 8);
		if (!images.isEmpty()) launcher->grab().save(images + "/continue-card-narrow.png");
		m_config->setMRU(oldMru);
	}

	void rescanDetectsChangesAndKeepsDistinctTitles() {
		QTemporaryDir library;
		const QString custom = library.filePath("Rampage Custom Adventure.gba");
		QVERIFY(QFile::copy(m_rom, custom));
		QVERIFY(QFile::copy(m_rom, library.filePath("Croc.gbc")));
		m_config->setOption("romDirectory", library.path());
		auto* launcher = new LauncherWindow(m_config);
		std::unique_ptr<LauncherWindow> launcherOwner(launcher);
		launcher->show();
		auto* grid = launcher->findChild<QListWidget*>("grid");
		QTRY_COMPARE(grid->count(), 2);
		QCOMPARE(grid->item(0)->text(), QString("Croc"));
		QCOMPARE(grid->item(1)->text(), QString("Rampage Custom Adventure"));
		QVERIFY(QFile::remove(custom));
		QVERIFY(QFile::copy(m_rom, library.filePath("Newly Added.gba")));
		QVERIFY(QMetaObject::invokeMethod(launcher, "refreshLibrary", Q_ARG(bool, true)));
		QTRY_VERIFY(grid->count() == 2 && grid->item(1)->text() == "Newly Added");
		launcherOwner.reset();
	}

	void welcomeWithoutFolderAndSettingsWithoutGame() {
		m_config->setOption("romDirectory", QString());
		m_config->setOption("lastDirectory", QString());
		FirstRunDialog welcome(m_config);
		auto* update = welcome.findChild<QCheckBox*>("welcomeAutoUpdate");
		QVERIFY(update);
#ifndef BUILD_UPDATER
		QVERIFY(update->isHidden());
#endif
		auto* controllerStatus = welcome.findChild<QLabel*>("detectedControllerStatus");
		QVERIFY(controllerStatus);
		QVERIFY(controllerStatus->text().contains("USB") || controllerStatus->text().contains("not included") || controllerStatus->text().startsWith("Detected controller:"));
		const QString images = qEnvironmentVariable("GBAX_QA_OUTPUT_DIR");
		if (!images.isEmpty()) { welcome.show(); QTest::qWait(100); welcome.grab().save(images + "/welcome.png"); }
		welcome.accept();
		QVERIFY(m_config->getOption("romDirectory").isEmpty());
		QVERIFY(m_config->getOption("shader").isEmpty());
		QVERIFY(m_config->getQtOption("firstRunComplete", "gbax").toBool());
		auto* launcher = new LauncherWindow(m_config);
		std::unique_ptr<LauncherWindow> launcherOwner(launcher);
		launcher->show();
		QVERIFY(QMetaObject::invokeMethod(launcher, "showSettings"));
		QPushButton* open = nullptr;
		for (auto* button : launcher->findChildren<QPushButton*>()) if (button->text() == "Open Emulator Settings") open = button;
		QVERIFY(open);
		open->click();
		QPointer<SettingsView> view;
		for (auto* widget : QApplication::topLevelWidgets()) if (auto* settings = qobject_cast<SettingsView*>(widget)) view = settings;
		QVERIFY(view && view->isVisible());
		view->close();
		launcherOwner.reset();
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	}

	void artworkStatesDistinguishQueueDownloadAndFailure() {
		QTemporaryDir library;
		for (int i = 0; i < 8; ++i) QVERIFY(QFile::copy(m_rom, library.filePath(QString("Status Fixture %1.gba").arg(i))));
		m_config->setOption("romDirectory", library.path());
		m_config->setQtOption("autoDownloadCovers", true, "gbaxLauncher");
		ArtworkNetwork network;
		network.imageStatus = 404;
		network.delay = 250;
		auto launcher = std::make_unique<LauncherWindow>(m_config, nullptr, &network);
		launcher->show();
		auto* grid = launcher->findChild<QListWidget*>("grid");
		QTRY_COMPARE(grid->count(), 8);
		auto hasStatus = [grid](const QString& status) { for (int i = 0; i < grid->count(); ++i) if (grid->item(i)->data(Qt::UserRole + 3).toString() == status) return true; return false; };
		QTRY_VERIFY(hasStatus("Queued"));
		QTRY_VERIFY(hasStatus("Downloading"));
		QVERIFY(!hasStatus("No cover found"));
		QTRY_VERIFY_WITH_TIMEOUT(hasStatus("No cover found"), 10000);
		QVERIFY(QMetaObject::invokeMethod(launcher.get(), "startBrowseCoverDownloads")); // Cancel the remaining work.
		launcher.reset();

		QTemporaryDir offlineLibrary;
		QVERIFY(QFile::copy(m_rom, offlineLibrary.filePath("Offline Status Fixture.gba")));
		m_config->setOption("romDirectory", offlineLibrary.path());
		ArtworkNetwork offline;
		offline.offline = true;
		launcher = std::make_unique<LauncherWindow>(m_config, nullptr, &offline);
		launcher->show();
		grid = launcher->findChild<QListWidget*>("grid");
		QTRY_COMPARE(grid->count(), 1);
		QTRY_COMPARE(grid->item(0)->data(Qt::UserRole + 3).toString(), QString("Download failed"));
		launcher.reset();
		m_config->setQtOption("autoDownloadCovers", false, "gbaxLauncher");
	}

	void emptySaveSlotsCollapseAndArtworkStaysVivid() {
		QTemporaryDir library;
		const QString first = library.filePath("Visual Fixture A.gba");
		const QString second = library.filePath("Visual Fixture B.gba");
		QVERIFY(QFile::copy(m_rom, first));
		QVERIFY(QFile::copy(m_rom, second));
		QImage cover(160, 180, QImage::Format_RGB32);
		const QColor original(25, 190, 140);
		cover.fill(original);
		QVERIFY(cover.save(library.filePath("Visual Fixture A.png")));
		m_config->setOption("romDirectory", library.path());
		m_config->setMRU({});
		auto launcher = std::make_unique<LauncherWindow>(m_config);
		launcher->show();
		auto* grid = launcher->findChild<QListWidget*>("grid");
		QTRY_COMPARE(grid->count(), 2);
		QTRY_VERIFY(grid->item(0)->data(Qt::UserRole + 2).toBool());
		auto selected = grid->item(0)->icon().pixmap(grid->iconSize(), QIcon::Selected).toImage();
		QCOMPARE(selected.pixelColor(selected.width()/2, selected.height()/2), original);
		QCOMPARE(grid->item(0)->font().family(), launcher->font().family());
		auto* toggle = launcher->findChild<QPushButton*>("saveSlotsToggle");
		auto* content = launcher->findChild<QWidget*>("saveSlotsContent");
		QVERIFY(toggle && content);
		QTRY_COMPARE(launcher->findChild<QLabel*>("detailsTitle")->text(), QString("Visual Fixture A"));
		QTRY_VERIFY(toggle->text().contains("0/4"));
		QVERIFY(content->isHidden());
		QVERIFY(toggle->parentWidget()->height() < 160);
		toggle->setFocus();
		QTest::keyClick(toggle, Qt::Key_Return);
		QTRY_VERIFY(content->isVisible());
		QTest::keyClick(toggle, Qt::Key_Return);
		QTRY_VERIFY(content->isHidden());
		QTest::qWait(100);
		QVERIFY(toggle->parentWidget()->height() < 160);
		const QString images = qEnvironmentVariable("GBAX_QA_OUTPUT_DIR");
		if (!images.isEmpty()) { QDir().mkpath(images); launcher->grab().save(images + "/empty-save-slots.png"); }
		// Use the genuine state created by playerLifecycleAndChrome.
		QVERIFY(QFile::copy(m_romDir.filePath("test.ss1"), library.filePath("Visual Fixture A.ss1")));
		grid->setCurrentRow(1);
		QTest::qWait(100);
		grid->setCurrentRow(0);
		QTRY_VERIFY(toggle->text().contains("1/4"));
		QVERIFY(content->isVisible());
		const auto slotButtons = content->findChildren<QPushButton*>("saveStatePreviewButton");
		QCOMPARE(slotButtons.size(), 4);
		QVERIFY(slotButtons.first()->isEnabled());
		QVERIFY(!slotButtons.last()->isEnabled());
		if (!images.isEmpty()) { QTest::qWait(100); launcher->grab().save(images + "/populated-save-slots.png"); }
	}

	void cataloguePreloadsWithoutScrolling() {
		QTemporaryDir library;
		m_config->setOption("romDirectory", library.path());
		m_config->setMRU({});
		m_config->setQtOption("autoDownloadCovers", true, "gbaxLauncher");
		ArtworkNetwork network;
		network.delay = 15;
		auto launcher = std::make_unique<LauncherWindow>(m_config, nullptr, &network);
		launcher->resize(1370, 900);
		launcher->show();
		QTest::qWait(150);
		QVERIFY(QMetaObject::invokeMethod(launcher.get(), "showBrowseGames"));
		auto* grid = launcher->findChild<QListWidget*>("grid");
		QVERIFY(QMetaObject::invokeMethod(launcher.get(), "showSettings"));
		QElapsedTimer elapsed;
		elapsed.start();
		qint64 previous = 0, longest = 0;
		QTimer heartbeat;
		heartbeat.setInterval(10);
		connect(&heartbeat, &QTimer::timeout, launcher.get(), [&]() { const auto now = elapsed.elapsed(); longest = qMax(longest, now - previous); previous = now; });
		heartbeat.start();
		QTRY_COMPARE_WITH_TIMEOUT(grid->count(), 947, 10000);
		QCOMPARE(grid->verticalScrollBar()->value(), 0);
		auto allCoversReady = [grid]() {
			for (int i = 0; i < grid->count(); ++i) if (!grid->item(i)->data(Qt::UserRole + 2).toBool()) return false;
			return grid->count() == 947;
		};
		QTRY_VERIFY_WITH_TIMEOUT(allCoversReady(), 30000);
		QTRY_COMPARE(network.active, 0);
		QVERIFY(network.peak <= 4);
		QVERIFY(longest < 1000);
		qInfo("CATALOGUE: all 947 thumbnail icons ready without scrolling in %lld ms; maxHeartbeatGap=%lld ms", elapsed.elapsed(), longest);
		// A jump to the last row must already have artwork, before any repaint timer fires.
		grid->scrollToItem(grid->item(946));
		const QImage icon = grid->item(946)->icon().pixmap(grid->iconSize()).toImage();
		QVERIFY(icon.pixelColor(icon.width()/2, icon.height()/2).green() > 150);
		const int requests = network.requested.size();
		QVERIFY(QMetaObject::invokeMethod(launcher.get(), "startBrowseCoverDownloads")); // Manual request: every cover is already cached.
		QTest::qWait(300);
		QCOMPARE(network.requested.size(), requests);
		launcher.reset();
		m_config->setQtOption("autoDownloadCovers", false, "gbaxLauncher");
	}

private:
	ConfigController* m_config;
	QTemporaryDir m_romDir;
	QString m_rom;
};

int main(int argc, char** argv) {
	// This optional Unix test isolates all preferences from the user's setup.
	QTemporaryDir configRoot;
	if (!configRoot.isValid()) return 1;
	qputenv("XDG_CONFIG_HOME", configRoot.path().toUtf8());
	qputenv("XDG_DATA_HOME", configRoot.path().toUtf8());
	qputenv("SDL_AUDIODRIVER", "dummy");
	QStandardPaths::setTestModeEnabled(true);
	ConfigController config;
	config.parseArguments(1, argv);
	config.setQtOption("displayDriver", static_cast<int>(QGBA::Display::Driver::QT));
	GBAApp app(argc, argv, &config);
	app.setQuitOnLastWindowClosed(false);
	app.initMultiplayer();
	PlayerTest test(&config);
	return QTest::qExec(&test, argc, argv);
}

#include "player.moc"
