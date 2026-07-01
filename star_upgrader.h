#ifndef STAR_UPGRADER_H
#define STAR_UPGRADER_H

#include <QList>

class Bench;
class Board;
class Unit;
class Player;

namespace StarUpgrader {
    void processStarUpgrades(Bench& bench, Board& board, QList<Unit*>& allUnits, Player& player);
}

#endif // STAR_UPGRADER_H
