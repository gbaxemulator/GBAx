/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This must be defined before anything else is included.
#define SDL_MAIN_HANDLED

#include "ConfigController.h"
#include "FirstRunDialog.h"
#include "GBAApp.h"
#include "LauncherWindow.h"
#include "LauncherData.h"
#include "Window.h"

#include <gbax/core/version.h>
#include <gbax/gba/interface.h>

#ifdef BUILD_SDL
#define SDL_MAIN_HANDLED
#include "platform/sdl/sdl-events.h"
#if SDL_VERSION_ATLEAST(3, 0, 0)
#include <SDL_main.h>
#endif
#endif

#include <QLibraryInfo>
#include <QDateTime>
#include <QElapsedTimer>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QDir>
#include <QTranslator>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMetaObject>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <signal.h>
#include <cerrno>
#endif

#include <QSplashScreen>
#include <QTimer>

#include <QDialog>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QRegularExpression>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QPixmap>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QProcess>
#include <QThread>
#include <QImageReader>
#include <QSslSocket>
#include <QTemporaryFile>


#ifdef BUILD_GLES2
#include <QSurfaceFormat>
#endif

#ifdef QT_STATIC
#include <QtPlugin>
#ifdef Q_OS_WIN
Q_IMPORT_PLUGIN(QJpegPlugin);
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin);
#ifdef BUILD_QT_MULTIMEDIA
Q_IMPORT_PLUGIN(QWindowsAudioPlugin);
Q_IMPORT_PLUGIN(DSServicePlugin);
#endif
#elif defined(Q_OS_MAC)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#ifdef BUILD_QT_MULTIMEDIA
Q_IMPORT_PLUGIN(CoreAudioPlugin);
Q_IMPORT_PLUGIN(AVFServicePlugin);
#endif
#elif defined(Q_OS_UNIX)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
Q_IMPORT_PLUGIN(QWaylandIntegrationPlugin);
#endif
#endif

#ifdef Q_OS_WIN
#include <process.h>
#include <wincon.h>
extern "C" __declspec (dllexport) DWORD NoHotPatch = 0x1;
#else
#include <unistd.h>
#endif

using namespace QGBA;

static QString launcherStatsKeyMain(const QString& path) {
	const QByteArray norm = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()).toUtf8();
	return QString::fromLatin1(QCryptographicHash::hash(norm, QCryptographicHash::Sha1).toHex());
}

static QString launcherCommandServerNameMain() {
	const QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
	const QByteArray key = QCryptographicHash::hash(exePath.toUtf8(), QCryptographicHash::Sha1).toHex();
	return QStringLiteral("GBAxLauncherBridge-%1").arg(QString::fromLatin1(key));
}



static bool waitForPidToExit(quint64 pid, int timeoutMs = 15000) {
	if (!pid) {
		return true;
	}
	QElapsedTimer timer;
	timer.start();
#ifdef Q_OS_WIN
	HANDLE handle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
	if (!handle) {
		return GetLastError() == ERROR_INVALID_PARAMETER;
	}
	const DWORD result = WaitForSingleObject(handle, static_cast<DWORD>(qMax(0, timeoutMs)));
	CloseHandle(handle);
	return result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
#else
	while (timer.elapsed() < timeoutMs) {
		if (::kill(static_cast<pid_t>(pid), 0) != 0 && errno == ESRCH) {
			return true;
		}
		QThread::msleep(100);
	}
	return false;
#endif
}

static bool runResetGbaxHelper(const ConfigController& configController) {
	const quint64 waitPid = configController.getArgvOption(QStringLiteral("wait-pid")).toULongLong();
	if (!waitForPidToExit(waitPid)) {
		QMessageBox::warning(nullptr, QObject::tr("Reset cancelled"), QObject::tr("GBAx is still running. Close it before trying to reset again."));
		return false;
	}
	if (!resetLauncherData(ConfigController::configDir())) {
		QMessageBox::warning(nullptr, QObject::tr("Reset failed"), QObject::tr("GBAx could not remove its settings files. Check that they are writable and try again."));
		return false;
	}

	const QString program = QCoreApplication::applicationFilePath();
	return QProcess::startDetached(program, QStringList(), QFileInfo(program).absolutePath());
}

static QString launcherUiServerNameMain() {
	const QString salt = QCoreApplication::applicationFilePath() + QStringLiteral("|launcher-ui|") + (qEnvironmentVariableIsSet("USERNAME") ? qEnvironmentVariable("USERNAME") : qEnvironmentVariable("USER"));
	return QStringLiteral("gbax-launcher-ui-") + QString::number(qHash(salt), 16);
}




