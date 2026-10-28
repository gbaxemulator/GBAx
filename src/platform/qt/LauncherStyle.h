/* Copyright (c) 2026 GBAx contributors
 * This Source Code Form is subject to the Mozilla Public License, v. 2.0.
 * See http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QIcon>

namespace QGBA {
inline QFont launcherUiFont() {
	static const QFont font = []() {
		QFont value = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
#ifdef Q_OS_WIN
		#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		const QStringList families = QFontDatabase::families();
#else
		const QStringList families = QFontDatabase().families();
#endif
		if (families.contains(QStringLiteral("Segoe UI"))) value.setFamily(QStringLiteral("Segoe UI"));
#endif
		value.setFamily(QFontInfo(value).family());
		value.setStyleHint(QFont::SansSerif);
		value.setStyleStrategy(QFont::PreferAntialias);
		return value;
	}();
	return font;
}

// Supplying the selected mode prevents the platform style tinting the cover.
inline QIcon launcherCoverIcon(const QPixmap& pixmap) {
	QIcon icon;
	for (QIcon::Mode mode : {QIcon::Normal, QIcon::Active, QIcon::Selected}) {
		icon.addPixmap(pixmap, mode, QIcon::Off);
		icon.addPixmap(pixmap, mode, QIcon::On);
	}
	return icon;
}
}
