#include "targeting.h"

namespace {

long long distanceSquared(Unit* first, Unit* second)
{//计算两个单位之间的距离平方
    const QPoint firstPos = first->position();//第一个单位的位置
    const QPoint secondPos = second->position();//第二个单位的位置
    const long long dx = firstPos.x() - secondPos.x();//横坐标差值
    const long long dy = firstPos.y() - secondPos.y();//纵坐标差值
    return dx * dx + dy * dy;
}

bool isCandidate(Unit* attacker, Unit* candidate)
{//判断某个单位是否可以作为攻击者的有效目标候选
    return candidate//候选单位存在
           && candidate != attacker//不是攻击者自身
           && candidate->isAlive()//候选单位处于存活状态
           && candidate->owner() != attacker->owner();//与攻击者不属于同一阵营
}

bool isBetterCandidate(Unit* attacker, Unit* candidate, Unit* currentBest)
{//判断候选目标是否比当前已选出的最佳目标更优，依次按距离、生命值、坐标比较
    if (!currentBest) {
        //当前还没有选出任何目标，候选直接成为最佳
        return true;
    }

    const long long candidateDistance = distanceSquared(attacker, candidate);//候选目标到攻击者的距离平方
    const long long bestDistance = distanceSquared(attacker, currentBest);//当前最佳目标到攻击者的距离平方

    if (candidateDistance != bestDistance) {
        //距离不同，优先选择更近的目标
        return candidateDistance < bestDistance;
    }

    if (candidate->hp() != currentBest->hp()) {
        //距离相同时，优先选择生命值更高的目标
        return candidate->hp() > currentBest->hp();
    }

    if (candidate->position().x() != currentBest->position().x()) {
        //距离和生命值都相同时，优先选择横坐标更小的目标
        return candidate->position().x() < currentBest->position().x();
    }

    return candidate->position().y() > currentBest->position().y();//以上条件均相同时，优先选择纵坐标更大的目标作为最终决胜条件
}

} // namespace

Unit* Targeting::selectTarget(Unit* attacker, const Board& board)
{//在棋盘上为攻击者选择一个目标单位
    return selectTarget(attacker, board.units());
}

Unit* Targeting::selectTarget(Unit* attacker, const QVector<Unit*>& units)
{//在给定的单位列表中为攻击者选择最优目标
    if (!attacker || !attacker->isAlive()) {
        //攻击者为空或已死亡，无法选择目标
        return nullptr;
    }

    Unit* best = nullptr;//当前选出的最佳目标
    for (Unit* unit : units) {
        if (!isCandidate(attacker, unit)) {
            //该单位不是有效的候选目标
            continue;
        }

        if (isBetterCandidate(attacker, unit, best)) {
            //该候选比当前最佳目标更优，更新最佳目标
            best = unit;
        }
    }

    return best;//返回最终选出的目标单位
}