static QPixmap createLauncherLoadingSplashPixmap() {
	QPixmap pixmap(960, 540);
	pixmap.fill(QColor(8, 12, 20));

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

	QLinearGradient bg(0, 0, pixmap.width(), pixmap.height());
	bg.setColorAt(0.0, QColor(7, 11, 18));
	bg.setColorAt(1.0, QColor(16, 24, 39));
	painter.fillRect(pixmap.rect(), bg);

	const QRect contentRect = pixmap.rect().adjusted(80, 60, -80, -60);
	QPixmap logoPixmap(":/res/gbax-logo-launcher.png");
	int y = contentRect.top() + 70;
	if (!logoPixmap.isNull()) {
		const int logoWidth = 320;
		const QPixmap scaledLogo = logoPixmap.scaledToWidth(logoWidth, Qt::SmoothTransformation);
		const int logoX = (pixmap.width() - scaledLogo.width()) / 2;
		painter.drawPixmap(logoX, y, scaledLogo);
		y += scaledLogo.height() + 36;
	}

	QFont titleFont(QStringLiteral("Segoe UI"), 22);
	titleFont.setWeight(QFont::DemiBold);
	painter.setFont(titleFont);
	painter.setPen(QColor(245, 247, 250));
	const QRect titleRect(contentRect.left(), y, contentRect.width(), 42);
	painter.drawText(titleRect, Qt::AlignHCenter | Qt::AlignVCenter, QObject::tr("Loading your games..."));
	y += 52;

	QFont statusFont(QStringLiteral("Segoe UI"), 12);
	painter.setFont(statusFont);
	painter.setPen(QColor(245, 247, 250, 200));
	const QRect statusRect(contentRect.left(), y, contentRect.width(), 28);
	painter.drawText(statusRect, Qt::AlignHCenter | Qt::AlignVCenter, QObject::tr("Please wait"));

	painter.end();
	return pixmap;
}

static QPixmap createLauncherLogoOnlySplashPixmap() {
	QPixmap logoPixmap(":/res/gbax-logo-launcher.png");
	if (logoPixmap.isNull()) {
		return QPixmap();
	}

	const QPixmap scaledLogo = logoPixmap.scaledToWidth(340, Qt::SmoothTransformation);
	QPixmap pixmap(scaledLogo.size());
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
	painter.drawPixmap(0, 0, scaledLogo);
	painter.end();
	return pixmap;
}

static QString artworkCacheDirPathMain() {
	return GBAApp::artworkDir();
}

static QString libretroSystemPathMain(const QString& systemFolder) {
	QString out = systemFolder;
	out.replace(QStringLiteral("_-_"), QStringLiteral("%20-%20"));
	out.replace(QChar('_'), QStringLiteral("%20"));
	return out;
}

static QString catalogCanonicalDisplayTitleMain(const QString& rawTitle) {
	QString t = rawTitle;
	t.remove(QRegularExpression(QStringLiteral("\\s*[\\(\\[].*$")));
	t.replace(QChar('_'), QChar(' '));
	t.replace(QChar('.'), QChar(' '));
	t.replace(QRegularExpression(QStringLiteral("\\s*[-–:]\\s*")), QStringLiteral(" "));
	return t.simplified();
}


static QString canonicalLibretroLookupTitleMain(QString title) {
	title = title.trimmed();
	title.replace(QChar(0x2019), QChar(0x27));
	title.replace(QChar(0x2018), QChar(0x27));
	title.replace(QStringLiteral("_"), QStringLiteral(" "));
	title.replace(QStringLiteral(":"), QStringLiteral(" - "));
	if (title.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) title.chop(4);
	while (true) {
		QString next = title;
		next.remove(QRegularExpression(QStringLiteral("\\s*\\([^\\)]*\\)\\s*$")));
		next = next.trimmed();
		if (next == title) break;
		title = next;
	}
	QRegularExpression artRe(QStringLiteral("^(.*),\\s*(The|A|An)$"), QRegularExpression::CaseInsensitiveOption);
	auto m = artRe.match(title);
	if (m.hasMatch()) {
		title = m.captured(2) + QStringLiteral(" ") + m.captured(1);
	}
	title.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
	return title.simplified();
}

static QString weakLibretroLookupTitleMain(QString title) {
	title = canonicalLibretroLookupTitleMain(title).toLower();
	title.replace(QStringLiteral("&"), QStringLiteral("and"));
	title.remove(QRegularExpression(QStringLiteral("[^a-z0-9]+")));
	return title;
}

