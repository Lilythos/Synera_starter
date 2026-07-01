#include "unit.h"
#include <cmath>
#include <algorithm>

int Unit::s_nextId = 0;

Unit::Unit(const QString& name
           , int maxHp
           , int atk
           , int range
           , int maxMana
           , Owner owner
           , const QStringList& traits
           , int star)
    : m_id(s_nextId++)
    , m_name(name)
    , m_position(0, 0)
    , m_maxHp(maxHp > 0 ? maxHp : 1)
    , m_hp(m_maxHp)
    , m_atk(atk >= 0 ? atk : 0)
    , m_range(range > 0 ? range : 1)
    , m_maxMana(maxMana >= 0 ? maxMana : 0)
    , m_mana(0)
    , m_attackIntervalMs(1000)
    , m_moveIntervalMs(300)
    , m_attackElapsedMs(0)
    , m_moveElapsedMs(0)
    , m_owner(owner)
    , m_traits(traits)
    , m_state(State::Idle)
    , m_star(star > 0 ? star : 1)
    , m_lastAction(ActionType::None)
    , m_equipment(EquipmentType::None)
    , m_synergyAtkBonus(0)
    , m_synergyAttackIntervalReductionMs(0)
    , m_synergyDeathHealBonus(0)
{}
void Unit::setMaxHp(int maxHpValue)
{
    m_maxHp = maxHpValue > 0 ? maxHpValue : 1;
    if (m_hp > maxHp()) {
        m_hp = maxHp();
    }
    if (m_hp <= 0) {
        m_state = State::Dead;
    }
}
int Unit::maxHp() const
{
    return std::max(1, m_maxHp + equipmentInfo(m_equipment).maxHpBonus);
}
int Unit::atk() const
{
    return std::max(0, m_atk + equipmentInfo(m_equipment).atkBonus + m_synergyAtkBonus);
}
int Unit::maxMana() const
{
    return std::max(0, m_maxMana + equipmentInfo(m_equipment).maxManaBonus);
}
int Unit::attackIntervalMs() const
{
    const EquipmentInfo& info = equipmentInfo(m_equipment);
    int interval = m_attackIntervalMs - info.attackIntervalReductionMs - m_synergyAttackIntervalReductionMs;
    interval = std::max(1, interval);

    if (info.attackSpeedPercentBonus > 0) {
        interval = interval * 100 / (100 + info.attackSpeedPercentBonus);//攻速加成
    }

    return std::max(1, interval);   
}

void Unit::setHp(int hp)
{
    if (hp <= 0) {
        m_hp = 0;
        m_state = State::Dead;
        return;
    }

    m_hp = hp > maxHp() ? maxHp() : hp;
    if (m_state == State::Dead) {
        m_state = State::Idle;
    }
}
void Unit::setAtk(int atk)
{
    m_atk = atk >= 0 ? atk : 0;
}
void Unit::setRange(int range)
{
    m_range = range > 0 ? range : 1;
}
void Unit::setMaxMana(int maxManaValue)
{
    m_maxMana = maxManaValue >= 0 ? maxManaValue : 0;
    if (m_mana > maxMana()) {
        m_mana = maxMana();
    }
}

void Unit::setMana(int mana)
{
    if (mana <= 0) {
        m_mana = 0;
        return;
    }

    m_mana = mana > maxMana() ? maxMana() : mana;
}

void Unit::setAttackIntervalMs(int attackIntervalMs)
{
    m_attackIntervalMs = attackIntervalMs > 0 ? attackIntervalMs : 1;
}

void Unit::setMoveIntervalMs(int moveIntervalMs)
{
    m_moveIntervalMs = moveIntervalMs > 0 ? moveIntervalMs : 1;
}

void Unit::setAttackElapsedMs(int value)
{
    m_attackElapsedMs = value > 0 ? value : 0;
}

void Unit::setMoveElapsedMs(int value)
{
    m_moveElapsedMs = value > 0 ? value : 0;
}

