#include "board.h"

#include <algorithm>

Board::Board()
    : m_cells(ROWS * COLS, nullptr)
{}

void Board::addUnit(Unit* unit, const QPoint& pos)
{//把单位放到指定棋盘格
    const int targetIdx = indexOf(pos);//目标格在一维数组中的下标
    //单位为空或目标坐标非法，直接失败
    if (!unit || targetIdx < 0) {
        return;
    }

    Unit* targetUnit = m_cells[targetIdx];//目标格当前已有的单位
    if (targetUnit && targetUnit != unit) {
        return;
    }

    if (m_unitToPosition.contains(unit)) {
        const QPoint oldPos = m_unitToPosition.value(unit);//单位原位置
        const int oldIdx = indexOf(oldPos);//原位置对应的一维下标
        if (oldIdx == targetIdx) {
            unit->setPosition(pos);
            return;
        }

        if (oldIdx >= 0 && m_cells[oldIdx] == unit) {
            m_cells[oldIdx] = nullptr;
            m_lastActions.remove(oldPos);
        }
    }

    m_cells[targetIdx] = unit;//目标格占用该单位
    m_unitToPosition[unit] = pos;//记录单位到坐标的映射
    unit->setPosition(pos);//同步单位自身坐标
    m_lastActions.remove(pos);//新单位进入该格后，旧动作记录失效
}

void Board::removeUnit(Unit* unit)
{//从棋盘上移除单位
    if (!unit || !m_unitToPosition.contains(unit)) {
        return;
    }

    const QPoint pos = m_unitToPosition.value(unit);
    const int idx = indexOf(pos);
    if (idx >= 0 && m_cells[idx] == unit) {
        m_cells[idx] = nullptr;
    }

    if (!unit->isAlive()) {
        unit->onDeath(*this, QVector<Unit*>());//死亡移除时触发死亡技能
    }

    m_unitToPosition.remove(unit);//移除单位到坐标的映射
    unit->setPosition(QPoint(-1, -1));//标记单位不在棋盘上

    if (unit->lastAction() == Unit::ActionType::DeathHeal) {
        //死亡治疗保留动作记录，供模仿者使用
        if (!m_lastActions.contains(pos)) {
            recordAction(pos, unit->lastAction(), 0, QPoint(-1, -1));
        }
    } else {
        m_lastActions.remove(pos);
    }
}

Unit* Board::getUnitAt(const QPoint& pos) const
{//查询指定格子上的单位
    const int idx = indexOf(pos);
    return idx < 0 ? nullptr : m_cells[idx];
}

bool Board::hasUnitAt(const QPoint& pos) const
{//判断指定格子是否被占用
    return getUnitAt(pos) != nullptr;
}

bool Board::containsUnit(Unit* unit) const
{//判断某单位是否在棋盘上
    return unit && m_unitToPosition.contains(unit);//非空且存在坐标映射
}

QPoint Board::positionOf(Unit* unit) const
{//查询单位当前位置
    if (!containsUnit(unit)) {
        return QPoint(-1, -1);
    }
    return m_unitToPosition.value(unit);
}

Unit::ActionType Board::lastActionAt(const QPoint& pos) const
{//查询某格上一次动作类型
    return lastActionRecordAt(pos).type;
}

void Board::recordAction(const QPoint& pos, Unit::ActionType action)
{//记录只有动作类型的简化行为
    recordAction(pos, action, 0, QPoint(-1, -1));//默认数值为 0，目标位置无效
}

QHash<QPoint, Board::ActionRecord> Board::actionRecords() const
{//导出所有动作记录，用于存档
    return m_lastActions;
}

void Board::restoreActionRecord(const QPoint& pos, const ActionRecord& record)
{//读档时恢复某个格子的动作记录
    if (!isValidPosition(pos) || record.type == Unit::ActionType::None) {
        m_lastActions.remove(pos);
        return;
    }

    m_lastActions[pos] = record;
}

Board::ActionRecord Board::lastActionRecordAt(const QPoint& pos) const
{//查询某格完整的上一次动作记录
    if (!isValidPosition(pos)) {
        return ActionRecord();
    }

    auto it = m_lastActions.constFind(pos);//查找保存过的动作记录
    if (it != m_lastActions.constEnd()) {
        return it.value();//有显式记录时直接返回
    }

    Unit* unit = getUnitAt(pos);//如果没有显式记录，尝试读取当前单位状态
    if (unit) {
        ActionRecord record;//临时动作记录
        record.type = unit->lastAction();//使用单位自身保存的上一次动作
        return record;//返回临时记录
    }

    return ActionRecord();//空格子返回默认空记录
}

