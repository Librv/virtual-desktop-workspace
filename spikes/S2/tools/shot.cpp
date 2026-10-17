// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT
// S2 spike: save a screenshot of the (nested) KWin through
// org.kde.KWin.ScreenShot2.CaptureArea. KWin only answers callers whose
// executable is listed in a .desktop file with
// X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2.
//
// Usage: s2-shot OUT.png [X Y W H]   (default 0 0 1920 1080)
#include <QGuiApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusUnixFileDescriptor>
#include <QImage>
#include <QVariantMap>

#include <thread>
#include <unistd.h>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() != 2 && args.size() != 6) {
        fprintf(stderr, "usage: s2-shot OUT.png [X Y W H]\n");
        return 2;
    }
    int x = 0, y = 0;
    uint w = 1920, h = 1080;
    if (args.size() == 6) {
        x = args[2].toInt();
        y = args[3].toInt();
        w = args[4].toUInt();
        h = args[5].toUInt();
    }

    int fds[2];
    if (pipe(fds) != 0) {
        perror("pipe");
        return 1;
    }
    QByteArray data;
    std::thread reader([&] {
        char buf[65536];
        ssize_t n;
        while ((n = read(fds[0], buf, sizeof buf)) > 0) {
            data.append(buf, n);
        }
    });

    QDBusMessage reply;
    {
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                      QStringLiteral("/org/kde/KWin/ScreenShot2"),
                                                      QStringLiteral("org.kde.KWin.ScreenShot2"),
                                                      QStringLiteral("CaptureArea"));
    QVariantMap options;
    options.insert(QStringLiteral("include-cursor"), true);
    msg << x << y << w << h << options << QVariant::fromValue(QDBusUnixFileDescriptor(fds[1]));
    reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 10000);
    }
    close(fds[1]);
    fprintf(stderr, "s2-shot: call returned\n");
    if (reply.type() == QDBusMessage::ErrorMessage) {
        fprintf(stderr, "s2-shot: %s: %s\n", qPrintable(reply.errorName()), qPrintable(reply.errorMessage()));
        close(fds[0]);
        reader.join();
        return 1;
    }
    reader.join();
    close(fds[0]);

    const QVariantMap meta = qdbus_cast<QVariantMap>(reply.arguments().value(0));
    fprintf(stderr, "s2-shot: metadata:");
    for (auto it = meta.cbegin(); it != meta.cend(); ++it) {
        fprintf(stderr, " %s=%s", qPrintable(it.key()), qPrintable(it.value().toString()));
    }
    fprintf(stderr, " bytes=%lld\n", static_cast<long long>(data.size()));

    const int iw = meta.value(QStringLiteral("width")).toInt();
    const int ih = meta.value(QStringLiteral("height")).toInt();
    const int stride = meta.value(QStringLiteral("stride")).toInt();
    const auto format = static_cast<QImage::Format>(meta.value(QStringLiteral("format")).toInt());
    if (iw <= 0 || ih <= 0 || data.size() < qint64(stride) * ih) {
        fprintf(stderr, "s2-shot: unexpected image data\n");
        return 1;
    }
    const QImage img(reinterpret_cast<const uchar *>(data.constData()), iw, ih, stride, format);
    if (!img.copy().save(args[1])) {
        fprintf(stderr, "s2-shot: cannot save %s\n", qPrintable(args[1]));
        return 1;
    }
    fprintf(stderr, "s2-shot: saved %s (%dx%d)\n", qPrintable(args[1]), iw, ih);
    return 0;
}
