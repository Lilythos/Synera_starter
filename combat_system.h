#ifndef COMBAT_SYSTEM_H
#define COMBAT_SYSTEM_H

#include "core/board.h"
#include "entity/unit.h"

class CombatSystem
{
public:
    static constexpr int MANA_GAIN_PER_ATTACK = 10;

    static bool isInAttackRange(Unit* attacker, Unit* target);
    static bool canBasicAttack(Unit* attacker, Unit* target);
    static bool basicAttack(Unit* attacker, Unit* target, Board* board = nullptr);
    static bool updateUnit(Unit* unit, Board& board, int deltaMs);
    static void updateBoard(Board& board, int deltaMs);
};

#endif // COMBAT_SYSTEM_H