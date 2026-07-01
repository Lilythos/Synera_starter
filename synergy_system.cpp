#include "synergy_system.h"
#include <QHash>
#include <QString>

namespace {//里面两个函数只能当前 cpp 文件内部使用

QHash<QString, int> countPlayerTraits(const Board& board)
{//统计我方所有羁绊数量
    QHash<QString, int> counts;//用于存放场上该羁绊棋子总数
    for (Unit* unit : board.units()) {
        //不存在的/属于敌方的/已死亡的棋子：跳过
        if (!unit || unit->owner() != Unit::Owner::PlayerCtrl || !unit->isAlive()) {
            continue;
        }

        for (const QString& trait : unit->traits()) {
            counts[trait] += 1;
        }
    }
    return counts;
}

bool hasTraitCount(const QHash<QString, int>& counts, const QString& trait, int threshold)
{//判断羁绊是否达到激活门槛
    return counts.value(trait, 0) >= threshold;
}

} // namespace

void SynergySystem::refreshSynergyBuffs(Board& board)
{//刷新羁绊属性加成：枪手、殉道者、模仿者、战意

    //先统计羁绊数
    const QHash<QString, int> counts = countPlayerTraits(board);
    //判断羁绊是否能激活
    const bool gunnerActive = hasTraitCount(counts, QString::fromUtf8("枪手"), 2);
    const int gunnerAtkBonus = hasTraitCount(counts, QString::fromUtf8("枪手"), 4) ? 20 : 10;
    const bool martyrActive = hasTraitCount(counts, QString::fromUtf8("殉道者"), 2);
    const bool mimicActive = hasTraitCount(counts, QString::fromUtf8("模仿者"), 2);
    const bool battleSpiritActive = hasTraitCount(counts, QString::fromUtf8("战意"), 3);

    for (Unit* unit : board.units()) {
        if (!unit || unit->owner() != Unit::Owner::PlayerCtrl || !unit->isAlive()) {
            continue;
        }

        unit->resetSynergyBonuses();//清空上一轮战斗的羁绊残留

        if (gunnerActive) {//全体效果
            unit->addSynergyAtkBonus(gunnerAtkBonus);
        }
        if (mimicActive && unit->traits().contains(QString::fromUtf8("模仿者"))) {
            unit->addSynergyAtkBonus(10);
        }
        if (martyrActive && unit->traits().contains(QString::fromUtf8("殉道者"))) {
            unit->addSynergyDeathHealBonus(5);
        }
        if (battleSpiritActive) {//全体效果
            unit->addSynergyAttackIntervalReductionMs(100);
        }
    }
}

void SynergySystem::applyPreCombatEffects(Board& board)
{//战斗开局羁绊：治疗师回血
    
    const QHash<QString, int> counts = countPlayerTraits(board);
    if (!hasTraitCount(counts, QString::fromUtf8("治疗师"), 2)) {
        return;
    }

    for (Unit* unit : board.units()) {
        if (!unit || unit->owner() != Unit::Owner::PlayerCtrl || !unit->isAlive()) {
            continue;
        }

        unit->heal(25);
    }
}

QStringList SynergySystem::activeSynergyDescriptions(const Board& board)
{//呈现羁绊给玩家
    const QHash<QString, int> counts = countPlayerTraits(board);
    QStringList descriptions;//存放所有激活羁绊的文字说明

    const int gunnerCount = counts.value(QString::fromUtf8("枪手"), 0);
    if (gunnerCount >= 2) {
        const int bonus = gunnerCount >= 4 ? 20 : 10;
        descriptions.append(QString::fromUtf8("枪手 %1: 所有我方单位 ATK +%2")
                                .arg(gunnerCount)
                                .arg(bonus));
    }

    const int healerCount = counts.value(QString::fromUtf8("治疗师"), 0);
    if (healerCount >= 2) {
        descriptions.append(QString::fromUtf8("治疗师 %1: 战斗开始前所有我方单位恢复 25 HP")
                                .arg(healerCount));
    }

    const int martyrCount = counts.value(QString::fromUtf8("殉道者"), 0);
    if (martyrCount >= 2) {
        descriptions.append(QString::fromUtf8("殉道者 %1: 殉道者死亡治疗 +5")
                                .arg(martyrCount));
    }

    const int mimicCount = counts.value(QString::fromUtf8("模仿者"), 0);
    if (mimicCount >= 2) {
        descriptions.append(QString::fromUtf8("模仿者 %1: 模仿者 ATK +10")
                                .arg(mimicCount));
    }

    const int battleSpiritCount = counts.value(QString::fromUtf8("战意"), 0);
    if (battleSpiritCount >= 3) {
        descriptions.append(QString::fromUtf8("战意 %1: 所有我方单位攻击间隔 -100ms")
                                .arg(battleSpiritCount));
    }

    return descriptions;
}
