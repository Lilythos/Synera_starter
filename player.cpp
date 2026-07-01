#include "player.h"

Player::Player()
{
    reset();
}

void Player::reset()
{
    m_hp = DEFAULT_HP;
    m_gold = DEFAULT_GOLD;
    m_level = DEFAULT_LEVEL;
    m_populationCap = DEFAULT_POPULATION_CAP;
    m_currentStage = DEFAULT_STAGE;
    m_winStreak = 0;
    m_lossStreak = 0;
    m_acquireCounter = 0;
    m_equipmentInventory.clear();
}

void Player::setHp(int hp)
{
    m_hp = hp < 0 ? 0 : hp;
}

void Player::takeDamage(int amount)
{
    if (amount <= 0) {
        return;
    }
    setHp(m_hp - amount);
}

void Player::setGold(int gold)
{
    m_gold = gold < 0 ? 0 : gold;
}

void Player::addGold(int amount)
{
    if (amount <= 0) {
        return;
    }
    m_gold += amount;
}

bool Player::spendGold(int amount)
{
    if (amount <= 0 || amount > m_gold) {
        return false;
    }

    m_gold -= amount;
    return true;
}

void Player::setLevel(int level)
{
    m_level = level < 1 ? 1 : level;
}

void Player::setPopulationCap(int populationCap)
{
    m_populationCap = populationCap < 1 ? 1 : populationCap;
}

void Player::setCurrentStage(int stage)
{
    m_currentStage = stage < 1 ? 1 : stage;
}

void Player::advanceStage()
{
    ++m_currentStage;
}

QList<EquipmentType> Player::equipmentInventory() const
{
    return m_equipmentInventory;
}

bool Player::addEquipment(EquipmentType equipmentType)
{
    if (equipmentType == EquipmentType::None || m_equipmentInventory.size() >= MAX_EQUIPMENT_INVENTORY) {
        return false;
    }

    m_equipmentInventory.append(equipmentType);
    return true;
}

EquipmentType Player::removeEquipmentAt(int index)
{
    //下标小于0/超过装备下标范围
    if (index < 0 || index >= m_equipmentInventory.size()) {
        return EquipmentType::None;
    }

    EquipmentType equipmentType = m_equipmentInventory.at(index);
    m_equipmentInventory.removeAt(index);
    return equipmentType;
}

int Player::equipmentInventorySize() const
{
    return m_equipmentInventory.size();
}

bool Player::hasEquipmentInventorySpace() const
{
    return m_equipmentInventory.size() < MAX_EQUIPMENT_INVENTORY;
}

long long Player::nextAcquiredOrder()
{
    return ++m_acquireCounter;
}

bool Player::upgradePopulation(int cost)
{
    if (cost <= 0) {
        return false;
    }

    if (!spendGold(cost)) {
        return false;
    }

    m_populationCap += 1;
    return true;
}

void Player::recordVictory()
{
    ++m_winStreak;//胜利会累积连胜
    m_lossStreak = 0;//胜利后连败中断
}

void Player::recordDefeat()
{
    ++m_lossStreak;//失败会累积连败
    m_winStreak = 0;//失败后连胜中断
}

void Player::setStreaks(int winStreak, int lossStreak)
{
    m_winStreak = winStreak < 0 ? 0 : winStreak;//连胜不能为负数
    m_lossStreak = lossStreak < 0 ? 0 : lossStreak;//连败不能为负数

    if (m_winStreak > 0 && m_lossStreak > 0) {
        //正常游戏中连胜和连败不会同时存在；读到异常存档时保留更长的一边
        if (m_winStreak >= m_lossStreak) {
            m_lossStreak = 0;
        } else {
            m_winStreak = 0;
        }
    }
}

int Player::currentStreakBonusGold() const
{
    const int streak = m_winStreak > 0 ? m_winStreak : m_lossStreak;//当前生效的连续胜负次数
    if (streak >= 5) {
        return 3;//5 连以上给最高额外金币
    }
    if (streak >= 3) {
        return 2;//3-4 连给中档额外金币
    }
    if (streak >= 2) {
        return 1;//2 连开始给少量额外金币
    }
    return 0;//首胜/首败没有 streak 奖励
}
