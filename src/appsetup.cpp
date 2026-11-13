// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "appsetup.h"

#include <QFontDatabase>
#include <QStringList>

void loadBundledFonts()
{
    const QStringList fonts = {
        QStringLiteral("Orbitron-Variable.ttf"), QStringLiteral("Rajdhani-Medium.ttf"),
        QStringLiteral("Rajdhani-SemiBold.ttf"), QStringLiteral("Rajdhani-Bold.ttf"),
        QStringLiteral("ShareTechMono-Regular.ttf"),
    };
    for (const QString &f : fonts)
        QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/CalcA/fonts/") + f);
}
