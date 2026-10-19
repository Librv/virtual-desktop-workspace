// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "browserprofile.h"
#include "execargs.h"

#include <KShell>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Vdw;

Q_DECLARE_METATYPE(QList<Vdw::BrowserProfile>)
Q_DECLARE_METATYPE(QList<QList<int>>)

class BrowserProfileTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_configHome;

    static QByteArray largeDocument()
    {
        QByteArray json = "{\"other\":{\"a\":[1,2,3]},\"profile\":{\"info_cache\":{";
        for (int i = 0; i < 5000; ++i) {
            json += (i ? "," : "") + QByteArray("\"Profile ") + QByteArray::number(i) + "\":{\"name\":\"P" + QByteArray::number(i) + "\"}";
        }
        json += "},\"last_used\":\"Default\"},\"zzz\":true}";
        return json;
    }

private Q_SLOTS:
    void initTestCase()
    {
        // Never read or write the real ~/.config.
        QVERIFY(m_configHome.isValid());
        qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_configHome.path()));
    }

    void parsing_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::addColumn<bool>("ok");
        QTest::addColumn<QList<BrowserProfile>>("expected");
        using L = QList<BrowserProfile>;
        QTest::newRow("empty input") << QByteArray() << false << L();
        QTest::newRow("malformed") << QByteArray("{\"profile\":") << false << L();
        QTest::newRow("top-level array") << QByteArray("[]") << false << L();
        QTest::newRow("missing profile") << QByteArray("{\"x\":1}") << false << L();
        QTest::newRow("profile not object") << QByteArray("{\"profile\":[]}") << false << L();
        QTest::newRow("missing info_cache") << QByteArray("{\"profile\":{}}") << false << L();
        QTest::newRow("info_cache not object") << QByteArray("{\"profile\":{\"info_cache\":[1]}}") << false << L();
        QTest::newRow("empty info_cache") << QByteArray("{\"profile\":{\"info_cache\":{}}}") << true << L();
        QTest::newRow("entry without name") << QByteArray("{\"profile\":{\"info_cache\":{\"Default\":{},\"Profile 2\":5,\"Profile 3\":{\"name\":7}}}}") << true
                                            << L{{QStringLiteral("Default"), QStringLiteral("Default")},
                                                 {QStringLiteral("Profile 2"), QStringLiteral("Profile 2")},
                                                 {QStringLiteral("Profile 3"), QStringLiteral("Profile 3")}};
        QTest::newRow("unicode") << QByteArray("{\"profile\":{\"info_cache\":{\"Профиль 1\":{\"name\":\"日本語 \\u00e9\"}}}}") << true
                                 << L{{QString::fromUtf8("Профиль 1"), QString::fromUtf8("日本語 é")}};
        QTest::newRow("real example") << QByteArray(
            "{\"profile\":{\"info_cache\":{\"Default\":{\"name\":\"You\"},\"Profile 1\":{\"name\":\"Private\"}},\"last_used\":\"Default\"}}")
                                      << true
                                      << L{{QStringLiteral("Default"), QStringLiteral("You")}, {QStringLiteral("Profile 1"), QStringLiteral("Private")}};
    }
    void parsing()
    {
        QFETCH(QByteArray, json);
        QFETCH(bool, ok);
        QFETCH(QList<BrowserProfile>, expected);
        QString error;
        const QList<BrowserProfile> got = parseLocalState(json, &error);
        QCOMPARE(error.isEmpty(), ok);
        QCOMPARE(got, expected);
        // A null error pointer must be accepted.
        QCOMPARE(parseLocalState(json, nullptr), expected);
    }

    void parsingLarge()
    {
        QString error;
        const QList<BrowserProfile> got = parseLocalState(largeDocument(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(got.size(), 5000);
        // Keys are sorted by QJsonObject, so check membership instead of order.
        QVERIFY(got.contains({QStringLiteral("Profile 4999"), QStringLiteral("P4999")}));
    }

    void reading_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<bool>("ok");
        QTest::addColumn<int>("count");
        QTest::newRow("missing") << QStringLiteral("missing") << false << 0;
        QTest::newRow("too large") << QStringLiteral("large") << false << 0;
        QTest::newRow("valid") << QStringLiteral("valid") << true << 2;
    }
    void reading()
    {
        QFETCH(QString, kind);
        QFETCH(bool, ok);
        QFETCH(int, count);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        if (kind != QLatin1String("missing")) {
            QFile f(dir.filePath(QStringLiteral("Local State")));
            QVERIFY(f.open(QIODevice::WriteOnly));
            if (kind == QLatin1String("large")) {
                QVERIFY(f.resize(16 * 1024 * 1024 + 1));
            } else {
                f.write("{\"profile\":{\"info_cache\":{\"Default\":{\"name\":\"You\"},\"Profile 1\":{\"name\":\"Private\"}}}}");
            }
        }
        QString error;
        const QList<BrowserProfile> got = readLocalState(dir.path(), &error);
        QCOMPARE(error.isEmpty(), ok);
        QCOMPARE(got.size(), count);
        QCOMPARE(readLocalState(dir.path(), nullptr).size(), count);
    }

    void browsers_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<bool>("found");
        QTest::newRow("helium") << QStringLiteral("helium") << true;
        QTest::newRow("with suffix") << QStringLiteral("helium.desktop") << false;
        QTest::newRow("other app") << QStringLiteral("org.kde.konsole") << false;
        QTest::newRow("empty") << QString() << false;
    }
    void browsers()
    {
        QFETCH(QString, id);
        QFETCH(bool, found);
        const Browser *b = findBrowser(id);
        QCOMPARE(b != nullptr, found);
        if (b) {
            QCOMPARE(b->configDirName, QStringLiteral("net.imput.helium"));
            const QString dir = userDataDir(*b);
            QVERIFY(dir.endsWith(QStringLiteral("/net.imput.helium")));
            QCOMPARE(dir, m_configHome.path() + QStringLiteral("/net.imput.helium"));
        }
    }

    void profileArgs_data()
    {
        QTest::addColumn<QString>("args");
        QTest::addColumn<QString>("profile");
        QTest::addColumn<bool>("ok");
        QTest::addColumn<QStringList>("expected");
        const QString flag = QStringLiteral("--profile-directory");
        QTest::newRow("empty args") << QString() << "Default" << true << QStringList{flag + "=Default"};
        QTest::newRow("no flag") << "--incognito" << "Profile 1" << true << QStringList{"--incognito", flag + "=Profile 1"};
        QTest::newRow("replace =") << "--profile-directory=Default --incognito" << "Profile 1" << true << QStringList{"--incognito", flag + "=Profile 1"};
        QTest::newRow("replace two elements") << "--profile-directory Default --incognito" << "Profile 1" << true
                                              << QStringList{"--incognito", flag + "=Profile 1"};
        QTest::newRow("flag at end") << "--incognito --profile-directory" << "Profile 1" << true << QStringList{"--incognito", flag + "=Profile 1"};
        QTest::newRow("flag before option") << "--profile-directory --incognito" << "Profile 1" << true << QStringList{"--incognito", flag + "=Profile 1"};
        QTest::newRow("several") << "--profile-directory=A x --profile-directory B --profile-directory=C" << "D" << true << QStringList{"x", flag + "=D"};
        QTest::newRow("unicode dir") << "" << QString::fromUtf8("Профиль 日本") << true << QStringList{flag + QString::fromUtf8("=Профиль 日本")};
        QTest::newRow("strip only") << "--profile-directory=Default --incognito" << "" << true << QStringList{"--incognito"};
        QTest::newRow("strip only, empty") << "" << "" << true << QStringList{};
        QTest::newRow("metachar") << "a | b" << "Default" << false << QStringList{};
        QTest::newRow("quoted kept") << "--title \"my tab\"" << "Default" << true << QStringList{"--title", "my tab", flag + "=Default"};
        QTest::newRow("unbalanced quote") << "--title \"my" << "Default" << false << QStringList{};
    }
    void profileArgs()
    {
        QFETCH(QString, args);
        QFETCH(QString, profile);
        QFETCH(bool, ok);
        QFETCH(QStringList, expected);
        const std::optional<QString> result = withProfileArgs(args, profile);
        QCOMPARE(result.has_value(), ok);
        if (!ok) {
            return;
        }
        KShell::Errors status = KShell::NoError;
        const QStringList split = KShell::splitArgs(*result, KShell::AbortOnMeta | KShell::TildeExpand, &status);
        QCOMPARE(status, KShell::NoError);
        QCOMPARE(split, expected);
    }

    void hasProfileOption_data()
    {
        QTest::addColumn<QString>("args");
        QTest::addColumn<bool>("expected");
        QTest::newRow("empty") << "" << false;
        QTest::newRow("other args") << "--incognito --x=1" << false;
        QTest::newRow("= form") << "--profile-directory=Default" << true;
        QTest::newRow("two tokens") << "--incognito --profile-directory Default" << true;
        QTest::newRow("no value") << "--profile-directory" << true;
        QTest::newRow("quoted") << "'--profile-directory=Profile 1'" << true;
        QTest::newRow("unsplittable") << "--profile-directory=Default 'unclosed" << false;
        QTest::newRow("similar option") << "--profile-directoryx" << false;
        QTest::newRow("similar option with =") << "--profile-directoryx=Default" << false;
        QTest::newRow("only a value") << "foo --profile-director" << false;
    }

    void hasProfileOption()
    {
        QFETCH(QString, args);
        QFETCH(bool, expected);
        QCOMPARE(Vdw::hasProfileOption(args), expected);
    }

    // The path vdw-ui takes: split the args again, then quote them for an Exec line.
    void profileArgsExecLine()
    {
        const std::optional<QString> result = withProfileArgs(QStringLiteral("--incognito"), QStringLiteral("100% ~x"));
        QVERIFY(result.has_value());
        KShell::Errors status = KShell::NoError;
        const QStringList split = KShell::splitArgs(*result, KShell::AbortOnMeta | KShell::TildeExpand, &status);
        QCOMPARE(status, KShell::NoError);
        QCOMPARE(split, (QStringList{"--incognito", "--profile-directory=100% ~x"}));
        const QString exec = quoteExecArgs(split);
        QVERIFY2(exec.contains("100%% ~x"), qPrintable(exec));
        QCOMPARE(KShell::splitArgs(exec), (QStringList{"--incognito", "--profile-directory=100%% ~x"}));
    }
    void profileLaunch_data()
    {
        QTest::addColumn<QString>("args");
        QTest::addColumn<QString>("profile");
        QTest::addColumn<QString>("readError");
        QTest::addColumn<QString>("expectedArgs");
        QTest::addColumn<bool>("warns");
        const QString listed = QStringLiteral("Profile 1");
        QTest::newRow("listed") << "--incognito" << listed << QString() << "--incognito --profile-directory='Profile 1'" << false;
        QTest::newRow("profile wins over args") << "--profile-directory=Default --x" << listed << QString() << "--x --profile-directory='Profile 1'" << false;
        QTest::newRow("not listed: stripped") << "--incognito --profile-directory=Gone" << "Gone" << QString() << "--incognito" << true;
        QTest::newRow("local state unreadable: stripped") << "--incognito" << listed << "no such file" << "--incognito" << true;
        QTest::newRow("unsplittable args: unchanged") << "--x 'unclosed" << listed << QString() << "--x 'unclosed" << false;
        QTest::newRow("unsplittable args, not listed: warns") << "--x 'unclosed" << "Gone" << QString() << "--x 'unclosed" << true;
    }

    void profileLaunch()
    {
        QFETCH(QString, args);
        QFETCH(QString, profile);
        QFETCH(QString, readError);
        QFETCH(QString, expectedArgs);
        QFETCH(bool, warns);
        const QList<BrowserProfile> profiles{{QStringLiteral("Default"), QStringLiteral("Default")}, {QStringLiteral("Profile 1"), QStringLiteral("Work")}};
        const ProfileLaunch result = Vdw::profileLaunch(args, profile, profiles, readError);
        QCOMPARE(result.warning.isEmpty(), !warns);
        // Compare the split form: quoting of the appended option is not the point.
        KShell::Errors status = KShell::NoError;
        const QStringList got = KShell::splitArgs(result.args, KShell::AbortOnMeta | KShell::TildeExpand, &status);
        KShell::Errors expectedStatus = KShell::NoError;
        const QStringList expected = KShell::splitArgs(expectedArgs, KShell::AbortOnMeta | KShell::TildeExpand, &expectedStatus);
        QCOMPARE(status, expectedStatus);
        QCOMPARE(got, expected);
        if (expectedStatus != KShell::NoError) {
            QCOMPARE(result.args, expectedArgs);
        }
    }

    void profileNotice_data()
    {
        QTest::addColumn<QString>("appName");
        QTest::addColumn<QString>("profile");
        QTest::addColumn<QString>("readError");
        QTest::addColumn<int>("issue");
        QTest::addColumn<QString>("title");
        QTest::addColumn<QString>("text");
        const int none = int(ProfileIssue::None);
        const int missing = int(ProfileIssue::Missing);
        const int unreadable = int(ProfileIssue::Unreadable);
        QTest::newRow("listed") << "Helium" << "Default" << QString() << none << QString() << QString();
        QTest::newRow("missing") << "Helium" << "Profile 9" << QString() << missing << "Helium opened the last-used profile"
                                 << "There is no profile with the name \"Profile 9\".";
        QTest::newRow("unreadable") << "Helium" << "Default" << "no such file" << unreadable << "Helium opened the last-used profile"
                                    << "vdw cannot read the list of Helium profiles. Helium did not open the profile \"Default\".";
        QTest::newRow("unicode") << QString::fromUtf8("Brave Büro") << QString::fromUtf8("Profil Ä 日本") << QString() << missing
                                 << QString::fromUtf8("Brave Büro opened the last-used profile")
                                 << QString::fromUtf8("There is no profile with the name \"Profil Ä 日本\".");
        QTest::newRow("unicode, unreadable") << QString::fromUtf8("Brave Büro") << QString::fromUtf8("Profil Ä 日本") << "denied" << unreadable
                                             << QString::fromUtf8("Brave Büro opened the last-used profile")
                                             << QString::fromUtf8(
                                                    "vdw cannot read the list of Brave Büro profiles. Brave Büro did not open the profile \"Profil Ä 日本\".");
        QTest::newRow("placeholder text in names") << "100%1 App" << "%2" << QString() << missing << "100%1 App opened the last-used profile"
                                                   << "There is no profile with the name \"%2\".";
    }

    void profileNotice()
    {
        QFETCH(QString, appName);
        QFETCH(QString, profile);
        QFETCH(QString, readError);
        QFETCH(int, issue);
        QFETCH(QString, title);
        QFETCH(QString, text);
        const QList<BrowserProfile> profiles{{QStringLiteral("Default"), QStringLiteral("Default")}};
        const ProfileLaunch launch = Vdw::profileLaunch(QString(), profile, profiles, readError);
        QCOMPARE(int(launch.issue), issue);
        QCOMPARE(launch.warning.isEmpty(), issue == int(ProfileIssue::None));
        const ProfileNotice notice = Vdw::profileNotice(appName, profile, launch.issue);
        QCOMPARE(notice.title, title);
        QCOMPARE(notice.text, text);
    }

    void slotLaunchUsesConfigDir()
    {
        const QString dir = userDataDir(*findBrowser(QStringLiteral("helium")));
        QVERIFY(QDir().mkpath(dir));
        QFile f(dir + QStringLiteral("/Local State"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"profile\":{\"info_cache\":{\"Default\":{\"name\":\"You\"},\"Profile 1\":{\"name\":\"Private\"}}}}");
        f.close();

        LocalStateCache cache;
        // Not a browser: args pass through untouched, even with a profile set
        // and args that would change if they were split and joined again.
        const Slot konsole{QStringLiteral("org.kde.konsole"), QStringLiteral("--workdir ~ --profile-directory=X"), {}, QStringLiteral("Profile 1")};
        ProfileLaunch launch = slotLaunch(konsole, cache);
        QCOMPARE(launch.args, konsole.args);
        QVERIFY(launch.warning.isEmpty());
        QVERIFY(cache.isEmpty());

        // A browser without a profile is not changed either.
        const Slot plain{QStringLiteral("helium"), QStringLiteral("--profile-directory=X"), {}, QString()};
        launch = slotLaunch(plain, cache);
        QCOMPARE(launch.args, plain.args);
        QVERIFY(cache.isEmpty());

        const Slot listed{QStringLiteral("helium"), QStringLiteral("--incognito"), {}, QStringLiteral("Profile 1")};
        launch = slotLaunch(listed, cache);
        QCOMPARE(KShell::splitArgs(launch.args), (QStringList{"--incognito", "--profile-directory=Profile 1"}));
        QVERIFY(launch.warning.isEmpty());

        // Local State is read once per cache: the second slot does not see the
        // file being gone.
        QVERIFY(QFile::remove(dir + QStringLiteral("/Local State")));
        const Slot second{QStringLiteral("helium"), QString(), {}, QStringLiteral("Default")};
        launch = slotLaunch(second, cache);
        QCOMPARE(KShell::splitArgs(launch.args), (QStringList{"--profile-directory=Default"}));
        QVERIFY(launch.warning.isEmpty());

        // A new cache reads again and finds no file.
        LocalStateCache fresh;
        launch = slotLaunch(second, fresh);
        QCOMPARE(launch.args, QString());
        QVERIFY(!launch.warning.isEmpty());
    }

    void launchChainsGroupsBrowserSlots_data()
    {
        QTest::addColumn<QStringList>("apps");
        QTest::addColumn<QStringList>("profiles");
        QTest::addColumn<QList<QList<int>>>("expected");

        const QString h = QStringLiteral("helium");
        const QString k = QStringLiteral("org.kde.konsole");
        QTest::newRow("no slots") << QStringList() << QStringList() << QList<QList<int>>();
        QTest::newRow("no browsers") << QStringList{k, k} << QStringList{"", ""} << QList<QList<int>>{{0}, {1}};
        QTest::newRow("one helium") << QStringList{k, h} << QStringList{"", "Default"} << QList<QList<int>>{{0}, {1}};
        QTest::newRow("two helium, profile on one") << QStringList{h, k, h, k} << QStringList{"Default", "", "", ""} << QList<QList<int>>{{0, 2}, {1}, {3}};
        QTest::newRow("two helium, no profile") << QStringList{k, h, k, h} << QStringList{"", "", "", ""} << QList<QList<int>>{{0}, {1, 3}, {2}};
        QTest::newRow("three helium") << QStringList{h, h, h} << QStringList{"", "Default", ""} << QList<QList<int>>{{0, 1, 2}};
    }

    void launchChainsGroupsBrowserSlots()
    {
        QFETCH(QStringList, apps);
        QFETCH(QStringList, profiles);
        QFETCH(QList<QList<int>>, expected);
        QList<Slot> slots;
        for (int i = 0; i < apps.size(); ++i) {
            slots.append({apps.at(i), QString(), {}, profiles.at(i)});
        }
        QCOMPARE(launchChains(slots), expected);
    }
};

QTEST_GUILESS_MAIN(BrowserProfileTest)
#include "browserprofiletest.moc"
