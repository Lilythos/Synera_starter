#ifndef UNIT_H
#define UNIT_H

#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVector>
#include "core/equipment.h"

class Board;

class Unit
{
public:
    enum class Owner {
        PlayerCtrl,
        EnemyCtrl
    };

    enum class State {
        Idle,
        Moving,
        Attacking,
        Casting,
        Dead
    };

    enum class ActionType {//这里主要是给模仿者使用
        None,
        Attack,
        Heal,
        DeathHeal
    };

    explicit Unit(const QString& name = QString("Unit"),
                  int maxHp = 300,
                  int atk = 30,
                  int range = 1,
                  int maxMana = 60,
                  Owner owner = Owner::PlayerCtrl,
                  const QStringList& traits = QStringList(),
                  int star = 1);
 
    virtual ~Unit() = default;//还需要对子类析构，故此处使用虚函数

    int id() const { return m_id; }
    QString name() const { return m_name; }
    QPoint position() const { return m_position; }

    int maxHp() const;//返回最大生命值，注意装备和羁绊会影响
    int hp() const { return m_hp; }
    int atk() const;
    int range() const { return m_range; }
    int maxMana() const;
    int mana() const { return m_mana; }
    int attackIntervalMs() const;
    int moveIntervalMs() const { return m_moveIntervalMs; }//移动间隔

    //下面返回不包含装备和羁绊的值，便于存档和读档时使用
    int baseMaxHp() const { return m_maxHp; }
    int baseAtk() const { return m_atk; }
    int baseMaxMana() const { return m_maxMana; }
    int baseAttackIntervalMs() const { return m_attackIntervalMs; }
    int baseMoveIntervalMs() const { return m_moveIntervalMs; }
    int attackElapsedMs() const { return m_attackElapsedMs; }
    int moveElapsedMs() const { return m_moveElapsedMs; }
    Owner owner() const { return m_owner; }
    QStringList traits() const { return m_traits; }
    State state() const { return m_state; }
    int star() const { return m_star; }

    //模仿者机制
    ActionType lastAction() const { return m_lastAction; }
    void setLastAction(ActionType action) { m_lastAction = action; }

    //获取和设置获得顺序，用于升星
    long long acquiredOrder() const { return m_acquiredOrder; }
    void setAcquiredOrder(long long order) { m_acquiredOrder = order; }

    EquipmentType equipment() const { return m_equipment; }
    bool hasEquipment() const { return m_equipment != EquipmentType::None; }
    EquipmentType equip(EquipmentType equipmentType);//穿装备并返回之前的装备
    EquipmentType unequip();

    void resetSynergyBonuses();
    void addSynergyAtkBonus(int bonus);
    void addSynergyAttackIntervalReductionMs(int reductionMs);
    void addSynergyDeathHealBonus(int bonus);
    int synergyDeathHealBonus() const { return m_synergyDeathHealBonus; }

    virtual bool isWarGod() const { return false; }//判断是否为战神

    void promoteStar();//升星

    //是否存活
    bool isAlive() const { return m_state != State::Dead && m_hp > 0; }
 
    void setName(const QString& name) { m_name = name; }
    void setPosition(const QPoint& pos) { m_position = pos; }
    void setMaxHp(int maxHp);
    void setHp(int hp);
    void setAtk(int atk);
    void setRange(int range);
    void setMaxMana(int maxMana);
    void setMana(int mana);
    void setAttackIntervalMs(int attackIntervalMs);
    void setMoveIntervalMs(int moveIntervalMs);
    void setAttackElapsedMs(int value);
    void setMoveElapsedMs(int value);
    void setOwner(Owner owner) { m_owner = owner; }
    void setTraits(const QStringList& traits) { m_traits = traits; }
    void setState(State state) { m_state = state; }
    void setStar(int star);

    void heal(int amount);//治疗单位
    void takeDamage(int amount);//单位受伤害
    void gainMana(int amount);
    void resetMana() { m_mana = 0; }
    void advanceCombatTimers(int deltaMs);//推进攻击和冷却时间
    bool canAttackNow() const;//判断是否可攻击
    bool canMoveNow() const;//判断是否可移动
    void consumeAttackCooldown();//消耗攻击冷却
    void consumeMoveCooldown();//消耗移动冷却
    void resetCombatTimers();

    virtual int basicAttackDamage();//返回普攻伤害（使用虚函数便于写枪手时重写）
    virtual bool castSkill(Board& board, const QVector<Unit*>& units);//释放主动技能
    virtual void onAfterBasicAttack(Unit* target, const QVector<Unit*>& units);//普攻后触发的钩子，便于拓展额外效果
    virtual void onAfterMoveStep(Board& board, const QVector<Unit*>& units);//治疗师和战神使用
    virtual bool onBeforeCombatAction(Board& board, const QVector<Unit*>& units);//普通战斗动作前的钩子（便于模仿者使用）
    virtual void onDeath(Board& board, const QVector<Unit*>& units);//死亡治疗（便于殉道者使用）

private:
    static int s_nextId;

    int m_id;
    QString m_name;
    QPoint m_position;
    int m_maxHp;
    int m_hp;
    int m_atk;
    int m_range;
    int m_maxMana;
    int m_mana;
    int m_attackIntervalMs;
    int m_moveIntervalMs;
    int m_attackElapsedMs;
    int m_moveElapsedMs;
    Owner m_owner;
    QStringList m_traits;
    State m_state;
    int m_star;
    ActionType m_lastAction;
    EquipmentType m_equipment;
    int m_synergyAtkBonus;
    int m_synergyAttackIntervalReductionMs;
    int m_synergyDeathHealBonus;
    long long m_acquiredOrder = 0;//无限模式使用long long
};

#endif // UNIT_H
