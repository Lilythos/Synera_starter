#ifndef BOARD_H
#define BOARD_H

#include <QHash>
#include <QPoint>
#include <QVector>
#include "entity/unit.h"

class Board
{
public:
    struct ActionRecord {
        Unit::ActionType type = Unit::ActionType::None;
        int amount = 0;
        QPoint targetPosition = QPoint(-1, -1);
    };
    static constexpr int ROWS = 8;
    static constexpr int COLS = 8;

    Board();
    ~Board() = default;

    void addUnit(Unit* unit, const QPoint& pos);
    void removeUnit(Unit* unit);
    Unit* getUnitAt(const QPoint& pos) const;
    bool hasUnitAt(const QPoint& pos) const;

    bool containsUnit(Unit* unit) const;
    QPoint positionOf(Unit* unit) const;
    bool moveUnit(Unit* unit, const QPoint& target);
    bool swapUnits(Unit* first, Unit* second);
    bool swapUnitsAt(const QPoint& firstPos, const QPoint& secondPos);
    QVector<Unit*> units() const;
    Unit::ActionType lastActionAt(const QPoint& pos) const;
    void recordAction(const QPoint& pos, Unit::ActionType action);
    ActionRecord lastActionRecordAt(const QPoint& pos) const;
    void recordAction(const QPoint& pos, Unit::ActionType action, int amount, const QPoint& targetPosition);
    QHash<QPoint, ActionRecord> actionRecords() const;
    void restoreActionRecord(const QPoint& pos, const ActionRecord& record);
    bool isValidPosition(const QPoint& pos) const;
    bool isPlayerHalf(const QPoint& pos) const;

    void clear();

private:
    int indexOf(const QPoint& pos) const;

    QVector<Unit*> m_cells;
    QHash<Unit*, QPoint> m_unitToPosition;
    QHash<QPoint, ActionRecord> m_lastActions;
};

#endif // BOARD_H