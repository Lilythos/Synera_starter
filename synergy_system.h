#ifndef CORE_SYNERGY_SYSTEM_H
#define CORE_SYNERGY_SYSTEM_H

#include "core/board.h"
#include <QStringList>

class SynergySystem
{
public:
    static void refreshSynergyBuffs(Board& board);
    static void applyPreCombatEffects(Board& board);
    static QStringList activeSynergyDescriptions(const Board& board);
};

#endif // CORE_SYNERGY_SYSTEM_H