static QStringList fetchLibretroGbaBoxartIndexMain() {
	static QStringList s_entries;
	static bool s_loaded = false;
	if (s_loaded) return s_entries;
	s_loaded = true;
	GBAApp* app = GBAApp::app();
	if (!app) return s_entries;
	QEventLoop loop;
	QNetworkReply* reply = app->httpGet(QUrl(QStringLiteral("https://thumbnails.libretro.com/Nintendo%20-%20Game%20Boy%20Advance/Named_Boxarts/")));
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();
	const QByteArray body = reply->readAll();
	reply->deleteLater();
	const QString html = QString::fromUtf8(body);
	QRegularExpression re(QStringLiteral("href=\"([^\"]+\\.png)\""));
	auto it = re.globalMatch(html);
	while (it.hasNext()) {
		QString name = it.next().captured(1);
		name = QUrl::fromPercentEncoding(name.toUtf8());
		if (!name.isEmpty()) s_entries << name;
	}
	s_entries.removeDuplicates();
	return s_entries;
}

static QString bestLibretroGbaBoxartNameForTitleMain(const QString& title) {
	const QStringList entries = fetchLibretroGbaBoxartIndexMain();
	if (entries.isEmpty()) return QString();
	const QString wantCanon = canonicalLibretroLookupTitleMain(title);
	const QString wantWeak = weakLibretroLookupTitleMain(title);
	QString best;
	int bestScore = -100000;
	for (const QString& file : entries) {
		QString plain = file;
		if (plain.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) plain.chop(4);
		const QString canon = canonicalLibretroLookupTitleMain(plain);
		const QString weak = weakLibretroLookupTitleMain(plain);
		int score = -1000;
		if (weak == wantWeak) score = 1000;
		else if (canon.compare(wantCanon, Qt::CaseInsensitive) == 0) score = 950;
		else if (!wantWeak.isEmpty() && weak.startsWith(wantWeak)) score = 800;
		else if (!wantWeak.isEmpty() && weak.contains(wantWeak)) score = 700;
		else continue;
		const int lenPenalty = qAbs(plain.size() - title.size());
		score -= lenPenalty;
		if (score > bestScore) {
			bestScore = score;
			best = plain;
		}
	}
	return best;
}
static void trimArtworkCacheMain(const QString& dirPath, int maxFiles = 1000, qint64 maxBytes = 300LL * 1024 * 1024) {
	QDir dir(dirPath);
	if (!dir.exists()) return;
	QFileInfoList files = dir.entryInfoList(QStringList() << QStringLiteral("*.png") << QStringLiteral("*.jpg") << QStringLiteral("*.jpeg") << QStringLiteral("*.webp"), QDir::Files, QDir::Time);
	qint64 total = 0;
	for (const QFileInfo& fi : files) total += fi.size();
	for (int i = files.size() - 1; i >= 0 && (files.size() > maxFiles || total > maxBytes); --i) {
		const QFileInfo fi = files.at(i);
		total -= fi.size();
		QFile::remove(fi.absoluteFilePath());
		files.removeAt(i);
	}
}

static QString coverThumbCacheDirPathMain() {
	return QDir(GBAApp::artworkDir()).filePath(QStringLiteral("thumbs"));
}

static QString prebuiltThumbnailPathForArtMain(const QString& artPath, const QString& variant) {
	const QFileInfo info(artPath);
	const QString keySource = QDir::toNativeSeparators(info.absoluteFilePath())
		+ QStringLiteral("|") + QString::number(info.lastModified().toMSecsSinceEpoch())
		+ QStringLiteral("|") + variant;
	const QByteArray key = QCryptographicHash::hash(keySource.toUtf8(), QCryptographicHash::Sha1).toHex();
	return QDir(coverThumbCacheDirPathMain()).filePath(QString::fromLatin1(key) + QStringLiteral(".png"));
}

static bool buildFittedThumbnailMain(const QString& artPath, const QSize& size, const QString& outPath) {
	QPixmap original(artPath);
	if (original.isNull()) {
		return false;
	}
	QPixmap fitted(size);
	fitted.fill(Qt::transparent);
	QPainter fitPainter(&fitted);
	fitPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
	const QPixmap scaledOriginal = original.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	const int drawX = (size.width() - scaledOriginal.width()) / 2;
	const int drawY = (size.height() - scaledOriginal.height()) / 2;
	fitPainter.drawPixmap(drawX, drawY, scaledOriginal);
	fitPainter.end();
	QDir().mkpath(QFileInfo(outPath).absolutePath());
	return fitted.save(outPath, "PNG");
}

static void ensurePrebuiltArtworkThumbsMain(const QString& artPath) {
	if (artPath.isEmpty() || !QFileInfo::exists(artPath)) {
		return;
	}
	QDir().mkpath(coverThumbCacheDirPathMain());
	buildFittedThumbnailMain(artPath, QSize(196, 214), prebuiltThumbnailPathForArtMain(artPath, QStringLiteral("grid")));
	buildFittedThumbnailMain(artPath, QSize(160, 160), prebuiltThumbnailPathForArtMain(artPath, QStringLiteral("square")));
}

