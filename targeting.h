#ifndef TARGETING_H
#define TARGETING_H

#include <QVector>
#include "board.h"

class Targeting
{
public:
    static Unit* selectTarget(Unit* attacker, const Board& board);
    static Unit* selectTarget(Unit* attacker, const QVector<Unit*>& units);
};

#endif // TARGETING_H