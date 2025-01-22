#pragma once

#include <QString>
#include <QTextStream>
#include <QVector2D>
#include <vector>
#include "Line.h"
#include <QColor>

class DxfHandler {
public:
    static bool saveDxf(const QString& filename, const std::vector<Line>& lines);
    static bool loadDxf(const QString& filename, std::vector<Line>& lines);

private:
    static int qColorToAcadColor(const QColor& color);
    static QColor acadColorToQColor(int colorNumber);
};
