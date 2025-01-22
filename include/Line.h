#pragma once

#ifndef LINE_H
#define LINE_H

#include <QVector2D>
#include <QColor>

struct Line {
    QVector2D start;
    QVector2D end;
    QColor color;  // Add color property

    Line(const QVector2D& s, const QVector2D& e, const QColor& c = QColor(255, 255, 255)) 
        : start(s), end(e), color(c) {}
};

#endif // LINE_H