#ifndef QPAINTERDRAWABLE_H
#define QPAINTERDRAWABLE_H

#include <QWidget>
#include <QMutex>
#include "mediabase.h"
#include "imagescale.h"
using namespace LQF;
namespace Ui {
class QPainterDrawable;
}
#include "framequeue.h"
class QPainterDrawable : public QWidget
{
    Q_OBJECT

public:
    explicit QPainterDrawable(QWidget *parent = 0);
    ~QPainterDrawable();
    int draw(const Frame *newVideoFrame, bool, bool);
private:
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *) override;
private:
    Ui::QPainterDrawable *ui;

    int x_ = 0; //  起始位置
    int y_ = 0;
    int video_width = 0;
    int video_height = 0;
    int img_width = 0;
    int img_height = 0;
    QImage img;
    VideoFrame dst_video_frame_;
    QMutex m_mutex;
    ImageScaler *img_scaler_ = NULL;
};

#endif // QPAINTERDRAWABLE_H