void Board::recordAction(const QPoint& pos, Unit::ActionType action, int amount, const QPoint& targetPosition)
{//记录某格发生的动作，包括动作数值和目标位置
    if (!isValidPosition(pos)) {
        return;
    }

    if (action == Unit::ActionType::None) {
        m_lastActions.remove(pos);
    } else {
        ActionRecord record;
        record.type = action;//动作类型
        record.amount = amount;//伤害或治疗数值
        record.targetPosition = targetPosition;//动作目标位置
        m_lastActions[pos] = record;//保存到对应格子
    }
}

bool Board::moveUnit(Unit* unit, const QPoint& target)
{//把棋盘上的单位移动到目标空格
    if (!containsUnit(unit) || !isValidPosition(target)) {
        return false;
    }

    const QPoint source = positionOf(unit);
    if (source == target) {
        return true;
    }

    const int sourceIdx = indexOf(source);//原位置下标
    const int targetIdx = indexOf(target);//目标位置下标
    if (sourceIdx < 0 || targetIdx < 0 || m_cells[targetIdx]) {
        return false;
    }

    if (m_cells[sourceIdx] == unit) {
        m_cells[sourceIdx] = nullptr;
        m_lastActions.remove(source);//移动后原格动作记录失效
    }

    m_cells[targetIdx] = unit;//占用目标格
    m_unitToPosition[unit] = target;//更新单位位置映射
    unit->setPosition(target);//同步单位自身坐标
    m_lastActions.remove(target);//目标格旧动作记录失效
    return true;
}

bool Board::swapUnits(Unit* first, Unit* second)
{//交换两个棋盘单位的位置
    if (!containsUnit(first) || !containsUnit(second) || first == second) {
        return false;
    }

    const QPoint firstPos = positionOf(first);//第一个单位位置
    const QPoint secondPos = positionOf(second);//第二个单位位置
    const int firstIdx = indexOf(firstPos);//第一个位置下标
    const int secondIdx = indexOf(secondPos);//第二个位置下标

    if (firstIdx < 0 || secondIdx < 0) {
        return false;
    }

    std::swap(m_cells[firstIdx], m_cells[secondIdx]);//交换格子占用
    m_unitToPosition[first] = secondPos;//更新第一个单位位置映射
    m_unitToPosition[second] = firstPos;//更新第二个单位位置映射
    first->setPosition(secondPos);//同步第一个单位坐标
    second->setPosition(firstPos);//同步第二个单位坐标
    std::swap(m_lastActions[firstPos], m_lastActions[secondPos]);//交换动作记录，保持跟位置一致
    return true;
}

bool Board::swapUnitsAt(const QPoint& firstPos, const QPoint& secondPos)
{//按两个坐标交换单位
    if (!isValidPosition(firstPos) || !isValidPosition(secondPos) || firstPos == secondPos) {
        return false;
    }

    Unit* first = getUnitAt(firstPos);//第一个坐标上的单位
    Unit* second = getUnitAt(secondPos);//第二个坐标上的单位
    if (!first || !second) {
        return false;
    }

    return swapUnits(first, second);
}

QVector<Unit*> Board::units() const
{//返回当前棋盘上的所有单位
    QVector<Unit*> result;
    result.reserve(m_unitToPosition.size());//提前预留空间，减少扩容

    for (auto it = m_unitToPosition.cbegin(); it != m_unitToPosition.cend(); ++it) {
        result.append(it.key());//映射表的 key 就是单位指针
    }

    return result;//返回棋盘单位列表
}

bool Board::isValidPosition(const QPoint& pos) const
{
    return pos.x() >= 0 && pos.x() < COLS && pos.y() >= 0 && pos.y() < ROWS;
}

bool Board::isPlayerHalf(const QPoint& pos) const
{
    return pos.y() >= ROWS / 2;
}

void Board::clear()
{//清空整个棋盘
    for (auto it = m_unitToPosition.cbegin(); it != m_unitToPosition.cend(); ++it) {
        if (it.key()) {
            it.key()->setPosition(QPoint(-1, -1));//清空前先把单位标记为不在棋盘上
        }
    }

    std::fill(m_cells.begin(), m_cells.end(), nullptr);//清空所有格子占用
    m_unitToPosition.clear();//清空单位位置映射
    m_lastActions.clear();//清空动作记录
}

int Board::indexOf(const QPoint& pos) const
{//把二维棋盘坐标转换为一维数组下标
    if (!isValidPosition(pos)) {
        return -1;//非法坐标返回 -1
    }
    return pos.y() * COLS + pos.x();//按行优先展开为一维下标
}
