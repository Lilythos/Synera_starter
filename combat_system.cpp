#include "combat_system.h"
#include "targeting.h"
#include "pathfinder.h"
#include <limits>

namespace {

long long distanceSquared(Unit* first, Unit* second)
{//计算两单位距离的平方
    if (!first || !second) {
        return std::numeric_limits<long long>::max();//任一指针为空，返回最大值（视为无限远）
    }

    const QPoint firstPos = first->position();//获取第一个单位的坐标
    const QPoint secondPos = second->position();//获取第二个单位的坐标
    const long long dx = firstPos.x() - secondPos.x();//x轴差值
    const long long dy = firstPos.y() - secondPos.y();//y轴差值
    return dx * dx + dy * dy;//返回距离的平方
}

long long distanceSquaredToPoint(const QPoint& first, const QPoint& second)
{//计算两个棋盘坐标之间距离的平方
    const long long dx = first.x() - second.x();//x轴差值
    const long long dy = first.y() - second.y();//y轴差值
    return dx * dx + dy * dy;//返回距离平方
}

QVector<QPoint> adjacentPositions(const QPoint& pos)
{//获取某格的四个相邻格
    return QVector<QPoint>({
        QPoint(pos.x() - 1, pos.y()),//左
        QPoint(pos.x() + 1, pos.y()),//右
        QPoint(pos.x(), pos.y() - 1),//上
        QPoint(pos.x(), pos.y() + 1)//下
    });
}

bool moveTowardTarget(Board& board, Unit* unit, Unit* target)
{//单位向目标移动一步
    if (!unit || !target) {
        return false;
    }

    const QPoint start = board.positionOf(unit);//当前单位位置
    const QPoint targetPos = target->position();//目标单位位置
    QVector<QPoint> bestPath;
    QPoint bestGoal(-1, -1);//目标位置初始化为无效坐标

    //优先遍历目标周围四个相邻格，寻找最短的可达攻击位置
    for (const QPoint& goal : adjacentPositions(targetPos)) {
        if (!board.isValidPosition(goal) || board.hasUnitAt(goal)) {
            continue;//跳过越界格或已被占用格
        }

        //路径是从自己的当前位置走到临格
        const QVector<QPoint> path = Pathfinder::findPath(board, start, goal);
        if (path.isEmpty()) {
            continue;//无法到达就跳过
        }

        //保留路径最短的终点
        if (bestPath.isEmpty() || path.size() < bestPath.size()) {
            bestPath = path;
            bestGoal = goal;
        }
    }

    if (bestGoal.x() < 0 || bestGoal.y() < 0) {
        //如果敌人身边都被占住，就退而求其次，寻找离目标最近的可达空格
        const long long currentDistance = distanceSquaredToPoint(start, targetPos);//当前到目标的距离
        long long bestGoalDistance = currentDistance;//候选格必须比当前位置更接近目标

        for (int row = 0; row < Board::ROWS; ++row) {
            for (int col = 0; col < Board::COLS; ++col) {
                const QPoint goal(col, row);//候选空格
                if (!board.isValidPosition(goal) || board.hasUnitAt(goal)) {
                    continue;//跳过越界格或被占用格
                }

                const long long goalDistance = distanceSquaredToPoint(goal, targetPos);//候选格到目标的距离
                if (goalDistance >= bestGoalDistance) {
                    continue;//不比当前位置更近，就没有移动价值
                }

                const QVector<QPoint> path = Pathfinder::findPath(board, start, goal);//尝试走到候选格
                if (path.isEmpty()) {
                    continue;//不可达则跳过
                }

                if (bestPath.isEmpty()
                    || goalDistance < bestGoalDistance
                    || (goalDistance == bestGoalDistance && path.size() < bestPath.size())) {
                    bestPath = path;//保存当前最优路径
                    bestGoal = goal;//保存当前最优目标格
                    bestGoalDistance = goalDistance;//更新最近距离
                }
            }
        }
    }

    if (bestGoal.x() < 0 || bestGoal.y() < 0) {
        return false;//没有找到有效目标格
    }

    //向最优目标格移动一步
    return Pathfinder::moveUnitOneStep(board, unit, bestGoal);
}

void removeDeadUnits(Board& board)
{//一键清除死亡单位

    const QVector<Unit*> units = board.units();
    for (Unit* unit : units) {
        //单位存在并且已死亡并且仍在棋盘上，就移除
        if (unit && !unit->isAlive() && board.containsUnit(unit)) {
            board.removeUnit(unit);
        }
    }
}

} // namespace

