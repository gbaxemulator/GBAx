#include "LauncherStyle.h"
/* Copyright (c) 2026 GBAx contributors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FirstRunDialog.h"

#include "GBAApp.h"
#include "ConfigController.h"
#include "GBAKeyEditor.h"
#include "InputController.h"

#ifdef BUILD_SDL
#include "input/SDLInputDriver.h"
#endif

#include <QButtonGroup>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSpacerItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QTimer>

#ifdef BUILD_SDL
#include "../sdl/sdl-events.h"
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QPalette>
#include <QColor>
#endif

using namespace QGBA;

static const char* const kFirstRunGroup = "gbax";
static const char* const kFirstRunKey = "firstRunComplete";

FirstRunDialog::FirstRunDialog(ConfigController* controller, QWidget* parent)
	: QDialog(parent)
	, m_controller(controller)
	, m_romsPathLabel(nullptr)
	, m_pickFolderButton(nullptr) {
	setWindowTitle(tr("Welcome to GBAx"));
	setFont(launcherUiFont());
	setModal(true);
	setMinimumWidth(720);
	setSizeGripEnabled(false);

    // Theme-aware accent styling
    // Use the current OS/Qt palette highlight color so the UI respects the user's theme.
    const QColor accent(181, 97, 255);
    const QColor accentLight = accent.lighter(135);
    const QColor accentDark  = accent.darker(135);

    const QString a   = accent.name(QColor::HexRgb);
    const QString aL  = accentLight.name(QColor::HexRgb);
    const QString aD  = accentDark.name(QColor::HexRgb);

    const int ar = accent.red();
    const int ag = accent.green();
    const int ab = accent.blue();

    // Subtle highlight for the peripheral buttons + modern glossy checked indicators
    setStyleSheet(QString(R"(
QDialog { background: #0f0c15; color: #f4effa; }
QLabel, QGroupBox, QCheckBox, QRadioButton { color: #e8e0ef; }
QPushButton { color: #f4effa; background: #221a2e; border: 1px solid #51405e; border-radius: 8px; padding: 8px 14px; }
QPushButton:disabled { color: #93849f; background: #17121f; }
QPushButton#startUsingGbax { background: %5; border: 1px solid %4; font-weight: 700; padding: 12px 24px; }
QGroupBox { border: 1px solid #51405e; border-radius: 8px; margin-top: 12px; padding-top: 12px; }
QGroupBox::title { subcontrol-origin: margin; left: 12px; }
QPushButton#welcomeOption {
    text-align: left;
    padding: 10px 14px;
    border-radius: 10px;
    border: 1px solid transparent;
}
QPushButton#welcomeOption:hover {
    background-color: rgba(%1, %2, %3, 30);
    border-color: rgba(%1, %2, %3, 150);
}
QPushButton#welcomeOption:pressed {
    background-color: rgba(%1, %2, %3, 56);
}
QPushButton#welcomeOption:focus {
    border-color: rgba(%1, %2, %3, 215);
}
QRadioButton::indicator, QCheckBox::indicator {
    width: 16px;
    height: 16px;
}
QRadioButton::indicator {
    border-radius: 8px;
}
QCheckBox::indicator {
    border-radius: 4px;
}
QRadioButton::indicator:unchecked, QCheckBox::indicator:unchecked {
    background: transparent;
    border: 1px solid #6a6a6a;
}
QRadioButton::indicator:checked, QCheckBox::indicator:checked {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 %4,
                                stop:1 %5);
    border: 1px solid %6;
    image: none;
}
)").arg(ar).arg(ag).arg(ab).arg(aL).arg(a).arg(aD));

	auto* root = new QVBoxLayout(this);
	root->setContentsMargins(18, 18, 18, 14);
	root->setSpacing(12);

	// Branding row
	auto* brandRow = new QHBoxLayout();
	brandRow->setSpacing(14);

	auto* logo = new QLabel(this);
	logo->setFixedWidth(320);
	logo->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	QPixmap pm(":/res/gbax-logo-tagline.png");
	if (!pm.isNull()) {
		logo->setPixmap(pm.scaledToWidth(320, Qt::SmoothTransformation));


    // Gentle "breathing" animation on the logo (doesn't replace the existing splash screen)
    // Qt doesn't have an "Alternate" direction; implement a simple ping‑pong by toggling direction on finish.
    m_logoEffect = new QGraphicsOpacityEffect(logo);
    logo->setGraphicsEffect(m_logoEffect);
    m_logoAnim = new QPropertyAnimation(m_logoEffect, "opacity", this);
    m_logoAnim->setDuration(1200);
    m_logoAnim->setStartValue(0.65);
    m_logoAnim->setEndValue(1.0);
    m_logoAnim->setEasingCurve(QEasingCurve::InOutSine);
    m_logoAnim->setLoopCount(1);
    m_logoAnim->setDirection(QAbstractAnimation::Forward);
    connect(m_logoAnim, &QPropertyAnimation::finished, this, [this]() {
        if (!m_logoAnim) return;
        m_logoAnim->setDirection(m_logoAnim->direction() == QAbstractAnimation::Forward
            ? QAbstractAnimation::Backward
            : QAbstractAnimation::Forward);
        m_logoAnim->start();
    });
    m_logoAnim->start();
	}
	brandRow->addWidget(logo);

	auto* headlineCol = new QVBoxLayout();
	headlineCol->setSpacing(6);

	auto* title = new QLabel(tr("Welcome"), this);
	QFont tf = title->font();
	tf.setPointSize(tf.pointSize() + 8);
	tf.setBold(true);
	title->setFont(tf);

	// Avoid duplicate "Welcome" under the main Welcome header
	auto* subtitle = new QLabel(tr("Let's get you set up in under a minute."), this);
	subtitle->setWordWrap(true);

	headlineCol->addWidget(title);
	headlineCol->addWidget(subtitle);
	headlineCol->addStretch(1);
	brandRow->addLayout(headlineCol, 1);

	root->addLayout(brandRow);

	// ROM folder section
	auto* romBox = new QVBoxLayout();
	romBox->setSpacing(6);

	auto* romLabel = new QLabel(tr("Default ROMs folder"), this);
	QFont rf = romLabel->font();
	rf.setBold(true);
	romLabel->setFont(rf);

	m_romsPathLabel = new QLabel(this);
	m_romsPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_romsPathLabel->setStyleSheet("color: #bdbdbd;");

	m_pickFolderButton = new QPushButton(tr("Choose folder…"), this);
	connect(m_pickFolderButton, &QPushButton::clicked, this, &FirstRunDialog::pickRomsFolder);

	// Initialize from existing lastDirectory if present
	QString lastDir = m_controller->getOption("romDirectory");
	if (lastDir.isEmpty()) lastDir = m_controller->getOption("lastDirectory");
	if (lastDir.isEmpty()) {
		m_romsPathLabel->setText(tr("Choose a folder now, or add your games later in Settings."));
	} else {
		setRomsFolder(lastDir);
	}

	auto* romRow = new QHBoxLayout();
	romRow->addWidget(m_romsPathLabel, 1);
	romRow->addWidget(m_pickFolderButton);

	romBox->addWidget(romLabel);
	romBox->addLayout(romRow);
	root->addLayout(romBox);

	// Auto-scan
	m_scanAutomatically = new QCheckBox(tr("Include subfolders when scanning for games"), this);
	m_scanAutomatically->setChecked(true);
	root->addWidget(m_scanAutomatically);

	// Graphics preset
	auto* gfxGroup = new QGroupBox(tr("Graphics preset"), this);
	auto* gfxLayout = new QVBoxLayout(gfxGroup);
	m_presetGroup = new QButtonGroup(this);
	m_presetGroup->setExclusive(true);

	m_presetPixelPerfect = new QRadioButton(tr("Original pixels (fastest)"), gfxGroup);
	m_presetHDSmooth = new QRadioButton(tr("HD Smooth (uses more graphics processing)"), gfxGroup);
	m_presetLCD = new QRadioButton(tr("LCD Handheld"), gfxGroup);

	m_presetGroup->addButton(m_presetPixelPerfect, 0);
	m_presetGroup->addButton(m_presetHDSmooth, 1);
	m_presetGroup->addButton(m_presetLCD, 2);

	m_presetPixelPerfect->setChecked(true);
	gfxLayout->addWidget(m_presetPixelPerfect);
	gfxLayout->addWidget(m_presetHDSmooth);
	gfxLayout->addWidget(m_presetLCD);
	root->addWidget(gfxGroup);

	// Other setup options
	auto* controlsLabel = new QLabel(tr("Configure controls (optional):"), this);
	root->addWidget(controlsLabel);

	auto* controlsButtons = new QHBoxLayout();
	m_configureKeyboard = new QPushButton(tr("Keyboard"), this);
	m_configureKeyboard->setObjectName("welcomeOption");
	m_configureController = new QPushButton(tr("Controller"), this);
	m_configureController->setObjectName("welcomeOption");
	controlsButtons->addWidget(m_configureKeyboard);
	controlsButtons->addWidget(m_configureController);
	controlsButtons->addStretch(1);
	root->addLayout(controlsButtons);

	// Live controller detection status (updates automatically while this welcome screen stays open)
	m_detectedControllerLabel = new QLabel(this);
	m_detectedControllerLabel->setObjectName("detectedControllerStatus");
	m_detectedControllerLabel->setWordWrap(true);
	m_detectedControllerLabel->setText(tr("No controller detected. Connect via USB or pair via Bluetooth; Controller will unlock automatically."));
	m_detectedControllerLabel->setStyleSheet("color: rgba(255,255,255,0.75); font-size: 12px; margin-left: 2px;");
	root->addWidget(m_detectedControllerLabel);

	connect(m_configureKeyboard, &QPushButton::clicked, this, [this]() {
		InputController inputController;
		inputController.setConfiguration(m_controller);

		QDialog dlg(this);
		dlg.setWindowTitle(tr("Keyboard"));
		dlg.setModal(true);

		auto* layout = new QVBoxLayout(&dlg);
		auto* editor = new GBAKeyEditor(&inputController, InputController::KEYBOARD, QString(), &dlg);
		layout->addWidget(editor);

		auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
		layout->addWidget(buttonBox);
		connect(buttonBox, &QDialogButtonBox::accepted, &dlg, [&]() {
			inputController.saveConfiguration(InputController::KEYBOARD);
			dlg.accept();
		});
		connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

		dlg.resize(920, 700);
		dlg.exec();
	});
	connect(m_configureController, &QPushButton::clicked, this, [this]() {
		#ifdef BUILD_SDL
		QDialog dlg(this);
		dlg.setWindowTitle(tr("Controllers"));
		dlg.setModal(true);

		// Reuse the welcome screen InputController.
		// A temporary InputController has no drivers attached, which makes it look like controllers
		// aren't detected inside the config dialog.
		m_welcomeInputController->setConfiguration(m_controller);
		m_welcomeInputController->update();

		auto* layout = new QVBoxLayout(&dlg);
		const QString controllerProfile = m_welcomeInputController->profileForType(SDL_BINDING_CONTROLLER);
		auto* editor = new GBAKeyEditor(m_welcomeInputController, SDL_BINDING_CONTROLLER, controllerProfile, &dlg);
		layout->addWidget(editor);

		auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
		layout->addWidget(buttonBox);
		connect(buttonBox, &QDialogButtonBox::accepted, &dlg, [&]() {
			if (!controllerProfile.isEmpty()) {
				m_welcomeInputController->saveProfile(SDL_BINDING_CONTROLLER, controllerProfile);
			} else {
				m_welcomeInputController->saveConfiguration(SDL_BINDING_CONTROLLER);
			}
			m_welcomeInputController->loadConfiguration(SDL_BINDING_CONTROLLER);
			m_controller->write();
			if (m_welcomeGamepadDriver) {
				m_welcomeGamepadDriver->loadConfiguration(m_controller);
			}
			dlg.accept();
		});
		connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

		dlg.resize(920, 700);
		dlg.exec();
		#else
		QMessageBox::information(this, tr("Controllers"), tr("Controller configuration is not available in this build."));
		#endif
	});

#ifdef BUILD_SDL
	// Poll input devices and keep the UI updated while the welcome screen stays open.
	m_welcomeInputController = new InputController(this, this);
	m_welcomeInputController->setConfiguration(m_controller);

	// The main window registers input drivers later in startup. The welcome dialog is shown
	// before that, so we register the SDL gamepad driver here to enable live detection.
	m_welcomeGamepadDriver = std::make_shared<SDLInputDriver>(m_welcomeInputController, this);
	m_welcomeGamepadDriver->loadConfiguration(m_controller);
	m_welcomeInputController->addInputDriver(m_welcomeGamepadDriver);
	m_welcomeInputController->setGamepadDriver(m_welcomeGamepadDriver->type());

	// Disable controller config until a controller is detected.
	m_configureController->setEnabled(false);
	m_configureController->setToolTip(tr("Connect a controller to configure it"));

	m_controllerDetectTimer = new QTimer(this);
	m_controllerDetectTimer->setInterval(500);
	connect(m_controllerDetectTimer, &QTimer::timeout, this, [this]() {
		if (!m_welcomeInputController || !m_detectedControllerLabel || !m_configureController) {
			return;
		}
		m_welcomeInputController->update();
		const QStringList pads = m_welcomeInputController->connectedGamepads();
		if (pads.isEmpty()) {
			m_detectedControllerLabel->setText(tr("No controller detected. Connect via USB or pair via Bluetooth; Controller will unlock automatically."));
			m_configureController->setEnabled(false);
			m_configureController->setToolTip(tr("Connect a controller to configure it"));
			return;
		}

		// Only show the active controller name. Some drivers can temporarily report duplicates while
		// (re)enumerating devices which makes the old “(+N)” counter appear to climb.
		m_detectedControllerLabel->setText(tr("Detected controller: %1").arg(pads.first()));
		m_configureController->setEnabled(true);
		m_configureController->setToolTip(QString());
	});
	m_controllerDetectTimer->start();

	// One immediate refresh so the UI is correct immediately.
	m_welcomeInputController->update();
	const QStringList pads = m_welcomeInputController->connectedGamepads();
	if (!pads.isEmpty()) {
		m_detectedControllerLabel->setText(tr("Detected controller: %1").arg(pads.first()));
		m_configureController->setEnabled(true);
		m_configureController->setToolTip(QString());
	}
#else
	m_configureController->setEnabled(false);
	m_detectedControllerLabel->setText(tr("Controller support is not included in this build. You can configure the keyboard instead."));
#endif

	m_portableMode = new QCheckBox(tr("Enable portable mode"), this);
	m_autoUpdate = new QCheckBox(tr("Check for updates automatically"), this);
	m_autoUpdate->setObjectName("welcomeAutoUpdate");
	m_autoUpdate->setChecked(true);
#ifndef BUILD_UPDATER
	m_autoUpdate->setChecked(false);
	m_autoUpdate->setEnabled(false);
	m_autoUpdate->hide();
	m_autoUpdate->setToolTip(tr("Automatic updates are not included in this build."));
#endif
	root->addWidget(m_portableMode);
	root->addWidget(m_autoUpdate);

	// Support row
	auto* support = new QLabel(this);
	support->setObjectName("helpLink");
	support->setText(
		tr("Need help or want to report a bug? Visit ") +
		QString("<a href=\"https://gbax-emulator.com/contact\">gbax-emulator.com/contact</a>"));
	support->setOpenExternalLinks(false);
	connect(support, &QLabel::linkActivated, this, [this](const QString&) { openContact(); });
	root->addWidget(support);

	root->addStretch(1);

	// Buttons
	auto* buttons = new QHBoxLayout();
	buttons->addStretch(1);

	auto* startBtn = new QPushButton(tr("Start using GBAx"), this);
	startBtn->setObjectName("startUsingGbax");
	startBtn->setDefault(true);
	connect(startBtn, &QPushButton::clicked, this, [this]() {
		accept();
	});

	buttons->addWidget(startBtn);
	buttons->addStretch(1);
	root->addLayout(buttons);
}

void FirstRunDialog::setRomsFolder(const QString& path) {
	if (path.isEmpty()) {
		return;
	}
	m_romsPathLabel->setText(path);
	m_romsFolder = path;
	// Re-use the existing lastDirectory behavior so the open dialog starts here.
	m_controller->setOption("lastDirectory", path);
	// Persist immediately.
	m_controller->write();
}

void FirstRunDialog::pickRomsFolder() {
	QString base = m_controller->getOption("lastDirectory");
	QString dir = QFileDialog::getExistingDirectory(this, tr("Select your ROMs folder"), base);
	if (dir.isEmpty()) {
		return;
	}
	setRomsFolder(dir);
}

void FirstRunDialog::openContact() {
	QDesktopServices::openUrl(QUrl("https://gbax-emulator.com/contact"));
}

void FirstRunDialog::accept() {
	// Persist first-run choices for later use.
	m_controller->setOption("romDirectory", m_romsFolder);
	m_controller->setOption("libraryAutoScan", m_scanAutomatically && m_scanAutomatically->isChecked() ? 1 : 0);
	m_controller->setOption("updateAutoCheck", m_autoUpdate && m_autoUpdate->isChecked() ? 1 : 0);
	// These are consumed by main.cpp after the main Window exists.
	m_controller->setQtOption("firstRunConfigureKeyboard", m_openKeyboard, kFirstRunGroup);
	m_controller->setQtOption("firstRunConfigureController", m_openController, kFirstRunGroup);

	// Graphics preset defaults
	const QString shadersDir = QDir(QGBA::GBAApp::dataDir()).filePath("shaders");
	if (m_presetPixelPerfect && m_presetPixelPerfect->isChecked()) {
		m_controller->setOption("shader", QString());
		m_controller->setOption("scaleMultiplier", 2);
	} else if (m_presetLCD && m_presetLCD->isChecked()) {
		m_controller->setOption("shader", QDir(shadersDir).filePath("lcd.shader"));
		m_controller->setOption("scaleMultiplier", 2);
	} else {
		// HD Smooth (default)
		m_controller->setOption("shader", QDir(shadersDir).filePath("xbr-lv3.shader"));
		m_controller->setOption("scaleMultiplier", 3);
	}

	// Portable mode is toggled by creating a portable.ini next to the executable
	if (m_portableMode && m_portableMode->isChecked()) {
		m_controller->makePortable();
		if (!ConfigController::isPortable()) {
			QMessageBox::warning(this, tr("Portable mode unavailable"), tr("GBAx could not create portable.ini beside the executable. Choose a writable folder or turn off portable mode."));
			return;
		}
	}

	// Ensure config is saved.
	m_controller->setQtOption(kFirstRunKey, true, kFirstRunGroup);
	m_controller->write();
	QDialog::accept();
}