class ArtworkCacheBuilderDialog final : public QDialog {
	Q_OBJECT
public:
	explicit ArtworkCacheBuilderDialog(ConfigController* config, QWidget* parent = nullptr)
		: QDialog(parent)
		, m_config(config) {
		setWindowTitle(tr("Build Artwork Cache"));
		setMinimumWidth(560);
		setModal(false);
		auto* layout = new QVBoxLayout(this);
		layout->setContentsMargins(16, 16, 16, 16);
		layout->setSpacing(10);
		auto* title = new QLabel(tr("Building Browse artwork cache in a separate helper process"), this);
		QFont tf = title->font(); tf.setBold(true); tf.setPointSize(tf.pointSize() + 1); title->setFont(tf);
		layout->addWidget(title);
		m_status = new QLabel(tr("Preparing catalog..."), this);
		m_status->setWordWrap(true);
		layout->addWidget(m_status);
		m_progress = new QProgressBar(this);
		m_progress->setRange(0, 0);
		m_started.start();
		layout->addWidget(m_progress);
		m_detail = new QLabel(QString(), this);
		m_detail->setTextInteractionFlags(Qt::TextSelectableByMouse);
		m_detail->setWordWrap(true);
		layout->addWidget(m_detail);
		auto* buttons = new QHBoxLayout();
		buttons->addStretch(1);
		m_closeButton = new QPushButton(tr("Close"), this);
		m_closeButton->setEnabled(false);
		connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
		buttons->addWidget(m_closeButton);
		layout->addLayout(buttons);
		loadCatalog();
		m_progress->setRange(0, m_entries.size());
		m_progress->setValue(0);
		connect(this, &QDialog::finished, qApp, &QCoreApplication::quit);
		QTimer::singleShot(0, this, [this]() { processNext(); });
	}
private:
	struct Entry { QString title; QString art; };
	void loadCatalog() {
		QFile file(QStringLiteral(":/res/gba_catalog_us.json"));
		if (!file.open(QIODevice::ReadOnly)) {
			file.setFileName(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("gba_catalog_us.json")));
			if (!file.open(QIODevice::ReadOnly)) {
				file.setFileName(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("res/gba_catalog_us.json")));
				file.open(QIODevice::ReadOnly);
			}
		}
		if (!file.isOpen()) return;
		const auto doc = QJsonDocument::fromJson(file.readAll());
		if (!doc.isArray()) return;
		for (const QJsonValue& value : doc.array()) {
			if (!value.isObject()) continue;
			const QJsonObject obj = value.toObject();
			const QString title = obj.value(QStringLiteral("title")).toString().trimmed();
			if (title.isEmpty()) continue;
			const QString art = obj.value(QStringLiteral("libretro_name")).toString().trimmed();
			m_entries.push_back({title, art.isEmpty() ? title : art});
		}
	}
	QString coverCachePathForCatalogTitle(const QString& title, const QString& art) const {
		const QByteArray key = QCryptographicHash::hash((QStringLiteral("v2|") + title + QStringLiteral("|") + art).toUtf8(), QCryptographicHash::Sha1).toHex();
		return QDir(artworkCacheDirPathMain()).filePath(QString::fromLatin1(key) + QStringLiteral(".png"));
	}
	QStringList candidatesFor(const Entry& e) const {
		QStringList out;
		out << e.art << e.title;
		const QString display = catalogCanonicalDisplayTitleMain(e.title);
		if (!display.isEmpty()) out << display;
		QString cleaned = e.art;
		cleaned.remove(QRegularExpression(QStringLiteral("\\s*[\\(\\[].*$")));
		cleaned = cleaned.simplified();
		if (!cleaned.isEmpty()) out << cleaned;
		out.removeAll(QString());
		out.removeDuplicates();
		return out;
	}
	void updateProgressText(const QString& currentTitle) {
		const int total = m_entries.size();
		const int processed = qMin(m_index, total);
		const qint64 elapsedMs = qMax<qint64>(1, m_started.elapsed());
		const double elapsedSeconds = elapsedMs / 1000.0;
		const double mbPerSecond = elapsedSeconds > 0.0 ? (double(m_downloadedBytes) / (1024.0 * 1024.0)) / elapsedSeconds : 0.0;
		double secondsRemaining = 0.0;
		if (processed > 0 && elapsedSeconds > 0.0) {
			const double itemsPerSecond = processed / elapsedSeconds;
			if (itemsPerSecond > 0.0) {
				secondsRemaining = qMax(0.0, (total - processed) / itemsPerSecond);
			}
		}

		QString etaText = tr("Calculating...");
		if (secondsRemaining > 0.0) {
			const int etaSeconds = qRound(secondsRemaining);
			const int minutes = etaSeconds / 60;
			const int seconds = etaSeconds % 60;
			if (minutes > 0) {
				etaText = tr("~%1m %2s").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));
			} else {
				etaText = tr("~%1s").arg(seconds);
			}
		}

		m_status->setText(tr("Building artwork cache... %1 / %2").arg(processed).arg(total));
		m_detail->setText(
			tr("Current: %1\nSpeed: %2 MB/s\nETA: %3\n\nDownloaded: %4\nAlready cached: %5\nFailed: %6")
				.arg(currentTitle.isEmpty() ? tr("(starting)") : currentTitle,
					QString::number(mbPerSecond, 'f', 1),
					etaText,
					QString::number(m_downloadedCount),
					QString::number(m_cachedCount),
					QString::number(m_failedCount))
		);
	}

	void processNext() {
		if (m_index >= m_entries.size()) {
			m_progress->setValue(m_entries.size());
			m_status->setText(tr("Artwork cache built successfully."));
			m_detail->setText(tr("%1 covers processed\n%2 downloaded\n%3 already cached\n%4 failed\n\nRestart GBAx to load the newly downloaded Browse artwork.")
				.arg(m_entries.size())
				.arg(m_downloadedCount)
				.arg(m_cachedCount)
				.arg(m_failedCount));
			m_closeButton->setEnabled(true);
			return;
		}
		const Entry e = m_entries.at(m_index);
		m_progress->setValue(m_index);
		updateProgressText(e.title);
		const QString outPath = coverCachePathForCatalogTitle(e.title, e.art);
		if (QFileInfo::exists(outPath)) {
			ensurePrebuiltArtworkThumbsMain(outPath);
			++m_cachedCount;
			++m_index;
			QTimer::singleShot(0, this, [this]() { processNext(); });
			return;
		}
		QDir().mkpath(artworkCacheDirPathMain());
		const QString system = QStringLiteral("Nintendo_-_Game_Boy_Advance");
		QStringList candidates = candidatesFor(e);
		const QString matchedArt = bestLibretroGbaBoxartNameForTitleMain(e.art);
		const QString matchedTitle = bestLibretroGbaBoxartNameForTitleMain(e.title);
		if (!matchedArt.isEmpty()) candidates.prepend(matchedArt);
		if (!matchedTitle.isEmpty() && matchedTitle.compare(matchedArt, Qt::CaseInsensitive) != 0) candidates.prepend(matchedTitle);
		candidates.removeAll(QString());
		candidates.removeDuplicates();
		bool saved = false;
		for (const QString& candidate : candidates) {
			const QUrl url(QStringLiteral("https://thumbnails.libretro.com/%1/Named_Boxarts/%2")
				.arg(libretroSystemPathMain(system), QString::fromUtf8(QUrl::toPercentEncoding(candidate + QStringLiteral(".png")))));
			QNetworkReply* reply = GBAApp::app()->httpGet(url);
			QEventLoop loop;
			QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
			loop.exec();
			const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			const QByteArray body = reply->readAll();
			reply->deleteLater();
			QPixmap pm;
			if (status == 200 && !body.isEmpty() && pm.loadFromData(body)) {
				QFile f(outPath);
				if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
					f.write(body);
					f.close();
					ensurePrebuiltArtworkThumbsMain(outPath);
					m_downloadedBytes += body.size();
					++m_downloadedCount;
					trimArtworkCacheMain(artworkCacheDirPathMain());
					saved = true;
				}
				break;
			}
		}
		if (!saved) {
			++m_failedCount;
		}
		++m_index;
		QTimer::singleShot(0, this, [this]() { processNext(); });
	}
	ConfigController* m_config = nullptr;
	QLabel* m_status = nullptr;
	QLabel* m_detail = nullptr;
	QProgressBar* m_progress = nullptr;
	QPushButton* m_closeButton = nullptr;
	QList<Entry> m_entries;
	QElapsedTimer m_started;
	qint64 m_downloadedBytes = 0;
	int m_downloadedCount = 0;
	int m_cachedCount = 0;
	int m_failedCount = 0;
	int m_index = 0;
};

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
	AttachConsole(ATTACH_PARENT_PROCESS);
