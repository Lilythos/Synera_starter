#include "bench.h"

#include <utility>

Bench::Bench()
    : m_slots(SIZE, nullptr)
{}

bool Bench::isValidIndex(int index) const
{//判断是否在备战区范围内
    return index >= 0 && index < m_slots.size();
}

bool Bench::isFull() const
{//判断是否已满
    return emptySlotCount() == 0;
}

bool Bench::isEmpty() const
{//判断是否以空
    return units().isEmpty();
}

int Bench::emptySlotCount() const
{//统计空槽位数量
    int count = 0;
    for (Unit* unit : m_slots) {
        if (!unit) {
            ++count;
        }
    }
    return count;
}

Unit* Bench::getUnitAt(int index) const
{//返回指定槽位上的单位
    return isValidIndex(index) ? m_slots[index] : nullptr;
}

bool Bench::hasUnitAt(int index) const
{//判断指定槽位是否已有单位
    return getUnitAt(index) != nullptr;
}

bool Bench::containsUnit(Unit* unit) const
{//判断棋子是否已在备战区中
    return indexOf(unit) != -1;
}

int Bench::indexOf(Unit* unit) const
{//查找棋子所在的槽位
    if (!unit) {
        return -1;
    }

    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i] == unit) {
            return i;
        }
    }

    return -1;
}

QVector<Unit*> Bench::units() const
{//返回备战区中的所有棋子
    QVector<Unit*> result;
    for (Unit* unit : m_slots) {
        if (unit) {
            result.append(unit);
        }
    }
    return result;
}

bool Bench::addUnit(Unit* unit)
{//将单位加入第一个可用的空槽位
    if (!unit || containsUnit(unit)) {
        return false;
    }

    for (int i = 0; i < m_slots.size(); ++i) {
        if (!m_slots[i]) {
            return addUnitAt(unit, i);
        }
    }

    return false;
}

bool Bench::addUnitAt(Unit* unit, int index)
{//将单位加入指定空槽位
    if (!unit || !isValidIndex(index) || m_slots[index] || containsUnit(unit)) {
        return false;
    }

    m_slots[index] = unit;
    unit->setPosition(QPoint(-1, -1));//标记为不在棋盘上
    return true;
}

bool Bench::moveUnit(int fromIndex, int toIndex)
{//在备战区内部移动单位
    if (!isValidIndex(fromIndex) || !isValidIndex(toIndex) || !m_slots[fromIndex]) {
        return false;
    }

    if (fromIndex == toIndex) {
        return true;
    }

    if (m_slots[toIndex]) {
        return false;
    }

    m_slots[toIndex] = m_slots[fromIndex];
    m_slots[fromIndex] = nullptr;
    return true;
}

bool Bench::swapSlots(int firstIndex, int secondIndex)
{//交换两个备战区槽位
    //确保至少要有一边存在单位
    if (!isValidIndex(firstIndex) || !isValidIndex(secondIndex)) {
        return false;
    }

    if (firstIndex == secondIndex) {
        return m_slots[firstIndex] != nullptr;
    }

    if (!m_slots[firstIndex] && !m_slots[secondIndex]) {
        return false;
    }

    std::swap(m_slots[firstIndex], m_slots[secondIndex]);
    return true;
}

Unit* Bench::removeUnitAt(int index)
{//从指定槽位移除并返回单位
    if (!isValidIndex(index)) {
        return nullptr;
    }

    Unit* unit = m_slots[index];
    m_slots[index] = nullptr;
    if (unit) {
        unit->setPosition(QPoint(-1, -1));//让该槽位变为空
    }
    return unit;
}

bool Bench::removeUnit(Unit* unit)
{//从当前所在槽位中移除指定单位。
    const int index = indexOf(unit);
    if (index == -1) {
        return false;
    }

    removeUnitAt(index);
    return true;
}

void Bench::clear()
{//清空所有槽位
    for (Unit* unit : m_slots) {
        if (unit) {
            unit->setPosition(QPoint(-1, -1));
        }
    }

    std::fill(m_slots.begin(), m_slots.end(), nullptr);
}
