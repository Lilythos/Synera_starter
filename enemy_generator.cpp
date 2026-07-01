#include "enemy_generator.h"

namespace {

int normalizedStage(int stage)
{
    return stage < 1 ? 1 : stage;//避免出现第 0 关或负数关
}

int scaledValue(int baseValue, int stage)
{//根据关卡对敌人数值进行成长缩放
    const int safeStage = normalizedStage(stage);//安全关卡数，保证至少为 1
    const int growthTier = (safeStage - 1) / 4;//每 4 关提升一个成长档位
    return baseValue * (100 + growthTier * 12) / 100;//每个档位增加约 12%
}

EnemySpawn makeEnemy(const QString& name,
                     int maxHp,
                     int atk,
                     int range,
                     int maxMana,
                     const QPoint& position,
                     const QStringList& traits,
                     int stage)
{//根据模板数据创建一个敌人生成配置
    EnemySpawn spawn;//敌人的生成数据，后续由 Game 转成真正的 Unit
    spawn.name = name;
    spawn.maxHp = scaledValue(maxHp, stage);//生命值随关卡成长
    spawn.atk = scaledValue(atk, stage);//攻击力随关卡成长
    spawn.range = range;
    spawn.maxMana = maxMana;
    spawn.position = position;
    spawn.traits = traits;
    return spawn;
}

} // namespace

QVector<EnemySpawn> EnemyGenerator::generate(int stage) const
{//按当前关卡生成敌方阵容
    const int safeStage = normalizedStage(stage);
    QVector<EnemySpawn> spawns;

    if (safeStage == 1) {
        spawns.append(makeEnemy(QString::fromUtf8("训练敌人"),
                                180,
                                18,
                                1,
                                60,
                                QPoint(3, 0),
                                QStringList({QString::fromUtf8("野兽")}),
                                safeStage));
        return spawns;
    }

    if (safeStage == 2) {
        spawns.append(makeEnemy(QString::fromUtf8("前排敌人"),
                                220,
                                24,
                                1,
                                60,
                                QPoint(2, 0),
                                QStringList({QString::fromUtf8("战士")}),
                                safeStage));
        spawns.append(makeEnemy(QString::fromUtf8("远程敌人"),
                                180,
                                28,
                                3,
                                60,
                                QPoint(5, 0),
                                QStringList({QString::fromUtf8("射手")}),
                                safeStage));
        return spawns;
    }

    spawns.append(makeEnemy(QString::fromUtf8("前排敌人"),
                            260,
                            28,
                            1,
                            60,
                            QPoint(2, 0),
                            QStringList({QString::fromUtf8("战士")}),
                            safeStage));
    spawns.append(makeEnemy(QString::fromUtf8("远程敌人"),
                            210,
                            34,
                            3,
                            60,
                            QPoint(5, 0),
                            QStringList({QString::fromUtf8("射手")}),
                            safeStage));
    spawns.append(makeEnemy(QString::fromUtf8("精英敌人"),
                            320,
                            40,
                            1,
                            80,
                            QPoint(3, 1),
                            QStringList({QString::fromUtf8("精英")}),
                            safeStage));

    return spawns;
}
