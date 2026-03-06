#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QPainter>
#include <QtCore/QTimer>

// This include gives us the "ViewMatrix" definition. 
// Do NOT define the struct again in this file.
#include "CS2/GameInformationHandler.h" 

class Overlay : public QWidget {
    Q_OBJECT

public:
    explicit Overlay(GameInformationhandler* handler, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    GameInformationhandler* m_handler;
    QTimer* m_timer;

    bool worldToScreen(const Vec3D<float>& worldPos, Vec2D<float>& screenPos, const ViewMatrix& viewMatrix);
};