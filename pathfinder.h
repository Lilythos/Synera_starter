#ifndef PATHFINDER_H
#define PATHFINDER_H

#include <QPoint>
#include <QVector>
#include "board.h"

class Pathfinder
{
public:
    static QVector<QPoint> findPath(const Board& board,
                                    const QPoint& start,
                                    const QPoint& goal);

    static bool moveUnitOneStep(Board& board,
                                Unit* unit,
                                const QPoint& goal);
};

#endif // PATHFINDER_H