#ifndef SHOP_H
#define SHOP_H

#include <QList>
#include <QVector>
#include "entity/unit.h"
#include "core/player.h"
#include "core/bench.h"
#include "core/hero_factory.h"

class Shop
{
public:
    static constexpr int SLOT_COUNT = 5;
    static constexpr int REFRESH_COST = 2;
    static constexpr int PURCHASE_COST = 3;

    Shop();
    ~Shop();

    bool refresh(Player& player, const HeroFactory& factory, QList<Unit*>& ownedUnits, bool chargeGold = true);
    bool purchase(int slotIndex, Player& player, Bench& bench);
    bool placeLoadedUnitAt(Unit* unit, int slotIndex);
    
    Unit* getUnitAt(int slotIndex) const;
    QVector<Unit*> units() const;
    bool isFull() const;
    bool isEmpty() const;
    int occupiedSlotCount() const;

    void clear(QList<Unit*>& ownedUnits);

private:
    bool isValidIndex(int slotIndex) const;
    bool clearSlot(int slotIndex, QList<Unit*>& ownedUnits);

    QVector<Unit*> m_slots;
};

#endif // SHOP_H