bool CombatSystem::isInAttackRange(Unit* attacker, Unit* target)
{//判断是否在攻击范围中
    if (!attacker || !target) {
        return false;
    }

    const long long range = attacker->range();

    //比较距离的平方和攻击范围的平方
    return distanceSquared(attacker, target) <= range * range;
}

bool CombatSystem::canBasicAttack(Unit* attacker, Unit* target)
{//判断是否能发动普攻
    return attacker
           && target
           && attacker->isAlive()
           && target->isAlive()
           && attacker->owner() != target->owner()//不是同一阵营
           && isInAttackRange(attacker, target);
}

bool CombatSystem::basicAttack(Unit* attacker, Unit* target, Board* board)
{//执行普通攻击
    if (!canBasicAttack(attacker, target)) {
        return false;
    }

    attacker->setState(Unit::State::Attacking);//切换为攻击状态
    const int damage = attacker->basicAttackDamage();//获取伤害值

    const QPoint targetPosition = target->position();//记录目标位置
    target->takeDamage(damage);//目标受伤
    attacker->onAfterBasicAttack(target, QVector<Unit*>());//触发攻击后效果
    attacker->gainMana(MANA_GAIN_PER_ATTACK);//攻击者回蓝
    
    //记录行为日志，供回放/UI使用
    if (board && attacker->isAlive()) {
        attacker->setLastAction(Unit::ActionType::Attack);   
        board->recordAction(attacker->position(), Unit::ActionType::Attack, damage, targetPosition);
    }
    return true;
}

bool CombatSystem::updateUnit(Unit* unit, Board& board, int deltaMs)
{//单个单位的每帧更新
    if (!unit || !unit->isAlive()) {
        return false;
    }

    unit->advanceCombatTimers(deltaMs);//计时推进

    //满蓝，则放大招
    if (unit->maxMana() > 0 && unit->mana() >= unit->maxMana()) {
        unit->setState(Unit::State::Casting);
        if (unit->castSkill(board, board.units())) {
            unit->resetMana();
            removeDeadUnits(board);
            return true;
        }
    }

    Unit* target = Targeting::selectTarget(unit, board);
    if (!target) {
        unit->setState(Unit::State::Idle);
        return false;
    }

    if (isInAttackRange(unit, target)) {//在攻击范围
        if (unit->canAttackNow()) {//且能够攻击
            if (unit->onBeforeCombatAction(board, QVector<Unit*>())) {//模仿者的钩子
                unit->consumeAttackCooldown();
                return true;
            }

            if (basicAttack(unit, target, &board)) {//执行普攻
                unit->consumeAttackCooldown();

                if (!target->isAlive()) {
                    board.removeUnit(target);
                }//只删单个

                return true;
            }
        } else {
            unit->setState(Unit::State::Attacking);
        }

        return false;
    }

    if (unit->canMoveNow()) {//不在攻击范围内，能移动
        if (moveTowardTarget(board, unit, target)) {
            unit->consumeMoveCooldown();
            unit->setState(Unit::State::Moving);
            return true;
        }
    }

    unit->setState(Unit::State::Moving);
    return false;
}

void CombatSystem::updateBoard(Board& board, int deltaMs)
{//全棋盘每帧更新
    if (deltaMs <= 0) {
        return;//防御性检查，忽略无效时间步
    }

    const QVector<Unit*> units = board.units();
    for (Unit* unit : units) {
        //跳过无效/已死/不在棋盘上的单位
        if (!unit || !unit->isAlive() || !board.containsUnit(unit)) {
            continue;//逐个更新
        }

        updateUnit(unit, board, deltaMs);
    }
}
