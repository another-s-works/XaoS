#include <QtWidgets>
#include <cstring>
#include <iostream>
#include <set>

#include "mainwindow.h"
#include "fractalwidget.h"

#include "ui.h"
#include "filter.h"
#include "grlib.h"
#include "ui_helper.h"
#include "version.h"

static void buildMenu(struct uih_context *uih, const char *name);
static void toggleMenu(struct uih_context *uih, const char *name);
static void popupMenu(struct uih_context *uih, const char *name);
static void showDialog(struct uih_context *c, const char *name);
static void showHelp(struct uih_context *c, const char *name);

struct gui_driver gui_driver = {
    /* setrootmenu */   buildMenu,
    /* enabledisable */ toggleMenu,
    /* menu */          popupMenu,
    /* dialog */        showDialog,
    /* help */          showHelp
};

static int imagePrint(struct image *image, int x, int y, const char *text, int fgcolor, int bgcolor, int mode);
static int imageTextWidth(struct image *image, const char *text);
static int imageTextHeight(struct image *image);
static int imageCharWidth(struct image *image, const char c);
static const char *saveImage(struct image *image, const char *filename);
static void freeImage(struct image *img);

struct image_driver image_driver =
{
    /* print */         imagePrint,
    /* textwidth */     imageTextWidth,
    /* textheight */    imageTextHeight,
    /* charwidth */     imageCharWidth,
    /* saveimage */     saveImage,
    /* freeimage */     freeImage
};

static struct params params[] = {
{NULL, 0, NULL, NULL}
};

static int initDriver();
static void getImageSize(int *w, int *h);
static void processEvents(int wait, int *mx, int *my, int *mb, int *k);
static void getMouse(int *x, int *y, int *b);
static void uninitDriver();
static void printText(int x, int y, const char *text);
static void redrawImage();
static int allocBuffers (char **b1, char **b2, void **data);
static void freeBuffers(char *b1, char *b2);
static void flipBuffers();
static void setCursorType(int type);

struct ui_driver qt_driver = {
    /* name */          "Qt Driver",
    /* init */          initDriver,
    /* getsize */       getImageSize,
    /* processevents */ processEvents,
    /* getmouse */      getMouse,
    /* uninit */        uninitDriver,
    /* set_color */     NULL,
    /* set_range */     NULL,
    /* print */         printText,
    /* display */       redrawImage,
    /* alloc_buffers */ allocBuffers,
    /* free_buffers */  freeBuffers,
    /* filp_buffers */  flipBuffers,
    /* mousetype */     setCursorType,
    /* flush */         NULL,
    /* textwidth */     12,
    /* textheight */    12,
    /* params */        params,
    /* flags */         PIXELSIZE,
    /* width */         0.025,
    /* height */        0.025,
    /* maxwidth */      0,
    /* maxheight */     0,
    /* imagetype */     UI_TRUECOLOR,
    /* palettestart */  0,
    /* paletteend */    256,
    /* maxentries */    255,
    /* rmask */         0xff0000,
    /* gmask */         0x00ff00,
    /* bmask */         0x0000ff,
    /* gui_driver */    &gui_driver,
    /* image_driver */  &image_driver
};

MainWindow *window;
FractalWidget *widget;

int
main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName("XaoS");
    QCoreApplication::setApplicationVersion(XaoS_VERSION);
    QCoreApplication::setOrganizationName("XaoS Project");
    QCoreApplication::setOrganizationDomain("xaos.sourceforge.net");

    QApplication app(argc, argv);

    QTranslator qtTranslator;
    qtTranslator.load("qt_" + QLocale::system().name(),
            QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    app.installTranslator(&qtTranslator);
    QTranslator xaosTranslator;
    xaosTranslator.load("XaoS_" + QLocale::system().name());
    app.installTranslator(&xaosTranslator);
    return MAIN_FUNCTION(argc, argv);
}

static int
initDriver()
{
    window = new MainWindow();
    widget = window->fractalWidget();
    window->show();

    QScreen *screen = window->windowHandle()->screen();
    qt_driver.width = 2.54 / screen->physicalDotsPerInchX();
    qt_driver.height = 2.54 / screen->physicalDotsPerInchY();
    printf("pixelsize: %fx%f\n", qt_driver.width, qt_driver.height);

    return 1;
}

static void
uninitDriver()
{
    delete window;
}

static int
allocBuffers (char **b1, char **b2, void **data)
{
    widget->createImages();
    *b1 = widget->imageBuffer1();
    *b2 = widget->imageBuffer2();
    *data = widget->imagePointer();
    return widget->imageBytesPerLine();
}

