#include "DxfHandler.h"
#include <fstream>
#include <iomanip>

bool DxfHandler::saveDxf(const QString& filename, const std::vector<Line>& lines) {
    std::ofstream file(filename.toStdString());
    if (!file) return false;

    // Write header
    file << "0\nSECTION\n2\nHEADER\n0\nENDSEC\n";
    file << "0\nSECTION\n2\nENTITIES\n";

    // Write lines
    for (const Line& line : lines) {
        file << "0\nLINE\n";
        file << "8\n0\n";  // Layer 0
        file << "62\n" << DxfHandler::qColorToAcadColor(line.color) << "\n";  // Color number
        file << "10\n" << std::fixed << std::setprecision(6) << line.start.x() << "\n";
        file << "20\n" << line.start.y() << "\n";
        file << "30\n0.0\n";
        file << "11\n" << line.end.x() << "\n";
        file << "21\n" << line.end.y() << "\n";
        file << "31\n0.0\n";
    }

    // Write footer
    file << "0\nENDSEC\n0\nEOF\n";
    return true;
}

bool DxfHandler::loadDxf(const QString& filename, std::vector<Line>& lines) {
    std::ifstream file(filename.toStdString());
    if (!file) return false;

    lines.clear();
    std::string line;
    float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    int colorNum = 7;  // Default white

    while (std::getline(file, line)) {
        if (line == "0") {
            std::getline(file, line);
            if (line == "LINE") {
                // Reset values for new line
                x1 = y1 = x2 = y2 = 0;
                colorNum = 7;
                
                // Read all line parameters
                std::string code;
                while (std::getline(file, code)) {
                    if (code == "0") break;  // End of entity
                    
                    std::string value;
                    std::getline(file, value);
                    
                    try {
                        if (code == "10") x1 = std::stof(value);
                        else if (code == "20") y1 = std::stof(value);
                        else if (code == "11") x2 = std::stof(value);
                        else if (code == "21") y2 = std::stof(value);
                        else if (code == "62") colorNum = std::stoi(value);
                    } catch (...) {
                        // Handle conversion errors
                        continue;
                    }
                }
                
                // Create line with correct color
                QColor color = DxfHandler::acadColorToQColor(colorNum);
                lines.push_back(Line(QVector2D(x1, y1), QVector2D(x2, y2), color));

                // If we broke out due to new entity, push the "0" back
                if (code == "0") {
                    line = code;
                    continue;
                }
            }
        }
    }

    return true;
}

int DxfHandler::qColorToAcadColor(const QColor& color) {
    // Extended AutoCAD color mapping
    if (color == Qt::red) return 1;
    if (color == Qt::yellow) return 2;
    if (color == Qt::green) return 3;
    if (color == Qt::cyan) return 4;
    if (color == Qt::blue) return 5;
    if (color == Qt::magenta) return 6;
    if (color == Qt::white) return 7;
    if (color == Qt::darkGray) return 8;
    if (color == Qt::lightGray) return 9;
    
    // Calculate closest AutoCAD color index based on RGB values
    int r = color.red();
    int g = color.green();
    int b = color.blue();
    
    // Basic AutoCAD color index calculation
    if (r > 200 && g < 50 && b < 50) return 1;  // Red
    if (r > 200 && g > 200 && b < 50) return 2;  // Yellow
    if (r < 50 && g > 200 && b < 50) return 3;  // Green
    if (r < 50 && g > 200 && b > 200) return 4;  // Cyan
    if (r < 50 && g < 50 && b > 200) return 5;  // Blue
    if (r > 200 && g < 50 && b > 200) return 6;  // Magenta
    
    return 7;  // Default to white
}

QColor DxfHandler::acadColorToQColor(int colorNumber) {
    // Extended AutoCAD color mapping
    switch (colorNumber) {
        case 1: return Qt::red;
        case 2: return Qt::yellow;
        case 3: return Qt::green;
        case 4: return Qt::cyan;
        case 5: return Qt::blue;
        case 6: return Qt::magenta;
        case 7: return Qt::white;
        case 8: return Qt::darkGray;
        case 9: return Qt::lightGray;
        default: return Qt::white;
    }
}
