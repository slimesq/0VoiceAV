#ifndef IJKPLAYER_QT_H
#define IJKPLAYER_QT_H
#include <QObject>


class IjkPlayerQt: public QObject
{
    Q_OBJECT

public:
    explicit IjkPlayerQt(QObject *parent = nullptr);
};

#endif // IJKPLAYER_QT_H
