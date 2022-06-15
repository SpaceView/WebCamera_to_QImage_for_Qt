#ifndef WEBCAME_H
#define WEBCAME_H

#include <QObject>
#include <QAbstractVideoSurface>
#include <QCamera>
#include <QCameraInfo>
#include <QVideoFrame>
#include <QMetaEnum>
#include <QImage>

#include <QDebug>

struct CameraInfo
{
    QString      identify;    /* Identify of device (maybe a device path) */
    QString      description; /* For display */
    QList<QSize> resolutionList;
    QStringList  formatList;
};

enum FormatType {
    MJPEG,
    YUYV
};

struct Format {
    int w;
    int h;
    FormatType imageFormat;
    int frameRate;
};


static QImage imageFromVideoFrame(const QVideoFrame& buffer)
{
    QImage img;
    QVideoFrame frame(buffer);  // make a copy we can call map (non-const) on
    frame.map(QAbstractVideoBuffer::ReadOnly);
    QImage::Format imageFormat = QVideoFrame::imageFormatFromPixelFormat(
                frame.pixelFormat());
    // BUT the frame.pixelFormat() is QVideoFrame::Format_Jpeg, and this is
    // mapped to QImage::Format_Invalid by
    // QVideoFrame::imageFormatFromPixelFormat
    if (imageFormat != QImage::Format_Invalid) {
        img = QImage(frame.bits(),
                     frame.width(),
                     frame.height(),
                     // frame.bytesPerLine(),
                     imageFormat);
    } else {
        // e.g. JPEG
        int nbytes = frame.mappedBytes();
        img = QImage::fromData(frame.bits(), nbytes);
    }
    frame.unmap();
    return img;
}

class WebCameraCapture : public QAbstractVideoSurface
{
    Q_OBJECT
public:
    enum PixelFormat {
        Format_Invalid,
        Format_ARGB32,
        Format_ARGB32_Premultiplied,
        Format_RGB32,
        Format_RGB24,
        Format_RGB565,
        Format_RGB555,
        Format_ARGB8565_Premultiplied,
        Format_BGRA32,
        Format_BGRA32_Premultiplied,
        Format_BGR32,
        Format_BGR24,
        Format_BGR565,
        Format_BGR555,
        Format_BGRA5658_Premultiplied,

        Format_AYUV444,
        Format_AYUV444_Premultiplied,
        Format_YUV444,
        Format_YUV420P,
        Format_YV12,
        Format_UYVY,
        Format_YUYV,
        Format_NV12,
        Format_NV21,
        Format_IMC1,
        Format_IMC2,
        Format_IMC3,
        Format_IMC4,
        Format_Y8,
        Format_Y16,

        Format_Jpeg,

        Format_CameraRaw,
        Format_AdobeDng,

#ifndef Q_QDOC
        NPixelFormats,
#endif
        Format_User = 1000
    };

    Q_ENUM(PixelFormat)

    explicit WebCameraCapture(QObject *parent = 0) : QAbstractVideoSurface(parent)
    {
    }

    QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType = QAbstractVideoBuffer::NoHandle) const override
    {
        Q_UNUSED(handleType);
        return QList<QVideoFrame::PixelFormat>()
                << QVideoFrame::Format_ARGB32
                << QVideoFrame::Format_ARGB32_Premultiplied
                << QVideoFrame::Format_RGB32
                << QVideoFrame::Format_RGB24
                << QVideoFrame::Format_RGB565
                << QVideoFrame::Format_RGB555
                << QVideoFrame::Format_ARGB8565_Premultiplied
                << QVideoFrame::Format_BGRA32
                << QVideoFrame::Format_BGRA32_Premultiplied
                << QVideoFrame::Format_BGR32
                << QVideoFrame::Format_BGR24
                << QVideoFrame::Format_BGR565
                << QVideoFrame::Format_BGR555
                << QVideoFrame::Format_BGRA5658_Premultiplied
                << QVideoFrame::Format_AYUV444
                << QVideoFrame::Format_AYUV444_Premultiplied
                << QVideoFrame::Format_YUV444
                << QVideoFrame::Format_YUV420P
                << QVideoFrame::Format_YV12
                << QVideoFrame::Format_UYVY
                << QVideoFrame::Format_YUYV
                << QVideoFrame::Format_NV12
                << QVideoFrame::Format_NV21
                << QVideoFrame::Format_IMC1
                << QVideoFrame::Format_IMC2
                << QVideoFrame::Format_IMC3
                << QVideoFrame::Format_IMC4
                << QVideoFrame::Format_Y8
                << QVideoFrame::Format_Y16
                << QVideoFrame::Format_Jpeg
                << QVideoFrame::Format_CameraRaw
                << QVideoFrame::Format_AdobeDng;
    }

    bool present(const QVideoFrame &frame) override
    {
        if (frame.isValid()) {
            /*
            //NOTE: for version > 5.15.x;
            QImage image = frame.image(); // This function was introduced in Qt 5.15.
            QImage::Format ifmt = image.format();
            qDebug() << ifmt;
            emit frameAvailable(image);
            */

            QImage image = imageFromVideoFrame(frame);
            emit frameAvailable(image);

            return true;
        }
        return false;
    }

signals:
    void frameAvailable(QImage& image);

};


