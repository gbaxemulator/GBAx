#include "LauncherStyle.h"
#include <QStyledItemDelegate>
#include <QStandardPaths>
/* Copyright (c) 2026 GBAx contributors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LauncherWindow.h"
#include "LauncherIpc.h"

#include "AboutScreen.h"
#include "ConfigController.h"
#include "GBAApp.h"
#include "VFileDevice.h"
#include "Window.h"
#include "SettingsView.h"
#include "ShortcutController.h"
#include "LogController.h"
#include "InputController.h"
#include "input/GamepadAxisEvent.h"
#include "input/GamepadButtonEvent.h"
#include "input/GamepadHatEvent.h"
#ifdef BUILD_SDL
#include <SDL.h>
#endif
#include "LoadSaveState.h"

#include <gbax/core/version.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QButtonGroup>
#include <QDir>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFormLayout>
#include <QDialog>
#include <QDateTime>
#include <QCryptographicHash>
#include <QFile>
#include <QSaveFile>
#include <QBuffer>
#include <QImageReader>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QResizeEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QMessageBox>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QMenu>
#include <QThread>
#include <QMetaObject>
#include <QProgressBar>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QLineEdit>
#include <QLocalSocket>
#include <QListWidget>
#include <QPainter>
#include <QSignalBlocker>
#include <QKeyEvent>
#include <QFileInfo>
#include <QResizeEvent>
#include <QFontMetrics>
#include <QPointer>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyle>
#include <QShowEvent>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QToolButton>
#include <QUrl>
#include <QStyle>
#include <QShowEvent>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <QVBoxLayout>

#ifdef BUILD_SDL
#include "input/SDLInputDriver.h"
#endif

#include <QtConcurrent>

#include <algorithm>
#include <sqlite3.h>

extern "C" {
#include "gbax-util/vfs.h"
#include "gbax/internal/gba/input.h"
#include "gbax/core/serialize.h"
}

using namespace QGBA;

namespace {
struct SaveStatePreviewResult {
	QImage image;
	QString label;
	QString statePath;
	bool hasState = false;
};

QMutex s_saveStatePreviewCacheMutex;
QHash<QString, SaveStatePreviewResult> s_saveStatePreviewCache;

QString saveStatePreviewCacheKey(const QString& statePath, const QSize& size) {
	QFileInfo info(statePath);
	return QStringLiteral("%1|%2x%3|%4|%5")
		.arg(QDir::toNativeSeparators(info.absoluteFilePath()))
		.arg(size.width())
		.arg(size.height())
		.arg(info.exists() ? info.size() : 0)
		.arg(info.exists() ? info.lastModified().toMSecsSinceEpoch() : 0);
}

SaveStatePreviewResult loadSaveStatePreviewResult(const QString& statePath, int slot, const QSize& size) {
	SaveStatePreviewResult result;
	result.statePath = statePath;
	result.label = QObject::tr("Slot %1").arg(slot);
	QFileInfo stateInfo(statePath);
	if (statePath.isEmpty() || !stateInfo.exists()) {
		return result;
	}
	result.hasState = true;
	QFile file(statePath);
	if (!file.open(QIODevice::ReadOnly)) {
		const QString stamp = QLocale().toString(stateInfo.lastModified(), QStringLiteral("d MMM yyyy\nHH:mm"));
		if (!stamp.isEmpty()) {
			result.label = QObject::tr("Slot %1\n%2").arg(slot).arg(stamp);
		}
		return result;
	}
	QByteArray data = file.readAll();
	file.close();
	VFileDevice memoryFile(data);
	mStateExtdata extdata;
	mStateExtdataInit(&extdata);
	if (!mStateExtdataDeserialize(&extdata, memoryFile)) {
		mStateExtdataDeinit(&extdata);
		const QString stamp = QLocale().toString(stateInfo.lastModified(), QStringLiteral("d MMM yyyy\nHH:mm"));
		if (!stamp.isEmpty()) {
			result.label = QObject::tr("Slot %1\n%2").arg(slot).arg(stamp);
		}
		return result;
	}
	QImage stateImage;
	QDateTime creation;
	mStateExtdataItem item;
	QSize imgSize(240, 160);
	if (mStateExtdataGet(&extdata, EXTDATA_SCREENSHOT, &item)) {
		mStateExtdataItem dims;
		if (mStateExtdataGet(&extdata, EXTDATA_SCREENSHOT_DIMENSIONS, &dims) && dims.size == sizeof(uint16_t[2])) {
			imgSize.setWidth(static_cast<uint16_t*>(dims.data)[0]);
			imgSize.setHeight(static_cast<uint16_t*>(dims.data)[1]);
		}
		if (item.size >= static_cast<int32_t>(imgSize.width() * imgSize.height() * 4)) {
			stateImage = QImage(reinterpret_cast<const uchar*>(item.data), imgSize.width(), imgSize.height(), QImage::Format_ARGB32).rgbSwapped().copy();
		}
	}
	if (mStateExtdataGet(&extdata, EXTDATA_META_TIME, &item) && item.size == sizeof(uint64_t)) {
		uint64_t creationUsec;
		LOAD_64LE(creationUsec, 0, item.data);
		creation = QDateTime::fromMSecsSinceEpoch(creationUsec / 1000LL);
	}
	const QDateTime stamp = creation.isValid() ? creation : stateInfo.lastModified();
	if (stamp.isValid()) {
		result.label = QObject::tr("Slot %1\n%2").arg(slot).arg(QLocale().toString(stamp, QStringLiteral("d MMM yyyy\nHH:mm")));
	}
	if (!stateImage.isNull()) {
		result.image = stateImage.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}
	mStateExtdataDeinit(&extdata);
	return result;
}
} // namespace

class ClickOnlyComboBox : public QComboBox {
public:
	explicit ClickOnlyComboBox(QWidget* parent = nullptr)
		: QComboBox(parent) {}

protected:
	void wheelEvent(QWheelEvent* event) override {
		event->ignore();
	}
};

static QString cleanedMetadataValue(const QVariantMap& map, const QString& key);

static QIcon sidebarIcon(const QString& resourcePath, const QSize& size, const QColor& color, qreal dpr = 1.0, int rightPadding = 0) {
	QImage source(resourcePath);
	if (source.isNull()) {
		return QIcon(resourcePath);
	}
	if (source.format() != QImage::Format_ARGB32_Premultiplied) {
		source = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
	}
	QImage tinted(source.size(), QImage::Format_ARGB32_Premultiplied);
	tinted.fill(Qt::transparent);
	{
		QPainter p(&tinted);
		p.setCompositionMode(QPainter::CompositionMode_Source);
		p.drawImage(0, 0, source);
		p.setCompositionMode(QPainter::CompositionMode_SourceIn);
		p.fillRect(tinted.rect(), color);
	}
	const QSize iconPixelSize(qMax(1, qRound(size.width() * dpr)), qMax(1, qRound(size.height() * dpr)));
	const int paddedWidth = iconPixelSize.width() + qMax(0, qRound(rightPadding * dpr));
	QImage scaled = tinted.scaled(iconPixelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	QImage padded(QSize(paddedWidth, iconPixelSize.height()), QImage::Format_ARGB32_Premultiplied);
	padded.fill(Qt::transparent);
	{
		QPainter p(&padded);
		p.setRenderHint(QPainter::SmoothPixmapTransform, true);
		p.drawImage(0, 0, scaled);
	}
	QPixmap pixmap = QPixmap::fromImage(padded);
	pixmap.setDevicePixelRatio(dpr);
	return QIcon(pixmap);
}

static QString libretroSystemPath(const QString& systemFolder) {
	QString out = systemFolder;
	out.replace(QStringLiteral("_-_"), QStringLiteral("%20-%20"));
	out.replace(QChar('_'), QStringLiteral("%20"));
	return out;
}

static QString coverCacheDirPath() {
	return GBAApp::artworkDir();
}

// Older builds stored covers alongside application resources. Read those in
// place, but never write into a signed bundle, AppImage or system install.
static QString existingManagedCover(const QString& preferred) {
	if (QFileInfo::exists(preferred)) return preferred;
	if (QStandardPaths::isTestModeEnabled()) return {};
	const QString legacy = QDir(GBAApp::dataDir()).filePath(QStringLiteral("artwork/cache/") + QFileInfo(preferred).fileName());
	return QFileInfo::exists(legacy) ? legacy : QString();
}

static QString coverThumbCacheDirPath() {
	return QDir(coverCacheDirPath()).filePath(QStringLiteral("thumbs"));
}

static QString cachedThumbnailPathForArt(const QString& artPath, const QSize& size) {
	const QFileInfo info(artPath);
	const QString keySource = QDir::toNativeSeparators(info.absoluteFilePath())
		+ QStringLiteral("|") + QString::number(info.lastModified().toMSecsSinceEpoch())
		+ QStringLiteral("|") + QString::number(size.width())
		+ QStringLiteral("x") + QString::number(size.height());
	const QByteArray key = QCryptographicHash::hash(keySource.toUtf8(), QCryptographicHash::Sha1).toHex();
	return QDir(coverThumbCacheDirPath()).filePath(QString::fromLatin1(key) + QStringLiteral(".png"));
}





static QString launcherLibraryCacheJsonPath() {
	return QDir(ConfigController::configDir()).filePath(QStringLiteral("library-cache.json"));
}

static QString cacheRootKey(const QString& rootDir, bool recursive) {
	return QStringLiteral("%1|%2").arg(QDir::toNativeSeparators(QFileInfo(rootDir).absoluteFilePath()), recursive ? QStringLiteral("1") : QStringLiteral("0"));
}

static QJsonObject loadLauncherLibraryCacheRoot() {
	QFile file(launcherLibraryCacheJsonPath());
	if (!file.open(QIODevice::ReadOnly)) {
		return QJsonObject();
	}
	const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
	file.close();
	return doc.isObject() ? doc.object() : QJsonObject();
}

static bool saveLauncherLibraryCacheRoot(const QJsonObject& root) {
	QDir().mkpath(ConfigController::configDir());
	QSaveFile file(launcherLibraryCacheJsonPath());
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	const QJsonDocument doc(root);
	const QByteArray payload = doc.toJson(QJsonDocument::Indented);
	const qint64 written = file.write(payload);
	return written == payload.size() && file.commit();
}

static QStringList loadCachedLauncherRomList(const QString& rootDir, bool recursive) {
	QStringList out;
	if (rootDir.isEmpty()) {
		return out;
	}
	const QJsonObject root = loadLauncherLibraryCacheRoot();
	const QJsonObject caches = root.value(QStringLiteral("caches")).toObject();
	const QJsonObject entry = caches.value(cacheRootKey(rootDir, recursive)).toObject();
	const QJsonArray roms = entry.value(QStringLiteral("roms")).toArray();
	for (const QJsonValue& value : roms) {
		const QString path = QDir::toNativeSeparators(value.toString());
		if (!path.isEmpty() && QFileInfo::exists(path)) {
			out.append(path);
		}
	}
	out.removeDuplicates();
	return out;
}

static bool hasCachedLauncherRomList(const QString& rootDir, bool recursive) {
	if (rootDir.isEmpty()) {
		return false;
	}
	const QJsonObject root = loadLauncherLibraryCacheRoot();
	const QJsonObject caches = root.value(QStringLiteral("caches")).toObject();
	const QJsonObject entry = caches.value(cacheRootKey(rootDir, recursive)).toObject();
	return entry.contains(QStringLiteral("roms"));
}


static QString canonicalLibretroLookupTitle(QString title) {
	static const QRegularExpression tags(QStringLiteral("\\s*\\([^\\)]*\\)\\s*$"));
	static const QRegularExpression article(QStringLiteral("^(.*),\\s*(The|A|An)$"), QRegularExpression::CaseInsensitiveOption);
	title = title.trimmed();
	title.replace(QChar(0x2019), QChar('\''));
	title.replace(QChar(0x2018), QChar('\''));
	title.replace('_', ' ');
	title.replace(":", " - ");
	if (title.endsWith(".png", Qt::CaseInsensitive)) title.chop(4);
	while (title.contains(tags)) title.remove(tags);
	const auto match = article.match(title.trimmed());
	if (match.hasMatch()) title = match.captured(2) + " " + match.captured(1);
	return title.simplified();
}

static QString weakLibretroLookupTitle(QString title) {
	title = canonicalLibretroLookupTitle(title).toLower();
	title.replace(QStringLiteral("&"), QStringLiteral("and"));
	title.remove(QRegularExpression(QStringLiteral("[^a-z0-9]+")));
	return title;
}





static void saveCachedLauncherRomList(const QString& rootDir, bool recursive, const QStringList& roms, const QJsonObject& archives = {}) {
	static QMutex cacheMutex;
	QMutexLocker lock(&cacheMutex);
	if (rootDir.isEmpty()) {
		return;
	}
	QJsonObject root = loadLauncherLibraryCacheRoot();
	QJsonObject caches = root.value(QStringLiteral("caches")).toObject();
	QJsonObject entry;
	entry.insert(QStringLiteral("rootDir"), QDir::toNativeSeparators(QFileInfo(rootDir).absoluteFilePath()));
	entry.insert(QStringLiteral("recursive"), recursive);
	entry.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
	QJsonArray romArray;
	for (const QString& rom : roms) {
		const QString normalized = QDir::toNativeSeparators(QFileInfo(rom).absoluteFilePath());
		if (!normalized.isEmpty()) {
			romArray.append(normalized);
		}
	}
	entry.insert(QStringLiteral("roms"), romArray);
	entry.insert(QStringLiteral("archives"), archives);
	caches.insert(cacheRootKey(rootDir, recursive), entry);
	root.insert(QStringLiteral("version"), 1);
	root.insert(QStringLiteral("caches"), caches);
	saveLauncherLibraryCacheRoot(root);
}

static QString cachedCoverPathForRom(const QString& romPath) {
	const QByteArray key = QCryptographicHash::hash(QDir::toNativeSeparators(QFileInfo(romPath).absoluteFilePath()).toUtf8(), QCryptographicHash::Sha1).toHex();
	return QDir(coverCacheDirPath()).filePath(QString::fromLatin1(key) + QStringLiteral(".png"));
}

static QStringList launcherArtworkSearchTerms(const QFileInfo& romInfo, const QString& normalizedTitle, const QVariantMap& metadata) {
	QStringList names;
	const QString metaTitle = cleanedMetadataValue(metadata, QStringLiteral("title"));
	const QString exactBase = romInfo.completeBaseName().simplified();
	if (!metaTitle.isEmpty()) names << metaTitle;
	if (!exactBase.isEmpty()) names << exactBase;
	if (!normalizedTitle.isEmpty()) names << normalizedTitle;
	QString exactNoLang = exactBase;
	exactNoLang.remove(QRegularExpression(QStringLiteral("\\s*\\((?:[A-Za-z]{2}(?:,[A-Za-z]{2})*)\\)\\s*$")));
	if (!exactNoLang.isEmpty()) names << exactNoLang.simplified();
	QString simplified = normalizedTitle;
	simplified.remove(QRegularExpression(QStringLiteral("\\b(The|A|An)\\b"), QRegularExpression::CaseInsensitiveOption));
	simplified = simplified.simplified();
	if (!simplified.isEmpty()) names << simplified;
	for (QString& n : names) {
		n = n.simplified();
	}
	names.removeAll(QString());
	names.removeDuplicates();
	return names;
}

static bool isRomFile(const QString& path) {
	const QString ext = QFileInfo(path).suffix().toLower();
	// Common formats mGBA supports directly or via archive support.
	return ext == "gba" || ext == "gb" || ext == "gbc" || ext == "zip" || ext == "7z";
}

static QStringList launcherCoverStems(const QFileInfo& romInfo, const QString& normalizedTitle) {
	QStringList stems{romInfo.completeBaseName(), normalizedTitle, romInfo.completeBaseName() + QStringLiteral(" box art"), normalizedTitle + QStringLiteral(" box art"), romInfo.completeBaseName() + QStringLiteral(" cover"), normalizedTitle + QStringLiteral(" cover")};
	stems.removeDuplicates();
	stems.removeAll(QString());
	return stems;
}

static QString launcherStatsKey(const QString& path) {
	const QByteArray norm = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()).toUtf8();
	return QString::fromLatin1(QCryptographicHash::hash(norm, QCryptographicHash::Sha1).toHex());
}

static QString catalogCanonicalDisplayTitle(const QString& rawTitle) {
	static const QRegularExpression tags("\\s*[\\(\\[].*$");
	QString title = rawTitle;
	title.remove(tags);
	return title.replace('_', ' ').simplified();
}

static int catalogRegionPreference(const QString& rawTitle) {
	const QString lower = rawTitle.toLower();
	if (lower.contains(QStringLiteral("(usa")) || lower.contains(QStringLiteral(", usa"))) return 0;
	if (lower.contains(QStringLiteral("(europe")) || lower.contains(QStringLiteral(", europe")) || lower.contains(QStringLiteral("(eur"))) return 1;
	if (lower.contains(QStringLiteral("(world"))) return 2;
	if (!lower.contains('(')) return 3;
	return 4;
}

static bool catalogTitleBetterThan(const QString& candidate, const QString& current) {
	const int cp = catalogRegionPreference(candidate);
	const int op = catalogRegionPreference(current);
	if (cp != op) return cp < op;
	if (candidate.size() != current.size()) return candidate.size() < current.size();
	return QString::compare(candidate, current, Qt::CaseInsensitive) < 0;
}

static QString normalizedCatalogLookupKey(QString title) {
	// These expressions are compiled once, not once per game and sort comparison.
	static const QRegularExpression extension("\\.(gba|gb|gbc|zip|7z)$", QRegularExpression::CaseInsensitiveOption);
	static const QRegularExpression tags("\\s*[\\(\\[][^\\]\\[\\)\\(]{1,60}[\\)\\]]");
	static const QRegularExpression version("\\b(v|rev)\\s*\\d+(?:[.\\-_]\\d+)*\\b", QRegularExpression::CaseInsensitiveOption);
	static const QRegularExpression region("\\b(usa|eur|europe|japan|world|proto|beta|demo|sample|hack|patched|romhack|translation|enhanced|compatible|sgb|version)\\b", QRegularExpression::CaseInsensitiveOption);
	static const QRegularExpression punctuation("\\s*[-–:]\\s*");
	title.remove(extension); // A title containing '/' or a decimal point is not a file path.
	title = canonicalLibretroLookupTitle(title);
	title.replace('&', QStringLiteral(" and "));
	title.remove(tags);
	title.remove(version);
	title.remove(region);
	title.replace('.', ' ');
	title.replace(punctuation, " ");
	return title.simplified().toLower();
}

static QString compactCatalogLookupKey(QString title) {
	static const QRegularExpression punctuation("[^a-z0-9]+");
	title = normalizedCatalogLookupKey(title);
	title.remove(punctuation);
	return title;
}

static bool catalogTitlesLooselyMatch(const QString& a, const QString& b) {
	if (a.trimmed().isEmpty() || b.trimmed().isEmpty()) {
		return false;
	}
	const QString an = normalizedCatalogLookupKey(a);
	const QString bn = normalizedCatalogLookupKey(b);
	if (an == bn) {
		return true;
	}
	const QString ac = compactCatalogLookupKey(a);
	const QString bc = compactCatalogLookupKey(b);
	if (ac.isEmpty() || bc.isEmpty()) {
		return false;
	}
	return ac == bc || ac.contains(bc) || bc.contains(ac);
}

static void insertCatalogLookupKey(QHash<QString, QString>& index, const QString& key, const QString& title) {
	if (key.isEmpty() || title.isEmpty()) {
		return;
	}
	const QString existing = index.value(key);
	if (existing.isEmpty() || catalogTitleBetterThan(title, existing)) {
		index.insert(key, title);
	}
}

static QString normalizedRomPath(const QString& path) {
	if (path.isEmpty()) {
		return QString();
	}
	return QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
}

static QStringList deduplicatedRomPaths(const QStringList& paths) {
	QStringList result;
	QSet<QString> seen;
	for (const QString& path : paths) {
		const QString normalized = normalizedRomPath(path);
		const QString key = normalized.toLower();
		if (normalized.isEmpty() || seen.contains(key)) {
			continue;
		}
		seen.insert(key);
		result.append(normalized);
	}
	return result;
}

static void trimArtworkCache(const QString& dirPath, int maxFiles = 1000, qint64 maxBytes = 300LL * 1024 * 1024) {
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

static QString catalogPrefix() { return QStringLiteral("catalog::"); }
static bool isCatalogEntryId(const QString& id) { return id.startsWith(catalogPrefix()); }
static QString titleFromCatalogId(const QString& id) { return isCatalogEntryId(id) ? id.mid(catalogPrefix().size()) : QString(); }
static QString installedLinksPath() { return QDir(ConfigController::configDir()).filePath(QStringLiteral("gbax-installed-links.json")); }
static QString hiddenInstalledPath() { return QDir(ConfigController::configDir()).filePath(QStringLiteral("gbax-hidden-installed.json")); }

static bool removePathRecursivelySafe(const QString& path) {
	if (path.isEmpty()) {
		return true;
	}
	QFileInfo info(path);
	if (!info.exists()) {
		return true;
	}
	if (info.isDir()) {
		QDir dir(path);
		return dir.removeRecursively();
	}
	return QFile::remove(path);
}

static QString launcherCommandServerName() {
	const QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
	const QByteArray key = QCryptographicHash::hash(exePath.toUtf8(), QCryptographicHash::Sha1).toHex();
	return QStringLiteral("GBAxLauncherBridge-%1").arg(QString::fromLatin1(key));
}


static QString launcherUiServerName() {
	const QString salt = QCoreApplication::applicationFilePath() + QStringLiteral("|launcher-ui|") + (qEnvironmentVariableIsSet("USERNAME") ? qEnvironmentVariable("USERNAME") : qEnvironmentVariable("USER"));
	return QStringLiteral("gbax-launcher-ui-") + QString::number(qHash(salt), 16);
}

static QString cleanedMetadataValue(const QVariantMap& map, const QString& key) {
	return map.value(key).toString().trimmed();
}

static QString launcherPlatformLabelForPath(const QString& path) {
	static QHash<QString, QString> s_platformLabelCache;
	const QString absPath = QFileInfo(path).absoluteFilePath();
	auto it = s_platformLabelCache.constFind(absPath);
	if (it != s_platformLabelCache.constEnd()) {
		return it.value();
	}

	QString detected = QStringLiteral("GBA");
	const QString ext = QFileInfo(path).suffix().trimmed().toLower();
	if (ext == QStringLiteral("gb")) {
		detected = QStringLiteral("GB");
	} else if (ext == QStringLiteral("gbc")) {
		detected = QStringLiteral("GBC");
	} else if (ext == QStringLiteral("gba")) {
		detected = QStringLiteral("GBA");
	} else if (ext == QStringLiteral("zip") || ext == QStringLiteral("7z")) {
		const QByteArray p8 = absPath.toUtf8();
		if (VDir* dir = VDirOpenArchive(p8.constData())) {
			while (VDirEntry* entry = dir->listNext(dir)) {
				if (!entry || entry->type(entry) != VFS_FILE) {
					continue;
				}
				const char* cName = entry->name(entry);
				if (!cName) {
					continue;
				}
				const QString innerExt = QFileInfo(QString::fromUtf8(cName)).suffix().trimmed().toLower();
				if (innerExt == QStringLiteral("gb")) {
					detected = QStringLiteral("GB");
					break;
				}
				if (innerExt == QStringLiteral("gbc")) {
					detected = QStringLiteral("GBC");
					break;
				}
				if (innerExt == QStringLiteral("gba")) {
					detected = QStringLiteral("GBA");
					break;
				}
			}
			dir->close(dir);
		}
	}

	s_platformLabelCache.insert(absPath, detected);
	return detected;
}

static QString bracketBadge(const QString& text) {
	const QString trimmed = text.trimmed();
	return trimmed.isEmpty() ? QString() : QStringLiteral("[%1]").arg(trimmed);
}

static QString launcherBadgeSummary(const QStringList& badges) {
	QStringList parts;
	for (const QString& b : badges) {
		const QString t = b.trimmed();
		if (!t.isEmpty()) parts << bracketBadge(t);
	}
	return parts.join(QStringLiteral("  "));
}
static QString htmlEscaped(const QString& text) {
	return text.toHtmlEscaped();
}

static QString launcherPlatformBadgeBackground(const QString& text) {
	const QString normalized = text.trimmed().toUpper();
	if (normalized == QStringLiteral("GB")) {
		return QStringLiteral("rgba(104, 229, 128, 0.28)");
	}
	if (normalized == QStringLiteral("GBC")) {
		return QStringLiteral("rgba(54, 199, 255, 0.28)");
	}
	return QStringLiteral("rgba(166, 91, 255, 0.32)");
}

static QString launcherPlatformBadgeBorder(const QString& text) {
	const QString normalized = text.trimmed().toUpper();
	if (normalized == QStringLiteral("GB")) {
		return QStringLiteral("rgba(158, 247, 170, 0.62)");
	}
	if (normalized == QStringLiteral("GBC")) {
		return QStringLiteral("rgba(121, 226, 255, 0.62)");
	}
	return QStringLiteral("rgba(210, 168, 255, 0.68)");
}

static QString launcherPlatformBadgeForeground(const QString& text) {
	const QString normalized = text.trimmed().toUpper();
	if (normalized == QStringLiteral("GB")) {
		return QStringLiteral("#f0fff0");
	}
	if (normalized == QStringLiteral("GBC")) {
		return QStringLiteral("#effbff");
	}
	return QStringLiteral("#fbf5ff");
}

static QFont launcherGridTitleFont() {
	QFont font = QGBA::launcherUiFont();
	font.setStyleStrategy(QFont::PreferAntialias);
	font.setPointSize(10);
	font.setWeight(QFont::DemiBold);
	return font;
}

class LauncherGridDelegate final : public QStyledItemDelegate {
public:
	using QStyledItemDelegate::QStyledItemDelegate;
protected:
	void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override {
		QStyledItemDelegate::initStyleOption(option, index);
		option->font = launcherGridTitleFont();
		option->fontMetrics = QFontMetrics(option->font);
	}
};

static QString launcherBadgeHtml(const QString& text, const QString& background, const QString& foreground, const QString& border = QString()) {
	const QString trimmed = text.trimmed();
	if (trimmed.isEmpty()) {
		return QString();
	}
	return QStringLiteral(R"(<span style="display:inline-block; margin-right:6px; margin-bottom:5px; padding:4px 11px; border-radius:11px; background:%1; color:%2; border:1px solid %4; font-weight:700;">%3</span>)")
		.arg(background, foreground, htmlEscaped(trimmed), border.isEmpty() ? QStringLiteral("transparent") : border);
}

static QString launcherBadgeRowHtml(const QStringList& badges) {
	QString html;
	for (int i = 0; i < badges.size(); ++i) {
		const QString badge = badges.at(i).trimmed();
		if (badge.isEmpty()) {
			continue;
		}
		const bool isPlatform = (i == 0);
		html += launcherBadgeHtml(badge,
			isPlatform ? launcherPlatformBadgeBackground(badge) : QStringLiteral("rgba(255,255,255,0.08)"),
			isPlatform ? launcherPlatformBadgeForeground(badge) : QStringLiteral("rgba(244,246,255,0.92)"),
			isPlatform ? launcherPlatformBadgeBorder(badge) : QStringLiteral("rgba(255,255,255,0.08)"));
	}
	return html;
}

static QString formatPlayDuration(qint64 secs) {
	if (secs <= 0) {
		return QObject::tr("0m");
	}
	const qint64 hours = secs / 3600;
	const qint64 minutes = (secs % 3600) / 60;
	if (hours > 0) {
		return QObject::tr("%1h %2m").arg(hours).arg(minutes);
	}
	return QObject::tr("%1m").arg(qMax<qint64>(1, minutes));
}

static constexpr int kGridMinColumns = 5;
static constexpr int kGridMaxColumns = 9;
static constexpr int kGridGap = 14;
static constexpr int kGridSidePadding = 18;
static constexpr qreal kGbaCoverAspect = 1.10;

static QSize launcherGridCoverSize() {
	// Default fallback before responsive sizing kicks in.
	return QSize(170, 188);
}

static QSize launcherGridCellSize() {
	return QSize(182, 228);
}

static QSize launcherListPlaceholderCoverSize() {
	return QSize(196, 212);
}

static QPixmap blurredLauncherPixmap(const QPixmap& source, const QSize& size);

static QString normalizedLauncherThemeName(QString theme) {
	theme = theme.trimmed().toLower();
	if (theme == QStringLiteral("dark blue") || theme == QStringLiteral("blue")) return QStringLiteral("blue");
	if (theme == QStringLiteral("retro green") || theme == QStringLiteral("green")) return QStringLiteral("green");
	if (theme == QStringLiteral("teal") || theme == QStringLiteral("neon")) return QStringLiteral("teal");
	if (theme == QStringLiteral("crimson") || theme == QStringLiteral("red")) return QStringLiteral("crimson");
	if (theme == QStringLiteral("amber") || theme == QStringLiteral("gold")) return QStringLiteral("amber");
	if (theme == QStringLiteral("ice") || theme == QStringLiteral("cyan") || theme == QStringLiteral("silver")) return QStringLiteral("ice");
	return QStringLiteral("purple");
}

static QColor launcherAccentForTheme(const QString& theme) {
	const QString normalized = normalizedLauncherThemeName(theme);
	if (normalized == QStringLiteral("blue")) return QColor(68, 142, 255);
	if (normalized == QStringLiteral("green")) return QColor(118, 202, 91);
	if (normalized == QStringLiteral("teal")) return QColor(0, 200, 196);
	if (normalized == QStringLiteral("crimson")) return QColor(224, 72, 106);
	if (normalized == QStringLiteral("amber")) return QColor(255, 183, 58);
	if (normalized == QStringLiteral("ice")) return QColor(123, 210, 255);
	return QColor(181, 97, 255);
}

static QString launcherSidebarTopForTheme(const QString& theme) {
	const QString normalized = normalizedLauncherThemeName(theme);
	if (normalized == QStringLiteral("blue")) return QStringLiteral("#0b1220");
	if (normalized == QStringLiteral("green")) return QStringLiteral("#0f1810");
	if (normalized == QStringLiteral("teal")) return QStringLiteral("#071615");
	if (normalized == QStringLiteral("crimson")) return QStringLiteral("#190d12");
	if (normalized == QStringLiteral("amber")) return QStringLiteral("#191308");
	if (normalized == QStringLiteral("ice")) return QStringLiteral("#0b1418");
	return QStringLiteral("#110d17");
}

static QString launcherSidebarBottomForTheme(const QString& theme) {
	const QString normalized = normalizedLauncherThemeName(theme);
	if (normalized == QStringLiteral("blue")) return QStringLiteral("#060a12");
	if (normalized == QStringLiteral("green")) return QStringLiteral("#090d09");
	if (normalized == QStringLiteral("teal")) return QStringLiteral("#040b0a");
	if (normalized == QStringLiteral("crimson")) return QStringLiteral("#0d0709");
	if (normalized == QStringLiteral("amber")) return QStringLiteral("#0d0905");
	if (normalized == QStringLiteral("ice")) return QStringLiteral("#060b0d");
	return QStringLiteral("#08060c");
}

static QString launcherContentBgForTheme(const QString& theme) {
	const QString normalized = normalizedLauncherThemeName(theme);
	if (normalized == QStringLiteral("blue")) return QStringLiteral("#0a1120");
	if (normalized == QStringLiteral("green")) return QStringLiteral("#0b130c");
	if (normalized == QStringLiteral("teal")) return QStringLiteral("#071514");
	if (normalized == QStringLiteral("crimson")) return QStringLiteral("#120b10");
	if (normalized == QStringLiteral("amber")) return QStringLiteral("#120f08");
	if (normalized == QStringLiteral("ice")) return QStringLiteral("#091317");
	return QStringLiteral("#0f0c15");
}

static QPixmap launcherBackdropPixmap(const QPixmap& source, const QSize& size, bool performanceMode) {
	if (performanceMode) {
		if (source.isNull() || !size.isValid()) {
			return {};
		}
		QPixmap scaled = source.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
		QImage tinted(size, QImage::Format_ARGB32_Premultiplied);
		tinted.fill(QColor(10, 12, 18));
		QPainter p(&tinted);
		p.setRenderHint(QPainter::SmoothPixmapTransform, true);
		p.setOpacity(0.3);
		p.drawPixmap(tinted.rect(), scaled, scaled.rect());
		QLinearGradient fade(0, 0, 0, size.height());
		fade.setColorAt(0.0, QColor(255, 255, 255, 18));
		fade.setColorAt(1.0, QColor(0, 0, 0, 145));
		p.fillRect(tinted.rect(), QColor(12, 10, 20, 185));
		p.fillRect(tinted.rect(), fade);
		p.end();
		return QPixmap::fromImage(tinted);
	}
	return blurredLauncherPixmap(source, size);
}

static QPixmap blurredLauncherPixmap(const QPixmap& source, const QSize& size) {
	if (source.isNull() || !size.isValid()) {
		return {};
	}
	QImage small = source.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
	const QSize downsampled(qMax(28, size.width() / 11), qMax(28, size.height() / 11));
	small = small.scaled(downsampled, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	for (int y = 0; y < small.height(); ++y) {
		auto* line = reinterpret_cast<QRgb*>(small.scanLine(y));
		for (int x = 0; x < small.width(); ++x) {
			const QColor c = QColor::fromRgb(line[x]);
			const int gray = qGray(line[x]);
			const int r = qBound(0, int((c.red() - 128) * 1.14 + 128 + (c.red() - gray) * 0.22), 255);
			const int g = qBound(0, int((c.green() - 128) * 1.14 + 128 + (c.green() - gray) * 0.22), 255);
			const int b = qBound(0, int((c.blue() - 128) * 1.14 + 128 + (c.blue() - gray) * 0.22), 255);
			line[x] = qRgba(r, g, b, c.alpha());
		}
	}
	small = small.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	QImage tinted(size, QImage::Format_ARGB32_Premultiplied);
	tinted.fill(QColor(12, 10, 20));
	QPainter p(&tinted);
	p.setRenderHint(QPainter::SmoothPixmapTransform, true);
	p.setOpacity(0.54);
	p.drawImage(tinted.rect(), small);
	p.fillRect(tinted.rect(), QColor(12, 10, 20, 176));
	QLinearGradient accentWash(0, 0, size.width(), size.height());
	accentWash.setColorAt(0.0, QColor(120, 74, 180, 42));
	accentWash.setColorAt(0.45, QColor(70, 42, 116, 18));
	accentWash.setColorAt(1.0, QColor(20, 14, 34, 0));
	p.fillRect(tinted.rect(), accentWash);
	QLinearGradient topFade(0, 0, 0, size.height());
	topFade.setColorAt(0.0, QColor(255, 255, 255, 26));
	topFade.setColorAt(0.24, QColor(255, 255, 255, 8));
	topFade.setColorAt(1.0, QColor(0, 0, 0, 110));
	p.fillRect(tinted.rect(), topFade);
	QRadialGradient vignette(size.width() * 0.5, size.height() * 0.35, qMax(size.width(), size.height()) * 0.78);
	vignette.setColorAt(0.0, QColor(255, 255, 255, 0));
	vignette.setColorAt(0.72, QColor(0, 0, 0, 32));
	vignette.setColorAt(1.0, QColor(0, 0, 0, 92));
	p.fillRect(tinted.rect(), vignette);
	p.end();
	return QPixmap::fromImage(tinted);
}


LauncherWindow::LauncherWindow(ConfigController* config, QWidget* parent, QNetworkAccessManager* artworkNetwork)
	: QMainWindow(parent)
	, m_config(config)
{
	setWindowTitle(tr("GBAx"));
	setFont(launcherUiFont());
	setMinimumSize(1000, 680);
	m_artworkWorkers.setMaxThreadCount(2);
	m_artworkNetwork = artworkNetwork ? artworkNetwork : GBAApp::app()->netman();
	m_artworkWorkers.setExpiryTimeout(15000);
	setAttribute(Qt::WA_StyledBackground, true);
	setAutoFillBackground(true);
	setStyleSheet(QStringLiteral("QMainWindow { background-color: #080c14; }"));

	loadLauncherAppearanceSettings();
	setAttribute(Qt::WA_StyledBackground, true);
	setAutoFillBackground(true);
	QPalette windowPal = palette();
	windowPal.setColor(QPalette::Window, QColor(8, 12, 20));
	setPalette(windowPal);
	loadFavorites();
	loadInstalledLinks();
	loadHiddenInstalledPaths();
	loadCoverDownloadPreference();

	auto* central = new QWidget(this);
	central->setObjectName("launcherRoot");
	central->setAttribute(Qt::WA_StyledBackground, true);
	central->setAutoFillBackground(true);
	central->setStyleSheet(QStringLiteral("#launcherRoot { background-color: #080c14; }"));
	QPalette centralPal = central->palette();
	centralPal.setColor(QPalette::Window, QColor(8, 12, 20));
	central->setPalette(centralPal);
	setCentralWidget(central);

	auto* root = new QHBoxLayout(central);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);

	// Sidebar
	auto* sidebar = new QWidget(central);
	sidebar->setObjectName("sidebar");
	sidebar->setFixedWidth(230);
	auto* sb = new QVBoxLayout(sidebar);
	sb->setContentsMargins(16, 16, 16, 16);
	sb->setSpacing(12);

	auto* brand = new QLabel(sidebar);
	brand->setObjectName("brandLogo");
	brand->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	brand->setContentsMargins(0, 0, 0, 4);
	const qreal launcherLogoDpr = std::max<qreal>(qApp->devicePixelRatio(), 2.0);
	QPixmap brandPixmap(":/res/gbax-logo-launcher.png");
	if (!brandPixmap.isNull()) {
		brandPixmap.setDevicePixelRatio(launcherLogoDpr);
		brand->setPixmap(brandPixmap);
		brand->setFixedSize(brandPixmap.size() / launcherLogoDpr);
	}
	sb->addWidget(brand, 0, Qt::AlignLeft | Qt::AlignTop);

	auto mkNavBtn = [this, sidebar](const QString& text) {
		auto* b = new QPushButton(text, sidebar);
		b->setObjectName("navButton");
		b->setCheckable(true);
		b->setAutoExclusive(true);
		b->setCursor(Qt::PointingHandCursor);
		b->setIconSize(QSize(24, 24));
		b->setMinimumHeight(58);
		b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		return b;
	};

	m_navLibrary = mkNavBtn(tr("Installed"));
	m_navFavorites = mkNavBtn(tr("Favorites"));
	m_navRecent = mkNavBtn(tr("Recently Played"));
	m_navBrowse = mkNavBtn(tr("Browse Games"));
	m_navSettings = mkNavBtn(tr("Settings"));

	sb->addSpacing(8);
	sb->addWidget(m_navLibrary);
	sb->addWidget(m_navFavorites);
	sb->addWidget(m_navRecent);
	sb->addWidget(m_navBrowse);
	sb->addWidget(m_navSettings);
	sb->addStretch(1);

	// Navigation behavior: the launcher is always "library mode" with Continue Playing shown.
	// Favorites simply filters the library grid to starred items.
	connect(m_navLibrary, &QPushButton::clicked, this, &LauncherWindow::showAllGames);
	connect(m_navFavorites, &QPushButton::clicked, this, &LauncherWindow::showFavorites);
	connect(m_navRecent, &QPushButton::clicked, this, &LauncherWindow::showRecentlyPlayed);
	connect(m_navBrowse, &QPushButton::clicked, this, &LauncherWindow::showBrowseGames);
	connect(m_navSettings, &QPushButton::clicked, this, &LauncherWindow::showSettings);

	m_navLibrary->setChecked(true);
	updateSidebarIcons();

	auto* footerBox = new QWidget(sidebar);
	footerBox->setObjectName("footerBox");
	auto* footerLayout = new QVBoxLayout(footerBox);
	footerLayout->setContentsMargins(0, 0, 0, 0);
	footerLayout->setSpacing(6);

	auto* footerTitle = new QLabel(tr("GBAx"), footerBox);
	footerTitle->setObjectName("footerText");
	footerLayout->addWidget(footerTitle);

	auto* footerVersion = new QLabel(tr("Version 2.0"), footerBox);
	footerVersion->setObjectName("footerText");
	footerLayout->addWidget(footerVersion);

	m_footerBrandButton = new QPushButton(tr("Powered by EmuCores"), footerBox);
	m_footerBrandButton->setObjectName("footerBrandLink");
	m_footerBrandButton->setCursor(Qt::PointingHandCursor);
	m_footerBrandButton->setFlat(true);
	m_footerBrandButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
	footerLayout->addWidget(m_footerBrandButton, 0, Qt::AlignLeft);

	auto setupFooterButton = [footerBox](QPushButton* button) {
		button->setObjectName("footerLinkButton");
		button->setCursor(Qt::PointingHandCursor);
		button->setMinimumWidth(82);
		button->setMaximumWidth(110);
	};

	auto* footerLinksTop = new QHBoxLayout();
	footerLinksTop->setContentsMargins(0, 4, 0, 0);
	footerLinksTop->setSpacing(6);
	m_aboutFooterButton = new QPushButton(tr("About"), footerBox);
	setupFooterButton(m_aboutFooterButton);
	m_websiteFooterButton = new QPushButton(tr("Website"), footerBox);
	setupFooterButton(m_websiteFooterButton);
	footerLinksTop->addWidget(m_aboutFooterButton);
	footerLinksTop->addWidget(m_websiteFooterButton);
	footerLinksTop->addStretch(1);
	footerLayout->addLayout(footerLinksTop);

	sb->addWidget(footerBox);

	connect(m_footerBrandButton, &QPushButton::clicked, this, []() {
		QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/emucores")));
	});
	connect(m_aboutFooterButton, &QPushButton::clicked, this, [this]() {
		AboutScreen about(this);
		about.exec();
	});
	connect(m_websiteFooterButton, &QPushButton::clicked, this, []() {
		QDesktopServices::openUrl(QUrl(QStringLiteral("https://gbax-emulator.com/")));
	});

	root->addWidget(sidebar);

	// Main content
	auto* content = new QWidget(central);
	content->setObjectName("content");
	root->addWidget(content, 1);
	auto* c = new QVBoxLayout(content);
	c->setContentsMargins(18, 16, 18, 16);
	c->setSpacing(14);

	m_mainStack = new QStackedWidget(content);
	m_libraryPage = new QWidget(m_mainStack);
	m_settingsPage = new QWidget(m_mainStack);
	m_mainStack->addWidget(m_libraryPage);
	m_mainStack->addWidget(m_settingsPage);
	c->addWidget(m_mainStack, 1);

	auto* libraryLayout = new QVBoxLayout(m_libraryPage);
	libraryLayout->setContentsMargins(0, 0, 0, 0);
	libraryLayout->setSpacing(14);

	// Top header (Steam-style page heading + search)
	auto* top = new QHBoxLayout();
	top->setSpacing(14);

	auto* heading = new QVBoxLayout();
	heading->setSpacing(2);
	m_pageEyebrow = new QLabel(tr("Desktop Library"), content);
	m_pageEyebrow->setObjectName("pageEyebrow");
	m_libraryTitle = new QLabel(tr("Installed"), content);
	m_libraryTitle->setObjectName("heroTitle");
	m_libraryTitle->setProperty("topPageTitle", true);
	m_librarySummary = new QLabel(tr("Loading your library…"), content);
	m_librarySummary->setObjectName("mutedText");
	heading->addWidget(m_pageEyebrow);
	heading->addWidget(m_libraryTitle);
	heading->addWidget(m_librarySummary);
	top->addLayout(heading, 1);

	m_search = new QLineEdit(content);
	m_search->setObjectName("searchBox");
	m_search->setPlaceholderText(tr("Search games"));
	m_search->setClearButtonEnabled(true);
	m_search->setMinimumWidth(260);
	m_searchDebounceTimer = new QTimer(this);
	m_searchDebounceTimer->setSingleShot(true);
	m_searchDebounceTimer->setInterval(140);
	connect(m_searchDebounceTimer, &QTimer::timeout, this, [this]() { filterChanged(m_search ? m_search->text() : QString()); });
	connect(m_search, &QLineEdit::textChanged, this, [this](const QString&) { if (m_searchDebounceTimer) m_searchDebounceTimer->start(); });
	m_selectionDebounceTimer = new QTimer(this);
	m_selectionDebounceTimer->setSingleShot(true);
	m_selectionDebounceTimer->setInterval(50);
	m_resizeDebounceTimer = new QTimer(this);
	m_resizeDebounceTimer->setSingleShot(true);
	m_resizeDebounceTimer->setInterval(80);
	connect(m_resizeDebounceTimer, &QTimer::timeout, this, [this]() { updateResponsiveGridMetrics(); updateDetailsTitleElide(); updateContinueCardHeight(); updateDynamicBackdrops(); });
	connect(m_selectionDebounceTimer, &QTimer::timeout, this, &LauncherWindow::applyPendingSelectionChange);

	m_refreshButton = new QPushButton(tr("Refresh"), content);
	m_refreshButton->setObjectName("secondaryButton");
	m_refreshButton->setMinimumHeight(42);
	m_refreshButton->setMinimumWidth(112);
	m_refreshButton->setCursor(Qt::PointingHandCursor);
	connect(m_refreshButton, &QPushButton::clicked, this, [this]() { refreshLibrary(true); });

	top->addWidget(m_search, 0);
	top->addWidget(m_refreshButton, 0);
	libraryLayout->addLayout(top);
	m_coverDownloadStatus = new QLabel(m_libraryPage);
	m_coverDownloadStatus->setObjectName("artworkDownloadStatus");
	m_coverDownloadStatus->setStyleSheet("color: #bda9ce; padding: 4px;");
	m_coverDownloadStatus->hide();
	libraryLayout->addWidget(m_coverDownloadStatus);

	// Continue playing card
	m_continueCard = new QWidget(content);
	m_continueCard->setObjectName("card");
	m_continueCard->setMinimumHeight(174);
	m_continueCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
	auto* contCard = m_continueCard;
	m_continueBackdrop = new QLabel(contCard);
	m_continueBackdrop->setObjectName("dynamicBackdrop");
	m_continueBackdrop->setScaledContents(false);
	m_continueBackdrop->lower();
	m_continueBackdrop->show();
	auto* cont = new QHBoxLayout(contCard);
	cont->setContentsMargins(20, 12, 20, 12);
	cont->setSpacing(18);
	cont->setAlignment(Qt::AlignVCenter);

	m_continueCover = new QLabel(contCard);
	m_continueCover->setObjectName("continueCover");
	m_continueCover->setFixedSize(120, 132);
	m_continueCover->setAlignment(Qt::AlignCenter);
	cont->addWidget(m_continueCover, 0, Qt::AlignVCenter);

	auto* contLeft = new QVBoxLayout();
	contLeft->setContentsMargins(0, 0, 0, 0);
	contLeft->setSpacing(0);
	contLeft->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	auto* contHeader = new QLabel(tr("Continue Playing"), contCard);
	contHeader->setObjectName("sectionTitle");
	contLeft->addWidget(contHeader, 0, Qt::AlignLeft);
	contLeft->addSpacing(2);

	m_continueTitle = new QLabel(tr("No recent game yet"), contCard);
	m_continueTitle->setObjectName("continueTitle");
	m_continueTitle->setWordWrap(true);
	m_continueTitle->setMinimumHeight(0);
	m_continueTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	m_continueTitle->setContentsMargins(0, 0, 0, 0);

	m_continueMetaRow = new QWidget(contCard);
	m_continueMetaRow->setObjectName("badgeRow");
	m_continueMetaLayout = new QHBoxLayout(m_continueMetaRow);
	m_continueMetaLayout->setContentsMargins(0, 0, 0, 0);
	m_continueMetaLayout->setSpacing(8);
	m_continueMetaLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	m_continueStats = new QLabel(QString(), contCard);
	m_continueStats->setObjectName("continueStats");
	m_continueStats->setWordWrap(true);
	m_continueStats->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	m_continueStats->setContentsMargins(0, 0, 0, 0);

	contLeft->addWidget(m_continueTitle);
	contLeft->addSpacing(6);
	contLeft->addWidget(m_continueMetaRow, 0, Qt::AlignLeft);
	contLeft->addSpacing(8);
	contLeft->addWidget(m_continueStats);

	cont->addLayout(contLeft, 1);

	m_resumeButton = new QPushButton(tr("Resume"), contCard);
	m_resumeButton->setObjectName("primaryButton");
	m_resumeButton->setCursor(Qt::PointingHandCursor);
	m_resumeButton->setEnabled(false);
	connect(m_resumeButton, &QPushButton::clicked, this, [this]() {
		// Continue Playing is intentionally tied to the last played game, not the
		// currently highlighted item in All Games. Browsing only changes the details pane.
		const QString resumePath = m_continueLaunchPath;
		if (resumePath.isEmpty()) {
			return;
		}

		QString latestStatePath;
		QDateTime latestStateTime;
		for (int slot = 1; slot <= qMax(4, m_saveStateButtons.size()); ++slot) {
			const QString statePath = savestatePathForRomSlot(resumePath, slot);
			const QFileInfo stateInfo(statePath);
			if (!statePath.isEmpty() && stateInfo.exists()) {
				const QDateTime modified = stateInfo.lastModified();
				if (latestStatePath.isEmpty() || modified > latestStateTime) {
					latestStatePath = statePath;
					latestStateTime = modified;
				}
			}
		}

		launchPath(resumePath, latestStatePath);
	});

	auto* chooseFolder = new QPushButton(tr("Choose ROMs folder…"), contCard);
	chooseFolder->setObjectName("secondaryButton");
	chooseFolder->setCursor(Qt::PointingHandCursor);
	connect(chooseFolder, &QPushButton::clicked, this, &LauncherWindow::chooseRomsFolder);

	auto* contActions = new QVBoxLayout();
	contActions->setContentsMargins(0, 0, 0, 0);
	contActions->setSpacing(10);
	contActions->addStretch(1);
	contActions->addWidget(m_resumeButton);
	contActions->addWidget(chooseFolder);
	contActions->addStretch(1);

	cont->addLayout(contActions, 0);

	libraryLayout->addWidget(contCard);

	// Library + Details
	auto* midRow = new QHBoxLayout();
	midRow->setSpacing(14);

	// Library grid
	auto* libCard = new QWidget(content);
	libCard->setObjectName("card");
	auto* lib = new QVBoxLayout(libCard);
	lib->setContentsMargins(16, 14, 16, 14);
	lib->setSpacing(10);

	auto* libTitleRow = new QHBoxLayout();
	libTitleRow->setContentsMargins(0, 0, 0, 0);
	libTitleRow->setSpacing(10);
	auto* libTitle = new QLabel(tr("All Games"), libCard);
	libTitle->setObjectName("librarySectionTitle");

	libTitleRow->addWidget(libTitle);
	libTitleRow->addStretch(1);
	auto* libHint = new QLabel(tr("Double-click to play"), libCard);
	libHint->setObjectName("mutedText");
	libTitleRow->addWidget(libHint);
	lib->addLayout(libTitleRow);


	m_grid = new QListWidget(libCard);
	m_grid->setObjectName("grid");
	m_grid->setFont(launcherGridTitleFont());
	m_grid->setItemDelegate(new LauncherGridDelegate(m_grid));
	m_grid->setViewMode(QListView::IconMode);
	m_grid->setResizeMode(QListView::Adjust);
	m_grid->setMovement(QListView::Static);
	m_grid->setSpacing(12);
	m_grid->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	m_grid->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
	m_grid->setLayoutMode(QListView::Batched);
	m_grid->setBatchSize(32);
	m_grid->setUniformItemSizes(true);
	m_grid->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_grid->setWordWrap(false);
	m_grid->setTextElideMode(Qt::ElideRight);
	m_grid->setWrapping(true);
	m_grid->setMouseTracking(true);
	m_grid->viewport()->setMouseTracking(true);
	m_grid->viewport()->setAttribute(Qt::WA_OpaquePaintEvent, false);
	m_grid->verticalScrollBar()->setSingleStep(24);
	m_grid->horizontalScrollBar()->setSingleStep(24);
	m_grid->setIconSize(launcherGridCoverSize());
	m_grid->setGridSize(launcherGridCellSize());
	updateResponsiveGridMetrics();
	m_grid->setSelectionMode(QAbstractItemView::SingleSelection);
	m_grid->setContextMenuPolicy(Qt::CustomContextMenu);
	m_grid->viewport()->installEventFilter(this);
	connect(m_grid, &QListWidget::itemEntered, this, [this](QListWidgetItem* item) { setHoveredItem(item); });

	connect(m_grid, &QListWidget::customContextMenuRequested, this, &LauncherWindow::showGameContextMenu);
	connect(m_grid, &QListWidget::itemSelectionChanged, this, &LauncherWindow::onSelectionChanged);
	connect(m_grid, &QListWidget::itemDoubleClicked, this, [this]() { launchSelected(); });

	lib->addWidget(m_grid, 1);
	midRow->addWidget(libCard, 3);

	// Details panel
	auto* detailsCard = new QWidget(content);
	detailsCard->setObjectName("card");
	detailsCard->setMinimumWidth(300);
	detailsCard->setMaximumWidth(370);
	detailsCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	auto* d = new QVBoxLayout(detailsCard);
	d->setContentsMargins(16, 14, 16, 14);
	d->setSpacing(12);

	// Hidden helper labels kept for cover generation/caching logic.
	m_detailsCover = new QLabel(detailsCard);
	m_detailsCover->setVisible(false);
	m_detailsCover->setFixedSize(160, 160);

	auto* detailsScroll = new QScrollArea(detailsCard);
	detailsScroll->setObjectName("detailsPaneScroll");
	detailsScroll->setWidgetResizable(true);
	detailsScroll->setFrameShape(QFrame::NoFrame);
	detailsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	detailsScroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

	m_detailsInfoCard = new QWidget();
	auto* detailsInfoCard = m_detailsInfoCard;
	detailsInfoCard->setObjectName("detailsInfoCard");
	detailsInfoCard->setAttribute(Qt::WA_StyledBackground, true);
	detailsInfoCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	detailsInfoCard->setMinimumHeight(0);
	detailsScroll->setWidget(detailsInfoCard);
	m_detailsBackdrop = new QLabel(detailsInfoCard);
	m_detailsBackdrop->setObjectName("dynamicBackdrop");
	m_detailsBackdrop->setScaledContents(false);
	m_detailsBackdrop->lower();
	m_detailsBackdrop->show();
	auto* infoLayout = new QVBoxLayout(detailsInfoCard);
	infoLayout->setContentsMargins(18, 18, 18, 18);
	infoLayout->setSpacing(14);

	auto* infoHeader = new QVBoxLayout();
	infoHeader->setContentsMargins(0, 0, 0, 0);
	infoHeader->setSpacing(12);

	auto* thumbRow = new QHBoxLayout();
	thumbRow->setContentsMargins(0, 0, 0, 0);
	thumbRow->setSpacing(0);
	m_detailsThumb = new QLabel(detailsInfoCard);
	m_detailsThumb->setObjectName("detailsThumb");
	m_detailsThumb->setFixedSize(104, 114);
	m_detailsThumb->setAlignment(Qt::AlignCenter);
	thumbRow->addWidget(m_detailsThumb, 0, Qt::AlignLeft | Qt::AlignTop);
	thumbRow->addStretch(1);
	infoHeader->addLayout(thumbRow);

	auto* infoText = new QVBoxLayout();
	infoText->setContentsMargins(0, 0, 0, 0);
	infoText->setSpacing(8);
	m_detailsTitle = new QLabel(tr("Select a game"), detailsInfoCard);
	m_detailsTitle->setObjectName("detailsTitle");
	m_detailsTitle->setWordWrap(true);
	m_detailsTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_detailsTitle->setToolTip(QString());
	infoText->addWidget(m_detailsTitle);
	m_detailsMetaRow = new QWidget(detailsInfoCard);
	m_detailsMetaRow->setObjectName("badgeRow");
	m_detailsMetaRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_detailsMetaLayout = new QHBoxLayout(m_detailsMetaRow);
	m_detailsMetaLayout->setContentsMargins(0, 0, 0, 0);
	m_detailsMetaLayout->setSpacing(8);
	m_detailsMetaLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	infoText->addWidget(m_detailsMetaRow);
	m_detailsStats = new QLabel(QString(), detailsInfoCard);
	m_detailsStats->setObjectName("detailsStats");
	m_detailsStats->setWordWrap(true);
	infoText->addWidget(m_detailsStats);
	m_detailsPath = new QLabel(QString(), detailsInfoCard);
	m_detailsPath->setObjectName("detailsPath");
	m_detailsPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_detailsPath->setWordWrap(true);
	m_detailsPath->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	infoText->addWidget(m_detailsPath);
	infoHeader->addLayout(infoText);
	infoLayout->addLayout(infoHeader);

	m_detailsDescription = new QLabel(tr("Select a game to view playtime, last played, and game details."), detailsInfoCard);
	m_detailsDescription->setObjectName("mutedText");
	m_detailsDescription->setWordWrap(true);
	m_detailsDescription->setVisible(false);
	infoLayout->addWidget(m_detailsDescription);

	// Buttons row (Play + Favorite)
	auto* btnRow = new QHBoxLayout();
	btnRow->setContentsMargins(0, 14, 0, 0);
	btnRow->setSpacing(12);

	m_playButton = new QPushButton(tr("PLAY GAME"), detailsInfoCard);
	m_playButton->setObjectName("primaryButton");
	m_playButton->setEnabled(false);
	m_playButton->setFixedHeight(52);
	m_playButton->setCursor(Qt::PointingHandCursor);
	connect(m_playButton, &QPushButton::clicked, this, &LauncherWindow::launchSelected);
	btnRow->addWidget(m_playButton, 1, Qt::AlignVCenter);

	m_favButton = new QPushButton(detailsInfoCard);
	m_favButton->setObjectName("secondaryButton");
	m_favButton->setToolTip(tr("Add/remove favorite"));
	m_favButton->setEnabled(false);
	m_favButton->setCursor(Qt::PointingHandCursor);
	m_favButton->setFixedSize(68, 52);
	m_favButton->setIconSize(QSize(20, 20));
	connect(m_favButton, &QPushButton::clicked, this, &LauncherWindow::toggleFavorite);
	btnRow->addWidget(m_favButton, 0, Qt::AlignVCenter);

	infoLayout->addLayout(btnRow);

	m_saveStateScroll = detailsScroll;
	m_saveStateCard = new QWidget(detailsInfoCard);
	m_saveStateCard->setObjectName("saveStateCard");
	m_saveStateCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	auto* saveStateLayout = new QVBoxLayout(m_saveStateCard);
	saveStateLayout->setContentsMargins(0, 12, 4, 0);
	saveStateLayout->setSpacing(10);
	m_saveSlotsToggle = new QPushButton(tr("Quick Save Slots"), m_saveStateCard);
	m_saveSlotsToggle->setObjectName("saveSlotsToggle");
	m_saveSlotsToggle->setCheckable(true);
	m_saveSlotsToggle->setCursor(Qt::PointingHandCursor);
	saveStateLayout->addWidget(m_saveSlotsToggle);
	m_saveSlotsSummary = new QLabel(m_saveStateCard);
	m_saveSlotsSummary->setObjectName("saveSlotsSummary");
	m_saveSlotsSummary->setWordWrap(true);
	saveStateLayout->addWidget(m_saveSlotsSummary);
	m_saveSlotsContent = new QWidget(m_saveStateCard);
	m_saveSlotsContent->setObjectName("saveSlotsContent");
	auto* slotsLayout = new QVBoxLayout(m_saveSlotsContent);
	slotsLayout->setContentsMargins(0, 0, 0, 0);
	slotsLayout->setSpacing(8);
	saveStateLayout->addWidget(m_saveSlotsContent);
	connect(m_saveSlotsToggle, &QPushButton::toggled, this, &LauncherWindow::updateSaveSlotExpansion);
	for (int i = 0; i < 4; ++i) {
		auto* slotButton = new QPushButton(tr("Slot %1").arg(i + 1), m_saveSlotsContent);
		slotButton->setObjectName("saveStatePreviewButton");
		slotButton->setCheckable(false);
		slotButton->setMinimumHeight(78);
		slotButton->setIconSize(QSize(64, 48));
		slotButton->setCursor(Qt::PointingHandCursor);
		slotButton->setToolTip(tr("Open the save/load screen for this game"));
		connect(slotButton, &QPushButton::clicked, this, [this, i]() {
			if (!m_selectedPath.isEmpty()) {
				launchSelectedSaveSlot(i + 1);
			}
		});
		m_saveStateButtons.append(slotButton);
		slotsLayout->addWidget(slotButton);
	}
	m_saveSlotsContent->hide();
	infoLayout->addWidget(m_saveStateCard);
	infoLayout->addStretch(1);

	d->addWidget(detailsScroll, 1);
	midRow->addWidget(detailsCard, 1);
	libraryLayout->addLayout(midRow, 1);

	// Settings page
	auto* settingsLayout = new QVBoxLayout(m_settingsPage);
	settingsLayout->setContentsMargins(0, 0, 0, 0);
	settingsLayout->setSpacing(14);

	auto* settingsHeader = new QWidget(m_settingsPage);
	auto* sh = new QVBoxLayout(settingsHeader);
	sh->setContentsMargins(0, 0, 0, 0);
	sh->setSpacing(2);
	auto* settingsEyebrow = new QLabel(tr("Launcher Settings"), settingsHeader);
	settingsEyebrow->setObjectName("pageEyebrow");
	auto* settingsTitle = new QLabel(tr("Settings"), settingsHeader);
	settingsTitle->setObjectName("heroTitle");
	auto* settingsSummary = new QLabel(tr("Configure your ROM library, artwork, and emulator settings."), settingsHeader);
	settingsSummary->setObjectName("mutedText");
	sh->addWidget(settingsEyebrow);
	sh->addWidget(settingsTitle);
	sh->addWidget(settingsSummary);
	settingsLayout->addWidget(settingsHeader);

	auto* settingsScroll = new QScrollArea(m_settingsPage);
	m_settingsScroll = settingsScroll;
	settingsScroll->setWidgetResizable(true);
	settingsScroll->setFrameShape(QFrame::NoFrame);
	settingsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	settingsScroll->setObjectName("settingsScroll");

	auto* settingsScrollContent = new QWidget(settingsScroll);
	auto* settingsScrollLayout = new QVBoxLayout(settingsScrollContent);
	settingsScrollLayout->setContentsMargins(0, 0, 6, 0);
	settingsScrollLayout->setSpacing(14);

	auto* settingsCard = new QWidget(settingsScrollContent);
	settingsCard->setObjectName("settingsCard");
	settingsScrollContent->setObjectName("settingsScrollContent");
	settingsScroll->viewport()->setObjectName("settingsViewport");
	m_settingsPage->setObjectName("settingsPageRoot");
	auto* sc = new QVBoxLayout(settingsCard);
	sc->setContentsMargins(18, 18, 18, 18);
	sc->setSpacing(14);

	auto makeSecondaryButton = [settingsCard](const QString& text) {
		auto* button = new QPushButton(text, settingsCard);
		button->setObjectName("secondaryButton");
		button->setCursor(Qt::PointingHandCursor);
		button->setMinimumHeight(40);
		button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
		return button;
	};

	auto* emulatorSection = new QLabel(tr("Emulator Settings"), settingsCard);
	emulatorSection->setObjectName("sectionTitle");
	sc->addWidget(emulatorSection);
	auto* emulatorInfo = new QLabel(tr("Adjust how GBAx handles gameplay and performance. Open the full emulator configuration to fine-tune graphics, audio, controls, and advanced emulation options."), settingsCard);
	emulatorInfo->setObjectName("mutedText");
	emulatorInfo->setWordWrap(true);
	sc->addWidget(emulatorInfo);

	auto* emulatorButtons = new QHBoxLayout();
	emulatorButtons->setSpacing(10);
	m_openEmulatorSettingsButton = makeSecondaryButton(tr("Open Emulator Settings"));
	auto* openEmulatorSettings = m_openEmulatorSettingsButton;
	emulatorButtons->addWidget(openEmulatorSettings, 0, Qt::AlignLeft);
	emulatorButtons->addStretch(1);
	sc->addLayout(emulatorButtons);

	connect(openEmulatorSettings, &QPushButton::clicked, this, &LauncherWindow::openEmulatorSettingsDialog);

	sc->addSpacing(8);

	auto* librarySection = new QLabel(tr("Library"), settingsCard);
	librarySection->setObjectName("sectionTitle");
	sc->addWidget(librarySection);

	auto* libraryInfo = new QLabel(tr("Manage your ROM folder, choose where GBAx looks for games, and refresh your library when files change."), settingsCard);
	libraryInfo->setObjectName("mutedText");
	libraryInfo->setWordWrap(true);
	sc->addWidget(libraryInfo);

	m_settingsRomPath = new QLabel(settingsCard);
	m_settingsRomPath->setObjectName("mutedText");
	m_settingsRomPath->setWordWrap(true);
	sc->addWidget(m_settingsRomPath);

	auto* settingsButtons = new QHBoxLayout();
	settingsButtons->setSpacing(10);
	m_chooseFolderSettingsButton = makeSecondaryButton(tr("Choose ROM Folder"));
	auto* chooseFolderSettings = m_chooseFolderSettingsButton;
	connect(chooseFolderSettings, &QPushButton::clicked, this, &LauncherWindow::chooseRomsFolder);
	settingsButtons->addWidget(chooseFolderSettings, 0, Qt::AlignLeft);
	m_refreshLibrarySettingsButton = makeSecondaryButton(tr("Refresh Library"));
	auto* refreshSettings = m_refreshLibrarySettingsButton;
	connect(refreshSettings, &QPushButton::clicked, this, [this]() { refreshLibrary(true); });
	settingsButtons->addWidget(refreshSettings, 0, Qt::AlignLeft);
	settingsButtons->addStretch(1);
	sc->addLayout(settingsButtons);

	m_recursiveScanCheck = new QCheckBox(tr("Scan subfolders automatically"), settingsCard);
	m_recursiveScanCheck->setChecked(m_config ? m_config->getOption("libraryAutoScan").toInt() != 0 : true);
	connect(m_recursiveScanCheck, &QCheckBox::toggled, this, [this](bool checked) {
		if (m_config) {
			m_config->setOption("libraryAutoScan", checked ? 1 : 0);
		}
		refreshLibrary();
	});
	sc->addWidget(m_recursiveScanCheck);

	m_skipSplashCheck = new QCheckBox(tr("Show splash screen on startup"), settingsCard);
	m_skipSplashCheck->setChecked(m_config ? m_config->getQtOption("showLauncherSplash", "gbax").toBool() : true);
	connect(m_skipSplashCheck, &QCheckBox::toggled, this, [this](bool checked) {
		if (m_config) {
			m_config->setQtOption("showLauncherSplash", checked, "gbax");
		}
	});
	sc->addWidget(m_skipSplashCheck);

	m_launchFullscreenCheck = new QCheckBox(tr("Launch games in fullscreen"), settingsCard);
	m_launchFullscreenCheck->setChecked(m_config ? m_config->getOption("fullscreen").toInt() != 0 : false);
	connect(m_launchFullscreenCheck, &QCheckBox::toggled, this, [this](bool checked) {
		if (m_config) {
			m_config->setOption("fullscreen", checked ? 1 : 0);
		}
	});
	sc->addWidget(m_launchFullscreenCheck);

	auto* appearanceSection = new QLabel(tr("Appearance"), settingsCard);
	appearanceSection->setObjectName("sectionTitle");
	sc->addWidget(appearanceSection);
	auto* appearanceInfo = new QLabel(tr("Choose an accent theme for GBAx and enable Performance Mode on lower-end systems."), settingsCard);
	appearanceInfo->setObjectName("mutedText");
	appearanceInfo->setWordWrap(true);
	sc->addWidget(appearanceInfo);

	m_themeCombo = new ClickOnlyComboBox(settingsCard);
	m_themeCombo->setObjectName("launcherThemeCombo");
	m_themeCombo->setFocusPolicy(Qt::ClickFocus);
	m_themeCombo->addItem(tr("Purple"), QStringLiteral("purple"));
	m_themeCombo->addItem(tr("Dark Blue"), QStringLiteral("blue"));
	m_themeCombo->addItem(tr("Retro Green"), QStringLiteral("green"));
	m_themeCombo->addItem(tr("Teal"), QStringLiteral("teal"));
	m_themeCombo->addItem(tr("Crimson"), QStringLiteral("crimson"));
	m_themeCombo->addItem(tr("Amber"), QStringLiteral("amber"));
	m_themeCombo->addItem(tr("Ice"), QStringLiteral("ice"));
	const int themeIndex = qMax(0, m_themeCombo->findData(m_launcherTheme));
	m_themeCombo->setCurrentIndex(themeIndex);
	connect(m_themeCombo, qOverload<int>(&QComboBox::activated), this, [this](int index) {
		if (!m_themeCombo || index < 0) {
			return;
		}
		m_launcherTheme = normalizedLauncherThemeName(m_themeCombo->itemData(index).toString());
		if (m_config) {
			m_config->setQtOption(QStringLiteral("launcherTheme"), m_launcherTheme, QStringLiteral("gbax"));
		}
		applyModernTheme();
		refreshCoverUi();
		updateDynamicBackdrops();
	});
	sc->addWidget(m_themeCombo);

	m_performanceModeCheck = new QCheckBox(tr("Performance Mode (reduce blur and heavy effects)"), settingsCard);
	m_performanceModeCheck->setChecked(m_performanceMode);
	connect(m_performanceModeCheck, &QCheckBox::toggled, this, [this](bool checked) {
		m_performanceMode = checked;
		if (m_config) {
			m_config->setQtOption(QStringLiteral("launcherPerformanceMode"), checked, QStringLiteral("gbax"));
		}
		applyModernTheme();
		refreshCoverUi();
		updateDynamicBackdrops();
	});
	sc->addWidget(m_performanceModeCheck);


	auto* controllerShortcutsSection = new QLabel(tr("Controller Shortcuts"), settingsCard);
	controllerShortcutsSection->setObjectName("sectionTitle");
	sc->addWidget(controllerShortcutsSection);
	auto* controllerShortcutsInfo = new QLabel(
		tr("Use your controller to navigate GBAx, launch games, switch sections, and move between the launcher and emulator without needing a keyboard or mouse."),
		settingsCard);
	controllerShortcutsInfo->setObjectName("mutedText");
	controllerShortcutsInfo->setWordWrap(true);
	sc->addWidget(controllerShortcutsInfo);
	auto* controllerShortcutsList = new QLabel(
		tr("• Hold START + SELECT (5 seconds) → Exit game\n"
		   "• A / X → Confirm\n"
		   "• B / Circle → Back\n"
		   "• L1 / LB - Toggle Fullscreen\n"
		   "• R1 / RB - Switch between the Launcher and the active game\n"
		   "• D-Pad → Navigate\n"
		   "• Left / Right Sticks - Scroll\n"
		   "• L3 / R3 (Launcher) → Previous / Next sidebar section\n"
		   "• L3 / R3 (Emulator) → Open Quick Menu"),
		settingsCard);
	controllerShortcutsList->setObjectName("mutedText");
	controllerShortcutsList->setWordWrap(true);
	sc->addWidget(controllerShortcutsList);


	auto* artworkSection = new QLabel(tr("Artwork"), settingsCard);
	artworkSection->setObjectName("sectionTitle");
	sc->addWidget(artworkSection);
	auto* artworkInfo = new QLabel(tr("Manage box art for your library. GBAx stores downloaded covers, generated thumbnails, and any manual artwork you add in the artwork cache folder."), settingsCard);
	artworkInfo->setObjectName("mutedText");
	artworkInfo->setWordWrap(true);
	sc->addWidget(artworkInfo);

	auto* artworkCacheInfo = new QLabel(tr("Covers load in the background as you browse. Download the full catalog below, or cancel at any time. New artwork appears immediately."), settingsCard);
	artworkCacheInfo->setObjectName("mutedText");
	artworkCacheInfo->setWordWrap(true);
	sc->addWidget(artworkCacheInfo);

	auto* artworkToolsButtons = new QHBoxLayout();
	artworkToolsButtons->setSpacing(10);
	m_buildArtworkCacheButton = makeSecondaryButton(tr("Download Missing Artwork"));
	auto* buildArtworkCacheButton = m_buildArtworkCacheButton;
	connect(buildArtworkCacheButton, &QPushButton::clicked, this, &LauncherWindow::startBrowseCoverDownloads);
	artworkToolsButtons->addWidget(buildArtworkCacheButton, 0, Qt::AlignLeft);
	auto* openArtworkFolderButton = makeSecondaryButton(tr("Open Artwork Folder"));
	connect(openArtworkFolderButton, &QPushButton::clicked, this, [this]() {
		const QString artDir = coverCacheDirPath();
		QDir().mkpath(artDir);
		QDesktopServices::openUrl(QUrl::fromLocalFile(artDir));
	});
	artworkToolsButtons->addWidget(openArtworkFolderButton, 0, Qt::AlignLeft);
	artworkToolsButtons->addStretch(1);
	sc->addLayout(artworkToolsButtons);

	m_autoDownloadCoversCheck = new QCheckBox(tr("Automatically download missing artwork"), settingsCard);
	m_autoDownloadCoversCheck->setToolTip(tr("Downloads library artwork after scanning and the full catalogue after opening Browse Games."));
	m_autoDownloadCoversCheck->setChecked(m_autoDownloadCovers);
	connect(m_autoDownloadCoversCheck, &QCheckBox::toggled, this, [this](bool checked) {
		m_autoDownloadCovers = checked;
		saveCoverDownloadPreference();
		if (checked) {
			m_bulkCoverDownloadDismissed = false;
			m_autoArtworkPaused = false;
			queueMissingCoversForLibrary();
			if (m_catalogArtworkRequested || m_browseOnly) queueMissingCoversForBrowseCatalog();
			queueMissingCoversForVisibleItems();
		} else {
			cancelArtworkDownloads();
		}
		updateBrowseCoverPrompt();
	});
	sc->addWidget(m_autoDownloadCoversCheck);

	sc->addSpacing(8);
	auto* troubleshootingSection = new QLabel(tr("Troubleshooting"), settingsCard);
	troubleshootingSection->setObjectName("sectionTitle");
	sc->addWidget(troubleshootingSection);
	auto* troubleshootingInfo = new QLabel(tr("Manage launcher data and resolve common issues. Use these tools to clear cached data, access GBAx system folders, report problems, or reset GBAx back to a clean first-run state."), settingsCard);
	troubleshootingInfo->setObjectName("mutedText");
	troubleshootingInfo->setWordWrap(true);
	sc->addWidget(troubleshootingInfo);

	auto* troubleButtonsLayout = new QVBoxLayout();
	troubleButtonsLayout->setSpacing(10);
	troubleButtonsLayout->setAlignment(Qt::AlignLeft);
	m_clearLauncherCacheButton = makeSecondaryButton(tr("Clear Launcher Cache"));
	auto* clearCacheButton = m_clearLauncherCacheButton;
	connect(clearCacheButton, &QPushButton::clicked, this, [this]() {
		const auto answer = QMessageBox::question(this, tr("Clear Launcher Cache"), tr("This rebuilds generated thumbnails and temporary launcher data. Downloaded and custom artwork, ROMs, favorites, saves, and settings are kept."), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (answer != QMessageBox::Yes) return;
		clearLauncherCaches(true);
		QMessageBox::information(this, tr("Launcher cache cleared"), tr("GBAx cleared its launcher cache."));
	});
	troubleButtonsLayout->addWidget(clearCacheButton, 0, Qt::AlignLeft);
	auto* openDataFolderButton = makeSecondaryButton(tr("Open GBAx Data Folder"));
	connect(openDataFolderButton, &QPushButton::clicked, this, []() {
		QDesktopServices::openUrl(QUrl::fromLocalFile(ConfigController::configDir()));
	});
	troubleButtonsLayout->addWidget(openDataFolderButton, 0, Qt::AlignLeft);
	auto* resetGbaxButton = makeSecondaryButton(tr("Reset GBAx (Clean Start)"));
	connect(resetGbaxButton, &QPushButton::clicked, this, [this]() {
		if (m_gameProcess && m_gameProcess->state() != QProcess::NotRunning) {
			QMessageBox::information(this, tr("Game still running"), tr("Close the running game before resetting GBAx."));
			return;
		}
		const auto answer = QMessageBox::warning(
			this,
			tr("Reset GBAx"),
			tr("This will close GBAx, reset its settings and library index, and reopen the first-run setup. ROMs, saves, artwork, and other files will be kept. Continue?"),
			QMessageBox::Yes | QMessageBox::No,
			QMessageBox::No);
		if (answer != QMessageBox::Yes) {
			return;
		}
		const QString program = QCoreApplication::applicationFilePath();
		QStringList args;
		args << QStringLiteral("--reset-gbax-helper") << QStringLiteral("--wait-pid") << QString::number(QCoreApplication::applicationPid());
		if (!QProcess::startDetached(program, args, QFileInfo(program).absolutePath())) {
			QMessageBox::warning(this, tr("Unable to reset GBAx"), tr("GBAx could not start the reset helper process."));
			return;
		}
		close();
		QCoreApplication::quit();
	});
	troubleButtonsLayout->addWidget(resetGbaxButton, 0, Qt::AlignLeft);
	m_reportBugSettingsButton = makeSecondaryButton(tr("Report a Bug"));
	connect(m_reportBugSettingsButton, &QPushButton::clicked, this, []() {
		QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/emucores/GBAx/issues")));
	});
	troubleButtonsLayout->addWidget(m_reportBugSettingsButton, 0, Qt::AlignLeft);
	sc->addLayout(troubleButtonsLayout);

	settingsScrollLayout->addWidget(settingsCard);
	settingsScrollLayout->addStretch(1);
	settingsScrollContent->setLayout(settingsScrollLayout);
	settingsScroll->setWidget(settingsScrollContent);
	settingsLayout->addWidget(settingsScroll, 1);

	// Cover refresh now only reconciles missing artwork / updates newly downloaded covers.
	m_coverRefreshTimer = new QTimer(this);
	m_coverRefreshTimer->setSingleShot(true);
	connect(m_coverRefreshTimer, &QTimer::timeout, this, &LauncherWindow::refreshVisibleCoverIcons);
	m_browseCoverQueueTimer = new QTimer(this);
	m_browseCoverQueueTimer->setSingleShot(true);
	m_browseCoverQueueTimer->setInterval(80);
	connect(m_browseCoverQueueTimer, &QTimer::timeout, this, &LauncherWindow::queueNextBrowseCoverBatch);

	m_coverUiBatchTimer = new QTimer(this);
	m_coverUiBatchTimer->setSingleShot(true);
	m_coverUiBatchTimer->setInterval(80);
	connect(m_coverUiBatchTimer, &QTimer::timeout, this, &LauncherWindow::flushPendingCoverUiRefreshes);
	m_libraryPopulateTimer = new QTimer(this);
	m_libraryPopulateTimer->setSingleShot(true);
	m_libraryPopulateTimer->setInterval(8);
	connect(m_libraryPopulateTimer, &QTimer::timeout, this, [this]() {
		if (m_gridContainsCatalog) appendMoreBrowseItems();
		else appendMoreInstalledItems();
	});
	m_analogScrollTimer = new QTimer(this);
	m_analogScrollTimer->setInterval(16);
	connect(m_analogScrollTimer, &QTimer::timeout, this, &LauncherWindow::applyAnalogScrollStep);
	m_controllerComboHoldTimer = new QTimer(this);
	m_controllerComboHoldTimer->setSingleShot(true);
	m_controllerComboHoldTimer->setInterval(5000);
	connect(m_controllerComboHoldTimer, &QTimer::timeout, this, [this]() {
		if (m_controllerSelectDown && m_controllerStartDown) {
			m_controllerComboHoldTriggered = true;
			requestCloseRunningGame();
		}
	});

	if (m_grid) {
		connect(m_grid->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value){
			scheduleVisibleCoverRefresh();
			if (m_browseOnly && m_grid && m_grid->verticalScrollBar() && value >= m_grid->verticalScrollBar()->maximum() - 240) {
				appendMoreBrowseItems();
			}
		});
		connect(m_grid->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this](int){ scheduleVisibleCoverRefresh(); });
	}

	// Kick initial scan shortly after show (so UI appears instantly)
	connect(&m_scanWatcher, &QFutureWatcher<QStringList>::finished, this, &LauncherWindow::onLibraryScanFinished);

	const QList<QWidget*> navTargets{m_grid, m_navLibrary, m_navFavorites, m_navRecent, m_navBrowse, m_navSettings,
		m_footerBrandButton, m_aboutFooterButton, m_websiteFooterButton,
		m_playButton, m_favButton, m_saveSlotsToggle, m_openEmulatorSettingsButton, m_chooseFolderSettingsButton, m_refreshLibrarySettingsButton, m_recursiveScanCheck,
		m_skipSplashCheck, m_launchFullscreenCheck, m_themeCombo, m_performanceModeCheck, m_buildArtworkCacheButton,
		m_autoDownloadCoversCheck, m_clearLauncherCacheButton, m_reportBugSettingsButton, m_factoryResetButton};
	for (QWidget* widget : navTargets) {
		if (!widget) continue;
		widget->setFocusPolicy(Qt::StrongFocus);
		widget->installEventFilter(this);
	}
	for (QPushButton* button : m_saveStateButtons) {
		if (!button) continue;
		button->setFocusPolicy(Qt::StrongFocus);
		button->installEventFilter(this);
	}
	setFocusPolicy(Qt::StrongFocus);
	qApp->installEventFilter(this);

#ifdef BUILD_SDL
	m_launcherInput = std::make_unique<InputController>(this, this);
	auto sdlDriver = std::make_shared<SDLInputDriver>(m_launcherInput.get());
	m_launcherInput->addInputDriver(sdlDriver);
	m_launcherInput->setGamepadDriver(sdlDriver->type());
	if (m_config) {
		m_launcherInput->setConfiguration(m_config);
		// Ensure launcher navigation has sensible default controller bindings even on a fresh install.
		m_launcherInput->loadConfiguration(sdlDriver->type());
	}
	QTimer::singleShot(0, this, [this]() {
		if (m_launcherInput) {
			m_launcherInput->update();
		}
	});
#endif

	updateBrowseCoverPrompt();
	updateCoverDownloadStatus();

	QTimer::singleShot(0, this, [this]() {
		loadCatalogTitles();
		if (m_browseOnly) {
			applyLauncherFilter();
		}
		updateBrowseCoverPrompt();
		updateCoverDownloadStatus();
	});
	QTimer::singleShot(35, this, [this]() { refreshLibrary(false); });
	QTimer::singleShot(70, this, &LauncherWindow::updateContinuePlaying);
	applyModernTheme();

}


void LauncherWindow::loadLauncherAppearanceSettings() {
	if (!m_config) {
		m_launcherTheme = QStringLiteral("purple");
		m_performanceMode = false;
		return;
	}
	m_launcherTheme = normalizedLauncherThemeName(m_config->getQtOption(QStringLiteral("launcherTheme"), QStringLiteral("gbax")).toString());
	if (m_launcherTheme.isEmpty()) {
		m_launcherTheme = QStringLiteral("purple");
	}
	m_performanceMode = m_config->getQtOption(QStringLiteral("launcherPerformanceMode"), QStringLiteral("gbax")).toBool();
}


bool LauncherWindow::event(QEvent* event) {
	if (handleLauncherNavigationEvent(this, event)) {
		return true;
	}
	return QMainWindow::event(event);
}




void LauncherWindow::completeStartupVisualReady() {
	if (m_startupVisualReadyEmitted) return;
	m_startupVisualReadyEmitted = true;
	emit startupVisualReady();
}

void LauncherWindow::showEvent(QShowEvent* event) {
	QMainWindow::showEvent(event);
	if (m_startupFirstShowHandled) {
		return;
	}
	m_startupFirstShowHandled = true;
	QTimer::singleShot(0, this, &LauncherWindow::completeStartupVisualReady);
	QPalette pal = palette();
	pal.setColor(QPalette::Window, QColor(8, 12, 20));
	setPalette(pal);
	if (centralWidget()) {
		centralWidget()->setAutoFillBackground(true);
		QPalette cpal = centralWidget()->palette();
		cpal.setColor(QPalette::Window, QColor(8, 12, 20));
		centralWidget()->setPalette(cpal);
	}
	QTimer::singleShot(0, this, [this]() {
		update();
		if (m_mainStack && m_mainStack->currentWidget() == m_settingsPage) {
			if (m_themeCombo) {
				m_themeCombo->setFocus();
			}
		} else if (m_grid && m_grid->count()) {
			m_grid->setFocus();
			if (m_grid->currentRow() < 0) {
				m_grid->setCurrentRow(0);
			}
		} else if (m_navLibrary) {
			m_navLibrary->setFocus();
		}
	});
}

LauncherWindow::~LauncherWindow() {
	if (m_settingsHostWindow) delete m_settingsHostWindow.data();
	cancelArtworkDownloads();
	++m_artworkGeneration;
	m_artworkWorkers.waitForDone();
	qApp->removeEventFilter(this);
	if (m_scanWatcher.isRunning()) {
		m_scanWatcher.cancel();
		m_scanWatcher.waitForFinished();
	}
	if (m_libraryScanCancel) {
		m_libraryScanCancel->store(true);
	}
	if (m_libraryScanThread) {
		m_libraryScanThread->quit();
		m_libraryScanThread->wait();
	}
}

void LauncherWindow::loadCoverDownloadPreference() {
	if (!m_config) {
		m_autoDownloadCovers = true;
		return;
	}
	const QVariant option = m_config->getQtOption(QStringLiteral("autoDownloadCovers"), QStringLiteral("gbaxLauncher"));
	m_autoDownloadCovers = option.isValid() ? option.toBool() : true;
}

void LauncherWindow::saveCoverDownloadPreference() const {
	if (!m_config) {
		return;
	}
	m_config->setQtOption(QStringLiteral("autoDownloadCovers"), m_autoDownloadCovers, QStringLiteral("gbaxLauncher"));
}

bool LauncherWindow::shouldAutoDownloadCovers() const {
	return m_autoDownloadCovers && !m_autoArtworkPaused;
}

bool LauncherWindow::hasMissingCoverForPath(const QString& path) const {
	if (path.isEmpty()) {
		return false;
	}
	return findCoverArtPath(path).isEmpty();
}





QString LauncherWindow::artworkStatusForPath(const QString& path) const {
	if (!findCoverArtPath(path).isEmpty()) return tr("Ready");
	const QString linked = isCatalogEntry(path) ? QString() : bestCatalogTitleForInstalledPath(path);
	const QString id = !linked.isEmpty() ? catalogIdForTitle(linked) : (isCatalogEntry(path) ? path : normalizedRomPath(path));
	if (m_coverDownloadsInFlight.contains(id)) return tr("Downloading");
	if (m_coverDownloadCandidateMap.contains(id)) return tr("Queued");
	if (m_config && m_config->getQtOption(launcherStatsKey(id), "launcherCoverRetry").toLongLong() > QDateTime::currentSecsSinceEpoch()) {
		return m_config->getQtOption(launcherStatsKey(id), "launcherCoverRetryReason").toString() == QStringLiteral("notFound") ? tr("No cover found") : tr("Download failed");
	}
	if (m_autoArtworkPaused) return tr("Paused");
	return shouldAutoDownloadCovers() || m_bulkCoverDownloadRequested ? tr("Queued") : tr("No artwork");
}

void LauncherWindow::updateBrowseCoverPrompt() {
	if (m_browseCoverPrompt) {
		m_browseCoverPrompt->setVisible(false);
	}
}

void LauncherWindow::updateCoverDownloadStatus() {
	const bool active = !m_coverDownloadsInFlight.isEmpty() || !m_coverDownloadPendingOrder.isEmpty() || m_bulkCoverDownloadRequested || m_libraryCoverQueueIndex < m_libraryCoverPaths.size() || (m_catalogArtworkRequested && shouldAutoDownloadCovers() && m_browseCoverQueueIndex < m_browsePreloadIds.size());
	if (m_coverDownloadStatus) {
		m_coverDownloadStatus->setVisible(active || m_coverDownloadCompletedCount > 0);
		m_coverDownloadStatus->setText(active
			? tr("Artwork · %1 / %2 processed · %3 downloading · you can keep browsing").arg(m_coverDownloadCompletedCount).arg(m_coverDownloadQueuedCount).arg(m_coverDownloadsInFlight.size())
			: tr("Artwork finished · %1 downloaded · %2 unavailable (use Change Cover for a custom image)").arg(m_coverDownloadCompletedCount - m_coverDownloadFailedCount).arg(m_coverDownloadFailedCount));
	}
	if (m_buildArtworkCacheButton) m_buildArtworkCacheButton->setText(active ? tr("Cancel Artwork Downloads") : tr("Download Missing Artwork"));
}

void LauncherWindow::queueMissingCoversForVisibleItems(bool manual) {
	if (!m_grid || !m_grid->viewport()) {
		return;
	}
	const QRect visible = m_grid->viewport()->rect().adjusted(-40, -40, 40, 40);
	for (int i = 0; i < m_grid->count(); ++i) {
		QListWidgetItem* item = m_grid->item(i);
		if (!item) {
			continue;
		}
		const QRect itemRect = m_grid->visualItemRect(item);
		if (!itemRect.isValid() || !visible.intersects(itemRect)) {
			continue;
		}
		const QString id = item->data(Qt::UserRole).toString();
		if (id.isEmpty() || !hasMissingCoverForPath(id)) {
			continue;
		}
		if (isCatalogEntry(id)) {
			queueAutoCoverDownloadForCatalogTitle(catalogTitleFromId(id), false);
		} else {
			queueAutoCoverDownloadForPath(id, manual);
		}
	}
}

void LauncherWindow::queueMissingCoversForBrowseCatalog() {
	m_catalogArtworkRequested = true;
	if (!shouldAutoDownloadCovers()) return;
	if (m_browsePreloadIds.isEmpty()) {
		for (const QString& title : m_catalogTitles) m_browsePreloadIds.append(catalogIdForTitle(title));
		m_browseCoverQueueIndex = 0;
	}
	if (m_browseCoverQueueTimer) m_browseCoverQueueTimer->start();
	updateCoverDownloadStatus();
}

void LauncherWindow::queueMissingCoversForLibrary() {
	if (!shouldAutoDownloadCovers()) return;
	// Snapshot the whole library, not the current search, page or viewport.
	// Resolve cached artwork in small batches to keep folder selection responsive.
	m_libraryCoverPaths = m_allRoms;
	m_libraryCoverQueueIndex = 0;
	if (m_browseCoverQueueTimer) m_browseCoverQueueTimer->start();
	updateCoverDownloadStatus();
}

void LauncherWindow::startBrowseCoverDownloads() {
	if (!m_coverDownloadsInFlight.isEmpty() || !m_coverDownloadPendingOrder.isEmpty() || m_bulkCoverDownloadRequested || m_libraryCoverQueueIndex < m_libraryCoverPaths.size() || (m_catalogArtworkRequested && shouldAutoDownloadCovers() && m_browseCoverQueueIndex < m_browsePreloadIds.size())) { cancelArtworkDownloads(); return; }
	m_autoArtworkPaused = false;
	m_bulkCoverDownloadRequested = true;
	m_coverDownloadQueuedCount = m_coverDownloadCompletedCount = m_coverDownloadFailedCount = 0;
	m_browsePreloadIds.clear();
	for (const QString& title : m_catalogTitles) m_browsePreloadIds.append(catalogIdForTitle(title));
	m_browseCoverQueueIndex = 0;
	queueNextBrowseCoverBatch();
}

void LauncherWindow::dismissBrowseCoverPrompt() {
}

void LauncherWindow::queueNextBrowseCoverBatch() {
	int examined = 0;
	if (shouldAutoDownloadCovers()) {
		while (m_libraryCoverQueueIndex < m_libraryCoverPaths.size() && examined < kBrowseCoverQueueChunkSize && m_coverDownloadPendingOrder.size() < 24) {
			++examined;
			const QString path = m_libraryCoverPaths.at(m_libraryCoverQueueIndex++);
			if (!m_hiddenInstalledPaths.contains(normalizedRomPath(path))) queueAutoCoverDownloadForPath(path, false);
		}
	}
	if (m_bulkCoverDownloadRequested || (m_catalogArtworkRequested && shouldAutoDownloadCovers())) {
		while (m_browseCoverQueueIndex < m_browsePreloadIds.size() && examined++ < kBrowseCoverQueueChunkSize && m_coverDownloadPendingOrder.size() < 24) {
			const QString id = m_browsePreloadIds.at(m_browseCoverQueueIndex++);
			queueAutoCoverDownloadForCatalogTitle(catalogTitleFromId(id), false);
		}
	}
	pumpCoverDownloadQueue();
	const bool moreLibrary = shouldAutoDownloadCovers() && m_libraryCoverQueueIndex < m_libraryCoverPaths.size();
	const bool moreCatalog = (m_bulkCoverDownloadRequested || (m_catalogArtworkRequested && shouldAutoDownloadCovers())) && m_browseCoverQueueIndex < m_browsePreloadIds.size();
	if (moreLibrary || moreCatalog) m_browseCoverQueueTimer->start();
	else if (m_coverDownloadsInFlight.isEmpty() && m_coverDownloadPendingOrder.isEmpty()) { m_bulkCoverDownloadRequested = false; updateCoverDownloadStatus(); }
}


void LauncherWindow::ensureArtworkIndex(const QString& system) {
	if (m_artworkIndexesReady.contains(system) || m_artworkIndexesLoading.contains(system)) return;
	m_artworkIndexesLoading.insert(system);
	const quint64 generation = m_downloadGeneration;
	const QString cache = QDir(coverCacheDirPath()).filePath("index-" + system + ".html");
	auto* cached = new QFutureWatcher<QByteArray>(this);
	connect(cached, &QFutureWatcher<QByteArray>::finished, this, [this, cached, system, generation]() {
		const QByteArray data = cached->result();
		cached->deleteLater();
		if (generation != m_downloadGeneration) return;
		if (!data.isEmpty() || !m_artworkNetwork) { parseArtworkIndex(system, data, generation); return; }
		QNetworkRequest request(QUrl(QStringLiteral("https://thumbnails.libretro.com/%1/Named_Boxarts/").arg(libretroSystemPath(system))));
		QNetworkReply* reply = m_artworkNetwork->get(request);
		m_coverReplies.insert(reply);
		QTimer::singleShot(8000, reply, [reply]() { if (!reply->isFinished()) reply->abort(); });
		connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64 total) { if (received > 4 * 1024 * 1024 || total > 4 * 1024 * 1024) reply->abort(); });
		connect(reply, &QNetworkReply::finished, this, [this, reply, system, generation]() {
			m_coverReplies.remove(reply);
			const bool ok = reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
			const QByteArray data = ok ? reply->read(4 * 1024 * 1024 + 1) : QByteArray();
			reply->deleteLater();
			if (generation == m_downloadGeneration) parseArtworkIndex(system, data.size() <= 4 * 1024 * 1024 ? data : QByteArray(), generation);
		});
	});
	cached->setFuture(QtConcurrent::run(&m_artworkWorkers, [cache]() {
		const QFileInfo info(cache);
		if (!info.exists() || info.lastModified().secsTo(QDateTime::currentDateTime()) > 7 * 86400 || info.size() > 4 * 1024 * 1024) return QByteArray();
		QFile file(cache);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	}));
}

void LauncherWindow::parseArtworkIndex(const QString& system, const QByteArray& data, quint64 generation) {
	auto* watcher = new QFutureWatcher<QHash<QString, QString>>(this);
	connect(watcher, &QFutureWatcher<QHash<QString, QString>>::finished, this, [this, watcher, system, generation]() {
		const auto index = watcher->result();
		watcher->deleteLater();
		if (generation != m_downloadGeneration) return;
		m_artworkIndexes.insert(system, index);
		m_artworkIndexesLoading.remove(system);
		m_artworkIndexesReady.insert(system);
		pumpCoverDownloadQueue();
	});
	const QString cache = QDir(coverCacheDirPath()).filePath("index-" + system + ".html");
	watcher->setFuture(QtConcurrent::run(&m_artworkWorkers, [data, cache]() {
		QHash<QString, QString> index;
		const QString html = QString::fromUtf8(data);
		const QRegularExpression links(QStringLiteral("href=[\"']([^\"']+\\.png)[\"']"), QRegularExpression::CaseInsensitiveOption);
		auto matches = links.globalMatch(html);
		while (matches.hasNext()) {
			QString name = QUrl::fromPercentEncoding(matches.next().captured(1).toUtf8());
			name.replace("&amp;", "&");
			name.chop(4);
			const QString key = compactCatalogLookupKey(name);
			if (!key.isEmpty() && (!index.contains(key) || catalogTitleBetterThan(name, index.value(key)))) index.insert(key, name);
		}
		if (!index.isEmpty()) {
			QDir().mkpath(QFileInfo(cache).absolutePath());
			QSaveFile file(cache);
			if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size()) file.commit();
		}
		return index;
	}));
}

void LauncherWindow::pumpCoverDownloadQueue() {
	while (m_coverDownloadsInFlight.size() < kMaxConcurrentCoverDownloads && !m_coverDownloadPendingOrder.isEmpty()) {
		const QString id = m_coverDownloadPendingOrder.first();
		const QString system = systemFolderNameForRom(id);
		if (!m_artworkIndexesReady.contains(system)) { ensureArtworkIndex(system); break; }
		m_coverDownloadPendingOrder.removeFirst();
		QStringList candidates = m_coverDownloadCandidateMap.take(id);
		QStringList exact;
		for (const QString& candidate : candidates) {
			const QString match = m_artworkIndexes.value(system).value(compactCatalogLookupKey(candidate));
			if (!match.isEmpty()) exact.append(match);
		}
		candidates = exact + candidates;
		candidates.removeDuplicates();
		const bool manual = m_coverDownloadManualMap.take(id);
		m_coverDownloadCatalogMap.remove(id);
		if (id.isEmpty() || candidates.isEmpty() || !hasMissingCoverForPath(id)) { ++m_coverDownloadCompletedCount; continue; }
		m_coverDownloadsInFlight.insert(id);
		queueCoverUiRefresh(id);
		m_coverDownloadStarted.insert(id, QDateTime::currentMSecsSinceEpoch());
		startCoverDownloadAttempt(id, candidates, 0, manual);
	}
	updateCoverDownloadStatus();
}

void LauncherWindow::queueCoverUiRefresh(const QString& id) {
	if (id.isEmpty()) {
		return;
	}
	m_coverUiPendingRefreshIds.insert(id);
	if (m_coverUiBatchTimer && !m_coverUiBatchTimer->isActive()) {
		m_coverUiBatchTimer->start();
	}
}

void LauncherWindow::flushPendingCoverUiRefreshes() {
	if (m_coverUiPendingRefreshIds.isEmpty()) {
		return;
	}
	const auto ids = m_coverUiPendingRefreshIds.values();
	m_coverUiPendingRefreshIds.clear();
	for (const QString& id : ids) {
		updateCoverUiForId(id);
	}
	if (m_grid && m_grid->viewport()) {
		m_grid->viewport()->update();
	}
	updateCoverDownloadStatus();
}

void LauncherWindow::updateCoverUiForId(const QString& id) {
	QStringList affected{id};
	if (isCatalogEntry(id)) {
		const QString linked = installedRomPathForCatalogId(id);
		if (!linked.isEmpty()) affected.append(linked);
	} else {
		const QString title = bestCatalogTitleForInstalledPath(id);
		if (!title.isEmpty()) affected.append(catalogIdForTitle(title));
	}
	for (const QString& path : affected) {
		const QString norm = isCatalogEntry(path) ? path : normalizedRomPath(path);
		m_coverPathCache.remove(norm);
		for (auto it = m_coverPixmapCache.begin(); it != m_coverPixmapCache.end();) {
			if (it.key().startsWith(norm + "|")) it = m_coverPixmapCache.erase(it);
			else ++it;
		}
		refreshArtworkWidgets(path);
	}
	m_backdropPixmapCache.clear();
}

void LauncherWindow::refreshArtworkWidgets(const QString& id) {
	if (m_grid) {
		for (int i = 0; i < m_grid->count(); ++i) {
			auto* item = m_grid->item(i);
			if (item->data(Qt::UserRole).toString() != id) continue;
			item->setData(Qt::UserRole + 1, false);
			updateItemVisual(item); // Install completed thumbnails before their rows are scrolled into view.
			break;
		}
	}
	if (m_selectedPath == id && m_detailsThumb) m_detailsThumb->setPixmap(cachedCoverPixmap(id, m_detailsThumb->size()));
	if (m_continueLaunchPath == id && m_continueCover) m_continueCover->setPixmap(cachedCoverPixmap(id, m_continueCover->size()));
	if (m_selectedPath == id || m_continueLaunchPath == id) updateDynamicBackdrops();
}

void LauncherWindow::finishCoverDownloadJob(const QString& id, bool success, bool manual, int retrySeconds) {
	m_coverDownloadsInFlight.remove(id);
	m_coverDownloadStarted.remove(id);
	++m_coverDownloadCompletedCount;
	if (!success) ++m_coverDownloadFailedCount;
	if (m_config) {
		m_config->setQtOption(launcherStatsKey(id), success ? qint64(0) : QDateTime::currentSecsSinceEpoch() + retrySeconds, "launcherCoverRetry");
		m_config->setQtOption(launcherStatsKey(id), success ? QString() : (retrySeconds == 86400 ? QStringLiteral("notFound") : QStringLiteral("network")), "launcherCoverRetryReason");
	}
	queueCoverUiRefresh(id);
	if (!success && manual) QMessageBox::information(this, tr("Cover unavailable"), tr("No cover could be downloaded. You can try again later or use Change Cover to choose an image."));
	pumpCoverDownloadQueue();
	if (m_bulkCoverDownloadRequested && m_browseCoverQueueIndex >= m_browsePreloadIds.size() && m_coverDownloadsInFlight.isEmpty() && m_coverDownloadPendingOrder.isEmpty()) m_bulkCoverDownloadRequested = false;
	updateCoverDownloadStatus();
}

void LauncherWindow::cancelArtworkDownloads() {
	m_autoArtworkPaused = true;
	m_libraryCoverPaths.clear();
	m_libraryCoverQueueIndex = 0;
	m_browsePreloadIds.clear();
	m_browseCoverQueueIndex = 0;
	++m_downloadGeneration;
	m_artworkIndexesLoading.clear();
	m_bulkCoverDownloadRequested = false;
	if (m_browseCoverQueueTimer) m_browseCoverQueueTimer->stop();
	m_coverDownloadPendingOrder.clear();
	m_coverDownloadCandidateMap.clear();
	m_coverDownloadManualMap.clear();
	const auto replies = m_coverReplies;
	m_coverReplies.clear();
	for (QNetworkReply* reply : replies) { disconnect(reply, nullptr, this, nullptr); reply->abort(); reply->deleteLater(); }
	m_coverDownloadsInFlight.clear();
	m_coverDownloadStarted.clear();
	m_coverAutoTried.clear();
	m_coverDownloadQueuedCount = m_coverDownloadCompletedCount = m_coverDownloadFailedCount = 0;
	if (m_grid) for (int i = 0; i < m_grid->count(); ++i) if (!m_grid->item(i)->data(Qt::UserRole + 2).toBool()) queueCoverUiRefresh(m_grid->item(i)->data(Qt::UserRole).toString());
	updateCoverDownloadStatus();
}

bool LauncherWindow::handleLauncherNavigationEvent(QObject* watched, QEvent* event) {
	int logicalKey = -1;
	bool activate = false;
	bool back = false;
	if (event->type() == QEvent::KeyPress) {
		auto* ke = static_cast<QKeyEvent*>(event);
		switch (ke->key()) {
		case Qt::Key_Up: logicalKey = GBA_KEY_UP; break;
		case Qt::Key_Down: logicalKey = GBA_KEY_DOWN; break;
		case Qt::Key_Left: logicalKey = GBA_KEY_LEFT; break;
		case Qt::Key_Right: logicalKey = GBA_KEY_RIGHT; break;
		case Qt::Key_Return:
		case Qt::Key_Enter:
		case Qt::Key_Space: activate = true; break;
		case Qt::Key_Escape: back = true; break;
		default: break;
		}
	} else if (event->type() == GamepadButtonEvent::Down() || event->type() == GamepadButtonEvent::Up()) {
		auto* ge = static_cast<GamepadButtonEvent*>(event);
		const int buttonKey = ge->platformKey();
		const int rawButton = ge->value();
		const bool isDown = event->type() == GamepadButtonEvent::Down();
		constexpr int kLeftStickButton = 7;
		constexpr int kRightStickButton = 8;
		constexpr int kLeftShoulderButton = 9;
		constexpr int kRightShoulderButton = 10;
		if (buttonKey == GBA_KEY_SELECT || buttonKey == GBA_KEY_START) {
			updateLauncherComboState(buttonKey == GBA_KEY_SELECT ? isDown : m_controllerSelectDown,
				buttonKey == GBA_KEY_START ? isDown : m_controllerStartDown);
			event->accept();
			return true;
		}
		if (isDown && (rawButton == kLeftShoulderButton || buttonKey == GBA_KEY_L)) {
			toggleLauncherFullscreen();
			event->accept();
			return true;
		}
		if (isDown && (rawButton == kRightShoulderButton || buttonKey == GBA_KEY_R)) {
			toggleToRunningGameWindow();
			event->accept();
			return true;
		}
		if (isDown) {
			if (rawButton == kLeftStickButton) {
				logicalKey = GBA_KEY_L;
			} else if (rawButton == kRightStickButton) {
				logicalKey = GBA_KEY_R;
			} else {
				logicalKey = buttonKey;
			}
			activate = logicalKey == GBA_KEY_A || logicalKey == GBA_KEY_START;
			back = logicalKey == GBA_KEY_B;
		}
		if (logicalKey == GBA_KEY_START) {
			activate = true;
		}
		back = logicalKey == GBA_KEY_B;
	} else if (event->type() == GamepadAxisEvent::Type()) {
		auto* gae = static_cast<GamepadAxisEvent*>(event);
		if (gae->axis() >= 0 && gae->axis() <= 3) {
			const int normalizedAxis = (gae->axis() == 2) ? 0 : (gae->axis() == 3 ? 1 : gae->axis());
			const int direction = gae->isNew() ? static_cast<int>(gae->direction()) : 0;
			setAnalogScrollDirection(normalizedAxis, direction);
			if (analogVerticalScrollBar() || analogHorizontalScrollBar()) {
				event->accept();
				return true;
			}
		}
		if (gae->isNew()) {
			logicalKey = gae->platformKey();
		}
	} else if (event->type() == GamepadHatEvent::Down()) {
		auto* ghe = static_cast<GamepadHatEvent*>(event);
		const int hatKeys = ghe->platformKeys();
		if (hatKeys & (1 << GBA_KEY_UP)) logicalKey = GBA_KEY_UP;
		else if (hatKeys & (1 << GBA_KEY_DOWN)) logicalKey = GBA_KEY_DOWN;
		else if (hatKeys & (1 << GBA_KEY_LEFT)) logicalKey = GBA_KEY_LEFT;
		else if (hatKeys & (1 << GBA_KEY_RIGHT)) logicalKey = GBA_KEY_RIGHT;
	}

	if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) {
		if (m_grid && (watched == m_grid || watched == m_grid->viewport())) {
			if (auto* current = m_grid->currentItem()) {
				updateItemVisual(current);
			}
		}
		return false;
	}

	if (logicalKey == -1 && !activate && !back) {
		return false;
	}

	if (logicalKey == GBA_KEY_UP || logicalKey == GBA_KEY_DOWN || logicalKey == GBA_KEY_LEFT || logicalKey == GBA_KEY_RIGHT || activate || back) {
		setAnalogScrollDirection(0, 0);
		setAnalogScrollDirection(1, 0);
	}


	QWidget* focus = QApplication::focusWidget();
	if (!focus || !isAncestorOf(focus)) {
		if (m_mainStack && m_mainStack->currentWidget() == m_settingsPage && m_themeCombo) {
			focus = m_themeCombo;
			m_themeCombo->setFocus();
		} else if (m_grid && m_grid->count()) {
			focus = m_grid;
			m_grid->setFocus();
			if (m_grid->currentRow() < 0) {
				m_grid->setCurrentRow(0);
			}
		} else {
			focus = m_navLibrary;
			if (m_navLibrary) m_navLibrary->setFocus();
		}
	}

	auto navButtons = QList<QPushButton*>{m_navLibrary, m_navFavorites, m_navRecent, m_navBrowse, m_navSettings};
	auto sidebarButtons = QList<QPushButton*>{m_navLibrary, m_navFavorites, m_navRecent, m_navBrowse, m_navSettings,
		m_footerBrandButton, m_aboutFooterButton, m_websiteFooterButton};
	auto currentSidebarIndex = [&navButtons]() {
		for (int i = 0; i < navButtons.size(); ++i) {
			if (navButtons[i] && navButtons[i]->isChecked()) {
				return i;
			}
		}
		return 0;
	};
	const int navIndex = sidebarButtons.indexOf(qobject_cast<QPushButton*>(focus));
	const int sectionNavIndex = navButtons.indexOf(qobject_cast<QPushButton*>(focus));
	const bool toggleFavoriteAction = false;

	if (focus == m_grid || watched == m_grid || (m_grid && watched == m_grid->viewport())) {
		if (logicalKey == GBA_KEY_UP) {
			if (m_grid && m_grid->currentRow() >= 0) {
				const int current = m_grid->currentRow();
				const int columns = qMax(1, m_grid->viewport()->width() / qMax(1, m_grid->gridSize().width()));
				if (current - columns < 0) {
					focusSidebarButton(qMax(0, navButtons.indexOf(m_navLibrary)));
					event->accept(); return true;
				}
			}
			moveGridSelection(-1, 0); event->accept(); return true; }
		if (logicalKey == GBA_KEY_DOWN) {
			if (m_grid && m_grid->currentItem()) {
				const int current = m_grid->currentRow();
				const int columns = qMax(1, m_grid->viewport()->width() / qMax(1, m_grid->gridSize().width()));
				if (current + columns >= m_grid->count() && m_playButton && m_playButton->isEnabled()) {
					m_playButton->setFocus();
					event->accept();
					return true;
				}
			}
			moveGridSelection(1, 0); event->accept(); return true; }
		if (logicalKey == GBA_KEY_LEFT) {
			if (m_grid && m_grid->currentRow() >= 0) {
				const int current = m_grid->currentRow();
				const int columns = qMax(1, m_grid->viewport()->width() / qMax(1, m_grid->gridSize().width()));
				if ((current % columns) == 0) {
					focusSidebarButton(qMax(0, navButtons.indexOf(m_navLibrary)));
					event->accept(); return true;
				}
			}
			moveGridSelection(0, -1); event->accept(); return true; }
		if (logicalKey == GBA_KEY_RIGHT) {
			if (m_grid && m_grid->currentRow() >= 0) {
				const int current = m_grid->currentRow();
				const int columns = qMax(1, m_grid->viewport()->width() / qMax(1, m_grid->gridSize().width()));
				const bool atRowEnd = ((current % columns) == columns - 1) || (current + 1 >= m_grid->count());
				if (atRowEnd && m_playButton && m_playButton->isEnabled()) {
					m_playButton->setFocus();
					event->accept(); return true;
				}
			}
			moveGridSelection(0, 1); event->accept(); return true; }
		if (activate) { launchSelected(); event->accept(); return true; }
		if (back) { focusSidebarButton(0); event->accept(); return true; }
		if (logicalKey == GBA_KEY_L) { const int next = (currentSidebarIndex() + navButtons.size() - 1) % navButtons.size(); focusSidebarButton(next); navButtons[next]->click(); event->accept(); return true; }
		if (logicalKey == GBA_KEY_R) { const int next = (currentSidebarIndex() + 1) % navButtons.size(); focusSidebarButton(next); navButtons[next]->click(); event->accept(); return true; }
	}

	if (navIndex >= 0) {
		if (logicalKey == GBA_KEY_UP) { focusSidebarButton((navIndex + sidebarButtons.size() - 1) % sidebarButtons.size()); event->accept(); return true; }
		if (logicalKey == GBA_KEY_DOWN) { focusSidebarButton((navIndex + 1) % sidebarButtons.size()); event->accept(); return true; }
		if (logicalKey == GBA_KEY_RIGHT) {
			if (sectionNavIndex < 0) {
				event->accept(); return true;
			}
			if (m_mainStack && m_mainStack->currentWidget() == m_settingsPage && m_openEmulatorSettingsButton) {
				m_openEmulatorSettingsButton->setFocus();
			} else if (m_grid && m_grid->count()) {
				m_grid->setFocus();
				if (m_grid->currentRow() < 0) m_grid->setCurrentRow(0);
			}
			event->accept(); return true; }
		if (logicalKey == GBA_KEY_L) { const int next = (currentSidebarIndex() + navButtons.size() - 1) % navButtons.size(); focusSidebarButton(next); navButtons[next]->click(); event->accept(); return true; }
		if (logicalKey == GBA_KEY_R) { const int next = (currentSidebarIndex() + 1) % navButtons.size(); focusSidebarButton(next); navButtons[next]->click(); event->accept(); return true; }
		if (activate) { if (sidebarButtons[navIndex]) sidebarButtons[navIndex]->click(); event->accept(); return true; }
	}

	QList<QPushButton*> detailButtons{m_playButton, m_favButton, m_saveSlotsToggle};
	for (auto* button : m_saveStateButtons) if (button->isVisible() && button->isEnabled()) detailButtons.append(button);
	const int detailIndex = detailButtons.indexOf(qobject_cast<QPushButton*>(focus));
	if (detailIndex >= 0) {
		QWidget* next = nullptr;
		if (back || logicalKey == GBA_KEY_LEFT) next = m_grid;
		else if (logicalKey == GBA_KEY_UP) next = detailIndex > 0 ? static_cast<QWidget*>(detailButtons.at(detailIndex - 1)) : m_grid;
		else if ((logicalKey == GBA_KEY_DOWN || logicalKey == GBA_KEY_RIGHT) && detailIndex + 1 < detailButtons.size()) next = detailButtons.at(detailIndex + 1);
		if (next) {
			next->setFocus();
			if (next != m_grid && m_saveStateScroll) m_saveStateScroll->ensureWidgetVisible(next, 0, 24);
			event->accept(); return true;
		}
		if (toggleFavoriteAction) { toggleFavorite(); event->accept(); return true; }
		if (activate) { detailButtons.at(detailIndex)->click(); event->accept(); return true; }
	}

	if (m_mainStack && m_mainStack->currentWidget() == m_settingsPage) {
		QList<QWidget*> settingsWidgets{
			m_openEmulatorSettingsButton,
			m_chooseFolderSettingsButton,
			m_refreshLibrarySettingsButton,
			m_recursiveScanCheck,
			m_skipSplashCheck,
			m_launchFullscreenCheck,
			m_themeCombo,
			m_performanceModeCheck,
			m_buildArtworkCacheButton,
			m_autoDownloadCoversCheck,
			m_clearLauncherCacheButton,
			m_reportBugSettingsButton,
			m_factoryResetButton
		};
		settingsWidgets.erase(std::remove(settingsWidgets.begin(), settingsWidgets.end(), nullptr), settingsWidgets.end());
		const int settingsIndex = settingsWidgets.indexOf(focus);
		if (settingsIndex >= 0) {
			if (logicalKey == GBA_KEY_DOWN && settingsIndex + 1 < settingsWidgets.size()) { settingsWidgets[settingsIndex + 1]->setFocus(); if (m_settingsScroll) m_settingsScroll->ensureWidgetVisible(settingsWidgets[settingsIndex + 1], 0, 32); event->accept(); return true; }
			if (logicalKey == GBA_KEY_UP) {
				if (settingsIndex > 0) { settingsWidgets[settingsIndex - 1]->setFocus(); if (m_settingsScroll) m_settingsScroll->ensureWidgetVisible(settingsWidgets[settingsIndex - 1], 0, 32); }
				else focusSidebarButton(navButtons.indexOf(m_navSettings));
				event->accept(); return true;
			}
			if (logicalKey == GBA_KEY_LEFT && focus == m_themeCombo) { event->accept(); return true; }
			if (logicalKey == GBA_KEY_RIGHT) {
				if (focus == m_themeCombo) { event->accept(); return true; }
			}
			if ((activate || logicalKey == GBA_KEY_A || logicalKey == GBA_KEY_START) && focus) {
				if (auto* cb = qobject_cast<QCheckBox*>(focus)) cb->toggle();
				else if (auto* button = qobject_cast<QPushButton*>(focus)) button->click();
				event->accept(); return true;
			}
			if (back) { focusSidebarButton(navButtons.indexOf(m_navSettings)); event->accept(); return true; }
		}
	}

	if (back && m_mainStack && m_mainStack->currentWidget() == m_settingsPage) {
		showAllGames();
		if (m_grid) m_grid->setFocus();
		event->accept();
		return true;
	}

	return false;
}

bool LauncherWindow::eventFilter(QObject* watched, QEvent* event) {
	QWidget* watchedWidget = qobject_cast<QWidget*>(watched);
	const bool belongsToLauncher = watched == this || (watchedWidget && (watchedWidget == this || isAncestorOf(watchedWidget)));

	if (watched == m_themeCombo && event->type() == QEvent::Wheel) {
		event->ignore();
		return true;
	}

	if (m_grid && watched == m_grid->viewport()) {
		if (event->type() == QEvent::Leave) {
			setHoveredItem(nullptr);
		} else if (event->type() == QEvent::MouseMove) {
			auto* me = static_cast<QMouseEvent*>(event);
			setHoveredItem(m_grid->itemAt(me->pos()));
		} else if (event->type() == QEvent::MouseButtonDblClick) {
			return true;
		} else if (event->type() == QEvent::Wheel) {
			auto* we = static_cast<QWheelEvent*>(event);
			QPoint pixelDelta = we->pixelDelta();
			QPoint angleDelta = we->angleDelta();
			const bool horizontal = (QApplication::keyboardModifiers() & Qt::ShiftModifier) || (pixelDelta.x() != 0) || (angleDelta.x() != 0);
			QScrollBar* bar = horizontal ? m_grid->horizontalScrollBar() : m_grid->verticalScrollBar();
			if (bar) {
				int delta = 0;
				if (horizontal) {
					delta = pixelDelta.x();
					if (!delta && angleDelta.x()) {
						delta = angleDelta.x() / 4;
					}
				} else {
					delta = pixelDelta.y();
					if (!delta && angleDelta.y()) {
						delta = angleDelta.y() / 4;
					}
				}
				if (delta != 0) {
					bar->setValue(std::clamp(bar->value() - delta, bar->minimum(), bar->maximum()));
					event->accept();
					return true;
				}
			}
		}
	}

	if (belongsToLauncher && handleLauncherNavigationEvent(watched, event)) {
		return true;
	}

	return QMainWindow::eventFilter(watched, event);
}

void LauncherWindow::updateSidebarIcons() {
	const QSize iconSize(24, 24);
	const int iconTextGap = 8;
	const QColor defaultColor(214, 220, 234);
	const QColor activeColor(255, 255, 255);
	auto apply = [&](QPushButton* button, const QString& resourcePath) {
		if (!button) {
			return;
		}
		const QColor color = button->isChecked() ? activeColor : defaultColor;
		button->setIcon(sidebarIcon(resourcePath, iconSize, color, button->devicePixelRatioF(), iconTextGap));
		button->setIconSize(QSize(iconSize.width() + iconTextGap, iconSize.height()));
	};
	apply(m_navLibrary, QStringLiteral(":/icons/play.png"));
	apply(m_navFavorites, QStringLiteral(":/icons/star.png"));
	apply(m_navRecent, QStringLiteral(":/icons/history.png"));
	apply(m_navBrowse, QStringLiteral(":/icons/gamepad.png"));
	apply(m_navSettings, QStringLiteral(":/icons/gear.png"));
}

void LauncherWindow::applyModernTheme() {
	m_launcherTheme = normalizedLauncherThemeName(m_launcherTheme);
	const QColor accent = launcherAccentForTheme(m_launcherTheme);
	const QString sidebarTop = launcherSidebarTopForTheme(m_launcherTheme);
	const QString sidebarBottom = launcherSidebarBottomForTheme(m_launcherTheme);
	const QString contentBg = launcherContentBgForTheme(m_launcherTheme);
	const int ar = accent.red();
	const int ag = accent.green();
	const int ab = accent.blue();
	const QColor accentLight = accent.lighter(135);
	const QColor accentMid = accent.lighter(112);
	const QColor accentDark = accent.darker(125);
	const QColor accentDeeper = accent.darker(165);
	const int alr = accentLight.red();
	const int alg = accentLight.green();
	const int alb = accentLight.blue();
	const int amr = accentMid.red();
	const int amg = accentMid.green();
	const int amb = accentMid.blue();
	const int adr = accentDark.red();
	const int adg = accentDark.green();
	const int adb = accentDark.blue();
	const int ahr = accentDeeper.red();
	const int ahg = accentDeeper.green();
	const int ahb = accentDeeper.blue();

	QPalette themedPalette = qApp->palette();
	themedPalette.setColor(QPalette::Highlight, accent);
	themedPalette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
	themedPalette.setColor(QPalette::Link, accent.lighter(130));
	themedPalette.setColor(QPalette::LinkVisited, accent.lighter(115));
	themedPalette.setColor(QPalette::Button, accentDark);
	themedPalette.setColor(QPalette::ButtonText, QColor(255, 255, 255));
	qApp->setPalette(themedPalette);
	setPalette(themedPalette);

	setStyleSheet(QString(R"(
QMainWindow { background: %4; }

#sidebar {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                               stop:0 %5,
                               stop:1 %6);
    border-right: 1px solid rgba(255,255,255,18);
}

#brandLogo { margin-bottom: 6px; }
#pageEyebrow { color: rgba(%1,%2,%3,205); font-size: 11px; font-weight: 700; text-transform: uppercase; letter-spacing: 1px; }
#heroTitle { color: rgba(255,255,255,235); font-size: 30px; font-weight: 700; }
#heroTitle[topPageTitle="true"] { font-size: 32px; font-weight: 800; }
#subSectionTitle { color: rgba(255,255,255,225); font-size: 14px; font-weight: 700; }

#navButton {
    text-align: left;
    padding: 12px 18px 12px 14px;
    border-radius: 16px;
    background: transparent;
    color: rgba(228,232,240,208);
    border: 1px solid transparent;
    font-size: 14px;
    font-weight: 500;
    min-height: 34px;
}
#navButton:checked {
    background-color: rgba(%1,%2,%3,42);
    color: rgba(255,255,255,248);
    border-color: rgba(%1,%2,%3,190);
    font-weight: 600;
}
#navButton:hover {
    background-color: rgba(%1,%2,%3,16);
    color: rgba(255,255,255,232);
    border-color: rgba(%1,%2,%3,92);
}
#navButton:focus {
    color: rgba(255,255,255,240);
    border-color: rgba(%1,%2,%3,170);
    background-color: rgba(%1,%2,%3,24);
}
#navButton:pressed {
    background-color: rgba(%1,%2,%3,30);
}
#navButton::icon {
    padding-right: 4px;
}

#footerText { color: rgba(255,255,255,110); font-size: 12px; padding-left: 2px; }
#footerBrandLink { background: transparent; border: 0; padding: 0 0 2px 2px; color: rgba(%1,%2,%3,230); font-size: 12px; text-align: left; }
#footerBrandLink:hover { color: rgba(%1,%2,%3,255); text-decoration: underline; }
#footerBrandLink:focus { border: 1px solid rgba(%1,%2,%3,165); border-radius: 6px; padding: 4px 6px; background: rgba(%1,%2,%3,14); }
#footerLinkButton { background: rgba(%1,%2,%3,18); color: #f2e8ff; border: 1px solid rgba(%1,%2,%3,120); border-radius: 8px; padding: 6px 10px; font-size: 12px; text-align: center; }
#footerLinkButton:hover { background: rgba(%1,%2,%3,24); border-color: rgba(%1,%2,%3,145); color: #f1e8ff; }
#footerLinkButton:focus { background: rgba(%1,%2,%3,30); border-color: rgba(%1,%2,%3,210); color: #ffffff; }
#footerLinkButton:pressed { background: rgba(%1,%2,%3,40); }
#footerStatusText { color: rgba(%1,%2,%3,215); font-size: 11px; padding-top: 4px; }

#content { background: %4; }

#searchBox {
    padding: 11px 15px;
    border-radius: 14px;
    min-height: 20px;
    border: 1px solid rgba(255,255,255,18);
    background: rgba(6,8,14,208);
    color: rgba(255,255,255,226);
    selection-background-color: rgba(%1,%2,%3,150);
}
#searchBox:hover {
    border-color: rgba(255,255,255,30);
    background: rgba(9,12,18,220);
}
#searchBox:focus {
    border-color: rgba(%1,%2,%3,220);
    background: rgba(10,14,22,230);
}

#card {
    background: rgba(255,255,255,4);
    border: 1px solid rgba(210,190,240,12);
    border-radius: 14px;
}
#card QLabel { background: transparent; }
#settingsPageRoot { background: %4; }
#settingsScroll { background: transparent; border: none; }
#settingsViewport { background: %4; }
#settingsScrollContent { background: %4; }
#settingsCard {
    background: %4;
    border: none;
    border-radius: 14px;
}
#settingsCard QLabel { background: transparent; }
#saveStateTitle { color: rgba(255,255,255,225); font-weight: 700; font-size: 14px; padding: 8px 0 6px 6px; margin: 0px; }

#librarySectionTitle, #sectionTitle { color: rgba(255,255,255,220); font-weight: 600; font-size: 16px; padding: 0px; margin: 0px; }
#continueTitle { color: rgba(255,255,255,238); font-weight: 700; font-size: 22px; padding: 0px 0px 6px 0px; margin: 0px; }
#continueMeta { color: rgba(235,238,250,0.92); font-size: 14px; font-weight: 600; padding: 2px 0px 0px 0px; }
#continueStats { color: rgba(226,229,246,0.78); font-size: 13px; padding: 0px; }
#detailsInfoCard {
    background: rgba(16,12,24,238);
    border: 1px solid rgba(210,190,240,14);
    border-radius: 16px;
}
#dynamicBackdrop {
    border-radius: 14px;
    background: rgba(255,255,255,0);
}
#continueCard QLabel { background: transparent; }
#detailsPaneScroll, #detailsPaneScroll > QWidget > QWidget {
    background: transparent;
    border: none;
}
#detailsThumb {
    background: rgba(255,255,255,5);
    border: 1px solid rgba(255,255,255,12);
    border-radius: 14px;
}
#detailsTitle { color: rgba(255,255,255,242); font-weight: 700; font-size: 18px; min-height: 28px; }
#detailsMeta { color: rgba(244,246,255,235); font-size: 14px; font-weight: 600; padding: 8px 0px 6px 0px; }
#detailsInfoCard QLabel { background: transparent; }
#detailsStats { color: rgba(255,255,255,190); font-size: 12px; }
#detailsPath { color: rgba(255,255,255,152); font-size: 12px; }
#mutedText { color: rgba(232,224,245,160); font-size: 12px; padding: 0px; margin: 0px; }
#continueCover { background: rgba(255,255,255,5); border: 1px solid rgba(255,255,255,18); border-radius: 12px; padding: 0px; }
#launcherThemeCombo { padding: 10px 12px; min-height: 22px; border-radius: 12px; color: rgba(255,255,255,220); border: 1px solid rgba(255,255,255,22); background: rgba(255,255,255,10); }
#launcherThemeCombo:hover, #launcherThemeCombo:focus { border-color: rgba(%1,%2,%3,205); background: rgba(%1,%2,%3,16); }
#launcherThemeCombo QAbstractItemView { background: #15111f; color: rgba(255,255,255,220); border: 1px solid rgba(%1,%2,%3,110); selection-background-color: rgba(%1,%2,%3,82); }
QCheckBox { color: rgba(255,255,255,210); spacing: 10px; }
QCheckBox::indicator { width: 18px; height: 18px; border-radius: 5px; border: 1px solid rgba(255,255,255,40); background: rgba(255,255,255,10); }
QCheckBox::indicator:checked { border-color: rgba(%1,%2,%3,220); background: rgba(%1,%2,%3,180); }

#primaryButton {
    padding: 12px 16px;
    border-radius: 12px;
    font-weight: 700;
    color: white;
    border: 1px solid rgba(%1,%2,%3,225);
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 rgba(%10,%11,%12,245),
                                stop:0.24 rgba(%1,%2,%3,232),
                                stop:0.68 rgba(%13,%14,%15,226),
                                stop:1 rgba(%16,%17,%18,218));
}
#primaryButton:hover {
    border-color: rgba(%1,%2,%3,255);
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 rgba(%10,%11,%12,248),
                                stop:0.28 rgba(%7,%8,%9,242),
                                stop:0.68 rgba(%1,%2,%3,238),
                                stop:1 rgba(%13,%14,%15,230));
}
#primaryButton:disabled {
    color: rgba(255,255,255,120);
    border-color: rgba(255,255,255,24);
    background: rgba(255,255,255,10);
}
#primaryButton:pressed {
    background: rgba(%13,%14,%15,240);
}
#primaryButton:focus, #secondaryButton:focus, #saveStatePreviewButton:focus {
    border-color: rgba(%1,%2,%3,230);
    background: rgba(%1,%2,%3,16);
}
#detailsScroll QScrollBar:vertical { width: 10px; background: transparent; margin: 2px 0; }
#detailsScroll QScrollBar::handle:vertical { background: rgba(255,255,255,22); border-radius: 5px; min-height: 28px; }
#detailsScroll QScrollBar::handle:vertical:hover { background: rgba(%1,%2,%3,120); }
#detailsScroll QScrollBar::add-line:vertical, #detailsScroll QScrollBar::sub-line:vertical, #detailsScroll QScrollBar::add-page:vertical, #detailsScroll QScrollBar::sub-page:vertical { background: none; border: none; }
#detailsPaneScroll QScrollBar:vertical { width: 10px; background: transparent; margin: 2px 0; }
#detailsPaneScroll QScrollBar::handle:vertical { background: rgba(255,255,255,22); border-radius: 5px; min-height: 28px; }
#detailsPaneScroll QScrollBar::handle:vertical:hover { background: rgba(%1,%2,%3,120); }
#detailsPaneScroll QScrollBar::add-line:vertical, #detailsPaneScroll QScrollBar::sub-line:vertical, #detailsPaneScroll QScrollBar::add-page:vertical, #detailsPaneScroll QScrollBar::sub-page:vertical { background: none; border: none; }

#secondaryButton {
    padding: 10px 14px;
    border-radius: 12px;
    color: rgba(255,255,255,220);
    border: 1px solid rgba(255,255,255,22);
    background: rgba(255,255,255,10);
}
#secondaryButton:hover {
    border-color: rgba(%1,%2,%3,205);
    background: rgba(%1,%2,%3,16);
}
#secondaryButton:pressed {
    background: rgba(%1,%2,%3,22);
}

#saveStateCard {
    background: rgba(8,11,18,126);
    border-top: 1px solid rgba(255,255,255,8);
    border-radius: 12px;
}
#saveSlotsToggle { text-align: left; padding: 10px; border: none; border-radius: 8px; background: transparent; color: #f3edf9; font-weight: 600; }
#saveSlotsToggle:hover, #saveSlotsToggle:focus { background: rgba(%1,%2,%3,22); border: 1px solid rgba(%1,%2,%3,150); }
#saveSlotsSummary { color: rgba(230,222,244,180); padding: 0px 10px 10px 10px; font-size: 12px; }
#saveStatePreviewButton {
    text-align: left;
    padding: 8px 12px;
    border-radius: 10px;
    color: rgba(255,255,255,220);
    border: 1px solid rgba(255,255,255,14);
    background: rgba(15,19,29,214);
}
#saveStatePreviewButton:hover {
    border-color: rgba(%1,%2,%3,175);
    background: rgba(22,18,34,226);
}
#saveStatePreviewButton:pressed {
    background: rgba(12,16,24,230);
}
#saveStatePreviewButton:disabled {
    color: rgba(255,255,255,145);
    border-color: rgba(255,255,255,10);
    background: rgba(12,15,22,178);
}

#grid {
    background: transparent;
    border: none;
    outline: none;
}
QMenu { background: #191421; color: #f4effa; border: 1px solid #51405e; padding: 6px; }
QMenu::item { padding: 8px 24px 8px 12px; }
QMenu::item:selected { background: rgba(%1,%2,%3,70); }
QScrollBar:vertical { width: 10px; background: transparent; }
QScrollBar::handle:vertical { background: rgba(210,190,240,65); border-radius: 5px; min-height: 32px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
#grid::item {
    border-radius: 16px;
    padding: 0px;
    margin: 0px;
    border: none;
    background: transparent;
    color: rgba(243,245,255,224);
    font-size: 10pt;
    font-weight: 600;
}
#grid::item:selected {
    background: transparent;
    border: none;
}
#grid::item:hover {
    background: transparent;
    border: none;
}

#launchOverlay {
    background: rgba(10,12,16,230);
    border: 1px solid rgba(%1,%2,%3,170);
    border-radius: 18px;
}
#launchOverlayTitle { color: rgba(255,255,255,235); font-size: 20px; font-weight: 700; }
#launchOverlaySubtitle { color: rgba(%7,%8,%9,220); font-size: 13px; font-weight: 700; }
#scanOverlayBackdrop { background: rgba(2, 3, 8, 140); }
#scanOverlayCard {
    background: rgba(10,12,16,240);
    border: 1px solid rgba(%1,%2,%3,170);
    border-radius: 20px;
}
#scanOverlayTitle { color: rgba(255,255,255,235); font-size: 22px; font-weight: 700; }
#scanOverlaySubtitle { color: rgba(255,255,255,185); font-size: 13px; }
#scanOverlayProgress { color: rgba(%7,%8,%9,230); font-size: 14px; font-weight: 700; }
#scanOverlayCurrentFile { color: rgba(255,255,255,210); font-size: 13px; }
#scanOverlayBar {
    min-height: 12px;
    max-height: 12px;
    border-radius: 6px;
    background: rgba(255,255,255,22);
    border: 1px solid rgba(255,255,255,20);
}
#scanOverlayBar::chunk {
    border-radius: 6px;
    background: rgba(%1,%2,%3,215);
}

)").arg(ar).arg(ag).arg(ab).arg(contentBg, sidebarTop, sidebarBottom).arg(alr).arg(alg).arg(alb).arg(amr).arg(amg).arg(amb).arg(adr).arg(adg).arg(adb).arg(ahr).arg(ahg).arg(ahb));

	updateSidebarIcons();
}

QString LauncherWindow::displayTitleForPath(const QString& path) const {
	if (isCatalogEntry(path)) {
		return catalogTitleFromId(path);
	}
	const QString norm = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
	auto it = m_titleCache.constFind(norm);
	if (it != m_titleCache.constEnd()) {
		return it.value();
	}
	QString title;
	for (auto linkIt = m_catalogInstalledLinks.constBegin(); linkIt != m_catalogInstalledLinks.constEnd(); ++linkIt) {
		if (QDir::toNativeSeparators(QFileInfo(linkIt.value()).absoluteFilePath()) == norm) {
			title = linkIt.key();
			break;
		}
	}
	if (title.isEmpty()) {
		title = normalizeTitle(QFileInfo(path).fileName());
	}
	if (title.isEmpty()) {
		const QVariantMap metadata = metadataForPath(path);
		title = cleanedMetadataValue(metadata, QStringLiteral("title"));
	}
	if (title.isEmpty()) {
		title = QFileInfo(path).completeBaseName();
	}
	m_titleCache.insert(norm, title);
	return title;
}

QPixmap LauncherWindow::cachedCoverPixmap(const QString& path, const QSize& size) const {
	const QString norm = isCatalogEntry(path) ? path : normalizedRomPath(path);
	const QString key = norm + "|" + QString::number(size.width()) + "x" + QString::number(size.height());
	auto cached = m_coverPixmapCache.constFind(key);
	if (cached != m_coverPixmapCache.constEnd()) return cached.value();
	const QString artPath = findCoverArtPath(path);
	const QString status = artPath.isEmpty() ? artworkStatusForPath(path) : tr("Loading artwork");
	const QPixmap placeholder = makeCover(displayTitleForPath(path), size, status);
	if (m_coverPixmapCache.size() >= 256) m_coverPixmapCache.clear();
	m_coverPixmapCache.insert(key, placeholder);
	if (artPath.isEmpty() || m_coverImagesInFlight.contains(key)) return placeholder;
	m_coverImagesInFlight.insert(key);
	auto* self = const_cast<LauncherWindow*>(this);
	auto* watcher = new QFutureWatcher<QImage>(self);
	const quint64 generation = m_artworkGeneration;
	connect(watcher, &QFutureWatcher<QImage>::finished, self, [self, watcher, key, path, generation]() {
		const QImage result = watcher->result();
		watcher->deleteLater();
		self->m_coverImagesInFlight.remove(key);
		if (generation != self->m_artworkGeneration) {
			self->m_coverPixmapCache.remove(key);
			self->refreshArtworkWidgets(path);
			return;
		}
		if (!result.isNull()) {
			if (self->m_coverPixmapCache.size() >= 256) self->m_coverPixmapCache.clear();
			self->m_coverPixmapCache.insert(key, QPixmap::fromImage(result));
			self->m_backdropPixmapCache.clear();
			self->refreshArtworkWidgets(path);
		}
	});
	watcher->setFuture(QtConcurrent::run(&self->m_artworkWorkers, [artPath, size]() -> QImage {
		const QString thumbnail = cachedThumbnailPathForArt(artPath, size);
		QImage image(thumbnail);
		if (!image.isNull()) return image;
		QImageReader reader(artPath);
		reader.setAutoTransform(true);
		if (reader.size().isValid()) reader.setScaledSize(reader.size().scaled(size, Qt::KeepAspectRatio));
		const QImage source = reader.read();
		if (source.isNull()) return {};
		image = QImage(size, QImage::Format_ARGB32_Premultiplied);
		image.fill(Qt::transparent);
		QPainter painter(&image);
		painter.setRenderHint(QPainter::Antialiasing);
		QPainterPath clip;
		clip.addRoundedRect(QRectF(image.rect()).adjusted(1, 1, -1, -1), 12, 12);
		painter.setClipPath(clip);
		painter.fillPath(clip, QColor(255, 255, 255, 6));
		const QImage scaled = source.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		painter.drawImage((size.width() - scaled.width()) / 2, (size.height() - scaled.height()) / 2, scaled);
		painter.end();
		QDir().mkpath(QFileInfo(thumbnail).absolutePath());
		QSaveFile file(thumbnail);
		if (file.open(QIODevice::WriteOnly) && image.save(&file, "PNG")) file.commit();
		return image;
	}));
	return placeholder;
}

QPixmap LauncherWindow::decoratedCoverForPath(const QString& path, const QSize& size, bool hovered, bool selected, bool focused) const {
	QPixmap base = cachedCoverPixmap(path, size);
	if (base.isNull()) base = makeCover(displayTitleForPath(path), size);
	if (!hovered && !selected && !focused) return base;
	QPixmap out = base.copy();
	QPainter painter(&out);
	painter.setRenderHint(QPainter::Antialiasing, true);
	QColor accent = launcherAccentForTheme(m_launcherTheme);
	accent.setAlpha(selected || focused ? 255 : 150);
	painter.setBrush(Qt::NoBrush);
	painter.setPen(QPen(accent, focused ? 3.5 : (selected ? 3.0 : 1.5)));
	painter.drawRoundedRect(out.rect().adjusted(2, 2, -2, -2), 12, 12);
	painter.end();
	return out;
}

void LauncherWindow::updateItemVisual(QListWidgetItem* item) {
	if (!item) {
		return;
	}
	const QString path = item->data(Qt::UserRole).toString();
	const QString status = artworkStatusForPath(path);
	item->setData(Qt::UserRole + 3, status);
	item->setData(Qt::AccessibleDescriptionRole, displayTitleForPath(path) + QStringLiteral(" · ") + status);
	const bool hovered = !m_hoveredPath.isEmpty() && path == m_hoveredPath;
	const bool selected = m_grid && item == m_grid->currentItem();
	const bool focused = selected && m_grid && m_grid->hasFocus();
	const QSize iconSize = (m_grid && m_grid->iconSize().isValid()) ? m_grid->iconSize() : launcherGridCoverSize();
	const QPixmap cover = decoratedCoverForPath(path, iconSize, hovered, selected, focused);
	const QString norm = isCatalogEntry(path) ? path : normalizedRomPath(path);
	const QString key = norm + "|" + QString::number(iconSize.width()) + "x" + QString::number(iconSize.height());
	const bool loading = m_coverImagesInFlight.contains(key);
	if (!loading || !item->data(Qt::UserRole + 2).toBool()) item->setIcon(launcherCoverIcon(cover));
	if (!loading && !findCoverArtPath(path).isEmpty()) item->setData(Qt::UserRole + 2, true);
	item->setData(Qt::UserRole + 1, true);
}

void LauncherWindow::setHoveredItem(QListWidgetItem* item) {
	const QString newPath = item ? item->data(Qt::UserRole).toString() : QString();
	if (newPath == m_hoveredPath) {
		return;
	}
	QString old = m_hoveredPath;
	m_hoveredPath = newPath;
	if (m_grid) {
		for (int i = 0; i < m_grid->count(); ++i) {
			QListWidgetItem* it = m_grid->item(i);
			if (!it) {
				continue;
			}
			const QString path = it->data(Qt::UserRole).toString();
			if (path == old || path == m_hoveredPath) {
				updateItemVisual(it);
			}
		}
	}
}

void LauncherWindow::showLaunchOverlay(const QString& title) {
	if (!centralWidget()) {
		return;
	}
	if (!m_launchOverlay) {
		auto* overlay = new QWidget(this);
		overlay->setObjectName("launchOverlay");
		overlay->setAttribute(Qt::WA_StyledBackground, true);
		overlay->hide();
		auto* layout = new QVBoxLayout(overlay);
		layout->setContentsMargins(28, 24, 28, 24);
		layout->setSpacing(8);
		auto* titleLabel = new QLabel(overlay);
		titleLabel->setObjectName("launchOverlayTitle");
		titleLabel->setWordWrap(true);
		auto* subtitleLabel = new QLabel(overlay);
		subtitleLabel->setObjectName("launchOverlaySubtitle");
		subtitleLabel->setWordWrap(true);
		layout->addWidget(titleLabel);
		layout->addWidget(subtitleLabel);
		m_launchOverlay = overlay;
		m_launchOverlayTitle = titleLabel;
		m_launchOverlaySubtitle = subtitleLabel;
	}
	m_launchOverlay->setGeometry(rect().adjusted(width()/2 - 180, height()/2 - 60, -(width()/2 - 180), -(height()/2 - 60)));
	m_launchOverlayTitle->setText(tr("Launching %1…").arg(title));
	m_launchOverlaySubtitle->setText(tr("Loading core…"));
	m_launchOverlay->raise();
	m_launchOverlay->show();
}

void LauncherWindow::hideLaunchOverlay() {
	if (m_launchOverlay) {
		m_launchOverlay->hide();
	}
}

bool LauncherWindow::isLibraryScanRunning() const {
	return m_libraryScanThread != nullptr;
}

void LauncherWindow::showLibraryScanOverlay() {
	if (!m_scanOverlay) {
		auto* row = new QWidget(m_libraryPage);
		row->setObjectName("libraryScanStatus");
		auto* layout = new QHBoxLayout(row);
		layout->setContentsMargins(12, 6, 12, 6);
		m_scanOverlayProgress = new QLabel(row);
		m_scanOverlayProgress->setObjectName("mutedText");
		layout->addWidget(m_scanOverlayProgress, 1);
		m_scanOverlayBar = new QProgressBar(row);
		m_scanOverlayBar->setObjectName("scanOverlayBar");
		m_scanOverlayBar->setTextVisible(false);
		m_scanOverlayBar->setRange(0, 0);
		m_scanOverlayBar->setFixedWidth(150);
		layout->addWidget(m_scanOverlayBar);
		m_scanOverlayCancelButton = new QPushButton(tr("Cancel scan"), row);
		m_scanOverlayCancelButton->setObjectName("secondaryButton");
		connect(m_scanOverlayCancelButton, &QPushButton::clicked, this, &LauncherWindow::cancelLibraryScan);
		layout->addWidget(m_scanOverlayCancelButton);
		qobject_cast<QVBoxLayout*>(m_libraryPage->layout())->insertWidget(1, row);
		m_scanOverlay = row;
	}
	m_scanOverlayCancelButton->setEnabled(true);
	m_scanOverlay->show();
	updateLibraryScanOverlay(0, 0, {}, true);
}

void LauncherWindow::hideLibraryScanOverlay() {
	if (m_scanOverlay) {
		m_scanOverlay->hide();
	}
}

void LauncherWindow::updateLibraryScanOverlay(int current, int, const QString&, bool) {
	if (m_scanOverlayProgress) m_scanOverlayProgress->setText(tr("Scanning in background · %1 games found").arg(current));
}

void LauncherWindow::cancelLibraryScan() {
	if (m_libraryScanCancel) {
		m_libraryScanCancel->store(true);
	}
	if (m_scanOverlayProgress) {
		m_scanOverlayProgress->setText(tr("Cancelling…"));
	}
	if (m_scanOverlayCurrentFile) {
		m_scanOverlayCurrentFile->setText(tr("Finishing the current step…"));
	}
	if (m_scanOverlayCancelButton) {
		m_scanOverlayCancelButton->setEnabled(false);
	}
}

void LauncherWindow::finalizeLibraryScan(const QStringList& results, bool cancelled) {
	if (m_refreshButton) { m_refreshButton->setEnabled(true); m_refreshButton->setText(tr("Refresh")); }
	if (m_refreshLibrarySettingsButton) { m_refreshLibrarySettingsButton->setEnabled(true); m_refreshLibrarySettingsButton->setText(tr("Scan Library")); }
	if (m_chooseFolderSettingsButton) m_chooseFolderSettingsButton->setEnabled(true);
	hideLibraryScanOverlay();
	if (!cancelled) {
		m_allRoms = deduplicatedRomPaths(results);
		autoLinkInstalledRoms();
		for (const QString& linked : m_catalogInstalledLinks) {
			if (QFileInfo::exists(linked)) m_allRoms.append(linked);
		}
		m_allRoms = deduplicatedRomPaths(m_allRoms);
		if (m_search) m_searchQuery = m_search->text().trimmed();
		const QString dir = m_config ? m_config->getOption("romDirectory") : QString();
		const bool recursive = m_config ? m_config->getOption("libraryAutoScan").toInt() != 0 : true;
		const QStringList roms = m_allRoms;
		const QJsonObject archives = m_archiveScanCache;
		if (!dir.isEmpty()) QtConcurrent::run([dir, recursive, roms, archives]() { saveCachedLauncherRomList(dir, recursive, roms, archives); });
		applyLauncherFilter();
		updateContinuePlaying();
		queueMissingCoversForLibrary();
	} else updateLibraryHeader();
	m_libraryScanCancel.reset();
}

QString LauncherWindow::normalizeTitle(const QString& fileName) const {
	static const QRegularExpression tags("\\s*[\\(\\[][^\\]\\[\\)\\(]{1,40}[\\)\\]]");
	QString title = QFileInfo(fileName).completeBaseName();
	title.replace('_', ' ');
	title.remove(tags);
	return title.simplified(); // Preserve hack names and versions rather than relabeling them.
}

QString LauncherWindow::compactPathForDisplay(const QString& path) const {
	if (isCatalogEntry(path)) {
		return tr("Browse Games catalog");
	}
	if (path.isEmpty()) {
		return QString();
	}
	const QFileInfo info(path);
	const QString file = info.fileName();
	const QString folder = info.dir().dirName();
	if (folder.isEmpty()) {
		return QDir::toNativeSeparators(file);
	}
	return QDir::toNativeSeparators(folder + QStringLiteral("/") + file);
}

QPixmap LauncherWindow::makeCover(const QString& title, const QSize& size, const QString& status) const {
	QPixmap pm(size);
	pm.fill(Qt::transparent);

	QPainter p(&pm);
	if (size.width() < 120 || size.height() < 100) {
		p.fillRect(pm.rect(), QColor(25, 20, 34));
		p.setPen(QColor(190, 175, 210));
		p.drawText(pm.rect(), Qt::AlignCenter, tr("Empty"));
		return pm;
	}
	p.setRenderHint(QPainter::Antialiasing, true);

	const QRect outer = pm.rect().adjusted(2, 2, -2, -2);
	QPainterPath path;
	path.addRoundedRect(outer, 18, 18);

	const QColor accent = launcherAccentForTheme(m_launcherTheme);
	QLinearGradient bg(0, outer.top(), 0, outer.bottom());
	bg.setColorAt(0.0, QColor(28, 22, 40));
	bg.setColorAt(0.58, QColor(18, 15, 28));
	bg.setColorAt(1.0, QColor(11, 10, 18));
	p.fillPath(path, bg);

	QLinearGradient topGlow(outer.topLeft(), QPointF(outer.right(), outer.top() + 12));
	topGlow.setColorAt(0.0, QColor(accent.red(), accent.green(), accent.blue(), 150));
	topGlow.setColorAt(1.0, QColor(accent.red(), accent.green(), accent.blue(), 18));
	p.fillRect(QRect(outer.left(), outer.top(), outer.width(), 14), topGlow);

	p.setPen(QPen(QColor(255, 255, 255, 34), 1));
	p.drawRoundedRect(outer, 18, 18);

	QRect cartridgeRect = outer.adjusted(18, 18, -18, -64);
	QPainterPath cartridge;
	cartridge.addRoundedRect(QRectF(cartridgeRect), 14, 14);
	p.fillPath(cartridge, QColor(255, 255, 255, 7));
	p.setPen(QPen(QColor(255,255,255,20), 1));
	p.drawPath(cartridge);

	QRect badgeRect = QRect(cartridgeRect.left() + 18, cartridgeRect.top() + 16, cartridgeRect.width() - 36, qMax(56, cartridgeRect.height() / 3));
	QPainterPath badge;
	badge.addRoundedRect(QRectF(badgeRect), 12, 12);
	QLinearGradient badgeFill(badgeRect.topLeft(), badgeRect.bottomRight());
	badgeFill.setColorAt(0.0, QColor(accent.red(), accent.green(), accent.blue(), 120));
	badgeFill.setColorAt(1.0, QColor(accent.red(), accent.green(), accent.blue(), 38));
	p.fillPath(badge, badgeFill);

	QFont logoFont = p.font();
	logoFont.setBold(true);
	logoFont.setPointSize(qMax(16, size.width() / 8));
	p.setFont(logoFont);
	p.setPen(QColor(255, 255, 255, 218));
	p.drawText(badgeRect, Qt::AlignCenter, QStringLiteral("GBAx"));

	QRect lineRect(cartridgeRect.left() + 22, badgeRect.bottom() + 18, cartridgeRect.width() - 44, 2);
	p.fillRect(lineRect, QColor(255, 255, 255, 16));
	p.fillRect(QRect(lineRect.left(), lineRect.top(), qMax(34, lineRect.width() / 2), lineRect.height()), QColor(accent.red(), accent.green(), accent.blue(), 110));

	const QString display = tr("GBAx Library");
	QFont titleFont = p.font();
	titleFont.setBold(true);
	titleFont.setPointSize(qMax(9, size.width() / 15));
	p.setFont(titleFont);
	p.setPen(QColor(255, 255, 255, 170));
	p.drawText(QRect(outer.left() + 14, outer.bottom() - 42, outer.width() - 28, 18), Qt::AlignLeft | Qt::AlignVCenter, display);

	QFont subFont = p.font();
	subFont.setBold(false);
	subFont.setPointSize(qMax(8, size.width() / 18));
	p.setFont(subFont);
	p.setPen(QColor(255, 255, 255, 110));
	p.drawText(QRect(outer.left() + 14, outer.bottom() - 24, outer.width() - 28, 14), Qt::AlignLeft | Qt::AlignVCenter, status.isEmpty() ? tr("Queued") : status);

	return pm;
}

QString LauncherWindow::findCoverArtPath(const QString& romPath) const {
	if (romPath.isEmpty()) return {};
	const QString norm = isCatalogEntry(romPath) ? romPath : normalizedRomPath(romPath);
	auto cached = m_coverPathCache.constFind(norm);
	if (cached != m_coverPathCache.constEnd()) return cached.value();
	if (isCatalogEntry(romPath)) {
		const QString candidate = coverCachePathForCatalogTitle(catalogTitleFromId(romPath));
		QString art = existingManagedCover(candidate);
		const QString linked = installedRomPathForCatalogId(romPath);
		if (art.isEmpty() && !linked.isEmpty()) art = findCoverArtPath(linked);
		m_coverPathCache.insert(norm, art);
		return art;
	}
	const QFileInfo romInfo(romPath);
	const QString managed = existingManagedCover(cachedCoverPathForRom(norm));
	if (!managed.isEmpty()) { m_coverPathCache.insert(norm, managed); return managed; }
	const QString title = bestCatalogTitleForInstalledPath(norm);
	if (!title.isEmpty()) {
		const QString shared = existingManagedCover(coverCachePathForCatalogTitle(title));
		if (!shared.isEmpty()) { m_coverPathCache.insert(norm, shared); return shared; }
	}
	const QStringList stems = launcherCoverStems(romInfo, displayTitleForPath(norm));
	QStringList dirs{romInfo.absolutePath()};
	const QString root = m_config ? m_config->getOption("romDirectory") : QString();
	for (const QString& parent : {romInfo.absolutePath(), root}) {
		if (parent.isEmpty()) continue;
		for (const QString& folder : {QStringLiteral("Artwork"), QStringLiteral("artwork"), QStringLiteral("covers"), QStringLiteral("boxart")}) dirs.append(QDir(parent).filePath(folder));
	}
	dirs.append(coverCacheDirPath());
	dirs.removeDuplicates();
	for (const QString& folder : dirs) {
		if (!QDir(folder).exists()) continue;
		for (const QString& stem : stems) {
			for (const QString& ext : {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("webp")}) {
				const QString candidate = QDir(folder).filePath(stem + "." + ext);
				if (QFileInfo::exists(candidate)) { m_coverPathCache.insert(norm, candidate); return candidate; }
			}
		}
	}
	m_coverPathCache.insert(norm, QString());
	return {};
}



QString LauncherWindow::savestatePathForRomSlot(const QString& romPath, int slot) const {
	if (romPath.isEmpty() || slot <= 0) {
		return QString();
	}
	const QFileInfo romInfo(romPath);
	QStringList dirCandidates;
	dirCandidates << romInfo.absolutePath();
	if (m_config) {
		const QString configured = m_config->getOption("savestatePath");
		if (!configured.isEmpty()) {
			QFileInfo configInfo(configured);
			if (configInfo.isRelative()) {
				dirCandidates << QDir(ConfigController::configDir()).filePath(configured);
				dirCandidates << QDir(GBAApp::dataDir()).filePath(configured); // legacy fallback
			} else {
				dirCandidates << configured;
			}
		}
	}
	dirCandidates.removeDuplicates();
	const QString fileName = romInfo.completeBaseName() + QStringLiteral(".ss%1").arg(slot);
	for (const QString& dirPath : dirCandidates) {
		const QString candidate = QDir(dirPath).filePath(fileName);
		if (QFileInfo::exists(candidate)) {
			return candidate;
		}
	}
	// Fall back to the preferred configured directory, otherwise ROM directory.
	return QDir(dirCandidates.isEmpty() ? romInfo.absolutePath() : dirCandidates.first()).filePath(fileName);
}

QPixmap LauncherWindow::saveStatePreviewForRomSlot(const QString& romPath, int slot, const QSize& size, QString* labelOut) const {
	const QString statePath = savestatePathForRomSlot(romPath, slot);
	if (labelOut) {
		*labelOut = tr("Slot %1").arg(slot);
	}
	QFileInfo stateInfo(statePath);
	if (statePath.isEmpty() || !stateInfo.exists()) {
		return makeCover(tr("S%1").arg(slot), size);
	}
	QFile file(statePath);
	if (!file.open(QIODevice::ReadOnly)) {
		if (labelOut) {
			*labelOut = tr("Slot %1\n%2").arg(slot).arg(QLocale().toString(stateInfo.lastModified().time(), QLocale::ShortFormat));
		}
		return makeCover(tr("S%1").arg(slot), size);
	}
	QByteArray data = file.readAll();
	file.close();
	VFileDevice memoryFile(data);
	mStateExtdata extdata;
	mStateExtdataInit(&extdata);
	if (!mStateExtdataDeserialize(&extdata, memoryFile)) {
		mStateExtdataDeinit(&extdata);
		if (labelOut) {
			*labelOut = tr("Slot %1\n%2").arg(slot).arg(QLocale().toString(stateInfo.lastModified().time(), QLocale::ShortFormat));
		}
		return makeCover(tr("S%1").arg(slot), size);
	}
	QImage stateImage;
	QDateTime creation;
	mStateExtdataItem item;
	QSize imgSize(240, 160);
	if (mStateExtdataGet(&extdata, EXTDATA_SCREENSHOT, &item)) {
		mStateExtdataItem dims;
		if (mStateExtdataGet(&extdata, EXTDATA_SCREENSHOT_DIMENSIONS, &dims) && dims.size == sizeof(uint16_t[2])) {
			imgSize.setWidth(static_cast<uint16_t*>(dims.data)[0]);
			imgSize.setHeight(static_cast<uint16_t*>(dims.data)[1]);
		}
		if (item.size >= static_cast<int32_t>(imgSize.width() * imgSize.height() * 4)) {
			stateImage = QImage((uchar*) item.data, imgSize.width(), imgSize.height(), QImage::Format_ARGB32).rgbSwapped().copy();
		}
	}
	if (mStateExtdataGet(&extdata, EXTDATA_META_TIME, &item) && item.size == sizeof(uint64_t)) {
		uint64_t creationUsec;
		LOAD_64LE(creationUsec, 0, item.data);
		creation = QDateTime::fromMSecsSinceEpoch(creationUsec / 1000LL);
	}
	if (labelOut) {
		const QDateTime stamp = creation.isValid() ? creation : stateInfo.lastModified();
		if (stamp.isValid()) {
			*labelOut = tr("Slot %1\n%2").arg(slot).arg(QLocale().toString(stamp, QStringLiteral("d MMM yyyy\nHH:mm")));
		} else {
			*labelOut = tr("Slot %1").arg(slot);
		}
	}
	QPixmap result = stateImage.isNull() ? makeCover(tr("S%1").arg(slot), size) : QPixmap::fromImage(stateImage.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	mStateExtdataDeinit(&extdata);
	return result;
}

void LauncherWindow::updateSaveStatePreviews() {
	const QString selectedPathSnapshot = m_selectedPath;
	const QString realPath = isCatalogEntry(selectedPathSnapshot) ? installedRomPathForCatalogId(selectedPathSnapshot) : selectedPathSnapshot;
	const bool hasRealRom = !realPath.isEmpty();
	int existingSlots = 0;
	const qulonglong requestId = property("_gbaxSavePreviewRequestId").toULongLong() + 1ULL;
	setProperty("_gbaxSavePreviewRequestId", QVariant::fromValue<qulonglong>(requestId));

	for (int i = 0; i < m_saveStateButtons.size(); ++i) {
		auto* button = m_saveStateButtons.at(i);
		if (!button) {
			continue;
		}

		const int slot = i + 1;
		const QSize iconSize = button->iconSize();
		const QString placeholderLabel = tr("Slot %1").arg(slot);
		button->setEnabled(false);
		button->setIcon(QIcon());
		button->setMinimumHeight(38);
		button->setText(placeholderLabel);

		const QString statePath = hasRealRom ? savestatePathForRomSlot(realPath, slot) : QString();
		button->setToolTip(hasRealRom ? statePath : tr("Save states appear for installed games"));

		if (!hasRealRom) {
			continue;
		}

		QFileInfo stateInfo(statePath);
		if (!stateInfo.exists()) {
			button->setText(tr("Slot %1 · Empty").arg(slot));
			continue;
		}
		button->setEnabled(true);
		button->setMinimumHeight(78);
		++existingSlots;

		const QString cacheKey = saveStatePreviewCacheKey(statePath, iconSize);
		{
			QMutexLocker locker(&s_saveStatePreviewCacheMutex);
			auto it = s_saveStatePreviewCache.constFind(cacheKey);
			if (it != s_saveStatePreviewCache.constEnd()) {
				const SaveStatePreviewResult cached = it.value();
				if (!cached.image.isNull()) {
					button->setIcon(QIcon(QPixmap::fromImage(cached.image)));
				}
				button->setText((cached.label.isEmpty() ? placeholderLabel : cached.label));
				continue;
			}
		}

		auto* watcher = new QFutureWatcher<SaveStatePreviewResult>(this);
		const QPointer<QPushButton> buttonPtr(button);
		connect(watcher, &QFutureWatcher<SaveStatePreviewResult>::finished, this,
			[this, watcher, buttonPtr, requestId, selectedPathSnapshot, slot, iconSize]() {
			const SaveStatePreviewResult result = watcher->result();
			watcher->deleteLater();
			if (!buttonPtr || property("_gbaxSavePreviewRequestId").toULongLong() != requestId ||
			    m_selectedPath != selectedPathSnapshot) {
				return;
			}
			buttonPtr->setIcon(result.image.isNull() ? QIcon() : QIcon(QPixmap::fromImage(result.image)));
			buttonPtr->setText((result.label.isEmpty() ? tr("Slot %1").arg(slot) : result.label));
		});
		watcher->setFuture(QtConcurrent::run([statePath, slot, iconSize, cacheKey]() {
			const SaveStatePreviewResult result = loadSaveStatePreviewResult(statePath, slot, iconSize);
			QMutexLocker locker(&s_saveStatePreviewCacheMutex);
			if (s_saveStatePreviewCache.size() >= 128) s_saveStatePreviewCache.clear();
			s_saveStatePreviewCache.insert(cacheKey, result);
			return result;
		}));
	}
	const bool hasSaves = existingSlots > 0;
	if (m_saveSlotsSelection != selectedPathSnapshot || hasSaves != m_saveSlotsHadSaves) m_saveSlotsToggle->setChecked(hasSaves);
	m_saveSlotsSelection = selectedPathSnapshot;
	m_saveSlotsHadSaves = hasSaves;
	m_saveSlotsToggle->setText(tr("Quick Save Slots (%1/%2)").arg(existingSlots).arg(m_saveStateButtons.size()));
	m_saveSlotsSummary->setText(!hasRealRom ? tr("Link or select an installed game to use save slots.") : (hasSaves ? tr("Select a saved slot to resume your game.") : tr("No saves yet. Create a quick save while playing.")));
	updateSaveSlotExpansion();

}

void LauncherWindow::updateSaveSlotExpansion() {
	if (!m_saveSlotsToggle || !m_saveSlotsContent) return;
	const bool expanded = m_saveSlotsToggle->isChecked();
	if (!expanded && m_saveSlotsContent->isAncestorOf(QApplication::focusWidget())) m_saveSlotsToggle->setFocus();
	m_saveSlotsContent->setVisible(expanded);
	QPixmap chevron(16, 16);
	chevron.fill(Qt::transparent);
	QPainter painter(&chevron);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(QPen(QColor(224, 211, 240), 2));
	if (expanded) { painter.drawLine(3, 5, 8, 10); painter.drawLine(8, 10, 13, 5); }
	else { painter.drawLine(5, 3, 10, 8); painter.drawLine(10, 8, 5, 13); }
	painter.end();
	m_saveSlotsToggle->setIcon(QIcon(chevron));
	m_saveSlotsToggle->setToolTip(expanded ? tr("Collapse save slots") : tr("Expand save slots"));
}

QScrollBar* LauncherWindow::analogVerticalScrollBar() const {
	QWidget* focus = QApplication::focusWidget();
	if (m_mainStack && m_mainStack->currentWidget() == m_settingsPage && m_settingsScroll) {
		return m_settingsScroll->verticalScrollBar();
	}
	if (focus && (focus == m_playButton || focus == m_favButton || focus == m_saveSlotsToggle || m_saveStateButtons.contains(qobject_cast<QPushButton*>(focus)))) {
		if (m_saveStateScroll) {
			return m_saveStateScroll->verticalScrollBar();
		}
	}
	if (m_grid) {
		return m_grid->verticalScrollBar();
	}
	return nullptr;
}

QScrollBar* LauncherWindow::analogHorizontalScrollBar() const {
	QWidget* focus = QApplication::focusWidget();
	if (focus && (focus == m_playButton || focus == m_favButton || focus == m_saveSlotsToggle || m_saveStateButtons.contains(qobject_cast<QPushButton*>(focus)))) {
		if (m_saveStateScroll) {
			return m_saveStateScroll->horizontalScrollBar();
		}
	}
	if (m_grid) {
		return m_grid->horizontalScrollBar();
	}
	return nullptr;
}

void LauncherWindow::setAnalogScrollDirection(int axis, int direction) {
	if (axis == 0) {
		m_analogScrollX = direction;
	} else if (axis == 1) {
		m_analogScrollY = direction;
	}
	const bool active = (m_analogScrollX != 0 || m_analogScrollY != 0) && (analogVerticalScrollBar() || analogHorizontalScrollBar());
	if (m_analogScrollTimer) {
		if (active) {
			if (!m_analogScrollTimer->isActive()) {
				m_analogScrollTimer->start();
			}
		} else {
			m_analogScrollTimer->stop();
		}
	}
}

void LauncherWindow::applyAnalogScrollStep() {
	QScrollBar* vBar = analogVerticalScrollBar();
	QScrollBar* hBar = analogHorizontalScrollBar();
	if ((!vBar && !hBar) || (m_analogScrollX == 0 && m_analogScrollY == 0)) {
		if (m_analogScrollTimer) {
			m_analogScrollTimer->stop();
		}
		return;
	}
	constexpr int verticalStep = 18;
	constexpr int horizontalStep = 14;
	if (vBar && m_analogScrollY != 0) {
		vBar->setValue(std::clamp(vBar->value() + (m_analogScrollY * verticalStep), vBar->minimum(), vBar->maximum()));
	}
	if (hBar && m_analogScrollX != 0) {
		hBar->setValue(std::clamp(hBar->value() + (m_analogScrollX * horizontalStep), hBar->minimum(), hBar->maximum()));
	}
}

void LauncherWindow::toggleLauncherFullscreen() {
	if (isMaximized()) {
		showNormal();
	} else {
		showMaximized();
	}
}

void LauncherWindow::updateLauncherComboState(bool selectDown, bool startDown) {
	const bool wasComboActive = m_controllerSelectDown && m_controllerStartDown;
	m_controllerSelectDown = selectDown;
	m_controllerStartDown = startDown;
	const bool comboActive = m_controllerSelectDown && m_controllerStartDown;
	if (comboActive && !wasComboActive) {
		m_controllerComboHoldTriggered = false;
		if (m_controllerComboHoldTimer) {
			m_controllerComboHoldTimer->start();
		}
	}
	if (!comboActive && wasComboActive) {
		if (m_controllerComboHoldTimer && m_controllerComboHoldTimer->isActive()) {
			m_controllerComboHoldTimer->stop();
		}
		m_controllerComboHoldTriggered = false;
	}
	if (!comboActive && m_controllerComboHoldTimer && m_controllerComboHoldTimer->isActive() && !(m_controllerSelectDown || m_controllerStartDown)) {
		m_controllerComboHoldTimer->stop();
	}
}

void LauncherWindow::toggleToRunningGameWindow() {
	if (!m_gameProcess || m_gameProcess->state() == QProcess::NotRunning) return;
#ifdef Q_OS_WIN
	AllowSetForegroundWindow(static_cast<DWORD>(m_gameProcess->processId()));
#endif
	sendLauncherCommand(this, launcherCommandServerName(), "show_window\n");
}

void LauncherWindow::requestCloseRunningGame() {
	if (!m_gameProcess || m_gameProcess->state() == QProcess::NotRunning) return;
	sendLauncherCommand(this, launcherCommandServerName(), "close_game\n");
}

void LauncherWindow::moveGridSelection(int deltaRow, int deltaCol) {
	if (!m_grid || !m_grid->count()) return;
	int index = m_grid->currentRow();
	if (index < 0) index = 0;
	const int columns = qMax(1, m_grid->viewport()->width() / qMax(1, m_grid->gridSize().width()));
	index += deltaCol;
	index += deltaRow * columns;
	index = qBound(0, index, m_grid->count() - 1);
	m_grid->setCurrentRow(index);
	m_grid->scrollToItem(m_grid->currentItem(), QAbstractItemView::PositionAtCenter);
	m_grid->setFocus();
}

void LauncherWindow::focusSidebarButton(int index) {
	QList<QPushButton*> sidebarButtons{m_navLibrary, m_navFavorites, m_navRecent, m_navBrowse, m_navSettings,
		m_footerBrandButton, m_aboutFooterButton, m_websiteFooterButton};
	if (index < 0 || index >= sidebarButtons.size() || !sidebarButtons[index]) return;
	sidebarButtons[index]->setFocus();
}

void LauncherWindow::restoreLauncherWindowState() {
	if (m_restoreFullscreen) {
		showFullScreen();
	} else if (m_restoreMaximized) {
		showMaximized();
	} else {
		showNormal();
		if (m_restoreGeometry.isValid()) setGeometry(m_restoreGeometry);
	}
	raise();
	activateWindow();
}

void LauncherWindow::chooseRomsFolder() {
	if (isLibraryScanRunning()) return;
	const QString start = m_config ? m_config->getOption("romDirectory") : QString();
	const QString dir = QFileDialog::getExistingDirectory(this, tr("Select ROMs folder"), start);
	if (dir.isEmpty()) {
		return;
	}
	cancelArtworkDownloads();
	m_autoArtworkPaused = false;
	m_config->setOption("romDirectory", dir);
	refreshLibrary(true);
}

void LauncherWindow::refreshLibrary() {
	refreshLibrary(false);
}

void LauncherWindow::refreshLibrary(bool forceRescan) {
	if (isLibraryScanRunning()) return;
	++m_artworkGeneration;
	m_coverPathCache.clear();
	m_coverPixmapCache.clear();
	m_backdropPixmapCache.clear();
	m_installedCatalogCache.clear();
	m_titleCache.clear();
	const QString dir = m_config ? m_config->getOption("romDirectory") : QString();
	const bool recursive = m_config ? m_config->getOption("libraryAutoScan").toInt() != 0 : true;
	const QJsonObject entry = loadLauncherLibraryCacheRoot().value("caches").toObject().value(cacheRootKey(dir, recursive)).toObject();
	m_archiveScanCache = entry.value("archives").toObject();
	if (m_startupRefreshPending || forceRescan || m_allRoms.isEmpty()) {
		QStringList cached;
		for (const QJsonValue& value : entry.value("roms").toArray()) {
			const QString path = normalizedRomPath(value.toString());
			if (!path.isEmpty()) cached.append(path);
		}
		m_allRoms = deduplicatedRomPaths(cached);
		applyLauncherFilter();
	}
	m_startupRefreshPending = false;
	completeStartupVisualReady();
	startLibraryScan(); // Validate cached paths and discover changes in the worker.
	updateContinuePlaying();
}

void LauncherWindow::startLibraryScan() {
	const QString dir = m_config ? m_config->getOption("romDirectory") : QString();
	const bool recursive = m_config ? m_config->getOption("libraryAutoScan").toInt() != 0 : true;

	if (isLibraryScanRunning()) {
		return;
	}


	if (m_refreshButton) {
		m_refreshButton->setEnabled(false);
		m_refreshButton->setText(tr("Scanning…"));
	}
	if (m_refreshLibrarySettingsButton) {
		m_refreshLibrarySettingsButton->setEnabled(false);
		m_refreshLibrarySettingsButton->setText(tr("Scanning…"));
	}
	if (m_chooseFolderSettingsButton) {
		m_chooseFolderSettingsButton->setEnabled(false);
	}
	showLibraryScanOverlay();

	updateLibraryHeader();

	if (dir.isEmpty() || !QDir(dir).exists()) {
		m_allRoms.clear();
		applyLauncherFilter();
		finalizeLibraryScan(QStringList(), false);
		return;
	}

	m_libraryScanCancel = std::make_shared<std::atomic_bool>(false);
	const auto cancelFlag = m_libraryScanCancel;
	const auto results = std::make_shared<QStringList>();
	const auto archives = std::make_shared<QJsonObject>();
	const QJsonObject cachedArchives = m_archiveScanCache;
	const QPointer<LauncherWindow> self(this);
	auto* thread = QThread::create([this, self, dir, recursive, cancelFlag, results, archives, cachedArchives]() {
		QSet<QString> uniq;
		QMetaObject::invokeMethod(this, [self]() {
			if (self) self->updateLibraryScanOverlay(0, 0, QString(), true);
		}, Qt::QueuedConnection);

		QDirIterator it(dir, {"*.gba", "*.gb", "*.gbc", "*.zip", "*.7z"}, QDir::Files | QDir::NoSymLinks,
			recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
		while (!cancelFlag->load() && it.hasNext()) {
			const QString path = normalizedRomPath(it.next());
			const QFileInfo info(path);
			const QString ext = info.suffix().toLower();
			if (ext == "zip" || ext == "7z") {
				const QString signature = QString::number(info.size()) + "|" + QString::number(info.lastModified().toMSecsSinceEpoch());
				QJsonObject check = cachedArchives.value(path).toObject();
				if (check.value("signature").toString() != signature) {
					check = QJsonObject{{"signature", signature}, {"accepted", archiveContainsRom(path)}};
				}
				archives->insert(path, check);
				if (!check.value("accepted").toBool()) continue;
			}
#ifdef Q_OS_WIN
			const QString key = path.toLower();
#else
			const QString& key = path;
#endif
			if (!uniq.contains(key)) {
				uniq.insert(key);
				results->append(path);
				// Keep large scans from filling the GUI event queue.
				if (results->size() % 128 == 0) {
					const int count = results->size();
					QMetaObject::invokeMethod(this, [self, count, path]() {
						if (self) self->updateLibraryScanOverlay(count, 0, path, true);
					}, Qt::QueuedConnection);
				}
			}
		}
	});
	m_libraryScanThread = thread;
	thread->setParent(this);
	// Results and cleanup are delivered only after the worker has stopped.
	connect(thread, &QThread::finished, this, [this, thread, results, cancelFlag, archives]() {
		if (m_libraryScanThread == thread) {
			m_libraryScanThread = nullptr;
			if (!cancelFlag->load()) m_archiveScanCache = *archives;
			finalizeLibraryScan(*results, cancelFlag->load());
		}
	});
	connect(thread, &QThread::finished, thread, &QObject::deleteLater);
	thread->start();
}

void LauncherWindow::onLibraryScanFinished() {
	// Legacy QtConcurrent scan path retained for compatibility; scans now finalize through finalizeLibraryScan().
}

void LauncherWindow::rebuildLibrary(const QString& rootDir, bool recursive) {
	m_allRoms.clear();
	m_filteredRoms.clear();
	m_grid->clear();

	if (rootDir.isEmpty() || !QDir(rootDir).exists()) {
		m_detailsTitle->setProperty("fullText", tr("Select a game"));
		updateDetailsTitleElide();
		populateBadgeRow(m_detailsMetaLayout, QStringList{tr("Library")});
		m_detailsStats->clear();
		m_detailsStats->setVisible(false);
		m_detailsPath->clear();
		m_detailsCover->setPixmap(makeCover(tr("No ROMs folder set"), m_detailsCover->size()));
		m_detailsThumb->setPixmap(makeCover(tr("No ROMs folder set"), m_detailsThumb->size()));
		updateDynamicBackdrops();
		m_playButton->setEnabled(false);
		return;
	}

	QDirIterator it(rootDir, QDir::Files | QDir::NoDotAndDotDot,
		recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
	while (it.hasNext()) {
		const QString path = it.next();
		if (isAcceptedRomPath(path)) {
			m_allRoms.append(normalizedRomPath(path));
		}
	}

	m_allRoms = deduplicatedRomPaths(m_allRoms);

	// Alphabetical by normalized title
	std::sort(m_allRoms.begin(), m_allRoms.end(), [this](const QString& a, const QString& b) {
		return displayTitleForPath(a).compare(displayTitleForPath(b), Qt::CaseInsensitive) < 0;
	});
	m_filteredRoms = m_allRoms;

	for (const QString& p : m_filteredRoms) {
		addItemForPath(p);
	}

	if (!m_grid->count()) {
		m_detailsCover->setPixmap(makeCover(tr("No ROMs found"), m_detailsCover->size()));
		m_detailsThumb->setPixmap(makeCover(tr("No ROMs found"), m_detailsThumb->size()));
		updateDynamicBackdrops();
		m_detailsTitle->setProperty("fullText", tr("No games found"));
	updateDetailsTitleElide();
		m_detailsPath->setText(tr("Add .gba/.gb/.gbc files (or archives) to: %1").arg(rootDir));
	}
}

void LauncherWindow::filterChanged(const QString& text) {
	m_searchQuery = text.trimmed();
	applyLauncherFilter();
}

void LauncherWindow::showAllGames() {
	m_favoritesOnly = false;
	m_recentOnly = false;
	m_browseOnly = false;
	if (m_mainStack && m_libraryPage) {
		m_mainStack->setCurrentWidget(m_libraryPage);
	}
	updateNavChecked();
	applyLauncherFilter();
	updateBrowseCoverPrompt();
	updateCoverDownloadStatus();
}

void LauncherWindow::showFavorites() {
	m_favoritesOnly = true;
	m_recentOnly = false;
	m_browseOnly = false;
	if (m_mainStack && m_libraryPage) {
		m_mainStack->setCurrentWidget(m_libraryPage);
	}
	updateNavChecked();
	applyLauncherFilter();
	updateBrowseCoverPrompt();
	updateCoverDownloadStatus();
}

void LauncherWindow::showRecentlyPlayed() {
	m_favoritesOnly = false;
	m_recentOnly = true;
	m_browseOnly = false;
	if (m_mainStack && m_libraryPage) {
		m_mainStack->setCurrentWidget(m_libraryPage);
	}
	updateNavChecked();
	applyLauncherFilter();
	updateBrowseCoverPrompt();
	updateCoverDownloadStatus();
}

void LauncherWindow::showBrowseGames() {
	m_favoritesOnly = false;
	m_recentOnly = false;
	m_browseOnly = true;
	if (m_mainStack && m_libraryPage) {
		m_mainStack->setCurrentWidget(m_libraryPage);
	}
	updateNavChecked();
	applyLauncherFilter();
	queueMissingCoversForBrowseCatalog();
	updateBrowseCoverPrompt();
	updateCoverDownloadStatus();
}

void LauncherWindow::showSettings() {
	m_favoritesOnly = false;
	m_recentOnly = false;
	m_browseOnly = false;
	if (m_mainStack && m_settingsPage) {
		m_mainStack->setCurrentWidget(m_settingsPage);
	}
	const QString rootDir = m_config ? m_config->getOption("romDirectory") : QString();
	if (m_settingsRomPath) {
		m_settingsRomPath->setText(rootDir.isEmpty() ? tr("No ROM folder selected yet.") : QDir::toNativeSeparators(rootDir));
	}
	if (m_recursiveScanCheck) {
		m_recursiveScanCheck->setChecked(m_config ? m_config->getOption("libraryAutoScan").toInt() != 0 : true);
	}
	if (m_skipSplashCheck) {
		m_skipSplashCheck->setChecked(m_config ? m_config->getQtOption("showLauncherSplash", "gbax").toBool() : true);
	}
	if (m_launchFullscreenCheck) {
		m_launchFullscreenCheck->setChecked(m_config ? m_config->getOption("fullscreen").toInt() != 0 : false);
	}
	if (m_autoDownloadCoversCheck) {
		m_autoDownloadCoversCheck->setChecked(m_autoDownloadCovers);
	}
	updateNavChecked();
	updateBrowseCoverPrompt();
	updateCoverDownloadStatus();
}

void LauncherWindow::updateNavChecked() {
	const bool settingsOpen = m_mainStack && m_settingsPage && m_mainStack->currentWidget() == m_settingsPage;
	if (m_navSettings) {
		m_navSettings->setChecked(settingsOpen);
	}
	if (m_navFavorites) {
		m_navFavorites->setChecked(!settingsOpen && m_favoritesOnly);
	}
	if (m_navRecent) {
		m_navRecent->setChecked(!settingsOpen && m_recentOnly);
	}
	if (m_navBrowse) {
		m_navBrowse->setChecked(!settingsOpen && m_browseOnly);
	}
	if (m_navLibrary) {
		m_navLibrary->setChecked(!settingsOpen && !m_favoritesOnly && !m_recentOnly && !m_browseOnly);
	}
	updateSidebarIcons();
}

void LauncherWindow::updateLibraryHeader() {
	if (m_continueCard) m_continueCard->setVisible(!m_browseOnly && !m_favoritesOnly && !m_recentOnly && !m_continueLaunchPath.isEmpty());
	if (auto* title = findChild<QLabel*>("librarySectionTitle")) title->setText(m_browseOnly ? tr("GBA Catalog") : (m_favoritesOnly ? tr("Favorite Games") : (m_recentOnly ? tr("Recent Games") : tr("All Games"))));
	const int count = m_filteredRoms.size();
	const bool hasSearch = !m_searchQuery.trimmed().isEmpty();
	const QString pageTitle = m_browseOnly ? tr("Browse Games") : (m_favoritesOnly ? tr("Favorites") : (m_recentOnly ? tr("Recently Played") : tr("Installed")));
	if (m_libraryTitle) {
		m_libraryTitle->setText(pageTitle);
	}
	if (m_pageEyebrow) {
		m_pageEyebrow->setText(hasSearch ? tr("Filtered Results") : (m_browseOnly ? tr("Built-in GBA Catalog") : (m_recentOnly ? tr("Play History") : tr("Installed Library"))));
	}
	if (m_librarySummary) {
		QString summary;
		if (isLibraryScanRunning()) {
			summary = tr("Scanning your ROM folder…");
		} else if (hasSearch) {
			summary = tr("%1 match%2 for \"%3\"").arg(count).arg(count == 1 ? QString() : QStringLiteral("es")).arg(m_searchQuery.trimmed());
		} else if (m_favoritesOnly) {
			summary = tr("%1 favorite game%2 ready to launch").arg(count).arg(count == 1 ? QString() : QStringLiteral("s"));
		} else if (m_recentOnly) {
			summary = tr("%1 recently played game%2").arg(count).arg(count == 1 ? QString() : QStringLiteral("s"));
		} else if (m_browseOnly) {
			int installed = 0;

			for (const QString& t : m_filteredRoms) {
				if (!installedRomPathForCatalogId(t).isEmpty()) ++installed;

			}
			summary = tr("%1 catalog games · %2 linked ROMs · %3").arg(count).arg(installed).arg(shouldAutoDownloadCovers() ? tr("artwork downloads in background") : tr("automatic artwork paused"));
		} else {
			summary = tr("%1 game%2 installed").arg(count).arg(count == 1 ? QString() : QStringLiteral("s"));
		}
		m_librarySummary->setText(summary);
	}
}

void LauncherWindow::updateEmptyState() {
	if (!m_grid || !m_detailsCover || !m_detailsThumb || !m_detailsTitle || !m_detailsPath || !m_detailsMetaLayout || !m_detailsStats) {
		return;
	}
	if (!m_filteredRoms.isEmpty()) {
		return;
	}
	m_selectedPath.clear();
	m_playButton->setEnabled(false);
	m_playButton->setFixedHeight(52);
	updateFavoriteUi();
	updateSaveStatePreviews();
	if (m_browseOnly) {
		m_detailsTitle->setProperty("fullText", tr("Browse the GBA catalog"));
		updateDetailsTitleElide();
		populateBadgeRow(m_detailsMetaLayout, QStringList{tr("Browse")});
		m_detailsStats->clear();
		m_detailsStats->setVisible(false);
		m_detailsPath->setText(tr("Use Browse Games to build your Installed collection."));
		m_detailsCover->setPixmap(makeCover(tr("Browse"), m_detailsCover->size()));
		m_detailsThumb->setPixmap(makeCover(tr("Browse"), m_detailsThumb->size()));
		updateDynamicBackdrops();
		return;
	}
	if (m_favoritesOnly) {
		m_detailsTitle->setProperty("fullText", tr("No favorites yet"));
	updateDetailsTitleElide();
		populateBadgeRow(m_detailsMetaLayout, QStringList{tr("Favorites")});
		m_detailsStats->clear();
		m_detailsStats->setVisible(false);
		m_detailsPath->setText(tr("Star games from the library to pin them here."));
		m_detailsCover->setPixmap(makeCover(tr("Favorites"), m_detailsCover->size()));
		m_detailsThumb->setPixmap(makeCover(tr("Favorites"), m_detailsThumb->size()));
		updateDynamicBackdrops();
		return;
	}
	if (m_recentOnly) {
		m_detailsTitle->setProperty("fullText", tr("No recent games yet"));
	updateDetailsTitleElide();
		populateBadgeRow(m_detailsMetaLayout, QStringList{tr("Recent")});
		m_detailsStats->clear();
		m_detailsStats->setVisible(false);
		m_detailsPath->setText(tr("Start a game from the library to build your recent list."));
		m_detailsCover->setPixmap(makeCover(tr("Recent"), m_detailsCover->size()));
		m_detailsThumb->setPixmap(makeCover(tr("Recent"), m_detailsThumb->size()));
		updateDynamicBackdrops();
		return;
	}
	if (!m_searchQuery.trimmed().isEmpty()) {
		m_detailsTitle->setProperty("fullText", tr("No matching games"));
	updateDetailsTitleElide();
		populateBadgeRow(m_detailsMetaLayout, QStringList{tr("Search")});
		m_detailsStats->clear();
		m_detailsStats->setVisible(false);
		m_detailsPath->setText(tr("Nothing matched \"%1\".").arg(m_searchQuery.trimmed()));
		m_detailsCover->setPixmap(makeCover(tr("No Results"), m_detailsCover->size()));
		m_detailsThumb->setPixmap(makeCover(tr("No Results"), m_detailsThumb->size()));
		updateDynamicBackdrops();
		return;
	}
	const QString rootDir = m_config ? m_config->getOption("romDirectory") : QString();
	if (rootDir.isEmpty() || !QDir(rootDir).exists()) {
		m_detailsTitle->setProperty("fullText", tr("Choose a ROM folder"));
	updateDetailsTitleElide();
		populateBadgeRow(m_detailsMetaLayout, QStringList{tr("Library")});
		m_detailsPath->setText(tr("Select a folder containing .gba, .gb, .gbc, .zip or .7z files."));
		m_detailsCover->setPixmap(makeCover(tr("No Folder"), m_detailsCover->size()));
		m_detailsThumb->setPixmap(makeCover(tr("No Folder"), m_detailsThumb->size()));
		updateDynamicBackdrops();
		return;
	}
	m_detailsTitle->setProperty("fullText", tr("No games found"));
	updateDetailsTitleElide();
	populateBadgeRow(m_detailsMetaLayout, QStringList{tr("Empty")});
	m_detailsPath->setText(tr("Add ROMs to: %1").arg(rootDir));
	m_detailsCover->setPixmap(makeCover(tr("Empty Library"), m_detailsCover->size()));
	m_detailsThumb->setPixmap(makeCover(tr("Empty Library"), m_detailsThumb->size()));
	updateDynamicBackdrops();
}

void LauncherWindow::applyLauncherFilter() {
	if (!m_grid) {
		return;
	}

	if (m_libraryPopulateTimer) m_libraryPopulateTimer->stop();
	m_thumbnailPreloadIndex = 0;
	m_gridContainsCatalog = m_browseOnly;
	const QString previousSelection = m_selectedPath;
	const QSignalBlocker blockGrid(m_grid);
	const QString q = m_searchQuery.trimmed();
	m_grid->clear();
	m_filteredRoms.clear();

	if (m_browseOnly) {
		const QString q = m_searchQuery.trimmed();
		const QStringList& sortedCatalogTitles = m_catalogTitles;
		for (const QString& title : sortedCatalogTitles) {
			const QString display = m_catalogDisplayTitles.value(title, title);
			const QVariantMap meta = m_catalogMetadata.value(title);
			QString haystack = title + QStringLiteral("\n") + display;
			haystack += QStringLiteral("\n") + cleanedMetadataValue(meta, QStringLiteral("developer"));
			haystack += QStringLiteral("\n") + cleanedMetadataValue(meta, QStringLiteral("publisher"));
			haystack += QStringLiteral("\n") + cleanedMetadataValue(meta, QStringLiteral("release_na"));
			haystack += QStringLiteral("\n") + cleanedMetadataValue(meta, QStringLiteral("multiplayer"));
			if (!q.isEmpty()) {
				bool matches = true;
				for (const QString& term : q.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts)) {
					if (!haystack.contains(term, Qt::CaseInsensitive)) { matches = false; break; }
				}
				if (!matches) continue;
			}
			m_filteredRoms.append(catalogIdForTitle(title));
		}
		m_browseLoadedCount = 0;
		appendMoreBrowseItems(kBrowseChunkSize);
		updateLibraryHeader();
		updateEmptyState();
		if (!m_grid->currentItem() && m_grid->count() > 0) m_grid->setCurrentRow(0);
		onSelectionChanged();
		updateBrowseCoverPrompt();
		updateCoverDownloadStatus();
		scheduleVisibleCoverRefresh();
		return;
	}

	QStringList recentPaths;
	QSet<QString> recentSet;
	if (m_recentOnly && m_config) {
		const QStringList mru = m_config->getMRU(ConfigController::MRU::ROM);
		for (const QString& entry : mru) {
			const QString norm = QDir::toNativeSeparators(QFileInfo(entry).absoluteFilePath());
			if (QFileInfo::exists(norm) && !recentSet.contains(norm)) {
				recentSet.insert(norm);
				recentPaths.append(norm);
			}
		}
	}

	for (const QString& p : m_allRoms) {
		const QString normInstalled = QDir::toNativeSeparators(QFileInfo(p).absoluteFilePath());
		if (m_hiddenInstalledPaths.contains(normInstalled)) {
			continue;
		}
		if (m_favoritesOnly && !isFavorite(p)) {
			continue;
		}
		if (m_recentOnly && !recentSet.contains(normInstalled)) {
			continue;
		}
		if (!q.isEmpty()) {
			const QVariantMap metadata = metadataForPath(p);
			QString haystack = normalizeTitle(QFileInfo(p).fileName()) + QStringLiteral("\n") + QFileInfo(p).fileName() + QStringLiteral("\n") + p;
			haystack += QStringLiteral("\n") + cleanedMetadataValue(metadata, QStringLiteral("title"));
			haystack += QStringLiteral("\n") + cleanedMetadataValue(metadata, QStringLiteral("genre"));
			haystack += QStringLiteral("\n") + cleanedMetadataValue(metadata, QStringLiteral("developer"));
			haystack += QStringLiteral("\n") + cleanedMetadataValue(metadata, QStringLiteral("publisher"));
			if (QFileInfo(p).fileName().contains(QStringLiteral("hack"), Qt::CaseInsensitive) || QFileInfo(p).fileName().contains(QStringLiteral("romhack"), Qt::CaseInsensitive)) {
				haystack += QStringLiteral("\nrom hack romhack");
			}
			bool matches = true;
			const QStringList terms = q.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
			for (const QString& term : terms) {
				if (!haystack.contains(term, Qt::CaseInsensitive)) {
					matches = false;
					break;
				}
			}
			if (!matches) {
				continue;
			}
		}
		m_filteredRoms.append(p);
	}

	if (m_recentOnly) {
		std::sort(m_filteredRoms.begin(), m_filteredRoms.end(), [&recentPaths](const QString& a, const QString& b) {
			const QString an = QDir::toNativeSeparators(QFileInfo(a).absoluteFilePath());
			const QString bn = QDir::toNativeSeparators(QFileInfo(b).absoluteFilePath());
			const int ai = recentPaths.indexOf(an);
			const int bi = recentPaths.indexOf(bn);
			if (ai != bi) {
				if (ai < 0) return false;
				if (bi < 0) return true;
				return ai < bi;
			}
			return QFileInfo(a).fileName().compare(QFileInfo(b).fileName(), Qt::CaseInsensitive) < 0;
		});
	} else {
		std::sort(m_filteredRoms.begin(), m_filteredRoms.end(), [this](const QString& a, const QString& b) {
			return displayTitleForPath(a).compare(displayTitleForPath(b), Qt::CaseInsensitive) < 0;
		});
	}

	m_pendingRestoreSelection = previousSelection;
	m_installedPopulateCount = 0;
	updateLibraryHeader();
	updateEmptyState();
	appendMoreInstalledItems();
	onSelectionChanged();
}

void LauncherWindow::appendMoreInstalledItems() {
	if (!m_grid || m_gridContainsCatalog) {
		return;
	}
	if (m_libraryPopulateTimer) {
		m_libraryPopulateTimer->stop();
	}
	const int end = std::min<int>(m_installedPopulateCount + kInstalledChunkSize, static_cast<int>(m_filteredRoms.size()));
	for (int i = m_installedPopulateCount; i < end; ++i) {
		addItemForPath(m_filteredRoms.at(i));
	}
	m_installedPopulateCount = end;
	updateResponsiveGridMetrics();
	m_grid->doItemsLayout();
	if (m_grid->viewport()) {
		m_grid->viewport()->update();
	}
	if (!m_pendingRestoreSelection.isEmpty()) {
		for (int i = 0; i < m_grid->count(); ++i) {
			QListWidgetItem* item = m_grid->item(i);
			if (item && item->data(Qt::UserRole).toString() == m_pendingRestoreSelection) {
				m_grid->setCurrentItem(item);
				m_pendingRestoreSelection.clear();
				break;
			}
		}
	}
	if (!m_grid->currentItem() && m_grid->count() > 0) m_grid->setCurrentRow(0);
	scheduleVisibleCoverRefresh();
	if (m_installedPopulateCount < m_filteredRoms.size()) {
		if (m_libraryPopulateTimer) {
			m_libraryPopulateTimer->start();
		}
		return;
	}
	if (!m_grid->currentItem() && m_grid->count() > 0) {
		m_grid->setCurrentRow(0);
	}
	updateBrowseCoverPrompt();
	updateCoverDownloadStatus();
	scheduleVisibleCoverRefresh();
}

void LauncherWindow::appendMoreBrowseItems(int count) {
	if (!m_gridContainsCatalog || !m_grid || m_browseLoadedCount >= m_filteredRoms.size()) {
		return;
	}
	if (count <= 0) {
		count = kBrowseChunkSize;
	}
	const int end = std::min<int>(m_browseLoadedCount + count, static_cast<int>(m_filteredRoms.size()));
	for (int i = m_browseLoadedCount; i < end; ++i) {
		addCatalogItem(catalogTitleFromId(m_filteredRoms.at(i)));
	}
	m_browseLoadedCount = end;
	if (m_browseLoadedCount < m_filteredRoms.size()) m_libraryPopulateTimer->start();
	updateResponsiveGridMetrics();
	if (m_grid) {
		m_grid->doItemsLayout();
		if (m_grid->viewport()) {
			m_grid->viewport()->update();
		}
	}
	scheduleVisibleCoverRefresh();
	if (m_browseOnly && m_bulkCoverDownloadRequested && m_browseCoverQueueTimer && !m_browseCoverQueueTimer->isActive()) {
		m_browseCoverQueueTimer->start();
	}
}


void LauncherWindow::clearLauncherCaches(bool refreshUi) {
	cancelArtworkDownloads();
	++m_artworkGeneration;
	m_coverPathCache.clear();
	m_coverPixmapCache.clear();
	m_backdropPixmapCache.clear();
	m_titleCache.clear();
	m_installedCatalogCache.clear();
	m_metadataCache.clear();
	// Original downloaded/custom covers are user data. Only rebuild derived thumbnails.
	const QString thumbnails = coverThumbCacheDirPath();
	QtConcurrent::run(&m_artworkWorkers, [thumbnails]() { QDir(thumbnails).removeRecursively(); });
	if (refreshUi) { applyLauncherFilter(); updateContinuePlaying(); }
}

void LauncherWindow::scheduleVisibleCoverRefresh() {
	if (!m_coverRefreshTimer) {
		return;
	}
	m_coverRefreshTimer->start(10);
}

void LauncherWindow::refreshVisibleCoverIcons() {
	if (!m_grid || !m_grid->viewport()) return;
	const QRect viewport = m_grid->viewport()->rect();
	const QRect ahead = viewport.adjusted(0, -viewport.height(), 0, viewport.height() * 2);
	int processed = 0;
	bool pending = false;
	auto prepare = [this, &processed](QListWidgetItem* item) {
		if (!item || item->data(Qt::UserRole + 1).toBool()) return false;
		if (processed >= 8 || m_coverImagesInFlight.size() >= 16) return false;
		updateItemVisual(item);
		++processed;
		return true;
	};
	for (const QRect& area : {viewport, ahead}) {
		for (int i = 0; i < m_grid->count(); ++i) {
			auto* item = m_grid->item(i);
			if (!item || item->data(Qt::UserRole + 1).toBool()) continue;
			if (!area.intersects(m_grid->visualItemRect(item))) continue;
			if (!prepare(item)) pending = true;
		}
	}
	while (m_thumbnailPreloadIndex < m_grid->count() && processed < 8 && m_coverImagesInFlight.size() < 16) {
		prepare(m_grid->item(m_thumbnailPreloadIndex++));
	}
	if (pending || m_thumbnailPreloadIndex < m_grid->count()) m_coverRefreshTimer->start(20);
}

qint64 LauncherWindow::playSecondsForPath(const QString& path) const {
	if (!m_config || path.isEmpty()) {
		return 0;
	}
	return m_config->getQtOption(launcherStatsKey(path) + QStringLiteral("/seconds"), QStringLiteral("launcherPlayStats")).toLongLong();
}

int LauncherWindow::launchCountForPath(const QString& path) const {
	if (!m_config || path.isEmpty()) {
		return 0;
	}
	return m_config->getQtOption(launcherStatsKey(path) + QStringLiteral("/launchCount"), QStringLiteral("launcherPlayStats")).toInt();
}

QDateTime LauncherWindow::lastPlayedForPath(const QString& path) const {
	if (!m_config || path.isEmpty()) {
		return {};
	}
	return m_config->getQtOption(launcherStatsKey(path) + QStringLiteral("/lastPlayed"), QStringLiteral("launcherPlayStats")).toDateTime();
}

QString LauncherWindow::statsSummaryForPath(const QString& path) const {
	QStringList parts;
	const qint64 secs = playSecondsForPath(path);
	if (secs > 0) {
		parts << tr("Played %1").arg(formatPlayDuration(secs));
	}
	const QDateTime last = lastPlayedForPath(path);
	if (last.isValid()) {
		parts << tr("Last played %1").arg(QLocale().toString(last.date(), QLocale::ShortFormat));
	}
	return parts.join(QStringLiteral(" • "));
}

QString LauncherWindow::detailMetaLineForPath(const QString& path) const {
	return launcherBadgeSummary(metadataBadgesForPath(path, true));
}

QStringList LauncherWindow::metadataBadgesForPath(const QString& path, bool includeDeveloper) const {
	const QVariantMap metadata = metadataForPath(path);
	const QString genre = cleanedMetadataValue(metadata, QStringLiteral("genre"));
	const int year = metadata.value(QStringLiteral("year")).toInt();
	const QString developer = cleanedMetadataValue(metadata, QStringLiteral("developer"));
	QStringList badges;
	badges << launcherPlatformLabelForPath(path);
	if (!genre.isEmpty()) badges << genre;
	if (year > 0) badges << QString::number(year);
	if (includeDeveloper && !developer.isEmpty()) badges << developer;
	return badges;
}

void LauncherWindow::populateBadgeRow(QHBoxLayout* layout, const QStringList& badges) const {
	if (!layout) {
		return;
	}
	while (QLayoutItem* item = layout->takeAt(0)) {
		if (QWidget* w = item->widget()) {
			w->deleteLater();
		}
		delete item;
	}
	layout->setSpacing(8);
	for (int i = 0; i < badges.size(); ++i) {
		const QString text = badges.at(i).trimmed();
		if (text.isEmpty()) {
			continue;
		}
		auto* badge = new QLabel(text);
		badge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		badge->setAlignment(Qt::AlignCenter);
		badge->setMinimumHeight(28);
		if (i == 0) {
			badge->setText(text.toUpper());
			badge->setMinimumWidth(62);
			badge->setStyleSheet(QStringLiteral("QLabel { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %1, stop:1 rgba(10, 12, 18, 0.92)); border: 1px solid %2; color: %3; border-radius: 10px; padding: 4px 12px; font-size: 12px; font-weight: 800; }")
				.arg(launcherPlatformBadgeBackground(text), launcherPlatformBadgeBorder(text), launcherPlatformBadgeForeground(text)));
		} else {
			badge->setStyleSheet(QStringLiteral("QLabel { background: rgba(255, 255, 255, 0.075); border: 1px solid rgba(255, 255, 255, 0.105); color: #e3def4; border-radius: 10px; padding: 4px 11px; font-size: 12px; font-weight: 650; }") );
		}
		layout->addWidget(badge, 0, Qt::AlignLeft | Qt::AlignVCenter);
	}
	layout->addStretch(1);
}

QString LauncherWindow::detailStatsLineForPath(const QString& path) const {
	QStringList parts;
	const qint64 secs = playSecondsForPath(path);
	if (secs > 0) {
		parts << tr("Played %1").arg(formatPlayDuration(secs));
	}
	const QDateTime last = lastPlayedForPath(path);
	if (last.isValid()) {
		parts << tr("Last played %1").arg(QLocale().toString(last.date(), QLocale::LongFormat));
	}
	return parts.join(QStringLiteral(" • "));
}

QStringList LauncherWindow::metadataCandidates(const QString& romPath) const {
	QFileInfo romInfo(romPath);
	if (!romInfo.exists()) {
		return {};
	}
	QStringList candidates;
	const QString stem = romInfo.completeBaseName();
	candidates << managedMetadataPathForRom(romPath);
	candidates << romInfo.absoluteDir().filePath(stem + QStringLiteral(".json"));
	candidates << romInfo.absoluteDir().filePath(stem + QStringLiteral(".metadata.json"));
	candidates.removeDuplicates();
	return candidates;
}

QString LauncherWindow::managedMetadataPathForRom(const QString& romPath) const {
	QFileInfo romInfo(romPath);
	return romInfo.absoluteDir().filePath(QStringLiteral("Metadata/") + romInfo.completeBaseName() + QStringLiteral(".json"));
}

QVariantMap LauncherWindow::readMetadataFromJsonFile(const QString& jsonPath) const {
	QFile file(jsonPath);
	if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
		return {};
	}
	QJsonParseError err{};
	const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject()) {
		return {};
	}
	QVariantMap map = doc.object().toVariantMap();
	for (auto it = map.begin(); it != map.end(); ++it) {
		#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		if (it.value().type() == QVariant::String) {
		#else
		if (it.value().typeId() == QMetaType::QString) {
		#endif
			it.value() = it.value().toString().trimmed();
		}
	}
	if (!map.contains(QStringLiteral("title"))) {
		map.insert(QStringLiteral("title"), QString());
	}
	return map;
}

QVariantMap LauncherWindow::metadataForPath(const QString& path) const {
	if (isCatalogEntry(path)) {
		return m_catalogMetadata.value(catalogTitleFromId(path));
	}
	const QString norm = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
	auto it = m_metadataCache.constFind(norm);
	if (it != m_metadataCache.constEnd()) {
		return it.value();
	}
	QVariantMap metadata;
	for (const QString& candidate : metadataCandidates(path)) {
		metadata = readMetadataFromJsonFile(candidate);
		if (!metadata.isEmpty()) {
			break;
		}
	}
	if (metadata.isEmpty()) {
		const QString linkedTitle = bestCatalogTitleForInstalledPath(norm);
		if (!linkedTitle.isEmpty()) {
			metadata = m_catalogMetadata.value(linkedTitle);
		}
	}
	m_metadataCache.insert(norm, metadata);
	return metadata;
}

void LauncherWindow::saveMetadataForPath(const QString& path, const QVariantMap& metadata) {
	if (path.isEmpty()) {
		return;
	}
	const QString target = managedMetadataPathForRom(path);
	QDir().mkpath(QFileInfo(target).absolutePath());
	QFile file(target);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		QMessageBox::warning(this, tr("Metadata not saved"), tr("GBAx could not write the metadata file."));
		return;
	}
	QJsonObject obj = QJsonObject::fromVariantMap(metadata);
	file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
	file.close();
	clearMetadataCache(path);
}

void LauncherWindow::clearMetadataCache(const QString& path) {
	if (path.isEmpty()) {
		m_metadataCache.clear();
		m_installedCatalogCache.clear();
		m_titleCache.clear();
		return;
	}
	const QString norm = normalizedRomPath(path);
	m_installedCatalogCache.remove(norm);
	m_metadataCache.remove(norm);
	m_titleCache.remove(norm);
}

QString LauncherWindow::combinedMetaLineForPath(const QString& path) const {
	return launcherBadgeSummary(metadataBadgesForPath(path, false));
}

void LauncherWindow::onSelectionChanged() {
	const QString previousSelectedPath = m_selectedPath;
	auto refreshSelectionVisuals = [this](const QString& oldPath, const QString& newPath) {
		if (!m_grid) {
			return;
		}
		for (int i = 0; i < m_grid->count(); ++i) {
			QListWidgetItem* it = m_grid->item(i);
			if (!it) {
				continue;
			}
			const QString itemPath = it->data(Qt::UserRole).toString();
			if (itemPath == oldPath || itemPath == newPath) {
				updateItemVisual(it);
			}
		}
	};
	QList<QListWidgetItem*> sel = m_grid ? m_grid->selectedItems() : QList<QListWidgetItem*>();
	const QString newPath = sel.isEmpty() ? QString() : sel.first()->data(Qt::UserRole).toString();
	m_selectedPath = newPath;
	m_pendingSelectedPath = newPath;
	refreshSelectionVisuals(previousSelectedPath, newPath);
	if (m_selectionDebounceTimer) {
		m_selectionDebounceTimer->start();
	} else {
		applyPendingSelectionChange();
	}
}

void LauncherWindow::applyPendingSelectionChange() {
	const QString appliedPath = m_pendingSelectedPath;
	if (appliedPath.isEmpty()) {
		m_selectedPath.clear();
		if (m_playButton) m_playButton->setEnabled(false);
		if (m_detailsThumb) m_detailsThumb->setPixmap(makeCover(tr("No Selection"), m_detailsThumb->size()));
		updateContinuePlaying();
		if (m_detailsTitle) {
			m_detailsTitle->setProperty("fullText", tr("Select a game"));
			updateDetailsTitleElide();
		}
		populateBadgeRow(m_detailsMetaLayout, QStringList{tr("GBA")});
		if (m_detailsStats) { m_detailsStats->clear(); m_detailsStats->setVisible(false); }
		if (m_detailsDescription) {
			m_detailsDescription->setText(tr("Select a game to view playtime, last played, and game details."));
			m_detailsDescription->setVisible(false);
		}
		if (m_detailsPath) { m_detailsPath->clear(); m_detailsPath->setVisible(false); }
		updateFavoriteUi();
		updateSaveStatePreviews();
		return;
	}
	m_selectedPath = appliedPath;
	const QString title = displayTitleForPath(m_selectedPath);
	if (m_detailsTitle) {
		m_detailsTitle->setProperty("fullText", title);
		updateDetailsTitleElide();
	}
	populateBadgeRow(m_detailsMetaLayout, metadataBadgesForPath(m_selectedPath, true));
	const QString stats = detailStatsLineForPath(m_selectedPath);
	if (m_detailsStats) {
		m_detailsStats->setText(stats);
		m_detailsStats->setVisible(!stats.isEmpty());
	}
	const QVariantMap metadata = metadataForPath(m_selectedPath);
	const QString description = cleanedMetadataValue(metadata, QStringLiteral("description"));
	if (m_detailsDescription) {
		m_detailsDescription->setVisible(!description.isEmpty());
		m_detailsDescription->setText(description);
	}
	if (m_detailsPath) {
		m_detailsPath->clear();
		m_detailsPath->setToolTip(QDir::toNativeSeparators(m_selectedPath));
		m_detailsPath->setVisible(false);
	}
	const QPixmap mainCover = cachedCoverPixmap(m_selectedPath, m_detailsCover->size());
	if (m_detailsCover) m_detailsCover->setPixmap(mainCover);
	if (m_detailsThumb) m_detailsThumb->setPixmap(cachedCoverPixmap(m_selectedPath, m_detailsThumb->size()));
	updateDynamicBackdrops();
	updateSaveStatePreviews();
	const bool catalog = isCatalogEntry(m_selectedPath);
	if (m_browseOnly && (m_autoDownloadCovers || m_bulkCoverDownloadRequested) && catalog) {
		queueAutoCoverDownloadForCatalogTitle(catalogTitleFromId(m_selectedPath), false);
	} else if (shouldAutoDownloadCovers()) {
		if (catalog) {
			queueAutoCoverDownloadForCatalogTitle(catalogTitleFromId(m_selectedPath), false);
		} else {
			queueAutoCoverDownloadForPath(m_selectedPath, false);
		}
	}
	if (m_playButton) {
		m_playButton->setText(catalog ? (installedRomPathForCatalogId(m_selectedPath).isEmpty() ? tr("LOCATE ROM…") : tr("PLAY GAME")) : tr("PLAY GAME"));
		m_playButton->setEnabled(true);
	}
	updateFavoriteUi();

	// Continue Playing stays locked to the last launched game.
	// Browsing All Games only updates the right-hand details pane.
	updateDynamicBackdrops();
	scheduleVisibleCoverRefresh();
}


void LauncherWindow::updateDetailsTitleElide() {
	if (!m_detailsTitle) return;
	const QString title = m_detailsTitle->property("fullText").toString();
	m_detailsTitle->setText(title);
	m_detailsTitle->setToolTip(title);
}

QPixmap LauncherWindow::cachedBackdropPixmap(const QString& path, const QSize& size, const QString&) const {
	if (path.isEmpty() || findCoverArtPath(path).isEmpty() || !size.isValid()) return {};
	const QString key = path + "|" + QString::number(size.width()) + "x" + QString::number(size.height()) + (m_performanceMode ? "p" : "f");
	auto cached = m_backdropPixmapCache.constFind(key);
	if (cached != m_backdropPixmapCache.constEnd()) return cached.value();
	// Blur a small source rather than decoding/upscaling a full cover for every resize.
	const QPixmap base = cachedCoverPixmap(path, QSize(196, 214));
	QPixmap out = launcherBackdropPixmap(base, size, m_performanceMode);
	if (m_backdropPixmapCache.size() >= 4) m_backdropPixmapCache.clear();
	m_backdropPixmapCache.insert(key, out);
	return out;
}

void LauncherWindow::updateContinueCardHeight() {
	if (!m_continueCard || !m_continueCard->isVisible() || !m_continueCard->layout()) return;
	auto* layout = m_continueCard->layout();
	layout->invalidate();
	const int needed = layout->hasHeightForWidth() ? layout->totalHeightForWidth(m_continueCard->width()) : layout->totalSizeHint().height();
	m_continueCard->setFixedHeight(qMax(174, needed + 4));
}

void LauncherWindow::updateDynamicBackdrops() {
	if (m_continueBackdrop && m_continueCard) {
		m_continueBackdrop->setGeometry(m_continueCard->rect());
		const QString heroPath = m_continueLaunchPath;
		m_continueBackdrop->setPixmap(cachedBackdropPixmap(heroPath, m_continueCard->size(), heroPath.isEmpty() ? tr("Continue") : displayTitleForPath(heroPath)));
		m_continueBackdrop->lower();
	}
	if (m_detailsBackdrop && m_detailsInfoCard) {
		m_detailsBackdrop->setGeometry(m_detailsInfoCard->rect());
		const QString fallback = m_detailsTitle ? m_detailsTitle->property("fullText").toString() : QString();
		m_detailsBackdrop->setPixmap(cachedBackdropPixmap(m_selectedPath, m_detailsInfoCard->size(), fallback.isEmpty() ? tr("GBAx") : fallback));
		m_detailsBackdrop->lower();
	}
}

void LauncherWindow::updateResponsiveGridMetrics() {
	if (!m_grid || !m_grid->viewport()) return;
	const int width = qMax(1, m_grid->viewport()->width() - 8);
	const int columns = qBound(1, width / 178, 8);
	const int cellWidth = qBound(110, width / columns, 206);
	const int coverWidth = cellWidth - 14;
	const QSize iconSize(coverWidth, qRound(coverWidth * kGbaCoverAspect));
	const QSize cellSize(cellWidth, iconSize.height() + 38);
	if (m_grid->iconSize() == iconSize && m_grid->gridSize() == cellSize) return;
	m_grid->setSpacing(0); // The cell includes its own gap; do not double-count spacing.
	m_grid->setIconSize(iconSize);
	m_grid->setGridSize(cellSize);
	m_thumbnailPreloadIndex = 0;
	for (int i = 0; i < m_grid->count(); ++i) m_grid->item(i)->setData(Qt::UserRole + 1, false);
	scheduleVisibleCoverRefresh();
}

void LauncherWindow::resizeEvent(QResizeEvent* event) {
	QMainWindow::resizeEvent(event);
	if (m_resizeDebounceTimer) m_resizeDebounceTimer->start();
}


void LauncherWindow::openEmulatorSettingsDialog() {
	if (!m_gameProcess || m_gameProcess->state() == QProcess::NotRunning) {
		if (!m_settingsHostWindow) m_settingsHostWindow = GBAApp::app()->newWindow(false);
		if (m_settingsHostWindow) m_settingsHostWindow->openSettingsWindow();
		return;
	}
#ifdef Q_OS_WIN
	AllowSetForegroundWindow(static_cast<DWORD>(m_gameProcess->processId()));
#endif
	sendLauncherCommand(this, launcherCommandServerName(), "open_settings\n", [this]() {
		QMessageBox::warning(this, tr("Settings unavailable"), tr("The player has not responded yet. Try again when the game window is ready."));
	});
}

void LauncherWindow::closeEvent(QCloseEvent* event) {
	cancelArtworkDownloads();
	if (m_libraryScanCancel) {
		m_libraryScanCancel->store(true);
	}
	if (m_libraryScanThread) {
		m_libraryScanThread->quit();
		m_libraryScanThread->wait();
	}
	if (m_scanWatcher.isRunning()) {
		m_scanWatcher.cancel();
		m_scanWatcher.waitForFinished();
	}
	QMainWindow::closeEvent(event);
	if (event->isAccepted()) {
		QCoreApplication::quit();
	}
}

void LauncherWindow::launchSelected() {
	if (m_selectedPath.isEmpty()) {
		return;
	}
	if (isCatalogEntry(m_selectedPath)) {
		const QString linked = installedRomPathForCatalogId(m_selectedPath);
		if (linked.isEmpty()) {
			installSelectedCatalogGame();
		} else {
			launchPath(linked);
		}
		return;
	}
	launchPath(m_selectedPath);
}

void LauncherWindow::launchSelectedSaveSlot(int slot) {
	if (m_selectedPath.isEmpty() || slot <= 0) {
		return;
	}
	const QString realPath = isCatalogEntry(m_selectedPath) ? installedRomPathForCatalogId(m_selectedPath) : m_selectedPath;
	const QString statePath = savestatePathForRomSlot(realPath, slot);
	if (!QFileInfo::exists(statePath)) {
		QMessageBox::information(this, tr("Save state not found"), tr("No save state exists for Slot %1 yet.").arg(slot));
		return;
	}
	launchPath(realPath, statePath);
}

void LauncherWindow::updateContinuePlaying() {
	QString last;
	if (m_config) {
		const QStringList mru = m_config->getMRU(ConfigController::MRU::ROM);
		if (!mru.isEmpty()) {
			last = mru.first();
		}
	}
	if (!last.isEmpty() && QFileInfo::exists(last)) {
		m_continueLaunchPath = last;
		{
			m_continueTitle->setText(displayTitleForPath(last));
			const QString stats = detailStatsLineForPath(last);
			populateBadgeRow(m_continueMetaLayout, metadataBadgesForPath(last, true));
			if (m_continueMetaRow) {
				m_continueMetaRow->setToolTip(QDir::toNativeSeparators(last));
			}
			if (m_continueStats) {
				QStringList compactStats;
				if (playSecondsForPath(last) > 0) compactStats << tr("Played %1").arg(formatPlayDuration(playSecondsForPath(last)));
				const QDateTime lastPlayed = lastPlayedForPath(last);
				if (lastPlayed.isValid()) compactStats << tr("Last played %1").arg(QLocale().toString(lastPlayed.date(), QStringLiteral("d MMM yyyy")));
				m_continueStats->setText(compactStats.join(QStringLiteral(" • ")));
				m_continueStats->setToolTip(stats);
				m_continueStats->setVisible(!stats.isEmpty());
			}
			if (m_continueCover) {
				m_continueCover->setPixmap(cachedCoverPixmap(last, m_continueCover->size()));
			}
			if (m_resumeButton) {
				m_resumeButton->setText(tr("Resume"));
			}
		}
		if (shouldAutoDownloadCovers()) {
			queueAutoCoverDownloadForPath(last, false);
		}
		m_resumeButton->setEnabled(true);
	} else {
		m_continueLaunchPath.clear();
		{
			m_continueTitle->setText(tr("No recent game yet"));
			populateBadgeRow(m_continueMetaLayout, QStringList{}); if (m_continueMetaRow) { m_continueMetaRow->setToolTip(QString()); }
			if (m_continueStats) { m_continueStats->setText(QString()); m_continueStats->setVisible(false); }
			if (m_continueCover) {
				m_continueCover->setPixmap(makeCover(tr("Recent"), m_continueCover->size()));
			}
			if (m_resumeButton) {
				m_resumeButton->setText(tr("Resume"));
			}
			m_resumeButton->setEnabled(false);
		}
	}
	if (m_settingsRomPath) {
		const QString rootDir = m_config ? m_config->getOption("romDirectory") : QString();
		m_settingsRomPath->setText(rootDir.isEmpty() ? tr("No ROM folder selected yet.") : QDir::toNativeSeparators(rootDir));
	}
	if (m_continueCard) m_continueCard->setVisible(!m_browseOnly && !m_favoritesOnly && !m_recentOnly && !m_continueLaunchPath.isEmpty());
	QTimer::singleShot(0, this, [this]() { updateContinueCardHeight(); updateDynamicBackdrops(); });
}

void LauncherWindow::launchPath(const QString& path) {
	launchPath(path, QString());
}

void LauncherWindow::launchPath(const QString& path, const QString& savestatePath) {
	if (path.isEmpty() || m_launchPending) {
		return;
	}
	if (m_gameProcess && m_gameProcess->state() != QProcess::NotRunning) {
		QMessageBox::information(this, tr("Game already running"), tr("Close the current game before launching another one."));
		return;
	}
	if (!QFileInfo(path).isFile()) {
		QMessageBox::warning(this, tr("Game not found"), tr("The selected ROM no longer exists. Refresh your library or choose its new location."));
		return;
	}
	const QString program = QCoreApplication::applicationFilePath();
	if (program.isEmpty()) {
		return;
	}
	if (!m_gameProcess) {
		m_gameProcess = new QProcess(this);
		m_gameProcess->setProcessChannelMode(QProcess::ForwardedChannels);
		connect(m_gameProcess, &QProcess::started, this, [this]() {
			m_launchPending = false;
			hideLaunchOverlay();
			const QString path = m_gameProcess->property("romPath").toString();
			if (m_config) {
				QStringList mru = m_config->getMRU(ConfigController::MRU::ROM);
				mru.removeAll(path);
				mru.prepend(path);
				while (mru.size() > ConfigController::MRU_LIST_SIZE) mru.removeLast();
				m_config->setMRU(mru, ConfigController::MRU::ROM);
			}
			updateContinuePlaying();
			if (m_playButton) m_playButton->setEnabled(!m_selectedPath.isEmpty());
		});
		connect(m_gameProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int, QProcess::ExitStatus) {
			if (m_config) m_config->reload();
			m_launchPending = false;
			hideLaunchOverlay();
			restoreLauncherWindowState();
			updateContinuePlaying();
			if (m_playButton) m_playButton->setEnabled(!m_selectedPath.isEmpty());
			if (m_resumeButton) m_resumeButton->setEnabled(!m_continueLaunchPath.isEmpty());
		});
		connect(m_gameProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
			if (error != QProcess::FailedToStart && error != QProcess::Crashed) return;
			m_launchPending = false;
			hideLaunchOverlay();
			restoreLauncherWindowState();
			if (m_playButton) m_playButton->setEnabled(!m_selectedPath.isEmpty());
			if (m_resumeButton) m_resumeButton->setEnabled(!m_continueLaunchPath.isEmpty());
			QMessageBox::warning(this, error == QProcess::Crashed ? tr("Game stopped unexpectedly") : tr("Launch failed"),
				m_gameProcess->errorString());
		});
	}

	m_launchPending = true;
	if (m_playButton) m_playButton->setEnabled(false);
	if (m_resumeButton) m_resumeButton->setEnabled(false);
	m_restoreFullscreen = isFullScreen();
	m_restoreMaximized = isMaximized();
	m_restoreGeometry = geometry();
	m_gameProcess->setProperty("romPath", path);
	showLaunchOverlay(displayTitleForPath(path));

	QStringList args;
	args << QStringLiteral("--launcher");
	const bool launchFullscreen = m_launchFullscreenCheck ? m_launchFullscreenCheck->isChecked() : (m_config && m_config->getOption("fullscreen").toInt() != 0);
	if (launchFullscreen) args << QStringLiteral("-f");
	if (!savestatePath.isEmpty()) {
		args << QStringLiteral("--savestate") << QDir::toNativeSeparators(QFileInfo(savestatePath).absoluteFilePath());
	}
	args << QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
	m_gameProcess->setWorkingDirectory(QFileInfo(program).absolutePath());
	if (m_config) m_config->write();
	m_gameProcess->start(program, args);
	// QProcess signals complete the handoff without blocking the GUI thread.
}

void LauncherWindow::showGameContextMenu(const QPoint& pos) {
	if (!m_grid) {
		return;
	}
	QListWidgetItem* item = m_grid->itemAt(pos);
	if (item) {
		m_grid->setCurrentItem(item);
		m_selectedPath = item->data(Qt::UserRole).toString();
	}
	if (m_selectedPath.isEmpty()) {
		return;
	}

	QMenu menu(this);
	const bool catalogSelected = isCatalogEntry(m_selectedPath);
	QAction* playAction = menu.addAction(catalogSelected ? (installedRomPathForCatalogId(m_selectedPath).isEmpty() ? tr("Locate ROM…") : tr("Play")) : tr("Play"));
	QAction* favAction = catalogSelected ? nullptr : menu.addAction(isFavorite(m_selectedPath) ? tr("Remove from Favorites") : tr("Add to Favorites"));
	menu.addSeparator();
	QAction* coverAction = catalogSelected ? nullptr : menu.addAction(tr("Change Cover…"));
	QAction* downloadCoverAction = menu.addAction(tr("Download Cover Online"));
		QAction* uninstallAction = (!catalogSelected && !m_selectedPath.isEmpty()) ? menu.addAction(tr("Hide from Library")) : nullptr;
	menu.addSeparator();
	QAction* editMetadataAction = catalogSelected ? nullptr : menu.addAction(tr("Edit Metadata…"));
	QAction* refreshMetadataAction = catalogSelected ? nullptr : menu.addAction(tr("Refresh Metadata"));
	menu.addSeparator();
	QAction* openFolderAction = catalogSelected ? nullptr : menu.addAction(tr("Open ROM Folder"));
	QAction* chosen = menu.exec(m_grid->viewport()->mapToGlobal(pos));
	if (chosen == playAction) {
		launchSelected();
	} else if (favAction && chosen == favAction) {
		toggleFavorite();
	} else if (coverAction && chosen == coverAction) {
		chooseCoverForSelected();
	} else if (chosen == downloadCoverAction) {
		downloadCoverOnlineForSelected();
		} else if (uninstallAction && chosen == uninstallAction) {
		uninstallSelectedFromLibrary();
	} else if (editMetadataAction && chosen == editMetadataAction) {
		editMetadataForSelected();
	} else if (refreshMetadataAction && chosen == refreshMetadataAction) {
		refreshMetadataForSelected();
	} else if (openFolderAction && chosen == openFolderAction) {
		openSelectedRomFolder();
	}
}

void LauncherWindow::chooseCoverForSelected() {
	if (m_selectedPath.isEmpty()) {
		return;
	}
	const QString selected = QFileDialog::getOpenFileName(this, tr("Choose cover image"), QFileInfo(m_selectedPath).absolutePath(), tr("Images (*.png *.jpg *.jpeg *.webp)"));
	if (selected.isEmpty()) {
		return;
	}
	const QFileInfo romInfo(m_selectedPath);
	const QFileInfo imgInfo(selected);
	const QString target = romInfo.absoluteDir().filePath(romInfo.completeBaseName() + QStringLiteral(".") + imgInfo.suffix().toLower());
	QFile::remove(target);
	if (!QFile::copy(selected, target)) {
		QMessageBox::warning(this, tr("Cover not updated"), tr("GBAx could not copy that image into your ROM folder."));
		return;
	}
	refreshCoverUi();
}

void LauncherWindow::refreshCoverUi() {
	++m_artworkGeneration;
	m_backdropPixmapCache.clear();
	m_coverPathCache.clear();
	m_coverPixmapCache.clear();
	applyLauncherFilter();
	updateContinuePlaying();
	onSelectionChanged();
	scheduleVisibleCoverRefresh();
}

QStringList LauncherWindow::managedCoverCandidates(const QString& romPath) const {
	const QFileInfo romInfo(romPath);
	if (!romInfo.exists()) {
		return {};
	}
	const QString normalized = displayTitleForPath(romInfo.absoluteFilePath());
	const QStringList stems = launcherCoverStems(romInfo, normalized);
	const QStringList exts{QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("webp")};
	QStringList dirs;
	dirs << romInfo.absolutePath();
	QDir currentDir(romInfo.absolutePath());
	dirs << currentDir.filePath(QStringLiteral("Artwork"));
	dirs << currentDir.filePath(QStringLiteral("artwork"));
	dirs << currentDir.filePath(QStringLiteral("covers"));
	dirs << currentDir.filePath(QStringLiteral("boxart"));
	const QString rootDir = m_config ? m_config->getOption("romDirectory") : QString();
	if (!rootDir.isEmpty()) {
		QDir root(rootDir);
		dirs << root.filePath(QStringLiteral("Artwork"));
		dirs << root.filePath(QStringLiteral("artwork"));
		dirs << root.filePath(QStringLiteral("covers"));
		dirs << root.filePath(QStringLiteral("boxart"));
	}
	dirs << coverCacheDirPath();
	dirs.removeDuplicates();

	QStringList out;
	for (const QString& dirPath : dirs) {
		QDir dir(dirPath);
		for (const QString& stem : stems) {
			for (const QString& ext : exts) {
				out << dir.filePath(stem + QStringLiteral(".") + ext);
			}
		}
	}
	out.removeDuplicates();
	return out;
}

void LauncherWindow::resetCoverForSelected() {
	if (m_selectedPath.isEmpty()) {
		return;
	}
	int removed = 0;
	for (const QString& candidate : managedCoverCandidates(m_selectedPath)) {
		if (QFileInfo::exists(candidate) && QFile::remove(candidate)) {
			++removed;
		}
	}
	if (!removed) {
		QMessageBox::information(this, tr("No custom cover found"), tr("GBAx could not find a managed cover image for this game to reset."));
		return;
	}
	refreshCoverUi();
}

void LauncherWindow::refreshCoverState() {
	if (m_selectedPath.isEmpty()) {
		return;
	}
	updateCoverUiForId(m_selectedPath);
	if (m_grid) {
		m_grid->doItemsLayout();
		if (m_grid->viewport()) {
			m_grid->viewport()->update();
		}
	}
	updateCoverDownloadStatus();
	scheduleVisibleCoverRefresh();
}

void LauncherWindow::editMetadataForSelected() {
	if (m_selectedPath.isEmpty()) {
		return;
	}

	QVariantMap metadata = metadataForPath(m_selectedPath);
	QDialog dialog(this);
	dialog.setWindowTitle(tr("Edit Metadata"));
	dialog.setModal(true);
	auto* layout = new QVBoxLayout(&dialog);
	auto* form = new QFormLayout();
	layout->addLayout(form);

	auto* titleEdit = new QLineEdit(cleanedMetadataValue(metadata, QStringLiteral("title")), &dialog);
	titleEdit->setPlaceholderText(displayTitleForPath(m_selectedPath));
	form->addRow(tr("Title"), titleEdit);

	auto* genreEdit = new QLineEdit(cleanedMetadataValue(metadata, QStringLiteral("genre")), &dialog);
	genreEdit->setPlaceholderText(tr("RPG, Platformer, Puzzle…"));
	form->addRow(tr("Genre"), genreEdit);

	auto* yearSpin = new QSpinBox(&dialog);
	yearSpin->setRange(0, 2100);
	yearSpin->setSpecialValueText(tr("Unknown"));
	yearSpin->setValue(metadata.value(QStringLiteral("year")).toInt());
	form->addRow(tr("Year"), yearSpin);

	auto* developerEdit = new QLineEdit(cleanedMetadataValue(metadata, QStringLiteral("developer")), &dialog);
	form->addRow(tr("Developer"), developerEdit);

	auto* publisherEdit = new QLineEdit(cleanedMetadataValue(metadata, QStringLiteral("publisher")), &dialog);
	form->addRow(tr("Publisher"), publisherEdit);

	auto* descriptionEdit = new QPlainTextEdit(cleanedMetadataValue(metadata, QStringLiteral("description")), &dialog);
	descriptionEdit->setPlaceholderText(tr("Optional game description…"));
	descriptionEdit->setFixedHeight(110);
	form->addRow(tr("Description"), descriptionEdit);

	auto* hint = new QLabel(tr("Metadata is saved locally in a JSON file inside a Metadata folder next to your ROMs."), &dialog);
	hint->setWordWrap(true);
	hint->setObjectName("mutedText");
	layout->addWidget(hint);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttons);

	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	QVariantMap out;
	if (!titleEdit->text().trimmed().isEmpty()) out.insert(QStringLiteral("title"), titleEdit->text().trimmed());
	if (!genreEdit->text().trimmed().isEmpty()) out.insert(QStringLiteral("genre"), genreEdit->text().trimmed());
	if (yearSpin->value() > 0) out.insert(QStringLiteral("year"), yearSpin->value());
	if (!developerEdit->text().trimmed().isEmpty()) out.insert(QStringLiteral("developer"), developerEdit->text().trimmed());
	if (!publisherEdit->text().trimmed().isEmpty()) out.insert(QStringLiteral("publisher"), publisherEdit->text().trimmed());
	if (!descriptionEdit->toPlainText().trimmed().isEmpty()) out.insert(QStringLiteral("description"), descriptionEdit->toPlainText().trimmed());
	saveMetadataForPath(m_selectedPath, out);
	refreshMetadataForSelected();
}

void LauncherWindow::refreshMetadataForSelected() {
	if (m_selectedPath.isEmpty()) {
		return;
	}
	clearMetadataCache(m_selectedPath);
	if (m_grid) {
		for (int i = 0; i < m_grid->count(); ++i) {
			QListWidgetItem* item = m_grid->item(i);
			if (!item) {
				continue;
			}
			const QString path = item->data(Qt::UserRole).toString();
			if (QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()) == QDir::toNativeSeparators(QFileInfo(m_selectedPath).absoluteFilePath())) {
				const QString title = displayTitleForPath(path);
				item->setText(title + (isFavorite(path) ? QStringLiteral("  ★") : QString()));
				item->setFont(launcherGridTitleFont());
				item->setForeground(QBrush(QColor(243, 245, 255, 226)));
				item->setTextAlignment(Qt::AlignHCenter);
				updateItemVisual(item);
				break;
			}
		}
	}
	if (m_selectedPath == m_continueLaunchPath) {
		updateContinuePlaying();
	}
	onSelectionChanged();
}

QString LauncherWindow::systemFolderNameForRom(const QString& path) const {
	if (isCatalogEntry(path)) return QStringLiteral("Nintendo_-_Game_Boy_Advance");
	const QString platform = launcherPlatformLabelForPath(path);
	if (platform == QStringLiteral("GB")) return QStringLiteral("Nintendo_-_Game_Boy");
	if (platform == QStringLiteral("GBC")) return QStringLiteral("Nintendo_-_Game_Boy_Color");
	return QStringLiteral("Nintendo_-_Game_Boy_Advance");
}



void LauncherWindow::downloadCoverOnlineForSelected() {
	if (m_selectedPath.isEmpty()) {
		return;
	}
	if (isCatalogEntry(m_selectedPath)) {
		queueAutoCoverDownloadForCatalogTitle(catalogTitleFromId(m_selectedPath), true);
		return;
	}
	queueAutoCoverDownloadForPath(m_selectedPath, true);
}

void LauncherWindow::queueAutoCoverDownloadForPath(const QString& path, bool manual) {
	const QString norm = normalizedRomPath(path);
	if (norm.isEmpty() || !findCoverArtPath(norm).isEmpty()) return;
	if (!manual && !shouldAutoDownloadCovers()) return;
	const QString linked = bestCatalogTitleForInstalledPath(norm);
	if (!linked.isEmpty()) { queueAutoCoverDownloadForCatalogTitle(linked, manual); return; }
	if (m_coverDownloadsInFlight.contains(norm) || m_coverDownloadCandidateMap.contains(norm)) return;
	if (!manual && (m_coverAutoTried.contains(norm) || (m_config && m_config->getQtOption(launcherStatsKey(norm), "launcherCoverRetry").toLongLong() > QDateTime::currentSecsSinceEpoch()))) return;
	const QFileInfo info(norm);
	QStringList names = launcherArtworkSearchTerms(info, normalizeTitle(info.fileName()), metadataForPath(norm));
	if (names.isEmpty()) return;
	m_coverAutoTried.insert(norm);
	m_coverDownloadCandidateMap.insert(norm, names.mid(0, 6));
	m_coverDownloadManualMap.insert(norm, manual);
	if (manual) m_coverDownloadPendingOrder.prepend(norm); else m_coverDownloadPendingOrder.append(norm);
	++m_coverDownloadQueuedCount;
	queueCoverUiRefresh(norm);
	pumpCoverDownloadQueue();
}

void LauncherWindow::startCoverDownloadAttempt(const QString& path, const QStringList& candidates, int index, bool manual) {
	if (!m_artworkNetwork) { finishCoverDownloadJob(path, false, manual); return; }
	if (index >= candidates.size()) { finishCoverDownloadJob(path, false, manual, 86400); return; }
	if (QDateTime::currentMSecsSinceEpoch() - m_coverDownloadStarted.value(path) > 20000) { finishCoverDownloadJob(path, false, manual); return; }
	const QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(candidates.at(index)));
	QNetworkRequest request(QUrl(QStringLiteral("https://thumbnails.libretro.com/%1/Named_Boxarts/%2.png").arg(libretroSystemPath(systemFolderNameForRom(path)), encoded)));
	request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("GBAx/2.0 artwork"));
	QNetworkReply* reply = m_artworkNetwork->get(request);
	m_coverReplies.insert(reply);
	QTimer::singleShot(8000, reply, [reply]() { if (!reply->isFinished()) reply->abort(); });
	connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64 total) { if (received > 8 * 1024 * 1024 || total > 8 * 1024 * 1024) reply->abort(); });
	connect(reply, &QNetworkReply::finished, this, [this, reply, path, candidates, index, manual]() {
		m_coverReplies.remove(reply);
		const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		const bool ok = reply->error() == QNetworkReply::NoError;
		const QByteArray data = reply->read(8 * 1024 * 1024 + 1);
		reply->deleteLater();
		if (status == 404) { startCoverDownloadAttempt(path, candidates, index + 1, manual); return; }
		// A disconnected service or rate limit will not improve with another filename.
		if (!ok || status != 200 || data.isEmpty() || data.size() > 8 * 1024 * 1024) { finishCoverDownloadJob(path, false, manual); return; }
		const QString output = isCatalogEntry(path) ? coverCachePathForCatalogTitle(catalogTitleFromId(path)) : cachedCoverPathForRom(path);
		auto* watcher = new QFutureWatcher<bool>(this);
		const quint64 generation = m_downloadGeneration;
		connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, path, manual, generation]() {
			const bool saved = watcher->result();
			watcher->deleteLater();
			if (!m_coverDownloadsInFlight.contains(path) || m_downloadGeneration != generation) return;
			finishCoverDownloadJob(path, saved, manual);
		});
		watcher->setFuture(QtConcurrent::run(&m_artworkWorkers, [data, output]() {
			QBuffer buffer;
			buffer.setData(data);
			buffer.open(QIODevice::ReadOnly);
			QImageReader reader(&buffer);
			reader.setDecideFormatFromContent(true);
			if (reader.size().isValid()) reader.setScaledSize(reader.size().scaled(QSize(1024, 1024), Qt::KeepAspectRatio));
			const QImage image = reader.read();
			if (image.isNull()) return false;
			QDir().mkpath(QFileInfo(output).absolutePath());
			QSaveFile file(output);
			return file.open(QIODevice::WriteOnly) && image.save(&file, "PNG") && file.commit();
		}));
	});
}

void LauncherWindow::openSelectedRomFolder() {
	if (m_selectedPath.isEmpty()) {
		return;
	}
	if (isCatalogEntry(m_selectedPath)) {
		const QString linked = installedRomPathForCatalogId(m_selectedPath);
		if (!linked.isEmpty()) {
			QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(linked).absolutePath()));
		}
		return;
	}
	QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_selectedPath).absolutePath()));
}

bool LauncherWindow::archiveContainsRom(const QString& path) const {
	const QByteArray p8 = QFileInfo(path).absoluteFilePath().toUtf8();
	VDir* dir = VDirOpenArchive(p8.constData());
	if (!dir) {
		return false;
	}
	bool found = false;
	// vfs.h: listNext returns a pointer; name/type are accessed via function pointers.
	while (VDirEntry* entry = dir->listNext(dir)) {
		if (!entry || entry->type(entry) != VFS_FILE) {
			continue;
		}
		const char* cName = entry->name(entry);
		if (!cName) {
			continue;
		}
		const QString name = QString::fromUtf8(cName);
		const QString ext = QFileInfo(name).suffix().toLower();
		if (ext == "gba" || ext == "gb" || ext == "gbc") {
			found = true;
			break;
		}
	}
	dir->close(dir);
	return found;
}

bool LauncherWindow::isAcceptedRomPath(const QString& path) const {
	const QFileInfo fi(path);
	if (!fi.exists() || !fi.isFile()) {
		return false;
	}
	const QString ext = fi.suffix().toLower();
	if (ext == "gba" || ext == "gb" || ext == "gbc") {
		return true;
	}
	if (ext == "zip" || ext == "7z") {
		// Avoid polluting the library with non-ROM archives.
		return archiveContainsRom(fi.absoluteFilePath());
	}
	return false;
}

void LauncherWindow::loadFavorites() {
	m_favorites.clear();
	if (!m_config) {
		return;
	}
	const QString raw = m_config->getOption("launcher.favorites");
	if (raw.isEmpty()) {
		return;
	}
	const QStringList parts = raw.split("\n", Qt::SkipEmptyParts);
	for (const QString& p : parts) {
		m_favorites.insert(QDir::toNativeSeparators(QFileInfo(p).absoluteFilePath()));
	}
}

void LauncherWindow::saveFavorites() {
	if (!m_config) {
		return;
	}
	QStringList out;
	out.reserve(m_favorites.size());
	for (const QString& p : m_favorites) {
		out << QDir::toNativeSeparators(p);
	}
	out.sort(Qt::CaseInsensitive);
	m_config->setOption("launcher.favorites", out.join("\n"));
}

bool LauncherWindow::isFavorite(const QString& path) const {
	return m_favorites.contains(QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()));
}

void LauncherWindow::updateFavoriteUi() {
	if (!m_favButton) {
		return;
	}
	const bool hasSel = !m_selectedPath.isEmpty();
	m_favButton->setEnabled(hasSel);
	const QSize favIconSize(20, 20);
	const QColor inactiveColor(214, 220, 234);
	const bool favorite = hasSel && isFavorite(m_selectedPath);
	const QColor activeColor = launcherAccentForTheme(m_launcherTheme);
	const QColor iconColor = favorite ? activeColor : inactiveColor;
	const QString iconPath = favorite ? QStringLiteral(":/icons/star-filled.png") : QStringLiteral(":/icons/star.png");
	m_favButton->setText(QString());
	m_favButton->setIcon(sidebarIcon(iconPath, favIconSize, iconColor, m_favButton->devicePixelRatioF()));
	m_favButton->setIconSize(favIconSize);
	if (!hasSel) {
		m_favButton->setToolTip(tr("Add/remove favorite"));
		return;
	}
	m_favButton->setToolTip(favorite ? tr("Remove from favorites") : tr("Add to favorites"));
}

void LauncherWindow::toggleFavorite() {
	if (m_selectedPath.isEmpty()) {
		return;
	}
	const QString norm = QDir::toNativeSeparators(QFileInfo(m_selectedPath).absoluteFilePath());
	if (m_favorites.contains(norm)) {
		m_favorites.remove(norm);
	} else {
		m_favorites.insert(norm);
	}
	saveFavorites();
	updateFavoriteUi();
	// Update ordering and star markers without rescanning the filesystem.
	applyLauncherFilter();
}




void LauncherWindow::queueAutoCoverDownloadForCatalogTitle(const QString& title, bool manual) {
	if (title.isEmpty()) return;
	const QString id = catalogIdForTitle(title);
	if (!findCoverArtPath(id).isEmpty() || m_coverDownloadsInFlight.contains(id) || m_coverDownloadCandidateMap.contains(id)) return;
	if (!manual && !shouldAutoDownloadCovers() && !m_bulkCoverDownloadRequested) return;
	if (!manual && !m_bulkCoverDownloadRequested && (m_coverAutoTried.contains(id) || (m_config && m_config->getQtOption(launcherStatsKey(id), "launcherCoverRetry").toLongLong() > QDateTime::currentSecsSinceEpoch()))) return;
	const QStringList candidates = catalogCoverCandidates(title);
	if (candidates.isEmpty()) return;
	m_coverAutoTried.insert(id);
	m_coverDownloadCandidateMap.insert(id, candidates.mid(0, 6));
	m_coverDownloadManualMap.insert(id, manual);
	if (manual) m_coverDownloadPendingOrder.prepend(id); else m_coverDownloadPendingOrder.append(id);
	++m_coverDownloadQueuedCount;
	queueCoverUiRefresh(id);
	pumpCoverDownloadQueue();
}

void LauncherWindow::loadInstalledLinks() {
	m_catalogInstalledLinks.clear();
	QFile file(installedLinksPath());
	if (!file.open(QIODevice::ReadOnly)) return;
	const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
	if (!doc.isObject()) return;
	const QJsonObject obj = doc.object();
	QSet<QString> seenPaths;
	bool sanitized = false;
	for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
		const QString rawPath = it.value().toString().trimmed();
		const QString normPath = normalizedRomPath(rawPath);
		if (it.key().trimmed().isEmpty() || normPath.isEmpty() || !QFileInfo::exists(normPath) || seenPaths.contains(normPath.toLower())) {
			sanitized = true;
			continue;
		}
		QString linkedTitle = it.key();
		const QString fileNameMatch = bestCatalogTitleForFileName(normPath);
		if (!fileNameMatch.isEmpty() && !catalogTitlesLooselyMatch(linkedTitle, fileNameMatch)) {
			linkedTitle = fileNameMatch;
			sanitized = true;
		}
		seenPaths.insert(normPath.toLower());
		m_catalogInstalledLinks.insert(linkedTitle, normPath);
		if (normPath != QDir::toNativeSeparators(QFileInfo(rawPath).absoluteFilePath())) {
			sanitized = true;
		}
	}
	if (sanitized) {
		saveInstalledLinks();
	}
}

void LauncherWindow::saveInstalledLinks() const {
	QJsonObject obj;
	for (auto it = m_catalogInstalledLinks.begin(); it != m_catalogInstalledLinks.end(); ++it) obj.insert(it.key(), it.value());
	QDir().mkpath(QFileInfo(installedLinksPath()).absolutePath());
	QFile file(installedLinksPath());
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

void LauncherWindow::loadHiddenInstalledPaths() {
	m_hiddenInstalledPaths.clear();
	QFile file(hiddenInstalledPath());
	if (!file.open(QIODevice::ReadOnly)) return;
	const auto doc = QJsonDocument::fromJson(file.readAll());
	if (!doc.isArray()) return;
	for (const QJsonValue& v : doc.array()) {
		const QString p = QDir::toNativeSeparators(QFileInfo(v.toString()).absoluteFilePath());
		if (!p.isEmpty()) m_hiddenInstalledPaths.insert(p);
	}
}

void LauncherWindow::saveHiddenInstalledPaths() const {
	QJsonArray arr;
	for (const QString& p : m_hiddenInstalledPaths) arr.append(p);
	QDir().mkpath(QFileInfo(hiddenInstalledPath()).absolutePath());
	QFile file(hiddenInstalledPath());
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

void LauncherWindow::removeCatalogLinksForPath(const QString& path) {
	const QString norm = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
	if (norm.isEmpty()) return;
	for (auto it = m_catalogInstalledLinks.begin(); it != m_catalogInstalledLinks.end();) {
		const QString linked = QDir::toNativeSeparators(QFileInfo(it.value()).absoluteFilePath());
		if (linked == norm) it = m_catalogInstalledLinks.erase(it);
		else ++it;
	}
}

void LauncherWindow::uninstallSelectedFromLibrary() {
	if (m_selectedPath.isEmpty() || isCatalogEntry(m_selectedPath)) return;
	const QString norm = QDir::toNativeSeparators(QFileInfo(m_selectedPath).absoluteFilePath());
	m_hiddenInstalledPaths.insert(norm);
	for (auto it = m_catalogInstalledLinks.begin(); it != m_catalogInstalledLinks.end();) {
		if (QDir::toNativeSeparators(QFileInfo(it.value()).absoluteFilePath()) == norm) it = m_catalogInstalledLinks.erase(it);
		else ++it;
	}
	saveInstalledLinks();
	saveHiddenInstalledPaths();
	applyLauncherFilter();
}

void LauncherWindow::loadCatalogTitles() {
	if (!m_catalogTitles.isEmpty()) return;
	m_catalogArtworkTitles.clear();
	m_catalogMetadata.clear();
	m_catalogLookupNormalized.clear();
	m_catalogLookupCompact.clear();
	m_installedCatalogCache.clear();

	auto tryLoadCatalogJson = [this](const QString& path) -> bool {
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) return false;
		const auto doc = QJsonDocument::fromJson(file.readAll());
		if (!doc.isArray()) return false;
		for (const QJsonValue& value : doc.array()) {
			if (!value.isObject()) continue;
			const QJsonObject obj = value.toObject();
			const QString title = obj.value(QStringLiteral("title")).toString().trimmed();
			if (title.isEmpty()) continue;
			if (title.contains(QStringLiteral("}}")) || title == QStringLiteral("Nintendo - Game Boy Advance")) continue;
			const QString art = obj.value(QStringLiteral("libretro_name")).toString().trimmed();
			m_catalogTitles.append(title);
			if (!art.isEmpty()) m_catalogArtworkTitles.insert(title, art);
			QVariantMap meta = obj.toVariantMap();
			if (!meta.isEmpty()) m_catalogMetadata.insert(title, meta);
		}
		return !m_catalogTitles.isEmpty();
	};

	if (!tryLoadCatalogJson(QStringLiteral(":/res/gba_catalog_us.json")) &&
		!tryLoadCatalogJson(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("gba_catalog_us.json"))) &&
		!tryLoadCatalogJson(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("res/gba_catalog_us.json")))) {
		QFile file(QStringLiteral(":/res/nointro.dat"));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			file.setFileName(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("nointro.dat")));
			if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
				file.setFileName(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("res/nointro.dat")));
				if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
			}
		}
		QMap<QString, QString> preferredByCanonical;
		while (!file.atEnd()) {
			const QString line = QString::fromUtf8(file.readLine()).trimmed();
			if (!line.startsWith(QStringLiteral("description \""))) continue;
			QString title = line.mid(QStringLiteral("description \"").size());
			if (title.endsWith(QStringLiteral("\""))) title.chop(1);
			if (title.startsWith(QStringLiteral("[BIOS]"))) continue;
			if (title.compare(QStringLiteral("Nintendo - Game Boy Advance"), Qt::CaseInsensitive) == 0) continue;
			const QString canonical = catalogCanonicalDisplayTitle(title).toLower();
			if (canonical.isEmpty()) continue;
			auto it = preferredByCanonical.find(canonical);
			if (it == preferredByCanonical.end() || catalogTitleBetterThan(title, it.value())) {
				preferredByCanonical[canonical] = title;
			}
		}
		m_catalogTitles = preferredByCanonical.values();
		for (const QString& t : std::as_const(m_catalogTitles)) m_catalogArtworkTitles.insert(t, t);
	}

	for (const QString& title : std::as_const(m_catalogTitles)) m_catalogDisplayTitles.insert(title, catalogCanonicalDisplayTitle(title));
	std::sort(m_catalogTitles.begin(), m_catalogTitles.end(), [this](const QString& a, const QString& b) {
		return m_catalogDisplayTitles.value(a).compare(m_catalogDisplayTitles.value(b), Qt::CaseInsensitive) < 0;
	});
	rebuildCatalogLookupIndexes();
}

void LauncherWindow::rebuildCatalogLookupIndexes() {
	m_catalogLookupNormalized.clear();
	m_catalogLookupCompact.clear();
	m_installedCatalogCache.clear();
	for (const QString& title : std::as_const(m_catalogTitles)) {
		m_catalogDisplayTitles.insert(title, catalogCanonicalDisplayTitle(title));
		insertCatalogLookupKey(m_catalogLookupNormalized, normalizedCatalogLookupKey(title), title);
		insertCatalogLookupKey(m_catalogLookupCompact, compactCatalogLookupKey(title), title);
		insertCatalogLookupKey(m_catalogLookupNormalized, normalizedCatalogLookupKey(catalogCanonicalDisplayTitle(title)), title);
		insertCatalogLookupKey(m_catalogLookupCompact, compactCatalogLookupKey(catalogCanonicalDisplayTitle(title)), title);
		insertCatalogLookupKey(m_catalogLookupNormalized, normalizedCatalogLookupKey(catalogArtworkNameForTitle(title)), title);
		insertCatalogLookupKey(m_catalogLookupCompact, compactCatalogLookupKey(catalogArtworkNameForTitle(title)), title);
		const QVariantMap meta = m_catalogMetadata.value(title);
		insertCatalogLookupKey(m_catalogLookupNormalized, normalizedCatalogLookupKey(cleanedMetadataValue(meta, QStringLiteral("title"))), title);
		insertCatalogLookupKey(m_catalogLookupCompact, compactCatalogLookupKey(cleanedMetadataValue(meta, QStringLiteral("title"))), title);
	}
}

QString LauncherWindow::bestCatalogTitleForName(const QString& rawName) const {
	if (rawName.trimmed().isEmpty()) return {};
	const QString exact = m_catalogLookupNormalized.value(normalizedCatalogLookupKey(rawName));
	return exact.isEmpty() ? m_catalogLookupCompact.value(compactCatalogLookupKey(rawName)) : exact;
}

QString LauncherWindow::bestCatalogTitleForFileName(const QString& path) const {
	const QFileInfo info(path);
	auto withLeadingArticle = [](QString value) {
		QRegularExpression re(QStringLiteral(R"(^(.*),\s*(The|A|An)(\s*[-–:]\s*.*)?$)"), QRegularExpression::CaseInsensitiveOption);
		auto m = re.match(value.trimmed());
		if (!m.hasMatch()) {
			return value;
		}
		QString out = m.captured(2) + QStringLiteral(" ") + m.captured(1);
		if (!m.captured(3).isEmpty()) {
			out += m.captured(3);
		}
		return out.simplified();
	};
	QStringList candidates;
	const QString fileName = info.fileName();
	const QString baseName = info.completeBaseName();
	const QString normalized = normalizeTitle(fileName);
	candidates << fileName
			   << baseName
			   << normalized
			   << canonicalLibretroLookupTitle(baseName)
			   << canonicalLibretroLookupTitle(normalized)
			   << withLeadingArticle(baseName)
			   << withLeadingArticle(normalized)
			   << canonicalLibretroLookupTitle(withLeadingArticle(baseName))
			   << canonicalLibretroLookupTitle(withLeadingArticle(normalized));
	candidates.removeDuplicates();
	for (const QString& candidate : std::as_const(candidates)) {
		const QString matched = bestCatalogTitleForName(candidate);
		if (!matched.isEmpty()) {
			return matched;
		}
	}
	return QString();
}

QString LauncherWindow::bestCatalogTitleForInstalledPath(const QString& path) const {
	const QString norm = normalizedRomPath(path);
	if (norm.isEmpty()) return {};
	auto cached = m_installedCatalogCache.constFind(norm);
	if (cached != m_installedCatalogCache.constEnd()) return cached.value();
	QString match;
	for (auto it = m_catalogInstalledLinks.constBegin(); it != m_catalogInstalledLinks.constEnd(); ++it) {
		if (it.value() == norm) { match = it.key(); break; }
	}
	// The built-in catalogue is GBA only. Never relabel a GB/GBC ROM as its GBA namesake.
	const QString ext = QFileInfo(norm).suffix().toLower();
	if (match.isEmpty() && ext != "gb" && ext != "gbc") match = bestCatalogTitleForFileName(norm);
	m_installedCatalogCache.insert(norm, match); // Cache misses as well as matches.
	return match;
}

void LauncherWindow::autoLinkInstalledRoms() {
	if (m_allRoms.isEmpty() || m_catalogTitles.isEmpty()) {
		return;
	}
	bool changed = false;
	QSet<QString> linkedPaths;
	for (auto it = m_catalogInstalledLinks.constBegin(); it != m_catalogInstalledLinks.constEnd(); ++it) {
		const QString linked = QDir::toNativeSeparators(QFileInfo(it.value()).absoluteFilePath());
		if (!linked.isEmpty()) {
			linkedPaths.insert(linked);
		}
	}
	for (const QString& rom : std::as_const(m_allRoms)) {
		const QString norm = QDir::toNativeSeparators(QFileInfo(rom).absoluteFilePath());
		if (norm.isEmpty() || m_hiddenInstalledPaths.contains(norm) || linkedPaths.contains(norm)) {
			continue;
		}
		const QString title = bestCatalogTitleForInstalledPath(norm);
		if (title.isEmpty()) {
			continue;
		}
		const QString existing = QDir::toNativeSeparators(QFileInfo(m_catalogInstalledLinks.value(title)).absoluteFilePath());
		if (existing.isEmpty() || !QFileInfo::exists(existing)) {
			m_catalogInstalledLinks.insert(title, norm);
			linkedPaths.insert(norm);
			changed = true;
		}
	}
	if (changed) {
		saveInstalledLinks();
		m_titleCache.clear();
		clearMetadataCache();
	}
}

bool LauncherWindow::isCatalogEntry(const QString& id) const { return isCatalogEntryId(id); }
QString LauncherWindow::catalogTitleFromId(const QString& id) const { return titleFromCatalogId(id); }
QString LauncherWindow::catalogIdForTitle(const QString& title) const { return catalogPrefix() + title; }
QString LauncherWindow::installedRomPathForCatalogId(const QString& id) const { return m_catalogInstalledLinks.value(catalogTitleFromId(id)); }
QString LauncherWindow::coverCachePathForCatalogTitle(const QString& title) const {
	const QString art = catalogArtworkNameForTitle(title);
	const QByteArray key = QCryptographicHash::hash((QStringLiteral("v2|") + title + QStringLiteral("|") + art).toUtf8(), QCryptographicHash::Sha1).toHex();
	return QDir(coverCacheDirPath()).filePath(QString::fromLatin1(key) + QStringLiteral(".png"));
}
QString LauncherWindow::catalogArtworkNameForTitle(const QString& title) const {
	return m_catalogArtworkTitles.value(title, title);
}
QStringList LauncherWindow::catalogCoverCandidates(const QString& title) const {
	QStringList out;
	const QString artworkTitle = catalogArtworkNameForTitle(title);
	if (!artworkTitle.isEmpty()) out << artworkTitle;
	out << title;
	QString cleaned = artworkTitle.isEmpty() ? title : artworkTitle;
	cleaned.remove(QRegularExpression(QStringLiteral("\\s*[\\(\\[].*$")));
	cleaned = cleaned.simplified();
	if (!cleaned.isEmpty()) out << cleaned;
	const QString display = m_catalogDisplayTitles.value(title, title);
	if (!display.isEmpty()) out << display;
	out.removeAll(QString());
	out.removeDuplicates();
	return out;
}


void LauncherWindow::installSelectedCatalogGame() {
	if (!isCatalogEntry(m_selectedPath)) return;
	const QString title = catalogTitleFromId(m_selectedPath);
	const QString file = QFileDialog::getOpenFileName(this, tr("Select ROM for %1").arg(title), m_config ? m_config->getOption("romDirectory") : QString(), tr("ROMs (*.gba *.gb *.gbc *.zip *.7z)"));
	if (file.isEmpty()) return;
	const QString norm = QDir::toNativeSeparators(QFileInfo(file).absoluteFilePath());
	removeCatalogLinksForPath(norm);
	m_catalogInstalledLinks.insert(title, norm);
	m_hiddenInstalledPaths.remove(norm);
	saveInstalledLinks();
	saveHiddenInstalledPaths();
	refreshLibrary();
	showAllGames();
}

void LauncherWindow::addCatalogItem(const QString& title) {
	const QString id = catalogIdForTitle(title);
	const QString display = m_catalogDisplayTitles.value(title, title);
	const QSize iconSize = (m_grid && m_grid->iconSize().isValid()) ? m_grid->iconSize() : launcherListPlaceholderCoverSize();
	auto* item = new QListWidgetItem(launcherCoverIcon(makeCover(QString(), iconSize)), display);
	item->setFont(launcherGridTitleFont());
	item->setForeground(QBrush(QColor(243, 245, 255, 226)));
	item->setTextAlignment(Qt::AlignHCenter);
	item->setData(Qt::UserRole, id);
	item->setData(Qt::UserRole + 1, false);
	const QString linked = installedRomPathForCatalogId(id);
	item->setToolTip(linked.isEmpty() ? title : tr("Installed: %1").arg(QDir::toNativeSeparators(linked)));
	item->setData(Qt::UserRole + 6, !linked.isEmpty());
	m_grid->addItem(item);
}

void LauncherWindow::addItemForPath(const QString& path) {
	const QString title = displayTitleForPath(path);
	const QSize iconSize = (m_grid && m_grid->iconSize().isValid()) ? m_grid->iconSize() : launcherListPlaceholderCoverSize();
	auto* item = new QListWidgetItem(launcherCoverIcon(makeCover(QString(), iconSize)), title);
	item->setFont(launcherGridTitleFont());
	item->setForeground(QBrush(QColor(243, 245, 255, 226)));
	item->setTextAlignment(Qt::AlignHCenter);
	item->setData(Qt::UserRole, path);
	item->setData(Qt::UserRole + 1, false);
	item->setToolTip(QDir::toNativeSeparators(path));
	m_grid->addItem(item);

}
