/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AboutScreen.h"

#include <gbax/core/version.h>

#include <QFile>
#include <QGuiApplication>
#include <QPixmap>
#include <QScrollArea>
#include <QScreen>
#include <QVBoxLayout>

using namespace QGBA;

AboutScreen::AboutScreen(QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
{
	m_ui.setupUi(this);

	QPixmap logo(":/res/gbax-logo-tagline.png");
	logo = logo.scaled(m_ui.logo->minimumSize() * devicePixelRatio(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	logo.setDevicePixelRatio(devicePixelRatio());
	m_ui.logo->setPixmap(logo);

	QLatin1String tree(gitBranch);
	if (tree == QLatin1String("(unknown)") || tree == QLatin1String("release")) {
		tree = QLatin1String("release");
	}

	QFile patronFile(":/res/patrons.txt");
	QStringList patronList;
	patronFile.open(QIODevice::ReadOnly | QIODevice::Text);
	while (true) {
		QByteArray line = patronFile.readLine();
		if (line.isEmpty()) {
			break;
		}
		patronList.append(QString::fromUtf8(line).trimmed());
	}

	m_ui.projectName->setText(QLatin1String(projectName));
	m_ui.projectVersion->setText(tr("Version %1").arg(QLatin1String(projectVersion)));

	{
		QString gitInfo = m_ui.gitInfo->text();
		gitInfo.replace("{gitBranch}", QLatin1String(gitBranch));
		gitInfo.replace("{gitCommit}", QLatin1String(gitCommit));
		m_ui.gitInfo->setText(gitInfo);

	}

	{
		QString description = QStringLiteral("GBAx is an open-source Game Boy Advance emulator with a built-in game launcher and library management, based on mGBA.");
		m_ui.description->setText(description);
	}

	{
		QString extraLinks = m_ui.extraLinks->text();
		extraLinks.replace("{gitBranch}", tree);
		m_ui.extraLinks->setText(extraLinks);
	}

	{
		QString patronsHeader = m_ui.patronsHeader->text();
		patronsHeader.replace("{projectName}", QLatin1String(projectName));
		m_ui.patronsHeader->setText(patronsHeader);
	}

	{
		QString patrons = m_ui.patrons->text();
		patrons.replace("{patrons}", patronList.join(" • "));
		m_ui.patrons->setText(patrons);
	}


	m_ui.patronsHeader->setVisible(true);
	m_ui.patronsHeader->setText(QStringLiteral("Release Notes"));
	m_ui.patrons->setVisible(true);
	m_ui.patrons->setText(QStringLiteral("<b>Version 2.0</b> — GBAx Launcher Experience Upgrade<br/><br/>GBAx is now a polished Game Boy Advance, Game Boy, and Game Boy Color launcher experience built around fast browsing, controller navigation, artwork, and a cleaner emulator handoff.<br/><br/>🎮 <b>Modern Game Launcher</b><br/>Browse Installed, Favorites, Recently Played, Browse Games, and Settings from a console-style interface with a premium dark GBAx theme.<br/><br/>🟣 <b>Continue Playing &amp; Resume Flow</b><br/>The Continue Playing panel now stays focused on your last launched game, with a Resume-first flow that can return you to the latest save state when available.<br/><br/>⚡ <b>Performance &amp; Responsiveness</b><br/>Improved search responsiveness, debounced selection updates, cached platform detection, and non-blocking quick-save slot loading to reduce launcher stalls and Not Responding states.<br/><br/>🖼 <b>Artwork, Covers &amp; Visual Library</b><br/>Automatic artwork support now works across GBA, GB, and GBC libraries, including zipped ROM platform detection for cleaner cover and badge matching.<br/><br/>🏷 <b>Platform Badges &amp; Typography</b><br/>Refined GB, GBC, and GBA badges with stronger visual identity, plus cleaner game-title typography under box art.<br/><br/>🎯 <b>Controller Navigation</b><br/>Controller shortcuts now support launcher section switching, emulator Quick Menu access, fullscreen toggling, smooth scrolling, back/confirm actions, and launcher/game switching.<br/><br/>🕹 <b>Emulator Window Polish</b><br/>Added a rounded, bordered player window, improved Quick Menu sizing, default Slot 1 save/load behavior, and a smoother launch handoff with x4-style default sizing.<br/><br/>📚 <b>Library Details</b><br/>The right-hand game details pane now has better layout behavior, scrollable content, cleaner badges, quick-save slot access, and improved handling on smaller screens.<br/><br/>🌐 <b>Website &amp; Support</b><br/>Updated GBAx links now point to https://gbax-emulator.com for website, help, and support flows.<br/><br/><b>Version 1.0</b> — Foundation Release<br/><br/>The beginning of GBAx — establishing the identity and foundation for a modern GBA experience.<br/><br/>• Introduced the GBAx branding and identity system<br/>• Built on the powerful mGBA core<br/>• Laid the groundwork for the integrated launcher and library system<br/>• Established the vision for a unified emulator and game management experience"));
	m_ui.patrons->setTextFormat(Qt::RichText);
	m_ui.patrons->setWordWrap(true);
	m_ui.patrons->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);

	m_ui.copyright->setText(QString::fromUtf8("© 2026 GBAx, licensed under the Mozilla Public License, version 2.0. Game Boy Advance is a registered trademark of Nintendo Co., Ltd."));
	m_ui.copyright->setVisible(true);

	// Make the lower release notes area properly scroll on smaller screens.
	if (m_ui.gridLayout_2) {
		m_ui.gridLayout_2->removeWidget(m_ui.patronsHeader);
		m_ui.gridLayout_2->removeWidget(m_ui.patrons);
		m_ui.gridLayout_2->removeWidget(m_ui.copyright);

		auto* notesContainer = new QWidget(this);
		auto* notesLayout = new QVBoxLayout(notesContainer);
		notesLayout->setContentsMargins(8, 0, 8, 0);
		notesLayout->setSpacing(10);
		notesLayout->addWidget(m_ui.patronsHeader);
		notesLayout->addWidget(m_ui.patrons);
		notesLayout->addWidget(m_ui.copyright);
		notesLayout->addStretch(1);

		auto* notesScroll = new QScrollArea(this);
		notesScroll->setObjectName(QStringLiteral("aboutNotesScroll"));
		notesScroll->setWidgetResizable(true);
		notesScroll->setFrameShape(QFrame::NoFrame);
		notesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		notesScroll->setWidget(notesContainer);
		m_ui.gridLayout_2->addWidget(notesScroll, 6, 0, 3, 2);
	}

	if (m_ui.gridLayout_2) {
		m_ui.gridLayout_2->setSizeConstraint(QLayout::SetDefaultConstraint);
	}
	setSizeGripEnabled(true);
	setMinimumSize(820, 620);
	resize(980, 720);
	if (QScreen* screen = QGuiApplication::primaryScreen()) {
		const QRect avail = screen->availableGeometry();
		resize(qMin(width(), int(avail.width() * 0.88)), qMin(height(), int(avail.height() * 0.88)));
		move(avail.center() - rect().center());
	}
}