#ifndef CORE_SAVE_LOAD_H
#define CORE_SAVE_LOAD_H

#include <QString>
#include "core/game_state.h"

class Player;
class Board;
class Bench;
class Shop;
class Unit;

namespace SaveLoad {

bool saveGameState(const Player& player,
				   const Board& board,
				   const Bench& bench,
				   const Shop& shop,
				   const QList<Unit*>& units,
				   GamePhase phase,
				   int stage,
				   const QString& path);

bool loadGameState(Player& player,
				   Board& board,
				   Bench& bench,
				   Shop& shop,
				   QList<Unit*>& ownedUnits,
				   GamePhase& outPhase,
				   int& outStage,
				   const QString& path);

} // namespace SaveLoad

#endif // CORE_SAVE_LOAD_H
