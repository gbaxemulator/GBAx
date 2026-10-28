/* Copyright (c) 2026 GBAx contributors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QMainWindow>
#include <QFutureWatcher>
#include <QThread>
#include <QThreadPool>
#include <QJsonObject>
#include <QPointer>
#include <QPixmap>
#include <memory>
#include <QSet>
#include <QStringList>
#include <QHash>
#include <QCloseEvent>
#include <atomic>
#include <memory>
#include <QVariant>
#include <QPointer>

class QListWidget;
class QListWidgetItem;
class QLabel;
class QHBoxLayout;
class QLineEdit;
class QPushButton;
class QFutureWatcherBase;
class QWidget;
class QCheckBox;
class QStackedWidget;
class QPoint;
class QTimer;
class QResizeEvent;
class QShowEvent;
class QDateTime;
class QNetworkAccessManager;
class QProcess;
class QNetworkReply;
class QFrame;
class QComboBox;
class QScrollArea;
class QScrollBar;
class QProgressBar;

namespace QGBA {

class ConfigController;
class Window;
class InputController;

class LauncherWindow final : public QMainWindow {
Q_OBJECT
public:
	explicit LauncherWindow(ConfigController* config, QWidget* parent = nullptr, QNetworkAccessManager* artworkNetwork = nullptr);
	~LauncherWindow() override;

Q_SIGNALS:
	void startupVisualReady();

protected:
	void closeEvent(QCloseEvent* event) override;
	bool event(QEvent* event) override;
	bool eventFilter(QObject* watched, QEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void showEvent(QShowEvent* event) override;

private slots:
	void chooseRomsFolder();
	void refreshLibrary();
	void refreshLibrary(bool forceRescan);
	void filterChanged(const QString& text);
	void showAllGames();
	void showFavorites();
	void showRecentlyPlayed();
	void showBrowseGames();
	void showSettings();
	void onLibraryScanFinished();
	void cancelLibraryScan();
	void appendMoreInstalledItems();
	void onSelectionChanged();
	void applyPendingSelectionChange();
	void launchSelected();
	void launchSelectedSaveSlot(int slot);
	void launchPath(const QString& path);
	void launchPath(const QString& path, const QString& savestatePath);
	void toggleFavorite();
	void showGameContextMenu(const QPoint& pos);
	void chooseCoverForSelected();
	void downloadCoverOnlineForSelected();
	void resetCoverForSelected();
	void refreshCoverState();
	void openSelectedRomFolder();
	void editMetadataForSelected();
	void refreshMetadataForSelected();

	// Manual / bulk cover download flow
	void startBrowseCoverDownloads();
	void dismissBrowseCoverPrompt();

private:
	void applyModernTheme();
	void loadLauncherAppearanceSettings();
	void rebuildLibrary(const QString& rootDir, bool recursive);
	void startLibraryScan();
	void showLibraryScanOverlay();
	void hideLibraryScanOverlay();
	void updateLibraryScanOverlay(int current, int total, const QString& currentFile, bool preparing = false);
	void finalizeLibraryScan(const QStringList& results, bool cancelled);
	bool isLibraryScanRunning() const;
	void applyLauncherFilter();
	void updateSidebarIcons();
	void updateNavChecked();
	void updateContinuePlaying();
	void updateLibraryHeader();
	void updateEmptyState();
	void clearLauncherCaches(bool refreshUi = true);
	QPixmap makeCover(const QString& title, const QSize& size, const QString& status = QString()) const;
	QPixmap loadCoverArtForPath(const QString& path, const QSize& size) const;
	QString findCoverArtPath(const QString& romPath) const;
	QString normalizeTitle(const QString& fileName) const;
	QString compactPathForDisplay(const QString& path) const;
	bool archiveContainsRom(const QString& path) const;
	bool isAcceptedRomPath(const QString& path) const;
	void loadFavorites();
	void saveFavorites();
	bool isFavorite(const QString& path) const;
	void updateFavoriteUi();
	void addItemForPath(const QString& path);
	QString displayTitleForPath(const QString& path) const;
	QPixmap cachedCoverPixmap(const QString& path, const QSize& size) const;
	void refreshCoverUi();
	void scheduleVisibleCoverRefresh();
	void refreshVisibleCoverIcons();
	QStringList managedCoverCandidates(const QString& romPath) const;
	QPixmap decoratedCoverForPath(const QString& path, const QSize& size, bool hovered, bool selected, bool focused) const;
	void updateItemVisual(QListWidgetItem* item);
	void setHoveredItem(QListWidgetItem* item);
	void showLaunchOverlay(const QString& title);
	void hideLaunchOverlay();
	qint64 playSecondsForPath(const QString& path) const;
	int launchCountForPath(const QString& path) const;
	QDateTime lastPlayedForPath(const QString& path) const;
	QString statsSummaryForPath(const QString& path) const;
	QVariantMap metadataForPath(const QString& path) const;
	QVariantMap readMetadataFromJsonFile(const QString& jsonPath) const;
	QStringList metadataCandidates(const QString& romPath) const;
	QString managedMetadataPathForRom(const QString& romPath) const;
	void saveMetadataForPath(const QString& path, const QVariantMap& metadata);
	void clearMetadataCache(const QString& path = QString());
	void openEmulatorSettingsDialog();
	QString combinedMetaLineForPath(const QString& path) const;
	QStringList metadataBadgesForPath(const QString& path, bool includeDeveloper = false) const;
	void populateBadgeRow(QHBoxLayout* layout, const QStringList& badges) const;
	QString detailMetaLineForPath(const QString& path) const;
	QString detailStatsLineForPath(const QString& path) const;
	QString systemFolderNameForRom(const QString& path) const;
	bool tryDownloadLibretroCover(const QString& path, QString* savedPathOut = nullptr);
	void queueAutoCoverDownloadForPath(const QString& path, bool manual = false);
	void queueAutoCoverDownloadForCatalogTitle(const QString& title, bool manual = false);
	void startCoverDownloadAttempt(const QString& path, const QStringList& candidates, int index, bool manual);
	void loadCoverDownloadPreference();
	void saveCoverDownloadPreference() const;
	bool shouldAutoDownloadCovers() const;
	bool hasMissingCoverForPath(const QString& path) const;
	QString artworkStatusForPath(const QString& path) const;
	void updateBrowseCoverPrompt();
	void updateCoverDownloadStatus();
	void updateStartupCoverDownloadProgress();
	void finishBlockingStartupSequence();
	void queueMissingCoversForVisibleItems(bool manual = false);
	void queueMissingCoversForBrowseCatalog();
	void queueMissingCoversForLibrary();
	void queueNextBrowseCoverBatch();
	void pumpCoverDownloadQueue();
	void finishCoverDownloadJob(const QString& id, bool success, bool manual, int retrySeconds = 3600);
	void updateCoverUiForId(const QString& id);
	void refreshArtworkWidgets(const QString& id);
	void cancelArtworkDownloads();
	void ensureArtworkIndex(const QString& system);
	void parseArtworkIndex(const QString& system, const QByteArray& data, quint64 generation);
	void queueCoverUiRefresh(const QString& id);
	void flushPendingCoverUiRefreshes();
	void updateSaveStatePreviews();
	void updateSaveSlotExpansion();
	QString savestatePathForRomSlot(const QString& romPath, int slot) const;
	QPixmap saveStatePreviewForRomSlot(const QString& romPath, int slot, const QSize& size, QString* labelOut = nullptr) const;
	void moveGridSelection(int deltaRow, int deltaCol);
	void focusSidebarButton(int index);
	QScrollBar* analogVerticalScrollBar() const;
	QScrollBar* analogHorizontalScrollBar() const;
	void setAnalogScrollDirection(int axis, int direction);
	void applyAnalogScrollStep();
	void toggleLauncherFullscreen();
	void updateLauncherComboState(bool selectDown, bool startDown);
	void toggleToRunningGameWindow();
	void requestCloseRunningGame();
	void restoreLauncherWindowState();
	void updateDetailsTitleElide();
	void updateDynamicBackdrops();
	void updateContinueCardHeight();
	QPixmap cachedBackdropPixmap(const QString& path, const QSize& size, const QString& fallbackTitle) const;
	void updateResponsiveGridMetrics();
	void ensureStartupOverlay();
	void completeStartupVisualReady();
	void loadCatalogTitles();
	void rebuildCatalogLookupIndexes();
	QString bestCatalogTitleForName(const QString& rawName) const;
	QString bestCatalogTitleForFileName(const QString& path) const;
	QString bestCatalogTitleForInstalledPath(const QString& path) const;
	void autoLinkInstalledRoms();
	void loadInstalledLinks();
	void saveInstalledLinks() const;
	void loadHiddenInstalledPaths();
	void saveHiddenInstalledPaths() const;
	void removeCatalogLinksForPath(const QString& path);
	void uninstallSelectedFromLibrary();
	void installSelectedCatalogGame();
	bool isCatalogEntry(const QString& id) const;
	QString catalogTitleFromId(const QString& id) const;
	QString catalogIdForTitle(const QString& title) const;
	QString installedRomPathForCatalogId(const QString& id) const;
	QString coverCachePathForCatalogTitle(const QString& title) const;
	QString catalogArtworkNameForTitle(const QString& title) const;
	QStringList catalogCoverCandidates(const QString& title) const;
	void addCatalogItem(const QString& title);
	void appendMoreBrowseItems(int count = 0);
	bool handleLauncherNavigationEvent(QObject* watched, QEvent* event);


	ConfigController* m_config = nullptr;

	QStringList m_allRoms;
	QJsonObject m_archiveScanCache;
	QStringList m_filteredRoms;
	QString m_searchQuery;
	bool m_favoritesOnly = false;
	bool m_recentOnly = false;
	bool m_browseOnly = false;
	QFutureWatcher<QStringList> m_scanWatcher;
	QPointer<QThread> m_libraryScanThread;
	std::shared_ptr<std::atomic_bool> m_libraryScanCancel;
	bool m_startupRefreshPending = true;
	bool m_blockingStartupScan = false;
	bool m_blockingStartupCoverDownloads = false;
	bool m_forceRescanRequested = false;

	// UI
	QListWidget* m_grid = nullptr;
	QLineEdit* m_search = nullptr;
	QLabel* m_pageEyebrow = nullptr;
	QLabel* m_libraryTitle = nullptr;
	QLabel* m_librarySummary = nullptr;
	QPushButton* m_refreshButton = nullptr;
	QWidget* m_continueCard = nullptr;
	QLabel* m_continueBackdrop = nullptr;
	QStackedWidget* m_mainStack = nullptr;
	QWidget* m_libraryPage = nullptr;
	QWidget* m_settingsPage = nullptr;
	QPushButton* m_navLibrary = nullptr;
	QPushButton* m_navFavorites = nullptr;
	QPushButton* m_navRecent = nullptr;
	QPushButton* m_navBrowse = nullptr;
	QPushButton* m_navSettings = nullptr;
	QPushButton* m_footerBrandButton = nullptr;
	QPushButton* m_aboutFooterButton = nullptr;
	QPushButton* m_websiteFooterButton = nullptr;

	// Browse cover prompt / status
	QFrame* m_browseCoverPrompt = nullptr;
	bool m_startupVisualReadyEmitted = false;
	bool m_startupFirstShowHandled = false;
	QPointer<QWidget> m_startupOverlay;
	QPointer<QLabel> m_startupOverlayStatus;
	QTimer* m_startupRevealTimer = nullptr;
	QLabel* m_browseCoverPromptText = nullptr;
	QPushButton* m_browseDownloadCoversButton = nullptr;
	QPushButton* m_browseSkipCoversButton = nullptr;
	QLabel* m_coverDownloadStatus = nullptr;
	QLabel* m_globalStatusFooter = nullptr;
	QCheckBox* m_autoDownloadCoversCheck = nullptr;

	// Continue playing panel
	QLabel* m_continueCover = nullptr;
	QLabel* m_continueTitle = nullptr;
	QWidget* m_continueMetaRow = nullptr;
	QHBoxLayout* m_continueMetaLayout = nullptr;
	QLabel* m_continueStats = nullptr;
	QPushButton* m_resumeButton = nullptr;
	QString m_continueLaunchPath;

	// Details panel
	QWidget* m_detailsInfoCard = nullptr;
	QLabel* m_detailsBackdrop = nullptr;
	QLabel* m_detailsCover = nullptr;
	QLabel* m_detailsThumb = nullptr;
	QLabel* m_detailsTitle = nullptr;
	QLabel* m_detailsPath = nullptr;
	QWidget* m_detailsMetaRow = nullptr;
	QHBoxLayout* m_detailsMetaLayout = nullptr;
	QLabel* m_detailsStats = nullptr;
	QLabel* m_detailsDescription = nullptr;
	QPushButton* m_playButton = nullptr;
	QPushButton* m_favButton = nullptr;
	QPushButton* m_openEmulatorSettingsButton = nullptr;
	QPushButton* m_chooseFolderSettingsButton = nullptr;
	QPushButton* m_refreshLibrarySettingsButton = nullptr;
	QPushButton* m_clearLauncherCacheButton = nullptr;
	QPushButton* m_buildArtworkCacheButton = nullptr;
	QPushButton* m_factoryResetButton = nullptr;
	QPushButton* m_reportBugSettingsButton = nullptr;
	QWidget* m_saveStateCard = nullptr;
	QPushButton* m_saveSlotsToggle = nullptr;
	QWidget* m_saveSlotsContent = nullptr;
	QLabel* m_saveSlotsSummary = nullptr;
	QString m_saveSlotsSelection;
	bool m_saveSlotsHadSaves = false;
	QPointer<QScrollArea> m_saveStateScroll;
	QList<QPushButton*> m_saveStateButtons;
	QLabel* m_settingsRomPath = nullptr;
	QCheckBox* m_recursiveScanCheck = nullptr;
	QCheckBox* m_skipSplashCheck = nullptr;
	QCheckBox* m_launchFullscreenCheck = nullptr;
	QComboBox* m_themeCombo = nullptr;
	QPointer<QScrollArea> m_settingsScroll;
	QCheckBox* m_performanceModeCheck = nullptr;

	QSet<QString> m_favorites; // normalized absolute paths
	QStringList m_catalogTitles;
	QHash<QString, QString> m_catalogArtworkTitles;
	QHash<QString, QVariantMap> m_catalogMetadata;
	QHash<QString, QString> m_catalogInstalledLinks;
	QHash<QString, QString> m_catalogLookupNormalized;
	QHash<QString, QString> m_catalogLookupCompact;
	QHash<QString, QString> m_catalogDisplayTitles;
	mutable QHash<QString, QString> m_installedCatalogCache;
	QSet<QString> m_hiddenInstalledPaths;

	QString m_selectedPath;
	QPointer<Window> m_lastGameWindow;
	QString m_pendingLaunchPath;
	mutable QHash<QString, QString> m_titleCache;
	mutable QHash<QString, QString> m_coverPathCache;
	mutable QHash<QString, QPixmap> m_coverPixmapCache;
	mutable QSet<QString> m_coverImagesInFlight;
	quint64 m_artworkGeneration = 0;
	QThreadPool m_artworkWorkers;
	mutable QHash<QString, QVariantMap> m_metadataCache;
	QTimer* m_coverRefreshTimer = nullptr;
	QTimer* m_browseCoverQueueTimer = nullptr;
	QTimer* m_coverUiBatchTimer = nullptr;
	QTimer* m_libraryPopulateTimer = nullptr;
	QTimer* m_searchDebounceTimer = nullptr;
	QTimer* m_selectionDebounceTimer = nullptr;
	QTimer* m_resizeDebounceTimer = nullptr;
	QTimer* m_analogScrollTimer = nullptr;
	int m_analogScrollX = 0;
	int m_analogScrollY = 0;
	QTimer* m_controllerComboHoldTimer = nullptr;
	bool m_controllerSelectDown = false;
	bool m_controllerStartDown = false;
	bool m_controllerComboHoldTriggered = false;
	QString m_hoveredPath;
	QPointer<QWidget> m_launchOverlay;
	QPointer<QWidget> m_scanOverlay;
	QPointer<QLabel> m_scanOverlayTitle;
	QPointer<QLabel> m_scanOverlaySubtitle;
	QPointer<QLabel> m_scanOverlayProgress;
	QPointer<QLabel> m_scanOverlayCurrentFile;
	QPointer<QProgressBar> m_scanOverlayBar;
	QPointer<QPushButton> m_scanOverlayCancelButton;
	QPointer<QLabel> m_launchOverlayTitle;
	QPointer<QLabel> m_launchOverlaySubtitle;
	QProcess* m_gameProcess = nullptr;
	bool m_launchPending = false;
	QPointer<Window> m_settingsHostWindow;
	std::unique_ptr<InputController> m_launcherInput;
	QSet<QString> m_coverAutoTried;
	QString m_pendingSelectedPath;
	mutable QHash<QString, QPixmap> m_backdropPixmapCache;
	QSet<QString> m_coverDownloadsInFlight;
	QSet<QNetworkReply*> m_coverReplies;
	QPointer<QNetworkAccessManager> m_artworkNetwork;
	QHash<QString, QHash<QString, QString>> m_artworkIndexes;
	QSet<QString> m_artworkIndexesLoading;
	QSet<QString> m_artworkIndexesReady;
	quint64 m_downloadGeneration = 0;
	QHash<QString, qint64> m_coverDownloadStarted;
	int m_coverDownloadFailedCount = 0;
	QStringList m_coverDownloadPendingOrder;
	QHash<QString, QStringList> m_coverDownloadCandidateMap;
	QHash<QString, bool> m_coverDownloadManualMap;
	QHash<QString, bool> m_coverDownloadCatalogMap;
	QSet<QString> m_coverUiPendingRefreshIds;
	bool m_autoDownloadCovers = false;
	bool m_autoArtworkPaused = false;
	QStringList m_libraryCoverPaths;
	int m_libraryCoverQueueIndex = 0;
	QSet<QString> m_startupCoverDownloadIds;
	int m_startupCoverDownloadTotal = 0;
	int m_startupCoverDownloadCompleted = 0;
	bool m_bulkCoverDownloadRequested = false;
	bool m_catalogArtworkRequested = false;
	bool m_gridContainsCatalog = false;
	int m_thumbnailPreloadIndex = 0;
	bool m_bulkCoverDownloadDismissed = false;
	bool m_browseBulkNeedsRestartNotice = false;
	bool m_browseBulkRestartNoticeShown = false;
	int m_coverDownloadQueuedCount = 0;
	int m_coverDownloadCompletedCount = 0;
	int m_browseCoverQueueIndex = 0;
	QStringList m_browsePreloadIds;
	int m_browseLoadedCount = 0;
	int m_installedPopulateCount = 0;
	QString m_pendingRestoreSelection;
	static constexpr int kBrowseChunkSize = 24;
	static constexpr int kInstalledChunkSize = 18;
	static constexpr int kBrowseCoverQueueChunkSize = 16;
	static constexpr int kMaxConcurrentCoverDownloads = 4;
	QString m_launcherTheme = QStringLiteral("purple");
	bool m_performanceMode = false;
	bool m_restoreFullscreen = false;
	bool m_restoreMaximized = false;
	QRect m_restoreGeometry;
};

} // namespace QGBA