class WebCamera : public QObject
{
    Q_OBJECT
public:
    explicit WebCamera(QCameraInfo cameraInfo = QCameraInfo::defaultCamera(), QObject *parent = 0) : QObject(parent)
    {
        m_started = false;
        m_camera = NULL;
        m_cameraCapture = NULL;

        if (cameraInfo.isNull()) {
            qDebug() << __LINE__ << __FUNCTION__;
            return;
        }
        m_cameraDeviceInfo = cameraInfo;

        m_cameraFormat.w = 640;
        m_cameraFormat.h = 480;
        m_cameraFormat.frameRate = 15;
        m_cameraFormat.imageFormat = MJPEG;

        m_cameraCapture = new WebCameraCapture;
        connect(m_cameraCapture, SIGNAL(frameAvailable(QImage&)), this, SLOT(grabImage(QImage&)));

        m_cameraPixelFormatEnum = QMetaEnum::fromType<WebCameraCapture::PixelFormat>();
    }

    ~WebCamera()
    {
        if (m_camera != NULL) {
            delete m_camera;
            m_camera = NULL;
        }

        if (m_cameraCapture != NULL) {
            delete m_cameraCapture;
            m_cameraCapture = NULL;
        }
    }

    static QList<CameraInfo> queryCameraDevices()
    {
        QList<CameraInfo> result;
        QList<QCameraInfo> infos = QCameraInfo::availableCameras();

        foreach (QCameraInfo each, infos) {
            CameraInfo info;
            info.identify = each.deviceName();
            info.description = each.description();

            result.append(info);
        }
        return result;
    }

    bool selectCamera(const QString &cameraIdentify)
    {
        bool restart = false;

        if (isStarted()) {
            CloseCamera();
            restart = true;
        }

        if (m_camera) {
            delete m_camera;
            m_camera = NULL;
        }

        m_cameraDeviceInfo = QCameraInfo(cameraIdentify.toLatin1());
        m_camera = new QCamera(m_cameraDeviceInfo);
        m_camera->setViewfinder(m_cameraCapture);

        /* Set the same format for new device */
        setCameraFormat(m_cameraFormat);

        if (restart) {
            OpenCamera();
        }
        return true;
    }

    CameraInfo queryCameraInfo()
    {
        queryCaps();
        return m_cameraInfo;
    }

    Format getCameraFormat()
    {
        QCameraViewfinderSettings set;
        set = m_camera->viewfinderSettings();

        m_cameraFormat.w = set.resolution().width();
        m_cameraFormat.h = set.resolution().height();
        m_cameraFormat.frameRate = set.minimumFrameRate();

        switch (set.pixelFormat()) {
        case QVideoFrame::Format_Jpeg:
            m_cameraFormat.imageFormat = MJPEG;
            break;

        case QVideoFrame::Format_YUYV:
            m_cameraFormat.imageFormat = YUYV;
            break;

        default:
            break;
        }

        return m_cameraFormat;
    }

    bool setCameraFormat(Format &format)
    {
        m_cameraFormat = format;

        QCameraViewfinderSettings set;

        switch (format.imageFormat) {
        case MJPEG:
            set.setPixelFormat(QVideoFrame::Format_Jpeg);
            break;

        case YUYV:
            set.setPixelFormat(QVideoFrame::Format_YUYV);
            break;
        }
        set.setResolution(m_cameraFormat.w, m_cameraFormat.h);

        //QCamera can change formats dynamically,
        //    it will use default format when unsupported format is set.
        m_camera->setViewfinderSettings(set);

        return true;
    }

    bool OpenCamera(int index = 0)
    {
        Q_UNUSED(index) // for compatibility
        if (m_started) {
            return false;
        }
        else {
            if (m_cameraDeviceInfo.isNull()) {
                return false;
            }
            else {
                if(NULL==m_camera){
                    m_camera = new QCamera(m_cameraDeviceInfo);
                }
                m_camera->setViewfinder(m_cameraCapture);
                m_camera->start();
                m_started = true;
            }

        }

        return true;
    }

    bool CloseCamera()
    {
        m_camera->stop();
        m_started = false;
        return true;
    }

    bool isStarted()
    {
        return m_started;
    }

    bool capture(QString filePath)
    {
        if (! m_started) {
            return false;
        }
        return m_image.save(filePath, "jpg");
    }

private:
    void queryCaps()
    {
        QList<QVideoFrame::PixelFormat> cameraFormat;

        m_cameraInfo.resolutionList = m_camera->supportedViewfinderResolutions();
        cameraFormat = m_camera->supportedViewfinderPixelFormats();

        m_cameraInfo.formatList.clear();
        for (int i = 0; i < cameraFormat.length(); i++) {
            m_cameraInfo.formatList.append(m_cameraPixelFormatEnum.valueToKey(cameraFormat.at(i)));
        }
    }

private slots:
    void grabImage(QImage& image)
    {
        //qDebug() << image.isNull();
        //m_image = image.mirrored(false, true);
        m_image = image;

        emit onImageOutput(image);
    }

signals:
    void onImageOutput(QImage& image);


private:
    QCamera *m_camera = NULL;
    WebCameraCapture *m_cameraCapture;

    QCameraInfo   m_cameraDeviceInfo;
    CameraInfo    m_cameraInfo;
    Format        m_cameraFormat;

    bool      m_started;
    QImage    m_image;
    QMetaEnum m_cameraPixelFormatEnum;
};

#endif // WEBCAME_H