static void
freeBuffers(char *b1, char *b2)
{
    widget->destroyImages();
}

static void
getImageSize(int *w, int *h)
{
    *w = widget->size().width();
    *h = widget->size().height();
}

static void
flipBuffers()
{
    widget->switchActiveImage();
}

static void
redrawImage()
{
    widget->repaint();
}

static void
processEvents(int wait, int *mx, int *my, int *mb, int *k)
{
    QCoreApplication::processEvents(wait ? QEventLoop::WaitForMoreEvents : QEventLoop::AllEvents);

    *mx = widget->mousePosition().x();
    *my = widget->mousePosition().y();
    *mb = widget->mouseButtons();
    *k = widget->keyCombination();
}

static void
getMouse(int *x, int *y, int *b)
{
    *x = widget->mousePosition().x();
    *y = widget->mousePosition().y();
    *b = widget->mouseButtons();
}

static void
printText(int x, int y, const char *text)
{
}

static void
setCursorType(int type)
{
    widget->setCursorType(type);
}

static void
buildMenu(struct uih_context *uih, const char *name)
{
    window->buildMenu(uih, name);
}

static void
popupMenu(struct uih_context *uih, const char *name)
{
    window->popupMenu(uih, name);
}

static void
toggleMenu(struct uih_context *uih, const char *name)
{
    window->toggleMenu(uih, name);
}

static void
showDialog(struct uih_context *c, const char *name)
{
    window->showDialog(c, name);
}

static void
showHelp(struct uih_context *c, const char *name)
{
    QDesktopServices::openUrl(QUrl(HELP_URL));
}

static QFont
getFont() {
    return QFont(QApplication::font().family(), 12);
}

static int
imagePrint(struct image *image, int x, int y,
               const char *text, int fgcolor, int bgcolor, int mode)
{
    char line[BUFSIZ];
    int pos = strcspn(text, "\n");
    strncpy(line, text, pos);
    line[pos] = '\0';

    QImage *qimage = reinterpret_cast<QImage **>(image->data)[image->currimage];
    QFontMetrics metrics(getFont(), qimage);
    QPainter painter(qimage);
    painter.setFont(getFont());

    if (mode == TEXT_PRESSED) {
        painter.setPen(fgcolor);
        painter.drawText(x + 1, y + 1 + metrics.ascent(), line);
    } else {
        painter.setPen(bgcolor);
        painter.drawText(x + 1, y + 1 + metrics.ascent(), line);
        painter.setPen(fgcolor);
        painter.drawText(x, y + metrics.ascent(), line);
    }

    return strlen(line);
}

static int
imageTextWidth(struct image *image, const char *text)
{
    char line[BUFSIZ];
    int pos = strcspn(text, "\n");
    strncpy(line, text, pos);
    line[pos] = '\0';

    QFontMetrics metrics(getFont());
    return metrics.width(line) + 1;
}

static int
imageTextHeight(struct image *image)
{
    QFontMetrics metrics(getFont());
    return metrics.height() + 1;
}

static int
imageCharWidth(struct image *image, const char c)
{
    QFontMetrics metrics(getFont());
    return metrics.width(c);
}

static const char *
saveImage(struct image *image, const char *filename)
{
    QImage *qimage = reinterpret_cast<QImage **>(image->data)[image->currimage];
    qimage->save(filename);
    return NULL;
}

static void
freeImage(struct image *img)
{
    QImage **data = (QImage **)(img->data);
    delete data[0];
    delete data[1];
    delete data;
    free(img);
}

extern "C" {

const struct image *
qt_create_image(int width, int height, struct palette* palette, float pixelwidth, float pixelheight)
{
    QImage **data = new QImage*[2];
    data[0] = new QImage(width, height, QImage::Format_RGB32);
    data[1] = new QImage(width, height, QImage::Format_RGB32);
    struct image* img = create_image_cont(width, height, data[0]->bytesPerLine(), 2, data[0]->bits(), data[1]->bits(), palette, NULL, DRIVERFREE, pixelwidth, pixelheight);
    if (!img) {
        delete data[0];
        delete data[1];
        delete data;
        return NULL;
    }
    img->data = data;
    img->driver = &image_driver;
    return img;
}

const char *
qt_locale()
{
    return QLocale::system().name().toStdString().c_str();
}

const char *
qt_gettext(char *text)
{
    static std::map<char *, char *> strings;
    char *trans = strings[text];
    if (trans == NULL) {
        trans = strdup(QCoreApplication::translate("", text).toStdString().c_str());
        strings[text] = trans;
    }
    return trans;
}

}