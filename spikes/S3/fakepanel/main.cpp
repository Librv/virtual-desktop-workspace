// Spike S3: a fake bottom panel (layer-shell surface with an exclusive zone)
// so PlacementArea can be checked in the nested session, which has no
// plasmashell. Throwaway.
#include <LayerShellQt/Window>
#include <QGuiApplication>
#include <QPainter>
#include <QRasterWindow>

class Panel : public QRasterWindow
{
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter(this).fillRect(QRect(QPoint(0, 0), size()),Qt::darkBlue);
    }
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const int height = argc > 1 ? atoi(argv[1]) : 44;
    Panel w;
    auto *ls = LayerShellQt::Window::get(&w);
    ls->setLayer(LayerShellQt::Window::LayerTop);
    ls->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorLeft
                                                 | LayerShellQt::Window::AnchorRight));
    ls->setExclusiveZone(height);
    ls->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    w.resize(1920, height);
    w.show();
    return app.exec();
}
