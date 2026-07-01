#ifndef BENCH_H
#define BENCH_H

#include <QVector>
#include "entity/unit.h"

class Bench
{
public:
    static constexpr int SIZE = 8;

    Bench();

    int size() const { return m_slots.size(); }
    bool isValidIndex(int index) const;
    bool isFull() const;
    bool isEmpty() const;
    int emptySlotCount() const;

    Unit* getUnitAt(int index) const;
    bool hasUnitAt(int index) const;
    bool containsUnit(Unit* unit) const;
    int indexOf(Unit* unit) const;
    QVector<Unit*> units() const;

    bool addUnit(Unit* unit);
    bool addUnitAt(Unit* unit, int index);
    bool moveUnit(int fromIndex, int toIndex);
    bool swapSlots(int firstIndex, int secondIndex);
    Unit* removeUnitAt(int index);
    bool removeUnit(Unit* unit);

    void clear();

private:
    QVector<Unit*> m_slots;
};

#endif // BENCH_H