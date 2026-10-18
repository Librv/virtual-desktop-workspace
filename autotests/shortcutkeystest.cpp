// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "shortcutkeys.h"
#include "keytext.h"

#include <QKeySequence>

#include <QTest>

using namespace Vdw;

namespace
{
int chord(const QString &sequence)
{
    const QKeySequence keys(sequence);
    return keys.isEmpty() ? 0 : keys[0].toCombined();
}
}

class ShortcutKeysTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testModifierKeys_data()
    {
        QTest::addColumn<QString>("sequence");
        QTest::addColumn<QList<Qt::Key>>("keys");
        QTest::newRow("ctrl+meta+d") << "Ctrl+Meta+D" << QList<Qt::Key>{Qt::Key_Control, Qt::Key_Meta};
        QTest::newRow("meta+ctrl+d") << "Meta+Ctrl+D" << QList<Qt::Key>{Qt::Key_Control, Qt::Key_Meta};
        QTest::newRow("all four") << "Ctrl+Alt+Shift+Meta+F9" << QList<Qt::Key>{Qt::Key_Control, Qt::Key_Meta, Qt::Key_Shift, Qt::Key_Alt};
        QTest::newRow("alt only") << "Alt+F2" << QList<Qt::Key>{Qt::Key_Alt};
        QTest::newRow("no modifier") << "F9" << QList<Qt::Key>{};
        QTest::newRow("empty") << "" << QList<Qt::Key>{};
    }
    void testModifierKeys()
    {
        QFETCH(QString, sequence);
        QFETCH(QList<Qt::Key>, keys);
        QCOMPARE(ShortcutKeys::modifierKeys(chord(sequence)), keys);
    }

    void testKeyText_data()
    {
        QTest::addColumn<QString>("sequence");
        QTest::addColumn<QString>("text");
        QTest::newRow("ctrl+meta+d") << "Ctrl+Meta+D" << "D";
        QTest::newRow("f9") << "Ctrl+Meta+F9" << "F9";
        QTest::newRow("empty") << "" << "";
    }
    void testKeyText()
    {
        QFETCH(QString, sequence);
        QFETCH(QString, text);
        QCOMPARE(ShortcutKeys::keyText(chord(sequence)), text);
    }

    void testModifierText_data()
    {
        QTest::addColumn<QString>("sequence");
        QTest::addColumn<QString>("text");
        QTest::newRow("ctrl+meta+d") << "Ctrl+Meta+D" << "Ctrl+Meta";
        QTest::newRow("meta+ctrl+d") << "Meta+Ctrl+D" << "Ctrl+Meta";
        QTest::newRow("all four") << "Ctrl+Alt+Shift+Meta+F9" << "Ctrl+Meta+Shift+Alt";
        QTest::newRow("alt only") << "Alt+F2" << "Alt";
        QTest::newRow("no modifier") << "F9" << "";
        QTest::newRow("empty") << "" << "";
    }
    void testModifierText()
    {
        QFETCH(QString, sequence);
        QFETCH(QString, text);
        QCOMPARE(ShortcutKeys::modifierText(chord(sequence)), text);
    }
};

QTEST_GUILESS_MAIN(ShortcutKeysTest)

#include "shortcutkeystest.moc"