#endif
#ifdef BUILD_SDL
#if SDL_VERSION_ATLEAST(2, 0, 0) // CPP does not shortcut function lookup
	SDL_SetMainReady();
#endif
#endif

	ConfigController configController;

	QLocale locale;
	if (!configController.getQtOption("language").isNull()) {
		locale = QLocale(configController.getQtOption("language").toString());
		QLocale::setDefault(locale);
	}

	if (configController.parseArguments(argc, argv)) {
		if (configController.args()->showHelp) {
			configController.usage(argv[0]);
			return 0;
		}
		if (configController.args()->showVersion) {
			version(argv[0]);
			return 0;
		}
	} else {
		configController.usage(argv[0]);
		return 1;
	}

	QApplication::setApplicationName(projectName);
	QApplication::setApplicationVersion(QStringLiteral("2.0.0"));
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
	QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif

#ifdef BUILD_GLES2
	QSurfaceFormat format;
	format.setVersion(3, 0);
	QSurfaceFormat::setDefaultFormat(format);
#endif

	const bool packageCheck = configController.getArgvOption(QStringLiteral("package-check")).toBool();
	if (packageCheck) configController.setOption("updateAutoCheck", false);
	GBAApp application(argc, argv, &configController);
	if (packageCheck) {
		// Run on the deployed application, with no ROMs, network calls or welcome
		// dialog. This verifies dependencies that --version cannot exercise.
		QJsonObject checks;
		checks.insert("tls", QSslSocket::supportsSsl());
		const auto formats = QImageReader::supportedImageFormats();
		checks.insert("png", formats.contains("png"));
		checks.insert("jpeg", formats.contains("jpeg") || formats.contains("jpg"));
		QFile catalog(QStringLiteral(":/res/gba_catalog_us.json"));
		checks.insert("catalog", catalog.open(QIODevice::ReadOnly) && !QJsonDocument::fromJson(catalog.readAll()).isNull());
		checks.insert("database", QFileInfo::exists(QDir(GBAApp::dataDir()).filePath("nointro.dat")));
		checks.insert("shaders", !QDir(QDir(GBAApp::dataDir()).filePath("shaders")).entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot).isEmpty());
		QDir().mkpath(GBAApp::artworkDir());
		QTemporaryFile writable(QDir(GBAApp::artworkDir()).filePath("package-check-XXXXXX"));
		checks.insert("writableArtwork", writable.open());
