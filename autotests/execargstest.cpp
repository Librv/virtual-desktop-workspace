#include "execargs.h"

#include <QTest>

using namespace Vdw;

class ExecArgsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testPlain()
    {
        QCOMPARE(quoteExecArgs({QStringLiteral("--workdir"), QStringLiteral("/tmp")}), QStringLiteral("--workdir /tmp"));
    }

    void testPercentIsDoubled()
    {
        QCOMPARE(quoteExecArgs({QStringLiteral("100%")}), QStringLiteral("100%%"));
        QCOMPARE(quoteExecArgs({QStringLiteral("%U")}), QStringLiteral("%%U"));
    }

    void testSpacesStayOneArgument()
    {
        QCOMPARE(quoteExecArgs({QStringLiteral("a b")}), QStringLiteral("'a b'"));
    }

    void testEmpty()
    {
        QCOMPARE(quoteExecArgs({}), QString());
    }
};

QTEST_GUILESS_MAIN(ExecArgsTest)

#include "execargstest.moc"
