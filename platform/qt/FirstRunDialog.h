/* Copyright (c) 2013-2025 mGBA contributors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QDialog>

#include <QString>

#include <memory>

class QLabel;
class QPushButton;
class QCheckBox;
class QRadioButton;
class QButtonGroup;
class QTimer;
class QGraphicsOpacityEffect;
class QPropertyAnimation;

namespace QGBA {

class ConfigController;
class InputController;
class InputDriver;

class FirstRunDialog : public QDialog {
	Q_OBJECT

public:
	explicit FirstRunDialog(ConfigController* controller, QWidget* parent = nullptr);

public slots:
	void setRomsFolder(const QString& path);
	void pickRomsFolder();
	void openContact();
	void accept() override;

private:
	ConfigController* m_controller = nullptr;

	// If the user chooses to configure controls during first-run, we defer opening
	// the Settings window until after the main Window is created.
	bool m_openKeyboard = false;
	bool m_openController = false;

	QLabel* m_romsPathLabel = nullptr;
	QString m_romsFolder;
	QPushButton* m_pickFolderButton = nullptr;
	QCheckBox* m_scanAutomatically = nullptr;

	QRadioButton* m_presetPixelPerfect = nullptr;
	QRadioButton* m_presetHDSmooth = nullptr;
	QRadioButton* m_presetLCD = nullptr;
	QButtonGroup* m_presetGroup = nullptr;

	QPushButton* m_configureKeyboard = nullptr;
	QPushButton* m_configureController = nullptr;
	QLabel* m_detectedControllerLabel = nullptr;
	QTimer* m_controllerDetectTimer = nullptr;
	InputController* m_welcomeInputController = nullptr;
	std::shared_ptr<InputDriver> m_welcomeGamepadDriver;
	QCheckBox* m_portableMode = nullptr;
	QCheckBox* m_autoUpdate = nullptr;
	QGraphicsOpacityEffect* m_logoEffect = nullptr;
	QPropertyAnimation* m_logoAnim = nullptr;
};

}