#ifdef BUILD_SDL
		checks.insert("controllerSupport", true);
#else
		checks.insert("controllerSupport", false);
#endif
		bool success = true;
		for (auto it = checks.begin(); it != checks.end(); ++it) success &= it.value().toBool();
		QJsonObject report{{"checks", checks}, {"passed", success}, {"qt", qVersion()}, {"version", projectVersion}};
		const QByteArray json = QJsonDocument(report).toJson(QJsonDocument::Compact);
		fprintf(stdout, "%s\n", json.constData());
		return success ? 0 : 2;
	}
	QPalette startupPalette;
	startupPalette.setColor(QPalette::Window, QColor(8, 12, 20));
	startupPalette.setColor(QPalette::Base, QColor(8, 12, 20));
	startupPalette.setColor(QPalette::AlternateBase, QColor(8, 12, 20));
	startupPalette.setColor(QPalette::Button, QColor(8, 12, 20));
	startupPalette.setColor(QPalette::WindowText, QColor(245, 247, 250));
	startupPalette.setColor(QPalette::Text, QColor(245, 247, 250));
	startupPalette.setColor(QPalette::ButtonText, QColor(245, 247, 250));
	application.setPalette(startupPalette);
	// Keep the app alive during emulator-window teardown/creation handoffs.
	// The launcher is our real shell, so transient "no visible game window" moments
	// should never cause the whole app to quit.
	const bool hasCliRom = configController.args() && configController.args()->fname;
	application.setQuitOnLastWindowClosed(hasCliRom);

	const bool artworkCacheBuilderMode = configController.getArgvOption(QStringLiteral("artwork-cache-builder")).toBool();
	const bool resetGbaxHelperMode = configController.getArgvOption(QStringLiteral("reset-gbax-helper")).toBool();
	if (resetGbaxHelperMode) {
		return runResetGbaxHelper(configController) ? 0 : 1;
	}
	if (artworkCacheBuilderMode) {
		application.setQuitOnLastWindowClosed(true);
		auto* dlg = new ArtworkCacheBuilderDialog(&configController);
		dlg->setAttribute(Qt::WA_DeleteOnClose, true);
		dlg->show();
		return application.exec();
	}

#ifndef Q_OS_MAC
	QApplication::setWindowIcon(QIcon(":/res/gbax-256.png"));
#endif

#ifdef Q_OS_UNIX
	QApplication::setDesktopFileName(QString("io.gbax.GBAx"));
