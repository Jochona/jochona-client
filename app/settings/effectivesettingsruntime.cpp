//
// SPDX-FileCopyrightText: Jochona Client Contributors
// SPDX-License-Identifier: GPL-3.0-only
//
#include "effectivesettingsresolver.h"

#include "streamingpreferences.h"

StreamingPreferences* EffectiveSettingsResolver::createPreferences(
        const QVariantMap& context) const
{
    StreamingPreferences* preferences = StreamingPreferences::get()->clone();
    preferences->applyVariantMap(resolve(context)
                                 .value(QStringLiteral("values")).toMap());
    return preferences;
}
