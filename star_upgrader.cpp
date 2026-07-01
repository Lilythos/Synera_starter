#include "star_upgrader.h"
#include "bench.h"
#include "board.h"
#include "entity/unit.h"
#include "player.h"

#include <QMap>
#include <algorithm>

namespace StarUpgrader {

void processStarUpgrades(Bench& bench, Board& board, QList<Unit*>& allUnits, Player& player)
{//执行棋子升星合并逻辑
    bool changed = true;

    while (changed) {
        changed = false;
        QMap<QString, QVector<Unit*>> groups;//对棋子进行分组

        for (int i = 0; i < Bench::SIZE; ++i) {
            Unit* u = bench.getUnitAt(i);
            if (!u) continue;
            if (u->isWarGod()) continue;//战神单位不参与升星合并
            
            const QString key = u->name() + QString("|") + QString::number(u->star());//拼接分组键：名称|星级
            groups[key].append(u);
        }

        for (Unit* u : board.units()) {
            if (!u) continue;
            if (u->isWarGod()) continue;
            const QString key = u->name() + QString("|") + QString::number(u->star());
            groups[key].append(u);
        }

        for (auto it = groups.begin(); it != groups.end(); ++it) {
            //查找满足合并条件的同组棋子
            QVector<Unit*>& vec = it.value();
            if (vec.size() < 3) continue;

            //按获取顺序倒序排序，保留最新获取的棋子
            std::sort(vec.begin(), vec.end(), [](Unit* a, Unit* b){
                return a->acquiredOrder() > b->acquiredOrder();
            });

            Unit* keep = vec[0];//保留排序后的第一个棋子

            //移除另外两个棋子
            Unit* remove1 = vec[1];
            Unit* remove2 = vec[2];

            //卸下第一个待移除棋子的装备，并返还给玩家
            const EquipmentType remove1Equipment = remove1->unequip();
            if (remove1Equipment != EquipmentType::None) {
                player.addEquipment(remove1Equipment);
            }

            //卸下第二个待移除棋子的装备，并返还给玩家
            const EquipmentType remove2Equipment = remove2->unequip();
            if (remove2Equipment != EquipmentType::None) {
                player.addEquipment(remove2Equipment);
            }

            //从备战区/棋盘移除第一个棋子
            if (bench.containsUnit(remove1)) {
                bench.removeUnit(remove1);
            } else if (board.containsUnit(remove1)) {
                board.removeUnit(remove1);
            }
            //从全局列表移除并释放内存
            allUnits.removeAll(remove1);
            delete remove1;

            //从备战区/棋盘移除第二个棋子
            if (bench.containsUnit(remove2)) {
                bench.removeUnit(remove2);
            } else if (board.containsUnit(remove2)) {
                board.removeUnit(remove2);
            }
            //从全局列表移除并释放内存
            allUnits.removeAll(remove2);
            delete remove2;

            keep->promoteStar();//保留的棋子执行升星操作

            changed = true;//标记本轮发生合并
            break; 
        }
    }
}

} // namespace