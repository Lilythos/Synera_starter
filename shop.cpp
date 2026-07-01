#include "shop.h"

Shop::Shop()
    : m_slots(SLOT_COUNT, nullptr)
{
}

Shop::~Shop() = default;

bool Shop::isValidIndex(int slotIndex) const
{//判断槛位下标是否在合法范围内
    return slotIndex >= 0 && slotIndex < m_slots.size();
}

bool Shop::isFull() const
{//判断商店所有槛位是否都已被占用
    return occupiedSlotCount() >= SLOT_COUNT;
}

bool Shop::isEmpty() const
{//判断商店是否完全没有单位
    return occupiedSlotCount() == 0;
}

int Shop::occupiedSlotCount() const
{//统计当前商店中已被占用的槛位数量
    int count = 0;
    for (Unit* unit : m_slots) {
        if (unit) {
            ++count;
        }
    }
    return count;
}

Unit* Shop::getUnitAt(int slotIndex) const
{//获取指定槛位上的单位
    return isValidIndex(slotIndex) ? m_slots.at(slotIndex) : nullptr;
}

QVector<Unit*> Shop::units() const
{//获取商店所有槛位的单位列表
    return m_slots;
}

bool Shop::clearSlot(int slotIndex, QList<Unit*>& ownedUnits)
{//清空指定槛位上的单位，并将其从已拥有单位列表中移除并释放
    if (!isValidIndex(slotIndex) || !m_slots[slotIndex]) {
        return false;
    }

    Unit* unit = m_slots[slotIndex];//该槛位上的单位
    m_slots[slotIndex] = nullptr;//清空该槛位
    ownedUnits.removeOne(unit);//从已拥有单位列表中移除
    delete unit;//释放该单位的内存
    return true;
}

void Shop::clear(QList<Unit*>& ownedUnits)
{//清空商店所有槛位上的单位
    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i]) {
            //该槛位存在单位，将其从已拥有单位列表中移除并释放
            ownedUnits.removeOne(m_slots[i]);
            delete m_slots[i];
            m_slots[i] = nullptr;
        }
    }
}

bool Shop::refresh(Player& player, const HeroFactory& factory, QList<Unit*>& ownedUnits, bool chargeGold)
{//刷新商店货架，重新生成所有槛位的单位
    if (chargeGold && !player.spendGold(REFRESH_COST)) {
        //需要收取刷新费用但玩家金币不足
        return false;
    }

    clear(ownedUnits);//先清空商店原有的单位

    for (int i = 0; i < SLOT_COUNT; ++i) {
        Unit* hero = (i < 2) ? factory.createRandomCombatHero() : factory.createRandomHero();//前两个槛位生成随机战斗英雄，其余槛位生成普通随机英雄
        hero->setAcquiredOrder(player.nextAcquiredOrder());//记录该单位的获取顺序
        m_slots[i] = hero;//将生成的单位放入对应槛位
        ownedUnits.append(hero);//加入已拥有单位列表
    }

    return true;
}

bool Shop::placeLoadedUnitAt(Unit* unit, int slotIndex)
{//读档时将还原好的单位放回商店指定槛位
    if (!unit || !isValidIndex(slotIndex) || m_slots[slotIndex]) {
        //单位为空、下标不合法，或该槛位已被占用
        return false;
    }

    for (Unit* existing : m_slots) {
        if (existing == unit) {
            //该单位已经存在于商店其他槛位中，避免重复放置
            return false;
        }
    }

    m_slots[slotIndex] = unit;//将单位放入指定槛位
    unit->setPosition(QPoint(-1, -1));//商店中的单位不在棋盘上，位置设为无效坐标
    return true;
}

bool Shop::purchase(int slotIndex, Player& player, Bench& bench)
{//玩家购买商店指定槛位的单位并放入备战席
    if (!isValidIndex(slotIndex) || !m_slots[slotIndex]) {
        return false;
    }

    if (bench.isFull()) {
        return false;
    }

    if (!player.spendGold(PURCHASE_COST)) {
        return false;
    }

    Unit* unit = m_slots[slotIndex];//该槛位上待购买的单位
    m_slots[slotIndex] = nullptr;//先清空该槛位

    if (!bench.addUnit(unit)) {
        //加入备战席失败
        player.addGold(PURCHASE_COST);
        m_slots[slotIndex] = unit;
        return false;
    }

    unit->setAcquiredOrder(player.nextAcquiredOrder());//记录该单位被购买时的获取顺序

    return true;
}