void Unit::setStar(int star)
{
    m_star = star > 0 ? star : 1;
}

void Unit::promoteStar()
{
    m_star = m_star > 0 ? m_star + 1 : 1;

    const double mult = 1.5;//倍率设为1.5
    const int newMaxHp = static_cast<int>(std::round(m_maxHp * mult));//强制转化为整型
    const int newAtk = static_cast<int>(std::round(m_atk * mult));

    setMaxHp(newMaxHp);
    setAtk(newAtk);
    setHp(maxHp());
}

void Unit::heal(int amount)//传要治疗的血量
{
    if (amount <= 0 || !isAlive()) {
        return;
    }

    setHp(m_hp + amount);
}

void Unit::takeDamage(int amount)
{
    if (amount <= 0) {
        return;
    }
    setHp(m_hp - amount);
}

int Unit::basicAttackDamage()
{
    return m_atk;
}

bool Unit::castSkill(Board& board, const QVector<Unit*>& units)
{
    (void)board;//为防止声明参数但不使用的警告
    (void)units;
    return false;
}

EquipmentType Unit::equip(EquipmentType equipmentType)//穿戴/替换当前装备
{
    if (equipmentType == EquipmentType::None) {
        return m_equipment;
    }

    EquipmentType previous = m_equipment;
    m_equipment = equipmentType;

    //装备效果
    if (m_hp > maxHp()) {
        m_hp = maxHp();
    }
    if (m_mana > maxMana()) {
        m_mana = maxMana();
    }
    return previous;//只能穿一个装备，返回原来的
}

EquipmentType Unit::unequip()//卸下当前装备
{
    EquipmentType previous = m_equipment;
    m_equipment = EquipmentType::None;
    if (m_hp > maxHp()) {
        m_hp = maxHp();
    }
    if (m_mana > maxMana()) {
        m_mana = maxMana();
    }
    return previous;
}

void Unit::resetSynergyBonuses()
{
    m_synergyAtkBonus = 0;
    m_synergyAttackIntervalReductionMs = 0;
    m_synergyDeathHealBonus = 0;
}

void Unit::addSynergyAtkBonus(int bonus)
{
    m_synergyAtkBonus += bonus;
}

void Unit::addSynergyAttackIntervalReductionMs(int reductionMs)
{
    m_synergyAttackIntervalReductionMs += reductionMs;
}

void Unit::addSynergyDeathHealBonus(int bonus)
{
    m_synergyDeathHealBonus += bonus;
}

void Unit::onAfterBasicAttack(Unit* target, const QVector<Unit*>& units)
{
    (void)target;
    (void)units;
}

void Unit::onAfterMoveStep(Board& board, const QVector<Unit*>& units)
{
    (void)board;
    (void)units;
}

bool Unit::onBeforeCombatAction(Board& board, const QVector<Unit*>& units)
{
    (void)board;
    (void)units;
    return false;
}

void Unit::onDeath(Board& board, const QVector<Unit*>& units)
{
    (void)board;
    (void)units;
}

void Unit::gainMana(int amount)
{
    if (amount <= 0) {
        return;
    }
    setMana(m_mana + amount);
}

void Unit::advanceCombatTimers(int deltaMs)
{
    if (deltaMs <= 0 || !isAlive()) {
        return;
    }

    m_attackElapsedMs += deltaMs;
    m_moveElapsedMs += deltaMs;
}

bool Unit::canAttackNow() const
{
    return m_attackElapsedMs >= attackIntervalMs() && isAlive();
}

bool Unit::canMoveNow() const
{
    return m_moveElapsedMs >= m_moveIntervalMs && isAlive();
}

void Unit::consumeAttackCooldown()
{
    m_attackElapsedMs = 0;
}

void Unit::consumeMoveCooldown()
{
    m_moveElapsedMs = 0;
}

void Unit::resetCombatTimers()
{
    m_attackElapsedMs = 0;
    m_moveElapsedMs = 0;
}