#endif

	QTranslator qtTranslator;
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
	if (qtTranslator.load(locale, "qt", "_", QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
#else
	if (qtTranslator.load(locale, "qt", "_", QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
#endif
		application.installTranslator(&qtTranslator);
	}

#ifdef QT_STATIC
	QTranslator qtStaticTranslator;
	qtStaticTranslator.load(locale, "qtbase", "_", ":/translations/");
	application.installTranslator(&qtStaticTranslator);
#endif

	QTranslator langTranslator;
	if (langTranslator.load(locale, binaryName, "-", ":/translations/")) {
		application.installTranslator(&langTranslator);
	}

	// Startup animation (splash)
	QSplashScreen* splash = nullptr;
	const bool splashPreferenceEnabled = configController.getQtOption("showLauncherSplash", "gbax").toBool();
	const bool firstRun = !configController.getQtOption("firstRunComplete", "gbax").toBool();
	const bool shouldShowRegularSplash = !hasCliRom && splashPreferenceEnabled && !firstRun;
	const bool shouldShowPostWelcomeLogoSplash = !hasCliRom && firstRun;
	auto destroySplash = [&splash]() {
		if (!splash) {
			return;
		}
		splash->close();
		splash->deleteLater();
		splash = nullptr;
	};
	auto showSplashPixmap = [&application, &splash, &destroySplash](const QPixmap& splashPixmap) {
		destroySplash();
		if (splashPixmap.isNull()) {
			return;
		}
		splash = new QSplashScreen(splashPixmap, Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
		splash->setAttribute(Qt::WA_TranslucentBackground, true);
		splash->setAttribute(Qt::WA_DeleteOnClose, false);
		splash->show();
		splash->raise();
		application.processEvents();
	};
	auto showLauncherSplash = [&showSplashPixmap]() {
		showSplashPixmap(createLauncherLoadingSplashPixmap());
	};
	auto showPostWelcomeLogoSplash = [&showSplashPixmap]() {
		showSplashPixmap(createLauncherLogoOnlySplashPixmap());
	};
	if (shouldShowRegularSplash) {
		showLauncherSplash();
	}

	// First-run welcome screen
	if (firstRun) {
		destroySplash();
		FirstRunDialog dlg(&configController);
		if (dlg.exec() != QDialog::Accepted) {
			return 0;
		}
		if (shouldShowPostWelcomeLogoSplash) {
			showPostWelcomeLogoSplash();
		}
	}

	LauncherWindow* launcher = nullptr;
	Window* w = nullptr;
	const QString cliRomPath = hasCliRom ? QDir::toNativeSeparators(QFileInfo(QString::fromUtf8(configController.args()->fname)).absoluteFilePath()) : QString();
	QElapsedTimer runtimeTimer;
	QLocalServer* launcherBridgeServer = nullptr;
	QLocalServer* launcherUiServer = nullptr;
	if (hasCliRom) {
		runtimeTimer.start();
		w = application.newWindow();
		if (w) {
			w->argumentsPassed();
			QObject::connect(w, &QObject::destroyed, &application, &QCoreApplication::quit);

			const QString serverName = launcherCommandServerNameMain();
			QLocalServer::removeServer(serverName);
			launcherBridgeServer = new QLocalServer(&application);
			if (launcherBridgeServer->listen(serverName)) {
				QObject::connect(launcherBridgeServer, &QLocalServer::newConnection, &application, [launcherBridgeServer, w = QPointer<Window>(w)]() {
					while (launcherBridgeServer->hasPendingConnections()) {
						QLocalSocket* socket = launcherBridgeServer->nextPendingConnection();
						QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, w]() {
							if (!socket->canReadLine()) return;
							const QString command = QString::fromUtf8(socket->readLine()).trimmed();
							if (command == QLatin1String("open_settings") && w) {
								QMetaObject::invokeMethod(w, [w]() {
									w->showNormal();
									w->setWindowState((w->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
									w->raise();
									w->activateWindow();
#ifdef Q_OS_WIN
									if (HWND hwnd = reinterpret_cast<HWND>(w->winId())) {
										ShowWindow(hwnd, SW_RESTORE);
										BringWindowToTop(hwnd);
										SetForegroundWindow(hwnd);
									}
#endif
									w->openSettingsWindow();
								}, Qt::QueuedConnection);
							} else if ((command == QLatin1String("show_window") || command == QLatin1String("close_game")) && w) {
								QMetaObject::invokeMethod(w, [w, command]() {
									if (command == QLatin1String("close_game")) {
										w->close();
										return;
									}
									if (w->windowState() & Qt::WindowMinimized) {
										w->setWindowState((w->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
										w->showNormal();
									} else {
										w->show();
									}
									w->raise();
									w->activateWindow();
#ifdef Q_OS_WIN
									if (HWND hwnd = reinterpret_cast<HWND>(w->winId())) {
										ShowWindow(hwnd, SW_RESTORE);
										BringWindowToTop(hwnd);
										SetForegroundWindow(hwnd);
									}
#endif
								}, Qt::QueuedConnection);
							}
							socket->disconnectFromServer();
						});
						QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
					}
				});
				QObject::connect(&application, &QCoreApplication::aboutToQuit, &application, [launcherBridgeServer, serverName]() {
					if (launcherBridgeServer) {
						launcherBridgeServer->close();
					}
					QLocalServer::removeServer(serverName);
				});
			}
		}
	} else {
		auto createAndShowLauncher = [&application, &configController, &launcher, &splash, &destroySplash, &launcherUiServer]() {
			launcher = new LauncherWindow(&configController);
			if (splash) {
				QObject::connect(launcher, &LauncherWindow::startupVisualReady, &application, [launcher, &splash, &destroySplash]() {
					if (splash) {
						splash->finish(launcher);
					}
					destroySplash();
				});
			}
			launcher->show();
			const QString uiServerName = launcherUiServerNameMain();
			QLocalServer::removeServer(uiServerName);
			launcherUiServer = new QLocalServer(&application);
			if (launcherUiServer->listen(uiServerName)) {
				QObject::connect(launcherUiServer, &QLocalServer::newConnection, &application, [launcherUiServer, launcher = QPointer<LauncherWindow>(launcher)]() {
					while (launcherUiServer->hasPendingConnections()) {
						QLocalSocket* socket = launcherUiServer->nextPendingConnection();
						QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, launcher]() {
							if (!socket->canReadLine()) return;
							const QString command = QString::fromUtf8(socket->readLine()).trimmed();
							if (command == QLatin1String("show_launcher") && launcher) {
								QMetaObject::invokeMethod(launcher, [launcher]() {
									if (launcher->windowState() & Qt::WindowMinimized) {
										launcher->setWindowState((launcher->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
										launcher->showNormal();
									} else {
										launcher->show();
									}
									launcher->raise();
									launcher->activateWindow();
#ifdef Q_OS_WIN
									if (HWND hwnd = reinterpret_cast<HWND>(launcher->winId())) {
										ShowWindow(hwnd, SW_RESTORE);
										BringWindowToTop(hwnd);
										SetForegroundWindow(hwnd);
									}
#endif
								}, Qt::QueuedConnection);
							}
							socket->disconnectFromServer();
						});
						QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
					}
				});
				QObject::connect(&application, &QCoreApplication::aboutToQuit, &application, [uiServerName, &launcherUiServer]() {
					if (launcherUiServer) {
						launcherUiServer->close();
					}
					QLocalServer::removeServer(uiServerName);
				});
			}
		};

		if (shouldShowPostWelcomeLogoSplash && splash) {
			QTimer::singleShot(900, &application, createAndShowLauncher);
		} else {
			createAndShowLauncher();
		}
	}


	// Clear any legacy first-run configure flags (settings are opened from the
	// welcome screen now, without closing it).
	configController.setQtOption("firstRunConfigureKeyboard", false, "gbax");
	configController.setQtOption("firstRunConfigureController", false, "gbax");
	if (splash && w) {
		QTimer::singleShot(900, &application, [w, &splash, &destroySplash]() {
			if (splash) {
				splash->finish(w);
			}
			destroySplash();
		});
	} else if (splash && !launcher) {
		QTimer::singleShot(2500, &application, [&destroySplash]() {
			destroySplash();
		});
	}

	application.initMultiplayer();

	int ret = application.exec();
	if (hasCliRom && !cliRomPath.isEmpty() && runtimeTimer.isValid()) {
		const QString key = launcherStatsKeyMain(cliRomPath);
		const QString group = QStringLiteral("launcherPlayStats");
		const qint64 elapsedSecs = qMax<qint64>(1, runtimeTimer.elapsed() / 1000);
		const qint64 currentSecs = configController.getQtOption(key + QStringLiteral("/seconds"), group).toLongLong();
		const int launchCount = configController.getQtOption(key + QStringLiteral("/launchCount"), group).toInt();
		configController.setQtOption(key + QStringLiteral("/seconds"), currentSecs + elapsedSecs, group);
		configController.setQtOption(key + QStringLiteral("/launchCount"), launchCount + 1, group);
		configController.setQtOption(key + QStringLiteral("/lastPlayed"), QDateTime::currentDateTime(), group);
		configController.setQtOption(key + QStringLiteral("/path"), cliRomPath, group);
	}
	if (ret != 0) {
		return ret;
	}
	QString invoke = application.invokeOnExit();
	if (!invoke.isNull()) {
		QByteArray proc = invoke.toUtf8();
#ifdef Q_OS_WIN
		_execl(proc.constData(), proc.constData(), NULL);
#else
		execl(proc.constData(), proc.constData(), NULL);
#endif
	}

	return ret;
}

#ifdef _WIN32
#include <gbax-util/string.h>
#include <vector>

extern "C"
int wmain(int argc, wchar_t* argv[]) {
	std::vector<char*> argv8;
	for (int i = 0; i < argc; ++i) {
		argv8.push_back(utf16to8(reinterpret_cast<uint16_t*>(argv[i]), wcslen(argv[i]) * 2));
	}
	__argv = argv8.data();
	int ret = main(argc, argv8.data());
	for (char* ptr : argv8) {
		free(ptr);
	}
	return ret;
}
#endif

#include "main.moc"
