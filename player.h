#ifndef PLAYER_H
#define PLAYER_H

#include <QList>
#include "core/equipment.h"

class Player
{
public:
    static constexpr int DEFAULT_HP = 100;//初始生命值
    static constexpr int DEFAULT_GOLD = 10;//初始金币数量
    static constexpr int DEFAULT_LEVEL = 1;//初始等级
    static constexpr int DEFAULT_POPULATION_CAP = 3;//初始人口上限
    static constexpr int DEFAULT_STAGE = 1;//初始所在关卡
    static constexpr int MAX_EQUIPMENT_INVENTORY = 10;//装备库存最大容量

    Player();

    int hp() const { return m_hp; }//获取当前生命值
    int gold() const { return m_gold; }
    int level() const { return m_level; }
    int populationCap() const { return m_populationCap; }//获取当前人口上限
    int currentStage() const { return m_currentStage; }//获取当前所在关卡
    int winStreak() const { return m_winStreak; }//获取当前连胜数
    int lossStreak() const { return m_lossStreak; }//获取当前连败数
    bool isAlive() const { return m_hp > 0; }//判断是否存活

    void reset();

    void setHp(int hp);//设置玩家生命值
    void takeDamage(int amount);//受到伤害，扣减生命值

    void setGold(int gold);//设置金币数量
    void addGold(int amount);//增加金币数量
    bool spendGold(int amount);//花费金币

    void setLevel(int level);//设置等级
    void setPopulationCap(int populationCap);//设置人口上限
    void setCurrentStage(int stage);//设置当前关卡
    void advanceStage();//进入下一关卡
    bool upgradePopulation(int cost);//花费指定金币提升人口上限
    static constexpr int POPUPGRADE_COST = 6;//提升人口上限所需的金币花费

    void recordVictory();//记录一场胜利，并刷新连胜/连败状态
    void recordDefeat();//记录一场失败，并刷新连胜/连败状态
    void setStreaks(int winStreak, int lossStreak);//读档时还原连胜/连败状态
    int currentStreakBonusGold() const;//根据当前连胜/连败计算额外金币

    QList<EquipmentType> equipmentInventory() const;//获取装备库存列表
    bool addEquipment(EquipmentType equipmentType);//向装备库存添加一件装备
    EquipmentType removeEquipmentAt(int index);//移除并返回指定下标处的装备
    int equipmentInventorySize() const;//获取当前装备库存中的装备数量
    bool hasEquipmentInventorySpace() const;//判断装备库存是否还有空余位置

    long long nextAcquiredOrder();//获取下一个装备获取顺序编号

private:
    int m_hp;
    int m_gold;
    int m_level;
    int m_populationCap;
    int m_currentStage;
    int m_winStreak;//当前连胜次数
    int m_lossStreak;//当前连败次数
    QList<EquipmentType> m_equipmentInventory;
    long long m_acquireCounter = 0;//装备获取顺序计数器，用于记录获取顺序
};

#endif // PLAYER_H
